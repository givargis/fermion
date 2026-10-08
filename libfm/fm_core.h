/**
 * Copyright (c) 2025-2026 Tony Givargis
 * University of California, Irvine
 * fm_core.h
 */

#ifndef FM_CORE_H
#define FM_CORE_H

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FM__ERRNO_FILE         -10
#define FM__ERRNO_MEMORY       -11
#define FM__ERRNO_SYNTAX       -12
#define FM__ERRNO_NUMERIC      -13
#define FM__ERRNO_SOFTWARE     -14
#define FM__ERRNO_ARGUMENTS    -15
#define FM__ERRNO_ARCHITECTURE -16

#define FM__U64(x) ( (uint64_t)(x) )

#define FM__ARRAY_SIZE(a) ( sizeof ((a)) / sizeof ((a)[0]) )

#define FM__TRACE(e)					\
	do {						\
		fm__trace((e), #e, __FILE__, __LINE__);	\
	}						\
	while (0)

void fm__trace(int errno_, const char *message, const char *file, int line);

void *fm__malloc(size_t n);

void *fm__realloc(void *p, size_t n);

char *fm__strdup(const char *s);

static inline int
fm__add_size(size_t a, size_t b, size_t *z)
{
	assert( z );

	if ((SIZE_MAX - b) < a) {
		FM__TRACE(FM__ERRNO_NUMERIC);
		return -1;
	}
	(*z) = a + b;
	return 0;
}

static inline int
fm__mul_size(size_t a, size_t b, size_t *z)
{
	assert( z );

	if (b && (SIZE_MAX / b) < a) {
		FM__TRACE(FM__ERRNO_NUMERIC);
		return -1;
	}
	(*z) = a * b;
	return 0;
}

static inline int
fm__add_int64(int64_t a, int64_t b, int64_t *z)
{
	assert( z );

	if (((0 < b) && ((INT64_MAX - b) < a)) ||
	    ((0 > b) && ((INT64_MIN - b) > a))) {
		FM__TRACE(FM__ERRNO_NUMERIC);
		return -1;
	}
	(*z) = a + b;
	return 0;
}

static inline int
fm__mul_int64(int64_t a, int64_t b, int64_t *z)
{
	assert( z );

	if (!a || !b) {
		(*z) = 0;
		return 0;
	}
	if (0 < a) {
		if (((0 < b) && ((INT64_MAX / b) < a)) ||
		    ((0 > b) && ((INT64_MIN / a) > b))) {
			FM__TRACE(FM__ERRNO_NUMERIC);
			return -1;
		}
	}
	else if (((0 < b) && ((INT64_MIN / b) > a)) ||
		 ((0 > b) && ((INT64_MAX / a) > b))) {
		FM__TRACE(FM__ERRNO_NUMERIC);
		return -1;
	}
	(*z) = a * b;
	return 0;
}

#endif
