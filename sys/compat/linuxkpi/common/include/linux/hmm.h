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

#ifndef _LINUXKPI_LINUX_HMM_H_
#define	_LINUXKPI_LINUX_HMM_H_

#include <linux/mm.h>
#include <linux/mmu_notifier.h>

/*
 * Heterogeneous memory management: mirror a range of a process address
 * space into a device page table, see Linux include/linux/hmm.h and
 * mm/hmm.c.  Only system memory is supported (no device private pages).
 *
 * Unlike Linux, which relies on the reverse map to invalidate pages that
 * are reclaimed or migrated, hmm_range_fault() keeps each page it returns
 * wired until the range is invalidated through the interval notifier, the
 * range is faulted again, or the notifier is removed.  The FreeBSD page
 * daemon and object-level operations do not report to vm_map notifiers,
 * so this is what keeps the pages from being freed while a device may use
 * them.
 */

/* An hmm_pfns entry: a PFN in the low bits and flags in the high bits. */
enum hmm_pfn_flags {
	/* Output flags. */
	HMM_PFN_VALID = 1UL << (BITS_PER_LONG - 1),
	HMM_PFN_WRITE = 1UL << (BITS_PER_LONG - 2),
	HMM_PFN_ERROR = 1UL << (BITS_PER_LONG - 3),
	HMM_PFN_ORDER_SHIFT = (BITS_PER_LONG - 8),

	/* Input flags. */
	HMM_PFN_REQ_FAULT = HMM_PFN_VALID,
	HMM_PFN_REQ_WRITE = HMM_PFN_WRITE,

	HMM_PFN_FLAGS = ~((1UL << HMM_PFN_ORDER_SHIFT) - 1),
};

struct hmm_range {
	struct mmu_interval_notifier *notifier;
	unsigned long notifier_seq;
	unsigned long start;
	unsigned long end;
	unsigned long *hmm_pfns;
	unsigned long default_flags;
	unsigned long pfn_flags_mask;
	void *dev_private_owner;
};

/* Default timeout, in milliseconds, for retrying hmm_range_fault(). */
#define	HMM_RANGE_DEFAULT_TIMEOUT	1000

static inline struct page *
hmm_pfn_to_page(unsigned long hmm_pfn)
{
	return (pfn_to_page(hmm_pfn & ~HMM_PFN_FLAGS));
}

static inline phys_addr_t
hmm_pfn_to_phys(unsigned long hmm_pfn)
{
	return (ptoa((vm_paddr_t)(hmm_pfn & ~HMM_PFN_FLAGS)));
}

/* Only base pages are reported. */
static inline unsigned int
hmm_pfn_to_map_order(unsigned long hmm_pfn __unused)
{
	return (0);
}

int	hmm_range_fault(struct hmm_range *range);

#endif /* _LINUXKPI_LINUX_HMM_H_ */
