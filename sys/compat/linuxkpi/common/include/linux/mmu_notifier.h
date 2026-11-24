/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2017 Mark Johnston <markj@FreeBSD.org>
 * All rights reserved.
 * Copyright (c) 2026 The FreeBSD Foundation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _LINUXKPI_LINUX_MMU_NOTIFIER_H_
#define	_LINUXKPI_LINUX_MMU_NOTIFIER_H_

#include <linux/compiler.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/interval_tree.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/mm_types.h>
#include <linux/rcupdate.h>

/*
 * MMU notifiers, see Linux include/linux/mmu_notifier.h and
 * mm/mmu_notifier.c.  They are implemented on top of the vm_map
 * notifiers of the FreeBSD VM: the notifier state of an mm_struct is bound
 * to the vmspace of the process that first subscribes, and invalidations
 * are reported when the mappings of that vmspace are removed, write
 * protected (including copy-on-write at fork) or discarded with
 * madvise(2).  Invalidations are always blockable.
 */

struct mmu_notifier;
struct mmu_notifier_range;
struct mmu_interval_notifier;
struct mmu_notifier_subscriptions;

enum mmu_notifier_event {
	MMU_NOTIFY_UNMAP = 0,
	MMU_NOTIFY_CLEAR,
	MMU_NOTIFY_PROTECTION_VMA,
	MMU_NOTIFY_PROTECTION_PAGE,
	MMU_NOTIFY_SOFT_DIRTY,
	MMU_NOTIFY_RELEASE,
	MMU_NOTIFY_MIGRATE,
	MMU_NOTIFY_EXCLUSIVE,
};

#define	MMU_NOTIFIER_RANGE_BLOCKABLE	(1 << 0)

struct mmu_notifier_range {
	struct mm_struct *mm;
	unsigned long start;
	unsigned long end;
	unsigned flags;
	enum mmu_notifier_event event;
	void *owner;
};

struct mmu_notifier_ops {
	void	(*release)(struct mmu_notifier *, struct mm_struct *);
	int	(*invalidate_range_start)(struct mmu_notifier *,
		    const struct mmu_notifier_range *);
	void	(*invalidate_range_end)(struct mmu_notifier *,
		    const struct mmu_notifier_range *);
	struct mmu_notifier *(*alloc_notifier)(struct mm_struct *);
	void	(*free_notifier)(struct mmu_notifier *);
};

struct mmu_notifier {
	struct hlist_node hlist;
	const struct mmu_notifier_ops *ops;
	struct mm_struct *mm;
	struct rcu_head rcu;
	unsigned int users;
};

struct mmu_interval_notifier_ops {
	bool	(*invalidate)(struct mmu_interval_notifier *,
		    const struct mmu_notifier_range *, unsigned long);
};

struct mmu_interval_notifier {
	struct interval_tree_node interval_tree;
	const struct mmu_interval_notifier_ops *ops;
	struct mm_struct *mm;
	struct hlist_node deferred_item;
	unsigned long invalidate_seq;
	/* LinuxKPI private, see linux_mmu_notifier.c. */
	int lkpi_deferred_op;
	struct vm_page **lkpi_held;
};

static inline int
mm_has_notifiers(struct mm_struct *mm)
{
	return (READ_ONCE(mm->notifier_subscriptions) != NULL);
}

static inline bool
mmu_notifier_range_blockable(const struct mmu_notifier_range *range)
{
	return ((range->flags & MMU_NOTIFIER_RANGE_BLOCKABLE) != 0);
}

int	mmu_notifier_register(struct mmu_notifier *, struct mm_struct *);
int	__mmu_notifier_register(struct mmu_notifier *, struct mm_struct *);
void	mmu_notifier_unregister(struct mmu_notifier *, struct mm_struct *);
struct mmu_notifier *mmu_notifier_get_locked(const struct mmu_notifier_ops *,
	    struct mm_struct *);
void	mmu_notifier_put(struct mmu_notifier *);
void	mmu_notifier_synchronize(void);

static inline struct mmu_notifier *
mmu_notifier_get(const struct mmu_notifier_ops *ops, struct mm_struct *mm)
{
	struct mmu_notifier *mn;

	down_write(&mm->mmap_sem);
	mn = mmu_notifier_get_locked(ops, mm);
	up_write(&mm->mmap_sem);
	return (mn);
}

int	mmu_interval_notifier_insert(struct mmu_interval_notifier *,
	    struct mm_struct *, unsigned long, unsigned long,
	    const struct mmu_interval_notifier_ops *);
int	mmu_interval_notifier_insert_locked(struct mmu_interval_notifier *,
	    struct mm_struct *, unsigned long, unsigned long,
	    const struct mmu_interval_notifier_ops *);
void	mmu_interval_notifier_remove(struct mmu_interval_notifier *);
unsigned long mmu_interval_read_begin(struct mmu_interval_notifier *);

/*
 * Called by the invalidate() callback, under the driver lock that is also
 * held around mmu_interval_read_retry().  cur_seq is always odd.
 */
static inline void
mmu_interval_set_seq(struct mmu_interval_notifier *mni, unsigned long cur_seq)
{
	WRITE_ONCE(mni->invalidate_seq, cur_seq);
}

/*
 * End a read side critical section started by mmu_interval_read_begin().
 * Returns true if the range was invalidated in between and the caller must
 * retry.  Must be called with the driver lock held.
 */
static inline bool
mmu_interval_read_retry(struct mmu_interval_notifier *mni, unsigned long seq)
{
	return (mni->invalidate_seq != seq);
}

/* Lockless and racy variant of mmu_interval_read_retry(). */
static inline bool
mmu_interval_check_retry(struct mmu_interval_notifier *mni, unsigned long seq)
{
	return (READ_ONCE(mni->invalidate_seq) != seq);
}

void	linux_mmu_notifier_subscriptions_destroy(struct mm_struct *);

#endif /* _LINUXKPI_LINUX_MMU_NOTIFIER_H_ */
