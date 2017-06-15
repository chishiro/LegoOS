/*
 * Copyright (c) 2016-2017 Wuklab, Purdue University. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _KERNEL_SCHED_SCHED_H_
#define _KERNEL_SCHED_SCHED_H_

#include <lego/rbtree.h>
#include <lego/sched.h>
#include <lego/sched_rt.h>

/* task_struct::on_rq states: */
#define TASK_ON_RQ_QUEUED	1
#define TASK_ON_RQ_MIGRATING	2

static inline int task_on_rq_queued(struct task_struct *p)
{
	return p->on_rq == TASK_ON_RQ_QUEUED;
}

static inline int task_on_rq_migrating(struct task_struct *p)
{
	return p->on_rq == TASK_ON_RQ_MIGRATING;
}

static inline int idle_policy(int policy)
{
	return policy == SCHED_IDLE;
}
static inline int fair_policy(int policy)
{
	return policy == SCHED_NORMAL || policy == SCHED_BATCH;
}

static inline int rt_policy(int policy)
{
	return policy == SCHED_FIFO || policy == SCHED_RR;
}

static inline bool valid_policy(int policy)
{
	return idle_policy(policy) || fair_policy(policy) ||
		rt_policy(policy);
}

static inline int task_has_rt_policy(struct task_struct *p)
{
	return rt_policy(p->policy);
}

/* Priority-queue data structure of the RT scheduling class: */
struct rt_prio_array {
	DECLARE_BITMAP(bitmap, MAX_RT_PRIO+1); /* include 1 bit for delimiter */
	struct list_head queue[MAX_RT_PRIO];
};

/* CFS-related fields in a runqueue */
struct cfs_rq {
	unsigned int		nr_running, h_nr_running;

	u64			exec_clock;
	u64			min_vruntime;

	struct rb_root		tasks_timeline;
	struct rb_node		*rb_leftmost;

	/*
	 * 'curr' points to currently running entity on this cfs_rq.
	 * It is set to NULL otherwise (i.e when none are currently running).
	 */
	struct sched_entity 	*curr, *next, *last, *skip;

};

/* Real-Time classes' related field in a runqueue: */
struct rt_rq {
	struct rt_prio_array	active;
	unsigned int		rt_nr_running;

#ifdef CONFIG_SMP
#endif

	int			rt_queued;
	int			rt_throttled;
	int			rt_time;
	int			rt_runtime;
	/* nests inside the rq lock: */
	spinlock_t		rt_runtime_lock;
};

/* Deadline class' related fields in a runqueue */
struct dl_rq {
	/* runqueue is an rbtree, ordered by deadline */
	struct rb_root		rb_root;
	struct rb_node		*rb_leftmost;

	unsigned long		dl_nr_running;
};

/*
 * This is the main, per-CPU runqueue data structure.
 *
 * Locking rule: those places that want to lock multiple runqueues
 * (such as the load balancing or the thread migration code), lock
 * acquire operations must be ordered by ascending &runqueue.
 */
struct rq {
	/* runqueue lock: */
	spinlock_t		lock;
	unsigned int		nr_running;
	unsigned int		nr_switches;

	struct cfs_rq		cfs;
	struct rt_rq		rt;
	struct dl_rq		dl;

	/*
	 * This is part of a global counter where only the total sum
	 * over all CPUs matters. A task can increase this counter on
	 * one CPU and if it got migrated afterwards it may decrease
	 * it on another CPU. Always updated under the runqueue lock:
	 */
	unsigned long		nr_uninterruptible;

	struct task_struct	*curr, *idle, *stop;

	unsigned int		clock_skip_update;
	u64			clock;
	u64			clock_task;

	atomic_t		nr_iowait;

#ifdef CONFIG_SMP
	/* cpu of this runqueue: */
	int			cpu;
	int			online;
#endif
};

static inline int cpu_of(struct rq *rq)
{
#ifdef CONFIG_SMP
	return rq->cpu;
#else
	return 0;
#endif
}

DECLARE_PER_CPU_SHARED_ALIGNED(struct rq, runqueues);

#define cpu_rq(cpu)		(&per_cpu(runqueues, (cpu)))
#define this_rq()		this_cpu_ptr(&runqueues)
#define task_rq(p)		cpu_rq(task_cpu(p))
#define cpu_curr(cpu)		(cpu_rq(cpu)->curr)

void init_cfs_rq(struct cfs_rq *cfs_rq);
void init_rt_rq(struct rt_rq *rt_rq);
void init_dl_rq(struct dl_rq *dl_rq);

static inline u64 rq_clock(struct rq *rq)
{
	return rq->clock;
}

static inline u64 rq_clock_task(struct rq *rq)
{
	return rq->clock_task;
}

static inline void add_nr_running(struct rq *rq, unsigned count)
{
	rq->nr_running += count;
}

static inline void sub_nr_running(struct rq *rq, unsigned count)
{
	rq->nr_running -= count;
}

static inline int task_current(struct rq *rq, struct task_struct *p)
{
	return rq->curr == p;
}

static inline int task_running(struct rq *rq, struct task_struct *p)
{
#ifdef CONFIG_SMP
	return p->on_cpu;
#else
	return task_current(rq, p);
#endif
}

#define ENQUEUE_WAKEUP		0x01
#define ENQUEUE_HEAD		0x02
#ifdef CONFIG_SMP
#define ENQUEUE_WAKING		0x04	/* sched_class::task_waking was called */
#else
#define ENQUEUE_WAKING		0x00
#endif
#define ENQUEUE_REPLENISH	0x08
#define ENQUEUE_RESTORE		0x10

#define DEQUEUE_SLEEP		0x01
#define DEQUEUE_SAVE		0x02

#define RETRY_TASK		((void *)-1UL)

struct sched_class {
	const struct sched_class *next;

	void (*enqueue_task)(struct rq *rq, struct task_struct *p, int flags);
	void (*dequeue_task)(struct rq *rq, struct task_struct *p, int flags);
	void (*yield_task)(struct rq *rq);
	bool (*yield_to_task)(struct rq *rq, struct task_struct *p, bool preempt);

	void (*check_preempt_curr)(struct rq *rq, struct task_struct *p, int flags);

	/*
	 * It is the responsibility of the pick_next_task() method that will
	 * return the next task to call put_prev_task() on the @prev task or
	 * something equivalent.
	 *
	 * May return RETRY_TASK when it finds a higher prio class has runnable
	 * tasks.
	 */
	struct task_struct * (*pick_next_task)(struct rq *rq,
					       struct task_struct *prev);
	void (*put_prev_task)(struct rq *rq, struct task_struct *p);

#ifdef CONFIG_SMP
	int  (*select_task_rq)(struct task_struct *p, int task_cpu, int sd_flag, int flags);
	void (*migrate_task_rq)(struct task_struct *p);

	void (*task_waking)(struct task_struct *task);
	void (*task_woken)(struct rq *this_rq, struct task_struct *task);

	void (*set_cpus_allowed)(struct task_struct *p,
				 const struct cpumask *newmask);
#endif

	void (*set_curr_task)(struct rq *rq);
	void (*task_tick)(struct rq *rq, struct task_struct *p, int queued);
	void (*task_fork)(struct task_struct *p);
	void (*task_dead)(struct task_struct *p);

	/*
	 * The switched_from() call is allowed to drop rq->lock, therefore we
	 * cannot assume the switched_from/switched_to pair is serliazed by
	 * rq->lock. They are however serialized by p->pi_lock.
	 */
	void (*switched_from)(struct rq *this_rq, struct task_struct *task);
	void (*switched_to)(struct rq *this_rq, struct task_struct *task);
	void (*prio_changed)(struct rq *this_rq, struct task_struct *task,
			     int oldprio);

	unsigned int (*get_rr_interval)(struct rq *rq,
					struct task_struct *task);

	void (*update_curr)(struct rq *rq);
};

static inline void put_prev_task(struct rq *rq, struct task_struct *prev)
{
	prev->sched_class->put_prev_task(rq, prev);
}

#define sched_class_highest	(&stop_sched_class)
#define for_each_class(class) \
   for (class = sched_class_highest; class; class = class->next)

extern const struct sched_class stop_sched_class;
extern const struct sched_class rt_sched_class;
extern const struct sched_class fair_sched_class;
extern const struct sched_class idle_sched_class;

void set_cpus_allowed_common(struct task_struct *p, const struct cpumask *new_mask);

#ifdef CONFIG_SMP
void do_set_cpus_allowed(struct task_struct *p,
			 const struct cpumask *new_mask);

int set_cpus_allowed_ptr(struct task_struct *p,
			 const struct cpumask *new_mask);
#else
static inline void do_set_cpus_allowed(struct task_struct *p,
				       const struct cpumask *new_mask)
{
}
static inline int set_cpus_allowed_ptr(struct task_struct *p,
				       const struct cpumask *new_mask)
{
	if (!cpumask_test_cpu(0, new_mask))
		return -EINVAL;
	return 0;
}
#endif

#define RQCF_REQ_SKIP	0x01
#define RQCF_ACT_SKIP	0x02

static inline void rq_clock_skip_update(struct rq *rq, bool skip)
{
	if (skip)
		rq->clock_skip_update |= RQCF_REQ_SKIP;
	else
		rq->clock_skip_update &= ~RQCF_REQ_SKIP;
}

void resched_curr(struct rq *rq);

#endif /* _KERNEL_SCHED_SCHED_H_ */
