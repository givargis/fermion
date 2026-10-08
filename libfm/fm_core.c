/**
 * Copyright (c) 2025-2026 Tony Givargis
 * University of California, Irvine
 * fm_core.c
 */

#include "fm_core.h"

#ifndef NDEBUG
static int trace = 1;
#else
static int trace = 0;
#endif

void
fm__trace(int errno_, const char *message, const char *file, int line)
{
	int hold;

	if (trace) {
		hold = errno;
		fprintf(stderr,
			"trace: %s:%d: %s (%d)\n",
			file ? file : "",
			line,
			errno_ ? (message ? message : "") : "^",
			errno_);
		errno = hold;
	}
	if (errno_) {
		errno = errno_;
	}
}

void *
fm__malloc(size_t n)
{
	void *p;

	assert( n );

	if (!(p = malloc(n))) {
		FM__TRACE(FM__ERRNO_MEMORY);
		return NULL;
	}
	return p;
}

void *
fm__realloc(void *p_, size_t n)
{
	void *p;

	assert( n );

	if (!(p = realloc(p_, n))) {
		FM__TRACE(FM__ERRNO_MEMORY);
		return NULL;
	}
	return p;
}

char *
fm__strdup(const char *s)
{
	size_t n, m;
	char *p;

	s = s ? s : "";
	n = strlen(s);
	if (fm__add_size(n, 1, &m)) {
		FM__TRACE(0);
		return NULL;
	}
	if (!(p = fm__malloc(m))) {
		FM__TRACE(0);
		return NULL;
	}
	memcpy(p, s, n);
	p[n] = '\0';
	return p;
}
