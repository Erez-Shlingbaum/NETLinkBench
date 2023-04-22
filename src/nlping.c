/*
 * (C) 2009 by Pablo Neira Ayuso <pneira@us.es>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <errno.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <stdint.h>
#include <signal.h>
#include <sys/time.h>

#include "lib.h"
#include "nlbench.h"

static void usage(char *prog)
{
	printf("Usage: %s [options]\n", prog);
	printf("-u\tnetlink (default is NETLINK_BENCHMARK)\n");
	printf("-h\tshow this help\n");
}

int main(int argc, char *argv[])
{
	int fd, i, bytes, args[3];
	struct nlmsghdr *nlh;
	char buf[128];
	int c, unit = NETLINK_BENCHMARK;

	while((c = getopt(argc, argv, "hu:")) != EOF) {
		switch(c) {
		case 'u':
			unit = atoi(optarg);
			break;
		case 'h':
			usage(argv[0]);
			exit(EXIT_SUCCESS);
			break;
		}
	}

	fd = libnetlink_create_socket(unit, 0);
	if (fd < 0) {
		perror("socket");
		exit(EXIT_FAILURE);
	}

	nlh = libnetlink_newmsg(NLMSG_NOOP, 0, 0);
	if (nlh == NULL) {
		perror("calloc");
		exit(EXIT_FAILURE);
	}

	printf("sending netlink NOOP\n");

	struct timeval start, stop;

again:
	gettimeofday(&start, NULL);

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

	gettimeofday(&stop, NULL);

	timersub(&stop, &start, &start);

	printf("%ld usecs\n", start.tv_usec);
	sleep(1);
	goto again;
}
