/**
 * Copyright (c) 2025-2026 Tony Givargis
 * University of California, Irvine
 * fm_bigint.h
 */

#ifndef FM_BIGINT_H
#define FM_BIGINT_H

#include <stddef.h>
#include <stdint.h>

typedef const struct fm__bigint *fm__bigint_t;

void fm__bigint_init(void);

void fm__bigint_free(fm__bigint_t z);

fm__bigint_t fm__bigint_int(int64_t i);

fm__bigint_t fm__bigint_string(const char *s);

fm__bigint_t fm__bigint_clone(fm__bigint_t a);

fm__bigint_t fm__bigint_neg(fm__bigint_t a);

fm__bigint_t fm__bigint_add(fm__bigint_t a, fm__bigint_t b);

fm__bigint_t fm__bigint_sub(fm__bigint_t a, fm__bigint_t b);

fm__bigint_t fm__bigint_mul(fm__bigint_t a, fm__bigint_t b);

fm__bigint_t fm__bigint_div(fm__bigint_t a, fm__bigint_t b);

fm__bigint_t fm__bigint_mod(fm__bigint_t a, fm__bigint_t b);

fm__bigint_t fm__bigint_shl(fm__bigint_t a, fm__bigint_t b);

fm__bigint_t fm__bigint_shr(fm__bigint_t a, fm__bigint_t b);

fm__bigint_t fm__bigint_not(fm__bigint_t a);

fm__bigint_t fm__bigint_and(fm__bigint_t a, fm__bigint_t b);

fm__bigint_t fm__bigint_xor(fm__bigint_t a, fm__bigint_t b);

fm__bigint_t fm__bigint_or(fm__bigint_t a, fm__bigint_t b);

int fm__bigint_cmp(fm__bigint_t a, fm__bigint_t b);

int fm__bigint_sign(fm__bigint_t a); /* -1|0|+1 */

size_t fm__bigint_bits(fm__bigint_t a);

double fm__bigint_double(fm__bigint_t a);

int fm__bigint_uint64(fm__bigint_t a, int clamp /* bool */, uint64_t *i);

int fm__bigint_int64(fm__bigint_t a, int clamp /* bool */, int64_t *i);

#endif
