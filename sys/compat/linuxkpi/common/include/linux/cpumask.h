/*
 * Copyright (c) 2026 The FreeBSD Foundation
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef	_LINUXKPI_LINUX_CPUMASK_H_
#define	_LINUXKPI_LINUX_CPUMASK_H_

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/cpuset.h>
#include <sys/smp.h>

typedef	cpuset_t	cpumask_t;

/*
 * On Linux, cpumask_t is a typedef of struct cpumask.  Here, cpumask_t is a
 * cpuset_t and struct cpumask only wraps a cpuset_t, so that the address of
 * a cpumask_t can be cast to a pointer to a struct cpumask.
 */
struct cpumask {
	cpuset_t	bits;
};

extern cpumask_t cpu_online_mask;
extern struct cpumask lkpi_cpu_none_mask;

#define	nr_cpu_ids	(mp_maxid + 1)
#define	cpu_none_mask	((const struct cpumask *)&lkpi_cpu_none_mask)

static inline unsigned int
cpumask_first(const struct cpumask *mask)
{
	int cpu;

	cpu = CPU_FFS(&mask->bits);
	if (cpu == 0)
		return (nr_cpu_ids);
	return (cpu - 1);
}

static inline unsigned int
cpumask_weight(const struct cpumask *mask)
{
	return (CPU_COUNT(&mask->bits));
}

#endif	/* _LINUXKPI_LINUX_CPUMASK_H_ */
