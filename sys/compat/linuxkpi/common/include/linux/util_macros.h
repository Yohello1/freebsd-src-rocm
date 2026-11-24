/*-
 * Copyright (c) 2026 The FreeBSD Foundation
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * `for_each_if()` was moved from <drm/drm_util.h> to <linux/util_macros.h> in
 * Linux 6.15.
 */

#ifndef _LINUXKPI_LINUX_UTIL_MACROS_H
#define _LINUXKPI_LINUX_UTIL_MACROS_H

#if !defined(LINUXKPI_VERSION) || (LINUXKPI_VERSION >= 61500)
#define for_each_if(condition) if (!(condition)) {} else
#endif

#endif /* _LINUXKPI_LINUX_UTIL_MACROS_H */
