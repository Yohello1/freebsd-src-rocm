/*
 * Copyright (c) 2026 The FreeBSD Foundation
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef _LINUXKPI_LINUX_SPRINTF_H_
#define _LINUXKPI_LINUX_SPRINTF_H_

#include <sys/types.h>

#include <linux/stdarg.h>

struct va_format {
	const char *fmt;
	va_list *va;
};

static inline int
vscnprintf(char *buf, size_t size, const char *fmt, va_list args)
{
	ssize_t ssize = size;
	int i;

	i = vsnprintf(buf, size, fmt, args);

	return ((i >= ssize) ? (ssize - 1) : i);
}

static inline int
scnprintf(char *buf, size_t size, const char *fmt, ...)
{
	va_list args;
	int i;

	va_start(args, fmt);
	i = vscnprintf(buf, size, fmt, args);
	va_end(args);

	return (i);
}

#endif  /* _LINUXKPI_LINUX_SPRINTF_H_ */
