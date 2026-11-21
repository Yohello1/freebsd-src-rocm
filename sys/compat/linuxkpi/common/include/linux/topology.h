/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 The FreeBSD Foundation
 * Copyright (c) 2025 Jean-Sébastien Pédron
 * Copyright (c) 2026 The FreeBSD Foundation
 *
 * This software was developed by Jean-Sébastien Pédron under sponsorship
 * from the FreeBSD Foundation.
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

#ifndef	_LINUXKPI_LINUX_TOPOLOGY_H_
#define	_LINUXKPI_LINUX_TOPOLOGY_H_

#include <sys/param.h>
#include <sys/smp.h>

#include <vm/vm.h>

#include <linux/cpumask.h>

#include <asm/topology.h>

#define	LOCAL_DISTANCE		10
#define	REMOTE_DISTANCE		20
#define	DISTANCE_BITS		8

static inline const struct cpumask *
cpumask_of_node(int node)
{
	if (node < 0)
		return ((const struct cpumask *)&cpu_online_mask);
	if (node >= vm_ndomains)
		return (cpu_none_mask);
	return ((const struct cpumask *)&cpuset_domain[node]);
}

/*
 * The distance between NUMA nodes is not known, so only distinguish
 * between local and remote nodes.
 */
static inline int
node_distance(int from, int to)
{
	return (from == to ? LOCAL_DISTANCE : REMOTE_DISTANCE);
}

#endif /* _LINUXKPI_LINUX_TOPOLOGY_H_ */
