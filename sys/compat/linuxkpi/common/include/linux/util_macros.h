/*-
 * Copyright (c) 2026 The FreeBSD Foundation
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef _LINUXKPI_LINUX_UTIL_MACROS_H
#define _LINUXKPI_LINUX_UTIL_MACROS_H

#if !defined(LINUXKPI_VERSION) || (LINUXKPI_VERSION >= 61500)
#ifndef for_each_if
#define for_each_if(condition) if (!(condition)) {} else
#endif /* for_each_if */
#endif

#endif /* _LINUXKPI_LINUX_UTIL_MACROS_H */
