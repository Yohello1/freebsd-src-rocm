/*
 * Copyright (c) 2026 The FreeBSD Foundation
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef	_LINUXKPI_LINUX_CPUFREQ_H_
#define	_LINUXKPI_LINUX_CPUFREQ_H_

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysctl.h>

#include <linux/kernel.h>

/*
 * Return the maximum frequency of a CPU in kHz, or 0 when it is unknown.
 * The hw.clockrate sysctl is used for all CPUs.
 */
static inline unsigned int
cpufreq_quick_get_max(unsigned int cpu __unused)
{
	size_t len;
	int clockrate;

	pr_debug("%s: TODO\n", __func__);
	len = sizeof(clockrate);
	if (kernel_sysctlbyname(curthread, "hw.clockrate", &clockrate, &len,
	    NULL, 0, NULL, 0) != 0 || clockrate <= 0)
		return (0);
	/* hw.clockrate is in MHz. */
	return ((unsigned int)clockrate * 1000);
}

#endif	/* _LINUXKPI_LINUX_CPUFREQ_H_ */
