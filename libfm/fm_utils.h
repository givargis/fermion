/**
 * Copyright (c) 2025-2026 Tony Givargis
 * University of California, Irvine
 * fm_utils.h
 */

#ifndef FM_UTILS_H
#define FM_UTILS_H

#include "fm_core.h"
#include "fm_dag.h"

#define FM__UTILS_ERR_LEXER(l, m)		\
	do {					\
		fm__utils_err(&(l)->stop,	\
			      (l)->errstr,	\
			      (l)->lineno,	\
			      (l)->column,	\
			      (m));		\
	}					\
	while (0)

#define FM__UTILS_ERR_PARSER(p, m)		\
	do {					\
		fm__utils_err(&(p)->stop,	\
			      (p)->errstr,	\
			      next(p)->lineno,	\
			      next(p)->column,	\
			      (m));		\
	}					\
	while (0)

#define FM__UTILS_ERR_DAG(d, m)			\
	do {					\
		fm__utils_err((d)->stop,	\
			      (d)->errstr,	\
			      (d)->lineno,	\
			      (d)->column,	\
			      (m));		\
	}					\
	while (0)

void fm__utils_err(int *stop,
		   char errstr[FM_ERRSTR_LEN],
		   size_t lineno,
		   size_t column,
		   const char *message);

int fm__utils_match_shape(const int64_t *a, const int64_t *b, size_t ndim);

size_t fm__utils_list_count(const struct fm__dag *dag);

enum fm_expr_op fm__utils_op2op(enum fm__dag_op op);

enum fm_dtype fm__utils_type2dtype(enum fm__dag_type type);

#endif
