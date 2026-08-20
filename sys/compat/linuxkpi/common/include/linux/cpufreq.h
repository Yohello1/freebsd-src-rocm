/*
 * Copyright (c) 2026 FreeBSD Foundation
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef _LINUX_CPUFREQ_H_
#define _LINUX_CPUFREQ_H_

#include <sys/pcpu.h>
#include <sys/sysctl.h>
#include <sys/types.h>

static inline unsigned int
cpufreq_quick_get_max(unsigned int cpu __unused)
{
	int clockrate = 0;
	size_t len = sizeof(clockrate);

	if (kernel_sysctlbyname(curthread, "hw.clockrate", &clockrate, &len,
            NULL, 0, NULL, 0) == 0) {
		/* hw.clockrate is in MHz. Linux expects kHz. */
		return ((unsigned int)clockrate * 1000);
	}

	/* Default fallback (e.g., 1.0 GHz) if the sysctl fails */
	return (1000000);
}

#endif /* _LINUX_CPUFREQ_H_ */
