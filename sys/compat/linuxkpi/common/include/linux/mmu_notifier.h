/*-
 * Copyright (c) 2017 Mark Johnston <markj@FreeBSD.org>
 * Copyright (c) 2026 The FreeBSD Foundation
 * All rights reserved.
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
#define _LINUXKPI_LINUX_MMU_NOTIFIER_H_

/*
 * XXX: These functions are stubbed out. This is a temporary placeholder
 * to allow compilation. Note that these functions have no implementation;
 * using them will result in silent failure, memory corruption, or system
 * instability. 
*/ 
#warning "linux/mmu_notifier.h is currently a stub; expect runtime issues"


struct mm_struct;
struct mmu_notifier;

struct mmu_notifier_ops {
    void (*release)(struct mmu_notifier *mn, struct mm_struct *mm);
    struct mmu_notifier *(*alloc_notifier)(struct mm_struct *mm);
    void (*free_notifier)(struct mmu_notifier *mn);
    void (*invalidate_range_start)(struct mmu_notifier *mn, const void *range);
    void (*invalidate_range_end)(struct mmu_notifier *mn, const void *range);
};

struct mmu_notifier {
    const struct mmu_notifier_ops *ops;
};


static inline int 
mmu_notifier_register(struct mmu_notifier *mn, struct mm_struct *mm) 
{
    return 0; 
}

static inline void 
mmu_notifier_unregister(struct mmu_notifier *mn, struct mm_struct *mm) 
{
}

static inline struct mmu_notifier *
mmu_notifier_get(const struct mmu_notifier_ops *ops, struct mm_struct *mm)
{
    if (ops && ops->alloc_notifier)
        return ops->alloc_notifier(mm);
    
    return NULL;
}

static inline void
mmu_notifier_put(struct mmu_notifier *mn)
{
    if (mn && mn->ops && mn->ops->free_notifier)
        mn->ops->free_notifier(mn);
}

static inline void 
mmu_notifier_synchronize(void)
{
}

#endif /* _LINUXKPI_LINUX_MMU_NOTIFIER_H_ */
