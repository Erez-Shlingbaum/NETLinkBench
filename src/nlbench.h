#ifndef _NLBENCH_H_
#define _NLBENCH_H_

#ifndef NETLINK_BENCHMARK
#define NETLINK_BENCHMARK 25
#endif

enum nlbench_msg_types {
	NLBENCH_MSG_BASE = 16,
	NLBENCH_MSG_UNICAST_PROCESS,
	NLBENCH_MSG_UNICAST_INTERRUPT,
	NLBENCH_MSG_MULTICAST_PROCESS,
	NLBENCH_MSG_MULTICAST_INTERRUPT,
	NLBENCH_MSG_MAX
};

enum nlbench_attr {
	NLB_UNSPEC,
	NLB_SIZE,		/* size of the message */
	NLB_NUM,		/* number of messages */
	NLB_RANDOM,		/* size of random distribution (in secs) */
	NLB_PID,		/* destination port id for unicast */
	__NLB_MAX
};
#define NLB_MAX			(__NLB_MAX - 1)

#define NLBENCH_GRP_NONE	0
#define NLBENCH_GRP		1
#define __NLBENCH_GRP_MAX	NLBENCH_GRP
#define NLBENCH_GRP_MAX		(__NLBENCH_GRP_MAX - 1)

#endif
