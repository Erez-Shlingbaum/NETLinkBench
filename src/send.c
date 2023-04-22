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
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <stdint.h>
#include <signal.h>
#include <sched.h>
#include <getopt.h>

#include "lib.h"
#include "nlbench.h"

static void usage(char *prog)
{
	printf("%s [options]\n", prog);
	printf("-t\ttype (\"[uni|multi]cast-[process|interrupt]\")\n");
	printf("-n\tnumber of messages\n");
	printf("-s\tsize of messages (in bytes)\n");
	printf("-r\trandom distribution (in secs)\n");
	printf("-c\tCPU affinity (starting by zero)\n");
	printf("-p\tPort ID (only for unicast)\n");
	printf("-h\tshow this help\n");
}

int main(int argc, char *argv[])
{
	int fd, i, bytes, args[4], flags = 0, cpuaffinity = -1;
	int type = 0;
	struct nlmsghdr *nlh;
	char buf[128], c;

	while((c = getopt(argc, argv, "t:n:s:r:c:p:h")) != EOF) {
	switch(c) {
	case 'n':
		args[0] = atoi(optarg);
		flags |= (1 << 0);
		break;
	case 's':
		args[1] = atoi(optarg);
		flags |= (1 << 1);
		break;
	case 'r':
		args[2] = atoi(optarg);
		flags |= (1 << 2);
		break;
	case 'p':
		args[3] = atoi(optarg);
		flags |= (1 << 3);
		break;
	case 't':
		if (strncmp("unicast-interrupt", optarg, strlen(optarg)) == 0) {
			type = NLBENCH_MSG_UNICAST_INTERRUPT;
		} else if (strncmp("multicast-interrupt",
			   optarg, strlen(optarg)) == 0) {
			type = NLBENCH_MSG_MULTICAST_INTERRUPT;
		} else if (strncmp("unicast-process",
			   optarg, strlen(optarg)) == 0) {
			type = NLBENCH_MSG_UNICAST_PROCESS;
		} else if (strncmp("multicast-process",
			   optarg, strlen(optarg)) == 0) {
			type = NLBENCH_MSG_MULTICAST_PROCESS;
		} else {
			printf("unknown type `%s'\n", optarg);
			exit(EXIT_FAILURE);
		}
		break;
	case 'c':
		cpuaffinity = atoi(optarg);
		break;
	case 'h':
		usage(argv[0]);
		exit(EXIT_SUCCESS);
		}
	}

	if (type == 0) {
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}
	if ((type == NLBENCH_MSG_UNICAST_INTERRUPT && flags != 0xf) ||
	    (type == NLBENCH_MSG_MULTICAST_INTERRUPT && flags != 0x7) ||
	    (type == NLBENCH_MSG_UNICAST_PROCESS && flags != 0xb) ||
	    (type == NLBENCH_MSG_MULTICAST_PROCESS && flags != 0x3)) {
		fprintf(stderr, "ERROR: wrong option combination!\n");
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	printf("num_msgs=%u size=%u randomsecs=%u\n",
		args[0], args[1], args[2]);

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
		printf("setting CPU affinity to `%d'\n", cpuaffinity);
	}

	fd = libnetlink_create_socket(NETLINK_BENCHMARK, 0);
	if (fd < 0) {
		perror("socket");
		exit(EXIT_FAILURE);
	}

	nlh = libnetlink_newmsg(type, 0, 1024);
	if (nlh == NULL) {
		perror("calloc");
		exit(EXIT_FAILURE);
	}

	libnetlink_addattr(nlh, NLB_NUM, &args[0], sizeof(int));
	libnetlink_addattr(nlh, NLB_SIZE, &args[1], sizeof(int));
	libnetlink_addattr(nlh, NLB_RANDOM, &args[2], sizeof(int));
	if (flags & (1 << 3))
		libnetlink_addattr(nlh, NLB_PID, &args[3], sizeof(int));

	if (libnetlink_send(fd, nlh) < 0) {
		perror("send");
		exit(EXIT_FAILURE);
	}

	if (libnetlink_recv(fd, buf, sizeof(buf)) < 0) {
		perror("recv");
		exit(EXIT_FAILURE);
	}

	{
		struct nlmsghdr *nlh = (struct nlmsghdr *)buf;
		struct nlmsgerr *err = NLMSG_DATA(nlh);
		if (nlh->nlmsg_type == NLMSG_ERROR && err->error != 0) {
			printf("Error: %s\n", strerror(-err->error));
			exit(EXIT_FAILURE);
		}
	}

	printf("Request succesfully sent.\n");
}
