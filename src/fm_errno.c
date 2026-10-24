/**
 * Copyright (c) 2025-2026 Tony Givargis
 * University of California, Irvine
 * fm_errno.c
 */

#include <stdio.h>
#include <errno.h>

#include "fm_errno.h"

#ifndef NDEBUG
static int trace = 1;
#else
static int trace = 0;
#endif

void
fm__errno(int e, const char *m, const char *file, int line)
{
	int hold;

	if (trace) {
		hold = errno;
		fprintf(stderr,
			"errno: %s:%d: %s (%d)\n",
			file ? file : "",
			line,
			e ? (m ? m : "") : "^",
			e);
		errno = hold;
	}
	if (e) {
		errno = e;
	}
}
