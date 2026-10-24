/**
 * Copyright (c) 2025-2026 Tony Givargis
 * University of California, Irvine
 * fm_utils.h
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <limits.h>
#include <assert.h>

#include "fm_errno.h"
#include "fm_utils.h"

void
fm__utils_init(void)
{
	if ((8 > sizeof (long)) || (INT_MAX > SIZE_MAX)) {
		FM__ERRNO(FM__ERRNO_ARCHITECTURE);
		abort();
	}
}

int
fm__utils_add_size(size_t a, size_t b, size_t *z)
{
	assert( z );

	if ((SIZE_MAX - b) < a) {
		FM__ERRNO(FM__ERRNO_NUMERIC);
		return -1;
	}
	(*z) = a + b;
	return 0;
}

int
fm__utils_mul_size(size_t a, size_t b, size_t *z)
{
	assert( z );

	if (b && (SIZE_MAX / b) < a) {
		FM__ERRNO(FM__ERRNO_NUMERIC);
		return -1;
	}
	(*z) = a * b;
	return 0;
}

int
fm__utils_add_int64(int64_t a, int64_t b, int64_t *z)
{
	assert( z );

	if (((0 < b) && ((FM__UTILS_INT64_MAX - b) < a)) ||
	    ((0 > b) && ((FM__UTILS_INT64_MIN - b) > a))) {
		FM__ERRNO(FM__ERRNO_NUMERIC);
		return -1;
	}
	(*z) = a + b;
	return 0;
}

int
fm__utils_mul_int64(int64_t a, int64_t b, int64_t *z)
{
	assert( z );

	if (!a || !b) {
		(*z) = 0;
		return 0;
	}
	if (0 < a) {
		if (((0 < b) && ((FM__UTILS_INT64_MAX / b) < a)) ||
		    ((0 > b) && ((FM__UTILS_INT64_MIN / a) > b))) {
			FM__ERRNO(FM__ERRNO_NUMERIC);
			return -1;
		}
	}
	else if (((0 < b) && ((FM__UTILS_INT64_MIN / b) > a)) ||
		 ((0 > b) && ((FM__UTILS_INT64_MAX / a) > b))) {
		FM__ERRNO(FM__ERRNO_NUMERIC);
		return -1;
	}
	(*z) = a * b;
	return 0;
}

void
fm__utils_mul_uint128(uint64_t a, uint64_t b, uint64_t *h, uint64_t *l)
{
#ifdef __SIZEOF_INT128__
	__uint128_t t = (__uint128_t)a * (__uint128_t)b;

	assert( h && l );

	*h = (uint64_t)(t >> 64);
	*l = (uint64_t)t;
#else
	uint64_t ah = a >> 32;
	uint64_t al = a & 0xffffffff;
	uint64_t bh = b >> 32;
	uint64_t bl = b & 0xffffffff;
	uint64_t t1 = bl * al;
	uint64_t t2 = bl * ah;
	uint64_t t3 = bh * al;
	uint64_t t4 = bh * ah;

	assert( h && l );

	t2 += t1 >> 32;
	t3 += t2;
	if (t3 < t2) {
		t4 += (uint64_t)1 << 32;
	}
	t4 += t3 >> 32;
	*h = t4;
	*l = (t3 << 32) | (t1 & 0xffffffff);
#endif
}

void
fm__utils_sprintf(char *buf, size_t size, const char *format, ...)
{
	va_list ap;
	int n;

	assert( buf && format );

	va_start(ap, format);
	n = vsnprintf(buf, size, format, ap);
	va_end(ap);
	if ((0 > n) || (size <= (size_t)n)) {
		FM__ERRNO(FM__ERRNO_SOFTWARE);
		abort();
	}
}
