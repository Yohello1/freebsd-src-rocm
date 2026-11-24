/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The FreeBSD Foundation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * MMU notifiers and HMM for LinuxKPI, built on vm_map notifiers.
 *
 * The notifier state of an mm_struct, struct mmu_notifier_subscriptions,
 * is allocated on first use and bound to the vmspace of the subscribing
 * process by registering a vm_map notifier on its map.  The binding holds
 * a reference on the mm_struct and lasts until the vmspace is torn down,
 * when the release callback runs the release() methods of the classic
 * notifiers, invalidates all interval notifiers with MMU_NOTIFY_RELEASE
 * and drops the reference.  The mm_struct of a process outlives exec(2);
 * the state is then bound again to the new vmspace on the next
 * subscription.
 *
 * Invalidations follow the sequence scheme of Linux mm/mmu_notifier.c:
 * invalidate_seq is odd while at least one invalidation is active.  While
 * any invalidation is active the interval tree and the list of classic
 * notifiers are not modified, so that they can be walked without holding
 * the subscription lock while calling methods that sleep; insertions and
 * removals are deferred until the last active invalidation ends, at which
 * point invalidate_seq becomes even again and waiters are woken up.
 *
 * Locking: (l) subscription lock.  (a) Only modified with the subscription
 * lock held and no invalidation active.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/refcount.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_extern.h>
#include <vm/vm_map.h>
#include <vm/vm_page.h>

#include <linux/hmm.h>
#include <linux/mm.h>
#include <linux/mmu_notifier.h>
#include <linux/rcupdate.h>
#include <linux/sched.h>

#define	LKPI_MNI_DEFER_NONE	0
#define	LKPI_MNI_DEFER_INSERT	1
#define	LKPI_MNI_DEFER_REMOVE	2

struct mmu_notifier_subscriptions {
	struct mtx		lock;
	struct mm_struct	*mm;		/* (c) */
	struct vm_map_notifier	vmn;
	struct vmspace		*vm;		/* (l) bound vmspace */
	bool			binding;	/* (l) */
	u_int			active;		/* (l) active invalidations */
	unsigned long		invalidate_seq;	/* (l) odd while active */
	struct rb_root_cached	itree;		/* (a) interval notifiers */
	struct hlist_head	deferred;	/* (l) deferred itree changes */
	struct hlist_head	list;		/* (a) classic notifiers */
	struct hlist_head	pending;	/* (l) classic, unlisted */
};

static MALLOC_DEFINE(M_LKPI_MMU, "lkpimmu", "LinuxKPI MMU notifiers");

static void lkpi_mn_vm_invalidate_start(struct vm_map_notifier *, vm_map_t,
    vm_offset_t, vm_offset_t, enum vm_map_notify_event);
static void lkpi_mn_vm_invalidate_end(struct vm_map_notifier *, vm_map_t,
    vm_offset_t, vm_offset_t, enum vm_map_notify_event);
static void lkpi_mn_vm_release(struct vm_map_notifier *, vm_map_t);

static const struct vm_map_notifier_ops lkpi_mn_vm_ops = {
	.invalidate_start = lkpi_mn_vm_invalidate_start,
	.invalidate_end = lkpi_mn_vm_invalidate_end,
	.release = lkpi_mn_vm_release,
};

static struct mmu_notifier_subscriptions *
lkpi_mn_subscriptions(struct mm_struct *mm)
{
	struct mmu_notifier_subscriptions *subs;

	subs = (void *)atomic_load_acq_ptr(
	    (volatile uintptr_t *)&mm->notifier_subscriptions);
	KASSERT(subs != NULL, ("%s: mm %p has no subscriptions", __func__,
	    mm));
	return (subs);
}

/*
 * Return the subscriptions of mm, allocating them if needed, and make sure
 * that they are bound to a vmspace.  Binding is only possible from a thread
 * of the process that owns mm.
 */
static int
lkpi_mn_subscribe(struct mm_struct *mm,
    struct mmu_notifier_subscriptions **subsp)
{
	struct mmu_notifier_subscriptions *subs;
	struct vmspace *vm;
	struct proc *p;

	subs = (void *)atomic_load_acq_ptr(
	    (volatile uintptr_t *)&mm->notifier_subscriptions);
	if (subs == NULL) {
		subs = malloc(sizeof(*subs), M_LKPI_MMU, M_WAITOK | M_ZERO);
		mtx_init(&subs->lock, "lkpimmu", NULL, MTX_DEF);
		subs->mm = mm;
		subs->vmn.ops = &lkpi_mn_vm_ops;
		subs->itree = RB_ROOT_CACHED;
		INIT_HLIST_HEAD(&subs->deferred);
		INIT_HLIST_HEAD(&subs->list);
		INIT_HLIST_HEAD(&subs->pending);
		if (atomic_cmpset_rel_ptr(
		    (volatile uintptr_t *)&mm->notifier_subscriptions,
		    (uintptr_t)NULL, (uintptr_t)subs) == 0) {
			mtx_destroy(&subs->lock);
			free(subs, M_LKPI_MMU);
			subs = lkpi_mn_subscriptions(mm);
		}
	}

	mtx_lock(&subs->lock);
	while (subs->binding)
		mtx_sleep(&subs->binding, &subs->lock, 0, "lkpimnb", 0);
	if (subs->vm != NULL) {
		mtx_unlock(&subs->lock);
		*subsp = subs;
		return (0);
	}
	p = curproc;
	if (current->mm != mm || (p->p_flag & P_SYSTEM) != 0) {
		mtx_unlock(&subs->lock);
		return (-EINVAL);
	}
	subs->binding = true;
	mtx_unlock(&subs->lock);

	/* The process cannot drop its own vmspace reference under us. */
	vm = p->p_vmspace;
	mmgrab(mm);
	vm_map_notifier_register(&vm->vm_map, &subs->vmn);

	mtx_lock(&subs->lock);
	subs->vm = vm;
	subs->binding = false;
	wakeup(&subs->binding);
	mtx_unlock(&subs->lock);
	*subsp = subs;
	return (0);
}

/*
 * Return a reference on the vmspace of mm, or NULL if there is none: mm
 * belongs to the calling process, or is bound to a vmspace by MMU notifier
 * subscriptions.
 */
struct vmspace *
linux_mm_vmspace_acquire(struct mm_struct *mm)
{
	struct mmu_notifier_subscriptions *subs;
	struct vmspace *vm;
	struct proc *p;

	p = curproc;
	if (current->mm == mm && (p->p_flag & P_SYSTEM) == 0)
		return (vmspace_acquire_ref(p));
	subs = (void *)atomic_load_acq_ptr(
	    (volatile uintptr_t *)&mm->notifier_subscriptions);
	if (subs == NULL)
		return (NULL);
	mtx_lock(&subs->lock);
	vm = subs->vm;
	if (vm != NULL && !refcount_acquire_if_not_zero(&vm->vm_refcnt))
		vm = NULL;
	mtx_unlock(&subs->lock);
	return (vm);
}

/* Called from linux_mm_dtor() when the last reference to mm is dropped. */
void
linux_mmu_notifier_subscriptions_destroy(struct mm_struct *mm)
{
	struct mmu_notifier_subscriptions *subs;

	subs = mm->notifier_subscriptions;
	if (subs == NULL)
		return;
	KASSERT(subs->vm == NULL && !subs->binding && subs->active == 0,
	    ("%s: subscriptions %p of mm %p in use", __func__, subs, mm));
	KASSERT(RB_EMPTY_ROOT(&subs->itree.rb_root) &&
	    hlist_empty(&subs->deferred) && hlist_empty(&subs->list) &&
	    hlist_empty(&subs->pending),
	    ("%s: subscriptions %p of mm %p not empty", __func__, subs, mm));
	mtx_destroy(&subs->lock);
	free(subs, M_LKPI_MMU);
	mm->notifier_subscriptions = NULL;
}

/* Drop the pages held by hmm_range_fault() for [start, end) of mni. */
static void
lkpi_mni_unhold(struct mmu_notifier_subscriptions *subs,
    struct mmu_interval_notifier *mni, unsigned long start, unsigned long end)
{
	vm_page_t m;
	unsigned long base, i, first, last;

	mtx_assert(&subs->lock, MA_OWNED);
	if (mni->lkpi_held == NULL)
		return;
	base = trunc_page(mni->interval_tree.start);
	first = atop(trunc_page(MAX(start, mni->interval_tree.start)) - base);
	last = atop(MIN(end - 1, mni->interval_tree.last) - base);
	for (i = first; i <= last; i++) {
		m = mni->lkpi_held[i];
		if (m != NULL) {
			mni->lkpi_held[i] = NULL;
			vm_page_unwire(m, PQ_ACTIVE);
		}
	}
}

static unsigned long
lkpi_mn_invalidate_begin(struct mmu_notifier_subscriptions *subs)
{
	unsigned long seq;

	mtx_lock(&subs->lock);
	if (subs->active++ == 0)
		subs->invalidate_seq++;
	seq = subs->invalidate_seq;
	mtx_unlock(&subs->lock);
	MPASS((seq & 1) != 0);
	return (seq);
}

static void
lkpi_mn_free_rcu(struct rcu_head *rcu)
{
	struct mmu_notifier *mn;
	struct mm_struct *mm;

	mn = container_of(rcu, struct mmu_notifier, rcu);
	mm = mn->mm;
	mn->ops->free_notifier(mn);
	mmdrop(mm);
}

/*
 * End an invalidation.  The last active one applies the deferred interval
 * tree changes, removes the classic notifiers that were put (or all of
 * them, for release), and wakes up waiters.
 */
static void
lkpi_mn_invalidate_end(struct mmu_notifier_subscriptions *subs, bool release)
{
	HLIST_HEAD(freelist);
	struct mmu_interval_notifier *mni;
	struct mmu_notifier *mn;
	struct hlist_node *tmp;

	mtx_lock(&subs->lock);
	KASSERT(subs->active > 0, ("%s: no active invalidation", __func__));
	if (--subs->active > 0) {
		mtx_unlock(&subs->lock);
		return;
	}
	hlist_for_each_entry_safe(mni, tmp, &subs->deferred, deferred_item) {
		if (mni->lkpi_deferred_op == LKPI_MNI_DEFER_INSERT)
			interval_tree_insert(&mni->interval_tree, &subs->itree);
		else
			interval_tree_remove(&mni->interval_tree, &subs->itree);
		mni->lkpi_deferred_op = LKPI_MNI_DEFER_NONE;
		hlist_del(&mni->deferred_item);
	}
	hlist_for_each_entry_safe(mn, tmp, &subs->list, hlist) {
		if (mn->users == 0 || release) {
			hlist_del_init(&mn->hlist);
			if (mn->users == 0)
				hlist_add_head(&mn->hlist, &freelist);
		}
	}
	hlist_for_each_entry_safe(mn, tmp, &subs->pending, hlist) {
		hlist_del(&mn->hlist);
		if (mn->users == 0)
			hlist_add_head(&mn->hlist, &freelist);
		else
			hlist_add_head(&mn->hlist, &subs->list);
	}
	subs->invalidate_seq++;
	wakeup(subs);
	mtx_unlock(&subs->lock);

	hlist_for_each_entry_safe(mn, tmp, &freelist, hlist) {
		hlist_del_init(&mn->hlist);
		call_rcu(&mn->rcu, lkpi_mn_free_rcu);
	}
}

static void
lkpi_mn_invalidate_itree(struct mmu_notifier_subscriptions *subs,
    const struct mmu_notifier_range *range, unsigned long seq)
{
	struct interval_tree_node *node;
	struct mmu_interval_notifier *mni;
	bool ret;

	for (node = interval_tree_iter_first(&subs->itree, range->start,
	    range->end - 1); node != NULL;
	    node = interval_tree_iter_next(node, range->start,
	    range->end - 1)) {
		mni = container_of(node, struct mmu_interval_notifier,
		    interval_tree);
		ret = mni->ops->invalidate(mni, range, seq);
		/* Can only fail for non-blockable ranges. */
		WARN_ON_ONCE(!ret);
		mtx_lock(&subs->lock);
		lkpi_mni_unhold(subs, mni, range->start, range->end);
		mtx_unlock(&subs->lock);
	}
}

static enum mmu_notifier_event
lkpi_mn_event(enum vm_map_notify_event event)
{
	switch (event) {
	case VM_MAP_NOTIFY_UNMAP:
		return (MMU_NOTIFY_UNMAP);
	case VM_MAP_NOTIFY_PROTECT:
		return (MMU_NOTIFY_PROTECTION_VMA);
	case VM_MAP_NOTIFY_CLEAR:
	default:
		return (MMU_NOTIFY_CLEAR);
	}
}

static void
lkpi_mn_vm_invalidate_start(struct vm_map_notifier *n, vm_map_t map __unused,
    vm_offset_t start, vm_offset_t end, enum vm_map_notify_event event)
{
	struct mmu_notifier_subscriptions *subs;
	struct mmu_notifier_range range;
	struct mmu_notifier *mn;
	unsigned long seq;
	int error;

	subs = __containerof(n, struct mmu_notifier_subscriptions, vmn);
	range = (struct mmu_notifier_range){
		.mm = subs->mm,
		.start = start,
		.end = end,
		.flags = MMU_NOTIFIER_RANGE_BLOCKABLE,
		.event = lkpi_mn_event(event),
	};
	seq = lkpi_mn_invalidate_begin(subs);
	hlist_for_each_entry(mn, &subs->list, hlist) {
		if (mn->ops->invalidate_range_start != NULL) {
			error = mn->ops->invalidate_range_start(mn, &range);
			/* Can only fail for non-blockable ranges. */
			WARN_ON_ONCE(error != 0);
		}
	}
	lkpi_mn_invalidate_itree(subs, &range, seq);
}

static void
lkpi_mn_vm_invalidate_end(struct vm_map_notifier *n, vm_map_t map __unused,
    vm_offset_t start, vm_offset_t end, enum vm_map_notify_event event)
{
	struct mmu_notifier_subscriptions *subs;
	struct mmu_notifier_range range;
	struct mmu_notifier *mn;

	subs = __containerof(n, struct mmu_notifier_subscriptions, vmn);
	range = (struct mmu_notifier_range){
		.mm = subs->mm,
		.start = start,
		.end = end,
		.flags = MMU_NOTIFIER_RANGE_BLOCKABLE,
		.event = lkpi_mn_event(event),
	};
	hlist_for_each_entry(mn, &subs->list, hlist) {
		if (mn->ops->invalidate_range_end != NULL)
			mn->ops->invalidate_range_end(mn, &range);
	}
	lkpi_mn_invalidate_end(subs, false);
}

/*
 * The vmspace is being torn down, like Linux exit_mmap(): call the
 * release() methods, invalidate everything and unbind.
 */
static void
lkpi_mn_vm_release(struct vm_map_notifier *n, vm_map_t map __unused)
{
	struct mmu_notifier_subscriptions *subs;
	struct mmu_notifier_range range;
	struct mmu_notifier *mn;
	struct mm_struct *mm;
	unsigned long seq;

	subs = __containerof(n, struct mmu_notifier_subscriptions, vmn);
	mm = subs->mm;
	range = (struct mmu_notifier_range){
		.mm = mm,
		.start = 0,
		.end = ULONG_MAX,
		.flags = MMU_NOTIFIER_RANGE_BLOCKABLE,
		.event = MMU_NOTIFY_RELEASE,
	};
	mtx_lock(&subs->lock);
	subs->vm = NULL;
	mtx_unlock(&subs->lock);
	seq = lkpi_mn_invalidate_begin(subs);
	hlist_for_each_entry(mn, &subs->list, hlist) {
		if (mn->ops->release != NULL)
			mn->ops->release(mn, mm);
	}
	lkpi_mn_invalidate_itree(subs, &range, seq);
	lkpi_mn_invalidate_end(subs, true);
	mmdrop(mm);
}

int
__mmu_notifier_register(struct mmu_notifier *mn, struct mm_struct *mm)
{
	struct mmu_notifier_subscriptions *subs;
	int error;

	error = lkpi_mn_subscribe(mm, &subs);
	if (error != 0)
		return (error);
	mn->mm = mm;
	mn->users = 1;
	mmgrab(mm);
	mtx_lock(&subs->lock);
	if (subs->active > 0)
		hlist_add_head(&mn->hlist, &subs->pending);
	else
		hlist_add_head(&mn->hlist, &subs->list);
	mtx_unlock(&subs->lock);
	return (0);
}

int
mmu_notifier_register(struct mmu_notifier *mn, struct mm_struct *mm)
{
	int error;

	down_write(&mm->mmap_sem);
	error = __mmu_notifier_register(mn, mm);
	up_write(&mm->mmap_sem);
	return (error);
}

/*
 * Unregister a notifier registered with mmu_notifier_register().  Like on
 * Linux, release() is called unless it already was.  Upon return no
 * methods of the notifier are running.
 */
void
mmu_notifier_unregister(struct mmu_notifier *mn, struct mm_struct *mm)
{
	struct mmu_notifier_subscriptions *subs;
	unsigned long seq;
	bool listed;

	subs = lkpi_mn_subscriptions(mm);
	mtx_lock(&subs->lock);
	listed = !hlist_unhashed(&mn->hlist);
	mtx_unlock(&subs->lock);
	if (listed && mn->ops->release != NULL)
		mn->ops->release(mn, mm);

	mtx_lock(&subs->lock);
	while (!hlist_unhashed(&mn->hlist) && subs->active > 0) {
		seq = subs->invalidate_seq;
		while (subs->invalidate_seq == seq)
			mtx_sleep(subs, &subs->lock, 0, "lkpimnu", 0);
	}
	hlist_del_init(&mn->hlist);
	mtx_unlock(&subs->lock);
	mmdrop(mm);
}

struct mmu_notifier *
mmu_notifier_get_locked(const struct mmu_notifier_ops *ops,
    struct mm_struct *mm)
{
	struct mmu_notifier_subscriptions *subs;
	struct mmu_notifier *mn;
	int error;

	error = lkpi_mn_subscribe(mm, &subs);
	if (error != 0)
		return (ERR_PTR(error));
	mtx_lock(&subs->lock);
	hlist_for_each_entry(mn, &subs->list, hlist) {
		if (mn->ops == ops && mn->users > 0)
			goto found;
	}
	hlist_for_each_entry(mn, &subs->pending, hlist) {
		if (mn->ops == ops && mn->users > 0)
			goto found;
	}
	mtx_unlock(&subs->lock);

	mn = ops->alloc_notifier(mm);
	if (IS_ERR(mn))
		return (mn);
	mn->ops = ops;
	error = __mmu_notifier_register(mn, mm);
	if (error != 0) {
		ops->free_notifier(mn);
		return (ERR_PTR(error));
	}
	return (mn);
found:
	mn->users++;
	mtx_unlock(&subs->lock);
	return (mn);
}

/*
 * Drop a reference obtained with mmu_notifier_get().  The last one removes
 * the notifier and frees it asynchronously with free_notifier(); this may
 * be called from the notifier's own methods, including release().
 */
void
mmu_notifier_put(struct mmu_notifier *mn)
{
	struct mmu_notifier_subscriptions *subs;

	subs = lkpi_mn_subscriptions(mn->mm);
	mtx_lock(&subs->lock);
	KASSERT(mn->users > 0, ("%s: notifier %p has no users", __func__, mn));
	if (--mn->users > 0) {
		mtx_unlock(&subs->lock);
		return;
	}
	if (!hlist_unhashed(&mn->hlist)) {
		if (subs->active > 0) {
			/* lkpi_mn_invalidate_end() will free it. */
			mtx_unlock(&subs->lock);
			return;
		}
		hlist_del_init(&mn->hlist);
	}
	mtx_unlock(&subs->lock);
	call_rcu(&mn->rcu, lkpi_mn_free_rcu);
}

/* Wait for all free_notifier() calls scheduled by mmu_notifier_put(). */
void
mmu_notifier_synchronize(void)
{
	rcu_barrier();
}

int
mmu_interval_notifier_insert(struct mmu_interval_notifier *mni,
    struct mm_struct *mm, unsigned long start, unsigned long length,
    const struct mmu_interval_notifier_ops *ops)
{
	struct mmu_notifier_subscriptions *subs;
	int error;

	if (length == 0 || start + length - 1 < start)
		return (-EOVERFLOW);
	error = lkpi_mn_subscribe(mm, &subs);
	if (error != 0)
		return (error);
	mni->interval_tree.start = start;
	mni->interval_tree.last = start + length - 1;
	mni->ops = ops;
	mni->mm = mm;
	mni->lkpi_held = NULL;
	mmgrab(mm);

	/*
	 * An insertion during an invalidation might overlap it, so make
	 * mmu_interval_read_begin() wait for the end of the invalidation.
	 */
	mtx_lock(&subs->lock);
	mni->invalidate_seq = subs->invalidate_seq;
	if (subs->active > 0) {
		mni->lkpi_deferred_op = LKPI_MNI_DEFER_INSERT;
		hlist_add_head(&mni->deferred_item, &subs->deferred);
	} else {
		mni->lkpi_deferred_op = LKPI_MNI_DEFER_NONE;
		interval_tree_insert(&mni->interval_tree, &subs->itree);
	}
	mtx_unlock(&subs->lock);
	return (0);
}

/* mmap_lock does not protect anything here; see mmu_notifier_get(). */
int
mmu_interval_notifier_insert_locked(struct mmu_interval_notifier *mni,
    struct mm_struct *mm, unsigned long start, unsigned long length,
    const struct mmu_interval_notifier_ops *ops)
{
	return (mmu_interval_notifier_insert(mni, mm, start, length, ops));
}

/*
 * Remove an interval notifier.  Upon return its invalidate() method is not
 * running and will not be called again.  Must not be called from
 * invalidate().
 */
void
mmu_interval_notifier_remove(struct mmu_interval_notifier *mni)
{
	struct mmu_notifier_subscriptions *subs;
	struct mm_struct *mm;
	vm_page_t *held;
	unsigned long seq;

	mm = mni->mm;
	subs = lkpi_mn_subscriptions(mm);
	mtx_lock(&subs->lock);
	if (mni->lkpi_deferred_op == LKPI_MNI_DEFER_INSERT) {
		hlist_del(&mni->deferred_item);
		mni->lkpi_deferred_op = LKPI_MNI_DEFER_NONE;
	} else if (subs->active > 0) {
		mni->lkpi_deferred_op = LKPI_MNI_DEFER_REMOVE;
		hlist_add_head(&mni->deferred_item, &subs->deferred);
		seq = subs->invalidate_seq;
		while (subs->invalidate_seq == seq)
			mtx_sleep(subs, &subs->lock, 0, "lkpimnr", 0);
	} else {
		interval_tree_remove(&mni->interval_tree, &subs->itree);
	}
	lkpi_mni_unhold(subs, mni, mni->interval_tree.start,
	    mni->interval_tree.last + 1);
	held = mni->lkpi_held;
	mni->lkpi_held = NULL;
	mtx_unlock(&subs->lock);
	free(held, M_LKPI_MMU);
	mmdrop(mm);
}

/*
 * Begin a read side critical section: wait for an invalidation of mni in
 * progress to end and return the sequence to pass to
 * mmu_interval_read_retry().
 */
unsigned long
mmu_interval_read_begin(struct mmu_interval_notifier *mni)
{
	struct mmu_notifier_subscriptions *subs;
	unsigned long seq;

	subs = lkpi_mn_subscriptions(mni->mm);
	mtx_lock(&subs->lock);
	while (subs->active > 0 &&
	    READ_ONCE(mni->invalidate_seq) == subs->invalidate_seq)
		mtx_sleep(subs, &subs->lock, 0, "lkpimnrb", 0);
	seq = READ_ONCE(mni->invalidate_seq);
	mtx_unlock(&subs->lock);
	return (seq);
}

/*
 * Fault in and report the pages of [range->start, range->end), see Linux
 * mm/hmm.c.  The range must lie within range->notifier.  Returns -EBUSY if
 * the range was invalidated since range->notifier_seq was obtained, and
 * -EFAULT if a page cannot be faulted with the requested access.  The pages
 * stay wired until they are invalidated, faulted again, or the notifier is
 * removed (see linux/hmm.h).
 */
int
hmm_range_fault(struct hmm_range *range)
{
	struct mmu_notifier_subscriptions *subs;
	struct mmu_interval_notifier *mni;
	struct vmspace *vm;
	vm_map_t map;
	vm_page_t m, *held, *pages;
	vm_offset_t va;
	vm_prot_t prot;
	unsigned long base, flags, i, npages, nheld, off, req;
	int error;

	mni = range->notifier;
	if (range->start >= range->end || offset_in_page(range->start) != 0 ||
	    offset_in_page(range->end) != 0 ||
	    range->start < mni->interval_tree.start ||
	    range->end - 1 > mni->interval_tree.last)
		return (-EINVAL);
	subs = lkpi_mn_subscriptions(mni->mm);
	base = trunc_page(mni->interval_tree.start);
	nheld = atop(trunc_page(mni->interval_tree.last) - base) + 1;
	npages = atop(range->end - range->start);
	off = atop(range->start - base);
	pages = mallocarray(npages, sizeof(*pages), M_LKPI_MMU,
	    M_WAITOK | M_ZERO);
	held = NULL;

	mtx_lock(&subs->lock);
	vm = subs->vm;
	if (vm != NULL && !refcount_acquire_if_not_zero(&vm->vm_refcnt))
		vm = NULL;
	if (vm != NULL && mni->lkpi_held == NULL) {
		mtx_unlock(&subs->lock);
		held = mallocarray(nheld, sizeof(*held), M_LKPI_MMU,
		    M_WAITOK | M_ZERO);
	} else
		mtx_unlock(&subs->lock);
	if (vm == NULL) {
		error = -EFAULT;
		goto out;
	}

	map = &vm->vm_map;
	error = 0;
	for (i = 0; i < npages; i++) {
		if (mmu_interval_check_retry(mni, range->notifier_seq)) {
			error = -EBUSY;
			break;
		}
		va = range->start + ptoa(i);
		req = (range->hmm_pfns[i] & range->pfn_flags_mask) |
		    range->default_flags;
		if ((req & HMM_PFN_REQ_FAULT) != 0) {
			prot = VM_PROT_READ;
			flags = HMM_PFN_VALID;
			if ((req & HMM_PFN_REQ_WRITE) != 0) {
				prot |= VM_PROT_WRITE;
				flags |= HMM_PFN_WRITE;
			}
			if (vm_fault_quick_hold_pages(map, va, PAGE_SIZE, prot,
			    &m, 1) == -1) {
				error = -EFAULT;
				break;
			}
		} else {
			/* Snapshot: report only what is mapped. */
			flags = HMM_PFN_VALID | HMM_PFN_WRITE;
			m = pmap_extract_and_hold(map->pmap, va, VM_PROT_WRITE);
			if (m == NULL) {
				flags = HMM_PFN_VALID;
				m = pmap_extract_and_hold(map->pmap, va,
				    VM_PROT_READ);
			}
			if (m == NULL) {
				range->hmm_pfns[i] = 0;
				continue;
			}
		}
		pages[i] = m;
		range->hmm_pfns[i] = page_to_pfn(m) | flags;
	}

	/*
	 * Publish the new holds, unless the range was invalidated meanwhile;
	 * the old holds of the range are dropped below.
	 */
	if (error == 0) {
		mtx_lock(&subs->lock);
		if (mni->lkpi_held == NULL) {
			mni->lkpi_held = held;
			held = NULL;
		}
		if (mmu_interval_read_retry(mni, range->notifier_seq)) {
			error = -EBUSY;
		} else {
			for (i = 0; i < npages; i++) {
				m = mni->lkpi_held[off + i];
				mni->lkpi_held[off + i] = pages[i];
				pages[i] = m;
			}
		}
		mtx_unlock(&subs->lock);
	}
	vmspace_free(vm);
out:
	for (i = 0; i < npages; i++) {
		if (pages[i] != NULL)
			vm_page_unwire(pages[i], PQ_ACTIVE);
	}
	free(held, M_LKPI_MMU);
	free(pages, M_LKPI_MMU);
	return (error);
}
