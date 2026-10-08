/**
 * Copyright (c) 2025-2026 Tony Givargis
 * University of California, Irvine
 * fm_bitset.c
 */

#include "fm_core.h"
#include "fm_bitset.h"

struct fm__bitset {
	size_t size;
	uint64_t *memory;
};

fm__bitset_t
fm__bitset_open(size_t size)
{
	struct fm__bitset *bitset;
	size_t n;

	if (!size) {
		FM__TRACE(FM__ERRNO_ARGUMENTS);
		return NULL;
	}
	if (!(bitset = fm__malloc(sizeof (struct fm__bitset)))) {
		FM__TRACE(0);
		return NULL;
	}
	memset(bitset, 0, sizeof (struct fm__bitset));
	bitset->size = size;
	n = bitset->size / 64 + (0 != (bitset->size % 64));
	if (!(bitset->memory = fm__malloc(n * sizeof (bitset->memory[0])))) {
		fm__bitset_close(bitset);
		FM__TRACE(0);
		return NULL;
	}
	memset(bitset->memory, 0, n * sizeof (bitset->memory[0]));
	return bitset;
}

void
fm__bitset_close(fm__bitset_t bitset)
{
	if (bitset) {
		free(bitset->memory);
		free(bitset);
	}
}

void
fm__bitset_set(fm__bitset_t bitset, size_t i)
{
	size_t q, r;

	assert( bitset );

	if (i < bitset->size) {
		q = i / 64;
		r = i % 64;
		bitset->memory[q] |= (FM__U64(1) << r);
	}
}

void
fm__bitset_clr(fm__bitset_t bitset, size_t i)
{
	size_t q, r;

	assert( bitset );

	if (i < bitset->size) {
		q = i / 64;
		r = i % 64;
		bitset->memory[q] &= ~(FM__U64(1) << r);
	}
}

int
fm__bitset_get(fm__bitset_t bitset, size_t i)
{
	size_t q, r;

	assert( bitset );

	if (i < bitset->size) {
		q = i / 64;
		r = i % 64;
		if ((bitset->memory[q] & (FM__U64(1) << r))) {
			return 1;
		}
	}
	return 0;
}
