/*
 * Copyright (c) 2026 The FreeBSD Foundation
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef	_LINUXKPI_LINUX_PROCESSOR_H_
#define	_LINUXKPI_LINUX_PROCESSOR_H_

#include <asm/processor.h>

#define	spin_begin()		do { } while (0)
#define	spin_cpu_relax()	cpu_relax()
#define	spin_end()		do { } while (0)

#define	spin_until_cond(cond) do {					\
	spin_begin();							\
	while (!(cond))							\
		spin_cpu_relax();					\
	spin_end();							\
} while (0)

#endif	/* _LINUXKPI_LINUX_PROCESSOR_H_ */
