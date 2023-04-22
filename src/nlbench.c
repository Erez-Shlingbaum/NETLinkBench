/*
 * (C) 2009 by Pablo Neira Ayuso <pneira@us.es>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <net/sock.h>
#include <linux/skbuff.h>
#include <linux/timer.h>
#include <linux/netlink.h>
#include <linux/random.h>
#include "nlbench.h"

static struct sock *nlbench;

struct nlbench_obj {
	struct timer_list	timeout;
	u32			msg_size;
	u32			dst_pid;
};

static struct sk_buff *nlbench_msg_alloc(int size, int type, gfp_t flags)
{
	struct sk_buff *skb;
	struct nlmsghdr *nlh;
	char *data;

	skb = alloc_skb(NLMSG_GOODSIZE, flags);
	if (skb == NULL)
		goto errout;

	/* we reserve space for an empty payload */
	nlh = nlmsg_put(skb, 0, 0, type, size, 0);
	if (nlh == NULL)
		goto errout_nlmsg;

	data = nlmsg_data(nlh);
	/* no payload data is currently added */
	nlmsg_end(skb, nlh);

	return skb;

errout_nlmsg:
	kfree(skb);
errout:
	return NULL;
}

static int nlbench_ucast_pro_handler(struct nlattr *cda[])
{
	int ret = 0;
	u32 num_msgs, msg_size, dst_pid, i;

	if (!cda[NLB_NUM] || !cda[NLB_SIZE] || !cda[NLB_PID])
		return -EINVAL;

	msg_size = nla_get_u32(cda[NLB_SIZE]);
	if (msg_size < 0 || msg_size > NLMSG_GOODSIZE)
		return -E2BIG;

	num_msgs = nla_get_u32(cda[NLB_NUM]);
	dst_pid = nla_get_u32(cda[NLB_PID]);

	for (i=0; i<num_msgs; i++) {
		struct sk_buff *skb;

		skb = nlbench_msg_alloc(msg_size,
					NLBENCH_MSG_UNICAST_PROCESS,
					GFP_KERNEL);
		if (skb == NULL) {
			/* continue but report to user-space that we
			 * are losing message due to allocation failures,
			 * not because of netlink itself */
			ret = -ENOMEM;
			continue;
		}
		netlink_unicast(nlbench, skb, dst_pid, MSG_DONTWAIT);
	}
	return ret;
}

static int nlbench_mcast_pro_handler(struct nlattr *cda[])
{
	int ret = 0;
	u32 num_msgs, msg_size, i;

	if (!cda[NLB_NUM] || !cda[NLB_SIZE])
		return -EINVAL;

	msg_size = nla_get_u32(cda[NLB_SIZE]);
	if (msg_size < 0 || msg_size > NLMSG_GOODSIZE)
		return -E2BIG;

	num_msgs = nla_get_u32(cda[NLB_NUM]);

	for (i=0; i<num_msgs; i++) {
		struct sk_buff *skb;

		skb = nlbench_msg_alloc(msg_size,
					NLBENCH_MSG_MULTICAST_PROCESS,
					GFP_KERNEL);
		if (skb == NULL) {
			/* continue but report to user-space that we
			 * are losing message due to allocation failures,
			 * not because of netlink itself */
			ret = -ENOMEM;
			netlink_set_err(nlbench, 0, NLBENCH_GRP, -ENOBUFS);
			continue;
		}
		netlink_broadcast(nlbench, skb, 0, NLBENCH_GRP, GFP_KERNEL);
	}
	return ret;
}

static void nlbench_ucast(struct timer_list* foo)
{
	struct nlbench_obj *obj = (struct nlbench_obj *) foo;
	struct sk_buff *skb;

	skb = nlbench_msg_alloc(obj->msg_size,
				NLBENCH_MSG_UNICAST_INTERRUPT,
				GFP_ATOMIC);
	if (skb == NULL)
		goto errout;

	kfree(obj);
	netlink_unicast(nlbench, skb, obj->dst_pid, MSG_DONTWAIT);
errout:
	return;
}

static int nlbench_ucast_int_handler(struct nlattr *cda[])
{
	struct nlbench_obj *obj;
	u32 delay, randomn, num_msgs, msg_size, dst_pid, i;

	if (!cda[NLB_RANDOM] || !cda[NLB_NUM] ||
	    !cda[NLB_SIZE] || !cda[NLB_PID])
		return -EINVAL;

	randomn = nla_get_u32(cda[NLB_RANDOM]);

	msg_size = nla_get_u32(cda[NLB_SIZE]);
	if (msg_size < 0 || msg_size > NLMSG_GOODSIZE)
		return -E2BIG;

	num_msgs = nla_get_u32(cda[NLB_NUM]);
	dst_pid = nla_get_u32(cda[NLB_PID]);

	for (i=0; i<num_msgs; i++) {
		obj = kzalloc(sizeof(struct nlbench_obj), GFP_KERNEL);
		if (obj == NULL)
			return -ENOMEM;

		/* use a random distribution to distribute timers */
		if (randomn == 0)
			delay = 0;
		else
			delay = prandom_u32() % (randomn * HZ);

		obj->msg_size = msg_size;
		obj->dst_pid = dst_pid;
		timer_setup(&obj->timeout, nlbench_ucast, 0);
		obj->timeout.expires = jiffies + delay;
		add_timer(&obj->timeout);
	}
	return 0;
}

static void nlbench_mcast(struct timer_list * data)
{
	struct nlbench_obj *obj = (struct nlbench_obj *)data;
	struct sk_buff *skb;

	skb = nlbench_msg_alloc(obj->msg_size,
				NLBENCH_MSG_MULTICAST_INTERRUPT,
				GFP_ATOMIC);
	if (skb == NULL)
		goto errout;

	kfree(obj);
	netlink_broadcast(nlbench, skb, 0, NLBENCH_GRP, GFP_ATOMIC);
	return;
errout:
	netlink_set_err(nlbench, 0, NLBENCH_GRP, -ENOBUFS);
}

static int nlbench_mcast_int_handler(struct nlattr *cda[])
{
	struct nlbench_obj *obj;
	u32 delay, random_, num_msgs, msg_size, i;

	if (!cda[NLB_RANDOM] || !cda[NLB_NUM] || !cda[NLB_SIZE])
		return -EINVAL;

	random_ = nla_get_u32(cda[NLB_RANDOM]);

	msg_size = nla_get_u32(cda[NLB_SIZE]);
	if (msg_size < 0 || msg_size > NLMSG_GOODSIZE)
		return -E2BIG;

	num_msgs = nla_get_u32(cda[NLB_NUM]);

	for (i=0; i<num_msgs; i++) {
		obj = kzalloc(sizeof(struct nlbench_obj), GFP_KERNEL);
		if (obj == NULL)
			return -ENOMEM;

		/* use a random distribution to distribute timers */
		if (random_ == 0)
			delay = 0;
		else
			delay = prandom_u32() % (random_ * HZ);

		obj->msg_size = msg_size;
		timer_setup(&obj->timeout, nlbench_mcast, 0);
		obj->timeout.expires = jiffies + delay;
		add_timer(&obj->timeout);
	}
	return 0;
}

static int nlbench_rcv_handle(const struct nlmsghdr *nlh, struct nlattr *cda[])
{
	int ret = -EOPNOTSUPP;

	switch(nlh->nlmsg_type) {
		case NLBENCH_MSG_MULTICAST_INTERRUPT:
			ret = nlbench_mcast_int_handler(cda);
			break;
		case NLBENCH_MSG_UNICAST_INTERRUPT:
			ret = nlbench_ucast_int_handler(cda);
			break;
		case NLBENCH_MSG_MULTICAST_PROCESS:
			ret = nlbench_mcast_pro_handler(cda);
			break;
		case NLBENCH_MSG_UNICAST_PROCESS:
			ret = nlbench_ucast_pro_handler(cda);
			break;
	}
	return ret;
}

static int nlbench_rcv_msg(struct sk_buff *skb, struct nlmsghdr *nlh, struct netlink_ext_ack *)
{
	int err;
	int min_len = NLMSG_SPACE(0);
	struct nlattr *cda[NLB_MAX+1];

	// if (security_netlink_recv(skb))
		// return -EPERM;

	if (nlh->nlmsg_len < min_len)
		return -EINVAL;

	{
		struct nlattr *attr = (void *)nlh + NLMSG_ALIGN(min_len);
		int attrlen = nlh->nlmsg_len - NLMSG_ALIGN(min_len);

		err = nla_parse(cda, NLB_MAX, attr, attrlen, NULL, NULL);
		if (err < 0)
			return err;
	}

	return nlbench_rcv_handle(nlh, cda);
}

static void
nlbench_rcv(struct sk_buff *skb)
{
	netlink_rcv_skb(skb, &nlbench_rcv_msg);
}

static int __init nlbench_init(void)
{
	struct netlink_kernel_cfg cfg = {
    	.input = nlbench_rcv,
	};
	nlbench = netlink_kernel_create(&init_net, NETLINK_BENCHMARK, &cfg);
	if (nlbench == NULL) {
		printk("netlinkbech: cannot create netlink socket.\n");
		return -ENOMEM;
	}
	printk("netlinkbech loaded.\n");
	return 0;
}

static void __exit nlbench_exit(void)
{
	printk("netlinkbench: removing module.\n");
	netlink_kernel_release(nlbench);
}

module_init(nlbench_init);
module_exit(nlbench_exit);

MODULE_AUTHOR("Pablo Neira Ayuso <pneira@us.es>");
MODULE_LICENSE("GPL");
