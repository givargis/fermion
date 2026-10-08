/**
 * Copyright (c) 2025-2026 Tony Givargis
 * University of California, Irvine
 * fm_fnc.c
 */

#include "fm_geometry.h"
#include "fm_utils.h"
#include "fm_fnc.h"

#define ERR FM__UTILS_ERR_DAG

static int
match(enum fm__fnc_type type,
      const struct fm__dag *dag,
      const struct fm__dag *arg0)
{
	if (!dag) {
		return 0;
	}
	if (FM__FNC_TYPE_BOOL_TENSOR == type) {
		return FM__DAG_TYPE_TBOOL == dag->type;
	}
	if (FM__FNC_TYPE_INT_TENSOR == type) {
		return fm__dag_is_int_tensor(dag);
	}
	if (FM__FNC_TYPE_FP_TENSOR == type) {
		return fm__dag_is_fp_tensor(dag);
	}
	if (FM__FNC_TYPE_NUMERIC_TENSOR == type) {
		return fm__dag_is_numeric_tensor(dag);
	}
	if (FM__FNC_TYPE_ANY_TENSOR == type) {
		return fm__dag_is_tensor(dag);
	}
	if (FM__FNC_TYPE_INT_SCALAR == type) {
		return fm__dag_is_int(dag);
	}
	if (FM__FNC_TYPE_BOOL_SCALAR == type) {
		return fm__dag_is_bool(dag);
	}
	if (FM__FNC_TYPE_SAME_AS_ARG0 == type) {
		return (arg0 &&
			fm__dag_is_tensor(dag) &&
			(dag->type == arg0->type));
	}
	if (FM__FNC_TYPE_SAME_AS_ARG0_OR_SCALAR == type) {
		if (!arg0 || !fm__dag_is_tensor(arg0)) {
			return 0;
		}
		if (fm__dag_is_tensor(dag)) {
			return dag->type == arg0->type;
		}
		if (FM__DAG_TYPE_TBOOL == arg0->type) {
			return fm__dag_is_bool(dag);
		}
		if (fm__dag_is_int(dag)) {
			return 1;
		}
		if (fm__dag_is_real(dag) && fm__dag_is_fp_tensor(arg0)) {
			return 1;
		}
	}
	return 0;
}

static int
same_shape(const struct fm_geometry *a, const struct fm_geometry *b)
{
	return ((a->ndim == b->ndim) &&
		fm__utils_match_shape(a->shape, b->shape, a->ndim));
}

static const struct fm__dag *
last_argument(const struct fm__dag *dag)
{
	const struct fm__dag *arg;

	arg = dag->right;
	while (arg && arg->right) {
		arg = arg->right;
	}
	return ((arg && (FM__DAG_OP_EXPR_LIST == arg->op)) ? arg->left : NULL);
}

static int
shape_axis(const struct fm__dag *fnc,
	   const struct fm__dag *dag,
	   size_t ndim,
	   int insertion,
	   size_t *axis_)
{
	int64_t axis, limit;

	if (!dag || !fm__dag_is_int(dag) || !dag->u.i) {
		FM__TRACE(FM__ERRNO_SOFTWARE);
		return -1;
	}
	if (fm__bigint_int64(dag->u.i, 0 /* clamp */, &axis)) {
		ERR(fnc, "function axis is outside the int64 range");
		FM__TRACE(FM__ERRNO_SYNTAX);
		return -1;
	}
	limit = (int64_t)ndim + (insertion ? 1 : 0);
	if (0 > axis) {
		axis += limit;
	}
	if ((0 > axis) || (limit <= axis)) {
		ERR(fnc,
		    insertion
		    ? "function axis is outside the result rank"
		    : "function axis is outside the tensor rank");
		FM__TRACE(FM__ERRNO_SYNTAX);
		return -1;
	}
	(*axis_) = (size_t)axis;
	return 0;
}

enum fm__dag_type
fm__fnc_type(const struct fm__dag *dag)
{
	const struct fm__dag *arg, *arg0;
	const struct fm__fnc *fnc;
	size_t n;

	assert( dag && (FM__FNCS_SIZE > dag->index) );

	fnc = &FM__FNCS[dag->index];
	n = fm__fnc_types_count(dag->index);
	arg = dag->right;
	arg0 = arg ? arg->left : NULL;
	for (size_t i=1; i<n; ++i) {
		if (!arg ||
		    (FM__DAG_OP_EXPR_LIST != arg->op) ||
		    !match(fnc->types[i], arg->left, arg0)) {
			return FM__DAG_TYPE_UNKNOWN;
		}
		arg = arg->right;
	}
	if (arg) {
		if (!fnc->masked ||
		    (FM__DAG_OP_EXPR_LIST != arg->op) ||
		    arg->right ||
		    !match(FM__FNC_TYPE_BOOL_TENSOR, arg->left, arg0)) {
			return FM__DAG_TYPE_UNKNOWN;
		}
	}
	if (FM__FNC_TYPE_INT_SCALAR == fnc->types[0]) {
		return FM__DAG_TYPE_INT;
	}
	if (FM__FNC_TYPE_SAME_AS_ARG0 == fnc->types[0]) {
		return ((arg0 && fm__dag_is_tensor(arg0))
			? arg0->type
			: FM__DAG_TYPE_UNKNOWN);
	}
	if (FM__FNC_TYPE_BOOL_TENSOR == fnc->types[0]) {
		return FM__DAG_TYPE_TBOOL;
	}
	if (FM__FNC_TYPE_INT64_TENSOR == fnc->types[0]) {
		return FM__DAG_TYPE_INT64;
	}
	return FM__DAG_TYPE_UNKNOWN;
}

size_t
fm__fnc_types_count(size_t index)
{
	size_t n;

	n = 0;
	if ((0 < index) && (FM__FNCS_SIZE > index)) {
		while ((FM__FNC_MAX_TYPES > n) && FM__FNCS[index].types[n]) {
			++n;
		}
	}
	return n;
}

enum fm_fold_op
fm__fnc_fold(size_t index)
{
	const char *name;

	if ((FM__FNCS_SIZE <= index) ||
	    ((FM_EXPR_OP_FOLD != FM__FNCS[index].op) &&
	     (FM_EXPR_OP_SCAN != FM__FNCS[index].op))) {
		return FM_FOLD_OP_UNKNOWN;
	}
	name = FM__FNCS[index].name;
	if (!strcmp(name, "reduce_min")) {
		return FM_FOLD_OP_MIN;
	}
	if (!strcmp(name, "reduce_max")) {
		return FM_FOLD_OP_MAX;
	}
	if (!strcmp(name, "sum") || !strcmp(name, "cumsum")) {
		return FM_FOLD_OP_SUM;
	}
	if (!strcmp(name, "count")) {
		return FM_FOLD_OP_COUNT;
	}
	if (!strcmp(name, "product") || !strcmp(name, "cumprod")) {
		return FM_FOLD_OP_PRODUCT;
	}
	if (!strcmp(name, "argmin")) {
		return FM_FOLD_OP_ARGMIN;
	}
	if (!strcmp(name, "argmax")) {
		return FM_FOLD_OP_ARGMAX;
	}
	if (!strcmp(name, "any")) {
		return FM_FOLD_OP_ANY;
	}
	if (!strcmp(name, "all")) {
		return FM_FOLD_OP_ALL;
	}
	return FM_FOLD_OP_UNKNOWN;
}

int
fm__fnc_static(const struct fm__dag *dag, int64_t *value)
{
	const struct fm_geometry *geometry;
	enum fm__fnc_shape rule;
	size_t axis;

	assert( dag && value );

	if (!dag->index ||
	    (FM__FNCS_SIZE <= dag->index) ||
	    !dag->right ||
	    !dag->right->left) {
		FM__TRACE(FM__ERRNO_SOFTWARE);
		return -1;
	}
	geometry = &dag->right->left->geometry;
	if (!fm__geometry_is_populated(geometry)) {
		FM__TRACE(FM__ERRNO_SOFTWARE);
		return -1;
	}
	rule = FM__FNCS[dag->index].shape;
	if (FM__FNC_SHAPE_NDIM == rule) {
		(*value) = (int64_t)geometry->ndim;
	}
	else if (FM__FNC_SHAPE_NUMEL == rule) {
		(*value) = geometry->numel;
	}
	else if (FM__FNC_SHAPE_AXIS == rule) {
		if (shape_axis(dag,
			       last_argument(dag),
			       geometry->ndim,
			       0,
			       &axis)) {
			FM__TRACE(0);
			return -1;
		}
		(*value) = geometry->shape[axis];
	}
	else {
		FM__TRACE(FM__ERRNO_SOFTWARE);
		return -1;
	}
	return 0;
}

int
fm__fnc_shape(struct fm_geometry *geometry,
	      const struct fm__dag *dag,
	      const struct fm_geometry * const argv[],
	      size_t argc)
{
	const struct fm_geometry *a, *b, *index, *source;
	const struct fm__dag *axis_dag;
	enum fm__fnc_shape rule;
	int64_t extent, stride;
	size_t axis, i, j;

	assert( geometry && dag && argv );

	if (!argc ||
	    (FM__FNC_MAX_ARGS < argc) ||
	    (FM__FNCS_SIZE <= dag->index)) {
		FM__TRACE(FM__ERRNO_SOFTWARE);
		return -1;
	}
	a = argv[0];
	rule = FM__FNCS[dag->index].shape;
	i = fm__fnc_types_count(dag->index) - 1;
	if (argc != i) {
		if (!FM__FNCS[dag->index].masked || (argc != (i + 1))) {
			FM__TRACE(FM__ERRNO_SOFTWARE);
			return -1;
		}
		b = argv[i];
		if (b->ndim > a->ndim) {
			ERR(dag, "mask rank exceeds the input rank");
			FM__TRACE(FM__ERRNO_SYNTAX);
			return -1;
		}
		for (i=0; i<b->ndim; ++i) {
			j = a->ndim - b->ndim + i;
			if ((1 != b->shape[i]) &&
			    (a->shape[j] != b->shape[i])) {
				ERR(dag,
				    "mask shape must broadcast "
				    "to the input shape");
				FM__TRACE(FM__ERRNO_SYNTAX);
				return -1;
			}
		}
	}
	if ((FM__FNC_SHAPE_SCAN == rule) ||
	    (FM__FNC_SHAPE_ARGSORT == rule) ||
	    (FM__FNC_SHAPE_TOPK == rule)) {
		axis_dag = dag->right->right->left;
		if (FM__FNC_SHAPE_TOPK == rule) {
			axis_dag = dag->right->right->right->left;
		}
		if (shape_axis(dag, axis_dag, a->ndim, 0, &axis)) {
			FM__TRACE(0);
			return -1;
		}
		(*geometry) = (*a);
		if (FM__FNC_SHAPE_TOPK == rule) {
			if (fm__bigint_int64(dag->right->right->left->u.i,
					     0, /* clamp */
					     &extent)) {
				ERR(dag, "topk k is outside the int64 range");
				FM__TRACE(FM__ERRNO_SYNTAX);
				return -1;
			}
			if ((0 > extent) || (a->shape[axis] < extent)) {
				ERR(dag,
				    "topk k must be between "
				    "zero and the axis extent");
				FM__TRACE(FM__ERRNO_SYNTAX);
				return -1;
			}
			geometry->shape[axis] = extent;
		}
		return 0;
	}
	if (FM__FNC_SHAPE_SAME == rule) {
		for (i=1; i<argc; ++i) {
			b = argv[i];
			if (!b->ndim) {
				continue;
			}
			if (!a->ndim) {
				a = b;
			}
			else if (!same_shape(a, b)) {
				ERR(dag,
				    "function tensor arguments "
				    "must have identical shapes");
				FM__TRACE(FM__ERRNO_SYNTAX);
				return -1;
			}
		}
		(*geometry) = (*a);
		return 0;
	}
	if (FM__FNC_SHAPE_FLATTEN == rule) {
		if (!a->numel) {
			(*geometry) = (*a);
			geometry->ndim = 1;
			geometry->shape[0] = 0;
			geometry->stride[0] = 1;
			return 0;
		}
		extent = 1;
		for (i=0; i<a->ndim; ++i) {
			if (fm__mul_int64(extent, a->shape[i], &extent)) {
				ERR(dag,
				    "flattened tensor size "
				    "exceeds int64 range");
				FM__TRACE(FM__ERRNO_SYNTAX);
				return -1;
			}
		}
		(*geometry) = (*a);
		geometry->ndim = 1;
		geometry->shape[0] = extent;
		if (!(stride = fm__geometry_flatten_stride(a))) {
			geometry->size = extent;
			geometry->numel = extent;
			geometry->offset = 0;
			geometry->stride[0] = 1;
		}
		else {
			geometry->stride[0] = stride;
		}
		return 0;
	}
	axis_dag = ((FM__FNC_SHAPE_REDUCE == rule)
		    ? dag->right->right->left
		    : last_argument(dag));
	if (FM__FNC_SHAPE_UNSQUEEZE == rule) {
		if (FM_MAX_NDIM <= a->ndim) {
			ERR(dag,
			    "function result exceeds maximum tensor rank");
			FM__TRACE(FM__ERRNO_SYNTAX);
			return -1;
		}
		if (shape_axis(dag, axis_dag, a->ndim, 1, &axis)) {
			FM__TRACE(0);
			return -1;
		}
	}
	else if (shape_axis(dag, axis_dag, a->ndim, 0, &axis)) {
		FM__TRACE(0);
		return -1;
	}
	if ((FM__FNC_SHAPE_REDUCE == rule) ||
	    (FM__FNC_SHAPE_SQUEEZE == rule)) {
		if ((FM__FNC_SHAPE_SQUEEZE == rule) && (1 != a->shape[axis])) {
			ERR(dag, "squeeze axis must have extent one");
			FM__TRACE(FM__ERRNO_SYNTAX);
			return -1;
		}
		(*geometry) = (*a);
		geometry->ndim = a->ndim - 1;
		for (i=0, j=0; i<a->ndim; ++i) {
			if (i != axis) {
				geometry->shape[j] = a->shape[i];
				geometry->stride[j++] = a->stride[i];
			}
		}
		return 0;
	}
	if (FM__FNC_SHAPE_UNSQUEEZE == rule) {
		(*geometry) = (*a);
		geometry->ndim = a->ndim + 1;
		for (i=geometry->ndim; i>(axis + 1); --i) {
			geometry->shape[i - 1] = a->shape[i - 2];
			geometry->stride[i - 1] = a->stride[i - 2];
		}
		geometry->shape[axis] = 1;
		if (axis < a->ndim) {
			if (fm__mul_int64(a->stride[axis],
					  a->shape[axis],
					  &geometry->stride[axis])) {
				geometry->stride[axis] = 1;
			}
		}
		else {
			geometry->stride[axis] = 1;
		}
		return 0;
	}
	b = argv[1];
	if (FM__FNC_SHAPE_CONCAT == rule) {
		if (a->ndim != b->ndim) {
			ERR(dag, "concat arguments must have identical ranks");
			FM__TRACE(FM__ERRNO_SYNTAX);
			return -1;
		}
		geometry->ndim = a->ndim;
		for (i=0; i<a->ndim; ++i) {
			if ((i != axis) && (a->shape[i] != b->shape[i])) {
				ERR(dag,
				    "concat argument shapes "
				    "must match outside "
				    "the concatenation axis");
				FM__TRACE(FM__ERRNO_SYNTAX);
				return -1;
			}
			geometry->shape[i] = a->shape[i];
		}
		if (fm__add_int64(a->shape[axis],
				  b->shape[axis],
				  &geometry->shape[axis])) {
			ERR(dag,
			    "concatenated tensor extent exceeds int64 range");
			FM__TRACE(FM__ERRNO_SYNTAX);
			return -1;
		}
		return 0;
	}
	index = b;
	if (FM__FNC_SHAPE_GATHER == rule) {
		if (a->ndim != index->ndim) {
			ERR(dag,
			    "gather input and index "
			    "must have identical ranks");
			FM__TRACE(FM__ERRNO_SYNTAX);
			return -1;
		}
		for (i=0; i<a->ndim; ++i) {
			if ((i != axis) && (a->shape[i] < index->shape[i])) {
				ERR(dag,
				    "gather index shape "
				    "exceeds the input shape");
				FM__TRACE(FM__ERRNO_SYNTAX);
				return -1;
			}
		}
		(*geometry) = (*index);
		return 0;
	}
	if (FM__FNC_SHAPE_SCATTER == rule) {
		source = argv[2];
		if ((a->ndim != index->ndim) || !same_shape(index, source)) {
			ERR(dag,
			    "scatter index and source must have "
			    "identical shapes and match the input rank");
			FM__TRACE(FM__ERRNO_SYNTAX);
			return -1;
		}
		for (i=0; i<a->ndim; ++i) {
			if ((i != axis) && (a->shape[i] < index->shape[i])) {
				ERR(dag,
				    "scatter index shape "
				    "exceeds the input shape");
				FM__TRACE(FM__ERRNO_SYNTAX);
				return -1;
			}
		}
		(*geometry) = (*a);
		return 0;
	}
	FM__TRACE(FM__ERRNO_SOFTWARE);
	return -1;
}

const struct fm__fnc FM__FNCS[] = {
	{
		0
	}, { /* indirect */
		"ndim",
		{
			FM__FNC_TYPE_INT_SCALAR,
			FM__FNC_TYPE_ANY_TENSOR
		},
		FM__FNC_SHAPE_NDIM,
		FM_EXPR_OP_UNKNOWN,
		0
	}, { /* indirect */
		"numel",
		{
			FM__FNC_TYPE_INT_SCALAR,
			FM__FNC_TYPE_ANY_TENSOR
		},
		FM__FNC_SHAPE_NUMEL,
		FM_EXPR_OP_UNKNOWN,
		0
	}, { /* indirect */
		"shape",
		{
			FM__FNC_TYPE_INT_SCALAR,
			FM__FNC_TYPE_ANY_TENSOR,
			FM__FNC_TYPE_INT_SCALAR
		},
		FM__FNC_SHAPE_AXIS,
		FM_EXPR_OP_UNKNOWN,
		0
	}, { /* indirect */
		"flatten",
		{
			FM__FNC_TYPE_SAME_AS_ARG0,
			FM__FNC_TYPE_ANY_TENSOR
		},
		FM__FNC_SHAPE_FLATTEN,
		FM_EXPR_OP_VIEW,
		0
	}, { /* indirect */
		"squeeze",
		{
			FM__FNC_TYPE_SAME_AS_ARG0,
			FM__FNC_TYPE_ANY_TENSOR,
			FM__FNC_TYPE_INT_SCALAR
		},
		FM__FNC_SHAPE_SQUEEZE,
		FM_EXPR_OP_VIEW,
		0
	}, { /* indirect */
		"unsqueeze",
		{
			FM__FNC_TYPE_SAME_AS_ARG0,
			FM__FNC_TYPE_ANY_TENSOR,
			FM__FNC_TYPE_INT_SCALAR
		},
		FM__FNC_SHAPE_UNSQUEEZE,
		FM_EXPR_OP_VIEW,
		0
	}, { /* indirect */
		"reduce_min",
		{
			FM__FNC_TYPE_SAME_AS_ARG0,
			FM__FNC_TYPE_NUMERIC_TENSOR,
			FM__FNC_TYPE_INT_SCALAR
		},
		FM__FNC_SHAPE_REDUCE,
		FM_EXPR_OP_FOLD,
		1
	}, { /* indirect */
		"reduce_max",
		{
			FM__FNC_TYPE_SAME_AS_ARG0,
			FM__FNC_TYPE_NUMERIC_TENSOR,
			FM__FNC_TYPE_INT_SCALAR
		},
		FM__FNC_SHAPE_REDUCE,
		FM_EXPR_OP_FOLD,
		1
	}, { /* indirect */
		"sum",
		{
			FM__FNC_TYPE_SAME_AS_ARG0,
			FM__FNC_TYPE_NUMERIC_TENSOR,
			FM__FNC_TYPE_INT_SCALAR
		},
		FM__FNC_SHAPE_REDUCE,
		FM_EXPR_OP_FOLD,
		1
	}, { /* indirect */
		"count",
		{
			FM__FNC_TYPE_INT64_TENSOR,
			FM__FNC_TYPE_ANY_TENSOR,
			FM__FNC_TYPE_INT_SCALAR
		},
		FM__FNC_SHAPE_REDUCE,
		FM_EXPR_OP_FOLD,
		1
	}, { /* indirect */
		"product",
		{
			FM__FNC_TYPE_SAME_AS_ARG0,
			FM__FNC_TYPE_NUMERIC_TENSOR,
			FM__FNC_TYPE_INT_SCALAR
		},
		FM__FNC_SHAPE_REDUCE,
		FM_EXPR_OP_FOLD,
		1
	}, { /* indirect */
		"argmin",
		{
			FM__FNC_TYPE_INT64_TENSOR,
			FM__FNC_TYPE_NUMERIC_TENSOR,
			FM__FNC_TYPE_INT_SCALAR
		},
		FM__FNC_SHAPE_REDUCE,
		FM_EXPR_OP_FOLD,
		0
	}, { /* indirect */
		"argmax",
		{
			FM__FNC_TYPE_INT64_TENSOR,
			FM__FNC_TYPE_NUMERIC_TENSOR,
			FM__FNC_TYPE_INT_SCALAR
		},
		FM__FNC_SHAPE_REDUCE,
		FM_EXPR_OP_FOLD,
		0
	}, { /* indirect */
		"any",
		{
			FM__FNC_TYPE_BOOL_TENSOR,
			FM__FNC_TYPE_ANY_TENSOR,
			FM__FNC_TYPE_INT_SCALAR
		},
		FM__FNC_SHAPE_REDUCE,
		FM_EXPR_OP_FOLD,
		1
	}, { /* indirect */
		"all",
		{
			FM__FNC_TYPE_BOOL_TENSOR,
			FM__FNC_TYPE_ANY_TENSOR,
			FM__FNC_TYPE_INT_SCALAR
		},
		FM__FNC_SHAPE_REDUCE,
		FM_EXPR_OP_FOLD,
		1
	}, { /* indirect */
		"cumsum",
		{
			FM__FNC_TYPE_SAME_AS_ARG0,
			FM__FNC_TYPE_NUMERIC_TENSOR,
			FM__FNC_TYPE_INT_SCALAR
		},
		FM__FNC_SHAPE_SCAN,
		FM_EXPR_OP_SCAN,
		0
	}, { /* indirect */
		"cumprod",
		{
			FM__FNC_TYPE_SAME_AS_ARG0,
			FM__FNC_TYPE_NUMERIC_TENSOR,
			FM__FNC_TYPE_INT_SCALAR
		},
		FM__FNC_SHAPE_SCAN,
		FM_EXPR_OP_SCAN,
		0
	}, { /* explicit */
		"abs",
		{
			FM__FNC_TYPE_SAME_AS_ARG0,
			FM__FNC_TYPE_NUMERIC_TENSOR
		},
		FM__FNC_SHAPE_SAME,
		FM_EXPR_OP_ABS,
		0
	}, { /* explicit */
		"signbit",
		{
			FM__FNC_TYPE_BOOL_TENSOR,
			FM__FNC_TYPE_FP_TENSOR
		},
		FM__FNC_SHAPE_SAME,
		FM_EXPR_OP_SIGNBIT,
		0
	}, { /* explicit */
		"copysign",
		{
			FM__FNC_TYPE_SAME_AS_ARG0,
			FM__FNC_TYPE_FP_TENSOR,
			FM__FNC_TYPE_SAME_AS_ARG0_OR_SCALAR
		},
		FM__FNC_SHAPE_SAME,
		FM_EXPR_OP_COPYSIGN,
		0
	}, { /* explicit */
		"min",
		{
			FM__FNC_TYPE_SAME_AS_ARG0,
			FM__FNC_TYPE_NUMERIC_TENSOR,
			FM__FNC_TYPE_SAME_AS_ARG0_OR_SCALAR
		},
		FM__FNC_SHAPE_SAME,
		FM_EXPR_OP_MIN,
		0
	}, { /* explicit */
		"max",
		{
			FM__FNC_TYPE_SAME_AS_ARG0,
			FM__FNC_TYPE_NUMERIC_TENSOR,
			FM__FNC_TYPE_SAME_AS_ARG0_OR_SCALAR
		},
		FM__FNC_SHAPE_SAME,
		FM_EXPR_OP_MAX,
		0
	}, { /* explicit */
		"clamp",
		{
			FM__FNC_TYPE_SAME_AS_ARG0,
			FM__FNC_TYPE_NUMERIC_TENSOR,
			FM__FNC_TYPE_SAME_AS_ARG0_OR_SCALAR,
			FM__FNC_TYPE_SAME_AS_ARG0_OR_SCALAR
		},
		FM__FNC_SHAPE_SAME,
		FM_EXPR_OP_CLAMP,
		0
	}, { /* explicit */
		"ceil",
		{
			FM__FNC_TYPE_SAME_AS_ARG0,
			FM__FNC_TYPE_FP_TENSOR
		},
		FM__FNC_SHAPE_SAME,
		FM_EXPR_OP_CEIL,
		0
	}, { /* explicit */
		"floor",
		{
			FM__FNC_TYPE_SAME_AS_ARG0,
			FM__FNC_TYPE_FP_TENSOR
		},
		FM__FNC_SHAPE_SAME,
		FM_EXPR_OP_FLOOR,
		0
	}, { /* explicit */
		"round",
		{
			FM__FNC_TYPE_SAME_AS_ARG0,
			FM__FNC_TYPE_FP_TENSOR
		},
		FM__FNC_SHAPE_SAME,
		FM_EXPR_OP_ROUND,
		0
	}, { /* explicit */
		"trunc",
		{
			FM__FNC_TYPE_SAME_AS_ARG0,
			FM__FNC_TYPE_FP_TENSOR
		},
		FM__FNC_SHAPE_SAME,
		FM_EXPR_OP_TRUNC,
		0
	}, { /* explicit */
		"isinf",
		{
			FM__FNC_TYPE_BOOL_TENSOR,
			FM__FNC_TYPE_FP_TENSOR
		},
		FM__FNC_SHAPE_SAME,
		FM_EXPR_OP_ISINF,
		0
	}, { /* explicit */
		"isnan",
		{
			FM__FNC_TYPE_BOOL_TENSOR,
			FM__FNC_TYPE_FP_TENSOR
		},
		FM__FNC_SHAPE_SAME,
		FM_EXPR_OP_ISNAN,
		0
	}, { /* explicit */
		"isnormal",
		{
			FM__FNC_TYPE_BOOL_TENSOR,
			FM__FNC_TYPE_FP_TENSOR
		},
		FM__FNC_SHAPE_SAME,
		FM_EXPR_OP_ISNORMAL,
		0
	}, { /* explicit */
		"isfinite",
		{
			FM__FNC_TYPE_BOOL_TENSOR,
			FM__FNC_TYPE_FP_TENSOR
		},
		FM__FNC_SHAPE_SAME,
		FM_EXPR_OP_ISFINITE,
		0
	}, { /* explicit */
		"exp",
		{
			FM__FNC_TYPE_SAME_AS_ARG0,
			FM__FNC_TYPE_FP_TENSOR
		},
		FM__FNC_SHAPE_SAME,
		FM_EXPR_OP_EXP,
		0
	}, { /* explicit */
		"exp2",
		{
			FM__FNC_TYPE_SAME_AS_ARG0,
			FM__FNC_TYPE_FP_TENSOR
		},
		FM__FNC_SHAPE_SAME,
		FM_EXPR_OP_EXP2,
		0
	}, { /* explicit */
		"log",
		{
			FM__FNC_TYPE_SAME_AS_ARG0,
			FM__FNC_TYPE_FP_TENSOR
		},
		FM__FNC_SHAPE_SAME,
		FM_EXPR_OP_LOG,
		0
	}, { /* explicit */
		"log1p",
		{
			FM__FNC_TYPE_SAME_AS_ARG0,
			FM__FNC_TYPE_FP_TENSOR
		},
		FM__FNC_SHAPE_SAME,
		FM_EXPR_OP_LOG1P,
		0
	}, { /* explicit */
		"sqrt",
		{
			FM__FNC_TYPE_SAME_AS_ARG0,
			FM__FNC_TYPE_FP_TENSOR
		},
		FM__FNC_SHAPE_SAME,
		FM_EXPR_OP_SQRT,
		0
	}, { /* explicit */
		"pow",
		{
			FM__FNC_TYPE_SAME_AS_ARG0,
			FM__FNC_TYPE_FP_TENSOR,
			FM__FNC_TYPE_SAME_AS_ARG0_OR_SCALAR
		},
		FM__FNC_SHAPE_SAME,
		FM_EXPR_OP_POW,
		0
	}, { /* explicit */
		"sin",
		{
			FM__FNC_TYPE_SAME_AS_ARG0,
			FM__FNC_TYPE_FP_TENSOR
		},
		FM__FNC_SHAPE_SAME,
		FM_EXPR_OP_SIN,
		0
	}, { /* explicit */
		"cos",
		{
			FM__FNC_TYPE_SAME_AS_ARG0,
			FM__FNC_TYPE_FP_TENSOR
		},
		FM__FNC_SHAPE_SAME,
		FM_EXPR_OP_COS,
		0
	}, { /* explicit */
		"tan",
		{
			FM__FNC_TYPE_SAME_AS_ARG0,
			FM__FNC_TYPE_FP_TENSOR
		},
		FM__FNC_SHAPE_SAME,
		FM_EXPR_OP_TAN,
		0
	}, { /* explicit */
		"sec",
		{
			FM__FNC_TYPE_SAME_AS_ARG0,
			FM__FNC_TYPE_FP_TENSOR
		},
		FM__FNC_SHAPE_SAME,
		FM_EXPR_OP_SEC,
		0
	}, { /* explicit */
		"cot",
		{
			FM__FNC_TYPE_SAME_AS_ARG0,
			FM__FNC_TYPE_FP_TENSOR
		},
		FM__FNC_SHAPE_SAME,
		FM_EXPR_OP_COT,
		0
	}, { /* explicit */
		"asin",
		{
			FM__FNC_TYPE_SAME_AS_ARG0,
			FM__FNC_TYPE_FP_TENSOR
		},
		FM__FNC_SHAPE_SAME,
		FM_EXPR_OP_ASIN,
		0
	}, { /* explicit */
		"acos",
		{
			FM__FNC_TYPE_SAME_AS_ARG0,
			FM__FNC_TYPE_FP_TENSOR
		},
		FM__FNC_SHAPE_SAME,
		FM_EXPR_OP_ACOS,
		0
	}, { /* explicit */
		"atan",
		{
			FM__FNC_TYPE_SAME_AS_ARG0,
			FM__FNC_TYPE_FP_TENSOR
		},
		FM__FNC_SHAPE_SAME,
		FM_EXPR_OP_ATAN,
		0
	}, { /* explicit */
		"atan2",
		{
			FM__FNC_TYPE_SAME_AS_ARG0,
			FM__FNC_TYPE_FP_TENSOR,
			FM__FNC_TYPE_SAME_AS_ARG0_OR_SCALAR
		},
		FM__FNC_SHAPE_SAME,
		FM_EXPR_OP_ATAN2,
		0
	}, { /* explicit */
		"erf",
		{
			FM__FNC_TYPE_SAME_AS_ARG0,
			FM__FNC_TYPE_FP_TENSOR
		},
		FM__FNC_SHAPE_SAME,
		FM_EXPR_OP_ERF,
		0
	}, { /* explicit */
		"erfc",
		{
			FM__FNC_TYPE_SAME_AS_ARG0,
			FM__FNC_TYPE_FP_TENSOR
		},
		FM__FNC_SHAPE_SAME,
		FM_EXPR_OP_ERFC,
		0
	}, { /* explicit */
		"lgamma",
		{
			FM__FNC_TYPE_SAME_AS_ARG0,
			FM__FNC_TYPE_FP_TENSOR
		},
		FM__FNC_SHAPE_SAME,
		FM_EXPR_OP_LGAMMA,
		0
	}, { /* explicit */
		"tgamma",
		{
			FM__FNC_TYPE_SAME_AS_ARG0,
			FM__FNC_TYPE_FP_TENSOR
		},
		FM__FNC_SHAPE_SAME,
		FM_EXPR_OP_TGAMMA,
		0
	}, { /* explicit */
		"normal_cdf",
		{
			FM__FNC_TYPE_SAME_AS_ARG0,
			FM__FNC_TYPE_FP_TENSOR
		},
		FM__FNC_SHAPE_SAME,
		FM_EXPR_OP_NORMAL_CDF,
		0
	}, { /* explicit */
		"normal_sf",
		{
			FM__FNC_TYPE_SAME_AS_ARG0,
			FM__FNC_TYPE_FP_TENSOR
		},
		FM__FNC_SHAPE_SAME,
		FM_EXPR_OP_NORMAL_SF,
		0
	}, { /* explicit */
		"relu",
		{
			FM__FNC_TYPE_SAME_AS_ARG0,
			FM__FNC_TYPE_FP_TENSOR
		},
		FM__FNC_SHAPE_SAME,
		FM_EXPR_OP_RELU,
		0
	}, { /* explicit */
		"gelu",
		{
			FM__FNC_TYPE_SAME_AS_ARG0,
			FM__FNC_TYPE_FP_TENSOR
		},
		FM__FNC_SHAPE_SAME,
		FM_EXPR_OP_GELU,
		0
	}, { /* explicit */
		"sigmoid",
		{
			FM__FNC_TYPE_SAME_AS_ARG0,
			FM__FNC_TYPE_FP_TENSOR
		},
		FM__FNC_SHAPE_SAME,
		FM_EXPR_OP_SIGMOID,
		0
	}, { /* explicit */
		"silu",
		{
			FM__FNC_TYPE_SAME_AS_ARG0,
			FM__FNC_TYPE_FP_TENSOR
		},
		FM__FNC_SHAPE_SAME,
		FM_EXPR_OP_SILU,
		0
	}, { /* explicit */
		"softplus",
		{
			FM__FNC_TYPE_SAME_AS_ARG0,
			FM__FNC_TYPE_FP_TENSOR
		},
		FM__FNC_SHAPE_SAME,
		FM_EXPR_OP_SOFTPLUS,
		0
	}, { /* explicit */
		"concat",
		{
			FM__FNC_TYPE_SAME_AS_ARG0,
			FM__FNC_TYPE_ANY_TENSOR,
			FM__FNC_TYPE_SAME_AS_ARG0,
			FM__FNC_TYPE_INT_SCALAR
		},
		FM__FNC_SHAPE_CONCAT,
		FM_EXPR_OP_CONCAT,
		0
	}, { /* explicit */
		"gather",
		{
			FM__FNC_TYPE_SAME_AS_ARG0,
			FM__FNC_TYPE_ANY_TENSOR,
			FM__FNC_TYPE_INT_TENSOR,
			FM__FNC_TYPE_INT_SCALAR
		},
		FM__FNC_SHAPE_GATHER,
		FM_EXPR_OP_GATHER,
		0
	}, { /* explicit */
		"scatter",
		{
			FM__FNC_TYPE_SAME_AS_ARG0,
			FM__FNC_TYPE_ANY_TENSOR,
			FM__FNC_TYPE_INT_TENSOR,
			FM__FNC_TYPE_SAME_AS_ARG0,
			FM__FNC_TYPE_INT_SCALAR
		},
		FM__FNC_SHAPE_SCATTER,
		FM_EXPR_OP_SCATTER,
		0
	}, { /* explicit */
		"argsort",
		{
			FM__FNC_TYPE_INT64_TENSOR,
			FM__FNC_TYPE_NUMERIC_TENSOR,
			FM__FNC_TYPE_INT_SCALAR,
			FM__FNC_TYPE_BOOL_SCALAR
		},
		FM__FNC_SHAPE_ARGSORT,
		FM_EXPR_OP_ARGSORT,
		0
	}, { /* explicit */
		"topk",
		{
			FM__FNC_TYPE_INT64_TENSOR,
			FM__FNC_TYPE_NUMERIC_TENSOR,
			FM__FNC_TYPE_INT_SCALAR,
			FM__FNC_TYPE_INT_SCALAR,
			FM__FNC_TYPE_BOOL_SCALAR,
			FM__FNC_TYPE_BOOL_SCALAR
		},
		FM__FNC_SHAPE_TOPK,
		FM_EXPR_OP_TOPK,
		0
	}
};

const size_t FM__FNCS_SIZE = FM__ARRAY_SIZE(FM__FNCS);
