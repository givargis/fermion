/**
 * Copyright (c) 2025-2026 Tony Givargis
 * University of California, Irvine
 * fm_trace.c
 */

#include <stdio.h>

#include "fm_trace.h"

void
fm__trace(const char *message, const char *file, int line)
{
	fprintf(stderr,
		"trace: %s:%d: %s\n",
		file ? file : "",
		line,
		message ? message : "^");
}
