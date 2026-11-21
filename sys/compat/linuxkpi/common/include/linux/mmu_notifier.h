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

#include <linux/err.h>
#include <linux/errno.h>
#include <linux/kernel.h>

struct mm_struct;
struct mmu_notifier;
struct mmu_notifier_range;

/*
 * MMU notifiers are not implemented.  Only enough of the interface is
 * provided to manage the lifetime of a notifier: the release and
 * invalidate callbacks are never called.
 */
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
	const struct mmu_notifier_ops	*ops;
	struct mm_struct		*mm;
};

static inline int
mmu_notifier_register(struct mmu_notifier *mn __unused,
    struct mm_struct *mm __unused)
{
	pr_debug("%s: TODO\n", __func__);
	return (-EINVAL);
}

static inline void
mmu_notifier_unregister(struct mmu_notifier *mn __unused,
    struct mm_struct *mm __unused)
{
	pr_debug("%s: TODO\n", __func__);
}

/*
 * Unlike on Linux, notifiers are not shared: every call allocates a new
 * notifier using the alloc_notifier callback.
 */
static inline struct mmu_notifier *
mmu_notifier_get(const struct mmu_notifier_ops *ops, struct mm_struct *mm)
{
	struct mmu_notifier *mn;

	pr_debug("%s: TODO\n", __func__);
	if (ops->alloc_notifier == NULL)
		return (ERR_PTR(-EINVAL));
	mn = ops->alloc_notifier(mm);
	if (IS_ERR(mn))
		return (mn);
	mn->ops = ops;
	mn->mm = mm;
	return (mn);
}

static inline void
mmu_notifier_put(struct mmu_notifier *mn)
{
	pr_debug("%s: TODO\n", __func__);
	if (mn->ops->free_notifier != NULL)
		mn->ops->free_notifier(mn);
}

static inline void
mmu_notifier_synchronize(void)
{
	pr_debug("%s: TODO\n", __func__);
}

#endif /* _LINUXKPI_LINUX_MMU_NOTIFIER_H_ */
