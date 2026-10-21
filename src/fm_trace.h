/**
 * Copyright (c) 2025-2026 Tony Givargis
 * University of California, Irvine
 * fm_trace.h
 */

#ifndef FM_TRACE_H
#define FM_TRACE_H

#ifndef NDEBUG
#  define FM__TRACE(m)					\
	do {						\
		fm__trace((m), __FILE__, __LINE__);	\
	}						\
	while (0)
#else
#  define FM__TRACE(m) ((void)0)
#endif

void fm__trace(const char *message, const char *file, int line);

#endif
