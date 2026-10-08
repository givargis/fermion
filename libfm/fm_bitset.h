/**
 * Copyright (c) 2025-2026 Tony Givargis
 * University of California, Irvine
 * fm_bitset.h
 */

#ifndef FM_BITSET_H
#define FM_BITSET_H

#include <stddef.h>

typedef struct fm__bitset *fm__bitset_t;

fm__bitset_t fm__bitset_open(size_t size);

void fm__bitset_close(fm__bitset_t bitset);

void fm__bitset_set(fm__bitset_t bitset, size_t i);

void fm__bitset_clr(fm__bitset_t bitset, size_t i);

int fm__bitset_get(fm__bitset_t bitset, size_t i); /* bool */

#endif
