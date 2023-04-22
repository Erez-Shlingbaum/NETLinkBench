/*
 * (C) 2009 by Pablo Neira Ayuso <pneira@us.es>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Description: very very simple netlink library for netlinkbench
 */

#include <stdlib.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <string.h>

#include "lib.h"

int
libnetlink_create_socket(int id, unsigned int groups)
{
	int fd, ret;
	struct sockaddr_nl local;

	fd = socket(AF_NETLINK, SOCK_RAW, id);
	if (fd < 0)
		return -1;

	memset(&local, 0, sizeof(struct sockaddr_nl));
	local.nl_family = AF_NETLINK;
	local.nl_groups = groups;

	ret = bind(fd, (struct sockaddr *) &local, sizeof(local));
	if (ret < 0)
		return -1;

	return fd;
}

int
libnetlink_destroy_socket(int fd)
{
	close(fd);
}

int
libnetlink_send(int fd, struct nlmsghdr *nlh)
{
	return send(fd, nlh, nlh->nlmsg_len, 0);
}

int
libnetlink_recv(int fd, void *data, int size)
{
	return recv(fd, data, size, 0);
}

struct nlmsghdr *libnetlink_newmsg(int type, unsigned int flags, int size)
{
	struct nlmsghdr *nlh;

	nlh = calloc(sizeof(struct nlmsghdr) + size, 1);
	if (nlh == NULL)
		return NULL;

	nlh->nlmsg_len = NLMSG_LENGTH(0);
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK | flags;
        nlh->nlmsg_type = type;
	return nlh;
}

void
libnetlink_addattr(struct nlmsghdr *nlh, int type, const void *data, int alen)
{
	struct nlattr *attr = NLMSG_TAIL(nlh);
	int len = NLA_LENGTH(alen);

	attr->nla_type	= type;
	attr->nla_len	= len;
	memcpy(NLA_DATA(attr), data, alen);
	nlh->nlmsg_len = NLMSG_ALIGN(nlh->nlmsg_len) + NLA_ALIGN(len);
}
