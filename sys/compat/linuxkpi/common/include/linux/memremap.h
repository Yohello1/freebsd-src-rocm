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

#ifndef _LINUXKPI_LINUX_MEMREMAP_H_
#define	_LINUXKPI_LINUX_MEMREMAP_H_

/*
 * Device memory page maps (Linux include/linux/memremap.h).
 *
 * FreeBSD has no ZONE_DEVICE: device memory cannot be given struct pages
 * that are managed by the VM.  As in Linux built without CONFIG_ZONE_DEVICE,
 * the types are provided so that drivers compile, but no page is ever a
 * device page and creating a page map fails with -ENXIO.  Drivers must
 * handle that failure (e.g. amdkfd then disables VRAM migration).
 */

#include <linux/bug.h>
#include <linux/err.h>
#include <linux/mm_types.h>
#include <linux/range.h>
#include <linux/types.h>

struct dev_pagemap;
struct device;
struct page;
struct vm_fault;

enum memory_type {
	MEMORY_DEVICE_PRIVATE = 1,
	MEMORY_DEVICE_COHERENT,
	MEMORY_DEVICE_FS_DAX,
	MEMORY_DEVICE_GENERIC,
	MEMORY_DEVICE_PCI_P2PDMA,
};

struct dev_pagemap_ops {
	void		(*page_free)(struct page *page);
	vm_fault_t	(*migrate_to_ram)(struct vm_fault *vmf);
	int		(*memory_failure)(struct dev_pagemap *pgmap,
			    unsigned long pfn, unsigned long nr_pages,
			    int mf_flags);
};

struct dev_pagemap {
	enum memory_type		type;
	unsigned int			flags;
	const struct dev_pagemap_ops	*ops;
	void				*owner;
	int				nr_range;
	struct range			range;
};

static inline bool
is_device_private_page(const struct page *page)
{
	return (false);
}

static inline bool
is_device_coherent_page(const struct page *page)
{
	return (false);
}

static inline bool
is_pci_p2pdma_page(const struct page *page)
{
	return (false);
}

static inline void *
devm_memremap_pages(struct device *dev, struct dev_pagemap *pgmap)
{
	return (ERR_PTR(-ENXIO));
}

static inline void
devm_memunmap_pages(struct device *dev, struct dev_pagemap *pgmap)
{
}

static inline struct dev_pagemap *
get_dev_pagemap(unsigned long pfn, struct dev_pagemap *pgmap)
{
	return (NULL);
}

#endif	/* _LINUXKPI_LINUX_MEMREMAP_H_ */
