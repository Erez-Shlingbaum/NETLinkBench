/*
 * (C) 2009 by Pablo Neira Ayuso <pneira@us.es>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <stdint.h>
#include <signal.h>
#include <sched.h>
#include <getopt.h>

#include "lib.h"
#include "nlbench.h"

static unsigned int events, cur_events;
static unsigned int enobufs, cur_enobufs;
static unsigned int errors, cur_errors;
static int lines, iterations, max_iterations = ~0U;
FILE *ofd;

static void usage(char *prog)
{
	printf("Usage: %s [options]\n", prog);
	printf("-b\tbuffer size\n");
	printf("-s\tscheduler (\"rr\", \"fifo\")\n");
	printf("-n\tnice value (if normal scheduling is used)\n");
	printf("-u\tnetlink socket unit (default is netlink_benchmark)\n");
 	printf("-g\tnetlink group (default is NLBENCH_GRP)\n");
	printf("-c\tCPU affinity (starting by zero)\n");
	printf("-i\titerations\n");
	printf("-f\tfile to store the output\n");
	printf("-h\tshow this help\n");
}

static void sigint_handler(int foo)
{
	char buf[128];

	sprintf(buf, "# total_events=%u total_enobufs=%u\n", events, enobufs);
	printf("%s", buf);
	if (ofd != NULL) {
		fputs(buf, ofd);
		fclose(ofd);
	}
	exit(EXIT_FAILURE);
}

static void handler(int foo)
{
	char buf[128];

	if (lines % 22 == 0) {
		sprintf(buf, "# events/s\tenobufs/s\terrors/s\n");
		printf("%s", buf);
		if (ofd != NULL)
			fputs(buf, ofd);
	}

	lines++;
	alarm(1);
	sprintf(buf, "%10u\t%10u\t%10u\n", cur_events, cur_enobufs, cur_errors);
	printf("%s", buf);
	if (ofd != NULL)
		fputs(buf, ofd);

	cur_events = cur_enobufs = cur_errors = 0;

	if (max_iterations != ~0U && ++iterations == max_iterations)
		sigint_handler(0);
}

int main(int argc, char *argv[])
{
	int fd;
	int sched = 0, buffersize = 0, niceval = 0, cpuaffinity = -1;
	int unit = NETLINK_BENCHMARK, group = NLBENCH_GRP;
	char buf[4096];
	char c, *file;

	printf("# pid=%u\n", getpid());

	signal(SIGINT, sigint_handler);
	signal(SIGTERM, sigint_handler);

	while((c = getopt(argc, argv, "b:s:n:hu:g:c:i:f:")) != EOF) {
		switch(c) {
		case 'b':
			buffersize = atoi(optarg);
			break;
		case 's':
			if (strcmp(optarg, "rr") == 0) {
				sched = SCHED_RR;
				printf("# using RR scheduler\n");
			} else if (strcmp(optarg, "fifo") == 0) {
				sched = SCHED_FIFO;
				printf("# using FIFO scheduler\n");
			} else {
				fprintf(stderr, "Unknown scheduler `%s'\n",
					optarg);
				exit(EXIT_FAILURE);
			}
			break;
		case 'n':
			niceval = atoi(optarg);
			break;
		case 'u':
			unit = atoi(optarg);
			printf("# listening to netlink unit %d\n", unit);
			break;
		case 'g':
			group = atoi(optarg);
			printf("# listening to group %d\n", group);
			break;
		case 'c':
			cpuaffinity = atoi(optarg);
			break;
		case 'i':
			max_iterations = atoi(optarg);
			break;
		case 'f':
			ofd = fopen(optarg, "w");
			break;
		case 'h':
			usage(argv[0]);
			exit(EXIT_SUCCESS);
		}
	}

	if (sched != SCHED_OTHER) {
		struct sched_param schedparam = {
			.sched_priority = sched_get_priority_max(sched),
		};

		if (sched_setscheduler(0, sched, &schedparam) == -1) {
			perror("sched");
			exit(EXIT_FAILURE);
		}
	} else if (nice) {
		printf("# setting nice to %d\n", niceval);
		nice(niceval);
	}

	if (cpuaffinity >= 0) {
		cpu_set_t cpuset;
		int ret;

		CPU_ZERO(&cpuset);
		CPU_SET(cpuaffinity, &cpuset);
		ret = sched_setaffinity(getpid(), sizeof(cpu_set_t), &cpuset);
		if (ret < 0) {
			perror("sched_setaffinity");
			exit(EXIT_FAILURE);
		}
		printf("# setting CPU affinity to `%d'\n", cpuaffinity);
	}

	fd = libnetlink_create_socket(unit, group);
	if (fd < 0) {
		perror("socket");
		exit(EXIT_FAILURE);
	}

	if (buffersize > 0) {
		socklen_t socklen;
		int ret;

		ret = setsockopt(fd, SOL_SOCKET, SO_RCVBUFFORCE,
				 &buffersize, sizeof(socklen_t));
		if (ret < 0) {
			perror("setsockopt");
			exit(EXIT_FAILURE);
		}
		getsockopt(fd, SOL_SOCKET, SO_RCVBUF, &buffersize, &socklen);
		printf("# using buffer size: %d\n", buffersize);
	}

	signal(SIGALRM, handler);
	alarm(1);

	while (1) {
		if (libnetlink_recv(fd, buf, sizeof(buf)) < 0) {
			if (errno == ENOBUFS) {
				enobufs++;
				cur_enobufs++;
				continue;
			}
			errors++;
			cur_errors++;
		}
		events++;
		cur_events++;
	}
}
