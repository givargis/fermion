/**
 * Copyright (c) 2025-2026 Tony Givargis
 * University of California, Irvine
 * fm_errno.h
 */

#ifndef FM_ERRNO_H
#define FM_ERRNO_H

#define FM__ERRNO_FILE         -10
#define FM__ERRNO_MEMORY       -11
#define FM__ERRNO_SYNTAX       -12
#define FM__ERRNO_NUMERIC      -13
#define FM__ERRNO_SOFTWARE     -14
#define FM__ERRNO_ARGUMENTS    -15
#define FM__ERRNO_ARCHITECTURE -16

#define FM__ERRNO(e)					\
	do {						\
		fm__errno((e), #e, __FILE__, __LINE__);	\
	}						\
	while (0)

void fm__errno(int e, const char *m, const char *file, int line);

#endif
