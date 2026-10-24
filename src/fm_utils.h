/**
 * Copyright (c) 2025-2026 Tony Givargis
 * University of California, Irvine
 * fm_utils.h
 */

#ifndef FM_UTILS_H
#define FM_UTILS_H

#include <stdint.h>
#include <stddef.h>

#define FM__UTILS_INT64_MIN  ( -9223372036854775807L - 1 )
#define	FM__UTILS_INT64_MAX  (  9223372036854775807L     )
#define	FM__UTILS_UINT64_MAX ( 18446744073709551615LU    )

void fm__utils_init(void);

int fm__utils_add_size(size_t a, size_t b, size_t *z);

int fm__utils_mul_size(size_t a, size_t b, size_t *z);

int fm__utils_add_int64(int64_t a, int64_t b, int64_t *z);

int fm__utils_mul_int64(int64_t a, int64_t b, int64_t *z);

void fm__utils_mul_uint128(uint64_t a, uint64_t b, uint64_t *h, uint64_t *l);

#endif
