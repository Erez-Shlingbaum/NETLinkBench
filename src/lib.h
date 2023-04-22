#ifndef _LIB_H_
#define _LIB_H_

int libnetlink_create_socket(int id, unsigned int groups);
int libnetlink_destroy_socket(int id);
int libnetlink_send(int fd, struct nlmsghdr *nlh);
int libnetlink_recv(int fd, void *data, int size);

struct nlmsghdr *libnetlink_newmsg(int type, unsigned int flags, int size);

#define NLA_ALIGNTO     4
#define NLA_ALIGN(len)  (((len) + NLA_ALIGNTO - 1) & ~(NLA_ALIGNTO - 1))
#define NLA_LENGTH(len) (NLA_ALIGN(sizeof(struct nlattr)) + (len))
#define NLA_DATA(nfa)   ((void *)(((char *)(nfa)) + NLA_LENGTH(0)))

#define NLMSG_TAIL(nlh) \
(((void *) (nlh)) + NLMSG_ALIGN((nlh)->nlmsg_len))

#endif
