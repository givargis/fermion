/**
 * Copyright (c) 2025-2026 Tony Givargis
 * University of California, Irvine
 * fm_fnc.h
 */

#ifndef FM_FNC_H
#define FM_FNC_H

#include "fm_dag.h"

#define FM__FNC_MAX_ARGS  5
#define FM__FNC_MAX_TYPES ( 1 + FM__FNC_MAX_ARGS ) /* [0] is ret */

enum fm__fnc_type {
	FM__FNC_TYPE_UNKNOWN,
	FM__FNC_TYPE_INT_SCALAR,
	FM__FNC_TYPE_BOOL_SCALAR,
	FM__FNC_TYPE_INT_TENSOR,
	FM__FNC_TYPE_BOOL_TENSOR,
	FM__FNC_TYPE_INT64_TENSOR,
	FM__FNC_TYPE_FP_TENSOR,
	FM__FNC_TYPE_NUMERIC_TENSOR,
	FM__FNC_TYPE_ANY_TENSOR,
	FM__FNC_TYPE_SAME_AS_ARG0,
	FM__FNC_TYPE_SAME_AS_ARG0_OR_SCALAR
};

enum fm__fnc_shape {
	FM__FNC_SHAPE_SAME,
	FM__FNC_SHAPE_REDUCE,
	FM__FNC_SHAPE_SCAN,
	FM__FNC_SHAPE_ARGSORT,
	FM__FNC_SHAPE_TOPK,
	FM__FNC_SHAPE_FLATTEN,
	FM__FNC_SHAPE_SQUEEZE,
	FM__FNC_SHAPE_UNSQUEEZE,
	FM__FNC_SHAPE_CONCAT,
	FM__FNC_SHAPE_GATHER,
	FM__FNC_SHAPE_SCATTER,
	FM__FNC_SHAPE_NDIM,
	FM__FNC_SHAPE_NUMEL,
	FM__FNC_SHAPE_AXIS
};

struct fm__fnc {
	const char *name;
	enum fm__fnc_type types[FM__FNC_MAX_TYPES];
	enum fm__fnc_shape shape;
	enum fm_expr_op op;
	int masked; /* bool */
};

extern const struct fm__fnc FM__FNCS[];

extern const size_t FM__FNCS_SIZE;

enum fm__dag_type fm__fnc_type(const struct fm__dag *dag);

size_t fm__fnc_types_count(size_t index);

enum fm_fold_op fm__fnc_fold(size_t index);

int fm__fnc_static(const struct fm__dag *dag, int64_t *value);

int fm__fnc_shape(struct fm_geometry *geometry,
		  const struct fm__dag *dag,
		  const struct fm_geometry * const argv[],
		  size_t argc);

#endif
