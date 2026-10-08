/**
 * Copyright (c) 2025-2026 Tony Givargis
 * University of California, Irvine
 * fm_geometry.c
 */

#include "fm_bitset.h"
#include "fm_fnc.h"
#include "fm_utils.h"
#include "fm_geometry.h"

#define ERR FM__UTILS_ERR_DAG

static int
contiguous_geometry(struct fm_geometry *geometry, const struct fm__dag *dag)
{
	int64_t extent, numel;
	size_t i;

	for (i=0; i<geometry->ndim; ++i) {
		if (!geometry->shape[i]) {
			for (i=0; i<geometry->ndim; ++i) {
				geometry->stride[i] = 1;
			}
			geometry->size = 0;
			geometry->numel = 0;
			geometry->offset = 0;
			return 0;
		}
	}
	if (geometry->ndim) {
		geometry->stride[geometry->ndim - 1] = 1;
		for (i=geometry->ndim - 1; i>0; --i) {
			extent = geometry->shape[i];
			if (fm__mul_int64(geometry->stride[i],
					  extent,
					  &geometry->stride[i - 1])) {
				ERR(dag,
				    "contiguous tensor "
				    "stride exceeds "
				    "int64 range");
				FM__TRACE(FM__ERRNO_SYNTAX);
				return -1;
			}
		}
	}
	numel = 1;
	for (i=0; i<geometry->ndim; ++i) {
		if (fm__mul_int64(numel, geometry->shape[i], &numel)) {
			ERR(dag, "tensor element count exceeds int64 range");
			FM__TRACE(FM__ERRNO_SYNTAX);
			return -1;
		}
	}
	geometry->size = numel;
	geometry->numel = numel;
	geometry->offset = 0;
	return 0;
}

static int
tensor_geometry(struct fm_geometry *geometry, const struct fm__dag *dag)
{
	int64_t size, numel, address, index[FM_MAX_NDIM];
	size_t i, j, k, order[FM_MAX_NDIM];
	fm__bitset_t bitset;
	int proven;

	/* parser contract */

	if ((1 > geometry->ndim) ||
	    (FM_MAX_NDIM < geometry->ndim) ||
	    geometry->offset ||
	    geometry->numel ||
	    geometry->size) {
		FM__TRACE(FM__ERRNO_SOFTWARE);
		return -1;
	}
	for (i=0; i<geometry->ndim; ++i) {
		if ((0 > geometry->shape[i]) || (0 >= geometry->stride[i])) {
			FM__TRACE(FM__ERRNO_SOFTWARE);
			return -1;
		}
	}

	/* special case */

	for (i=0; i<geometry->ndim; ++i) {
		if (!geometry->shape[i]) {
			geometry->size = 0;
			geometry->numel = 0;
			return 0;
		}
	}

	/* order dimensions whose extent > 1 and sort */

	for (i=k=0; i<geometry->ndim; ++i) {
		if (1 < geometry->shape[i]) {
			order[k++] = i;
		}
	}
	for (i=0; i<k; ++i) {
		for (j=i+1; j<k; ++j) {
			if (geometry->stride[order[i]] >
			    geometry->stride[order[j]]) {
				size_t temp = order[i];
				order[i] = order[j];
				order[j] = temp;
			}
		}
	}

	/* quick non-overlap proof */

	size = 1;
	proven = 1;
	for (i=0; i<k; ++i) {
		int64_t delta;

		if (geometry->stride[order[i]] < size) {
			proven = 0;
			break;
		}
		if (fm__mul_int64(geometry->shape[order[i]] - 1,
				  geometry->stride[order[i]],
				  &delta) ||
		    fm__add_int64(size, delta, &size)) {
			ERR(dag, "storage size for tensor is too large");
			FM__TRACE(FM__ERRNO_SYNTAX);
			return -1;
		}
	}

	/* numel and size */

	size = 1;
	numel = 1;
	for (i=0; i<geometry->ndim; ++i) {
		int64_t delta;
		if (fm__mul_int64(numel, geometry->shape[i], &numel)) {
			ERR(dag, "element count for tensor is too large");
			FM__TRACE(FM__ERRNO_SYNTAX);
			return -1;
		}
		if (fm__mul_int64(geometry->shape[i] - 1,
				  geometry->stride[i],
				  &delta) ||
		    fm__add_int64(size, delta, &size)) {
			ERR(dag, "storage size for tensor is too large");
			FM__TRACE(FM__ERRNO_SYNTAX);
			return -1;
		}
	}
	if (proven) {
		geometry->size = size;
		geometry->numel = numel;
		return 0;
	}

	/* slow non-overlap proof */

	address = 0;
	memset(index, 0, sizeof (index));
	if (!(bitset = fm__bitset_open((size_t)size))) {
		FM__TRACE(0);
		return -1;
	}
	for (;;) {
		if (fm__bitset_get(bitset, (size_t)address)) {
			fm__bitset_close(bitset);
			ERR(dag, "strides for tensor overlap");
			FM__TRACE(FM__ERRNO_SYNTAX);
			return -1;
		}
		fm__bitset_set(bitset, (size_t)address);
		for (i=geometry->ndim; i>0; --i) {
			j = i - 1;
			if (++index[j] < geometry->shape[j]) {
				address += geometry->stride[j];
				break;
			}
			address -= ((geometry->shape[j] - 1) *
				    geometry->stride[j]);
			index[j] = 0;
		}
		if (!i) {
			break;
		}
	}
	fm__bitset_close(bitset);
	geometry->size = size;
	geometry->numel = numel;
	return 0;
}

static int
apply_slices(struct fm_geometry *view, const struct fm__dag *list)
{
	int64_t start, stop, step, extent, delta, offset;
	const struct fm__dag *dag, *slice;
	struct fm_geometry result;
	size_t input, output;
	const char *error;
	uint64_t divisor;

	dag = list;
	error = NULL;
	result = (*view);
	input = output = 0;
	offset = view->offset;
	while (list) {
		if ((FM__DAG_OP_EXPR_SLICES != list->op) ||
		    !(slice = list->left) ||
		    (FM__DAG_OP_EXPR_SLICE != slice->op) ||
		    (view->ndim <= input)) {
			ERR(dag, "invalid tensor slice list");
			FM__TRACE(FM__ERRNO_SYNTAX);
			return -1;
		}
		if (((FM__DAG_SLICE_SCALAR == slice->slice) &&
		     !slice->cond) ||
		    (slice->cond &&
		     (FM__DAG_TYPE_INT != slice->cond->type)) ||
		    (slice->left &&
		     (FM__DAG_TYPE_INT != slice->left->type)) ||
		    (slice->right &&
		     (FM__DAG_TYPE_INT != slice->right->type))) {
			FM__TRACE(FM__ERRNO_SOFTWARE);
			return -1;
		}
		if (FM__DAG_SLICE_SCALAR == slice->slice) {
			if (fm__bigint_int64(slice->cond->u.i,
					     0, /* clamp */
					     &start)) {
				ERR(dag,
				    "tensor index is outside the int64 range");
				FM__TRACE(FM__ERRNO_SYNTAX);
				return -1;
			}
			if ((0 > start) &&
			    fm__add_int64(start,
					  view->shape[input],
					  &start)) {
				ERR(dag,
				    "tensor index is outside the int64 range");
				FM__TRACE(FM__ERRNO_SYNTAX);
				return -1;
			}
			if ((0 > start) || (view->shape[input] <= start)) {
				ERR(dag,
				    "tensor index is outside "
				    "the dimension extent");
				FM__TRACE(FM__ERRNO_SYNTAX);
				return -1;
			}
			if (fm__mul_int64(start,
					  view->stride[input],
					  &delta) ||
			    fm__add_int64(offset, delta, &offset)) {
				if (!error) {
					error = "tensor index offset is "
						"outside the int64 range";
				}
				offset = 0;
			}
		}
		else {
			step = 1;
			if (slice->right &&
			    fm__bigint_int64(slice->right->u.i,
					     0, /* clamp */
					     &step)) {
				ERR(dag,
				    "slice step is outside the int64 range");
				FM__TRACE(FM__ERRNO_SYNTAX);
				return -1;
			}
			if (!step) {
				ERR(dag, "slice step cannot be zero");
				FM__TRACE(FM__ERRNO_SYNTAX);
				return -1;
			}
			if (0 < step) {
				start = 0;
				stop = view->shape[input];
				if (slice->cond &&
				    fm__bigint_int64(slice->cond->u.i,
						     1, /* clamp */
						     &start)) {
					ERR(dag,
					    "slice start is "
					    "outside the "
					    "int64 range");
					FM__TRACE(FM__ERRNO_SYNTAX);
					return -1;
				}
				if (slice->left &&
				    fm__bigint_int64(slice->left->u.i,
						     1, /* clamp */
						     &stop)) {
					ERR(dag,
					    "slice stop is "
					    "outside the "
					    "int64 range");
					FM__TRACE(FM__ERRNO_SYNTAX);
					return -1;
				}
				if (slice->cond &&
				    (0 > start) &&
				    fm__add_int64(start,
						  view->shape[input],
						  &start)) {
					ERR(dag,
					    "slice start is "
					    "outside the "
					    "int64 range");
					FM__TRACE(FM__ERRNO_SYNTAX);
					return -1;
				}
				if (slice->left &&
				    (0 > stop) &&
				    fm__add_int64(stop,
						  view->shape[input],
						  &stop)) {
					ERR(dag,
					    "slice stop is "
					    "outside the "
					    "int64 range");
					FM__TRACE(FM__ERRNO_SYNTAX);
					return -1;
				}
				if (0 > start) {
					start = 0;
				}
				if (view->shape[input] < start) {
					start = view->shape[input];
				}
				if (0 > stop) {
					stop = 0;
				}
				if (view->shape[input] < stop) {
					stop = view->shape[input];
				}
				extent = ((start < stop)
					  ? 1 + (stop - 1 - start) / step
					  : 0);
			}
			else {
				start = view->shape[input] - 1;
				stop = -1;
				if (slice->cond &&
				    fm__bigint_int64(slice->cond->u.i,
						     1, /* clamp */
						     &start)) {
					ERR(dag,
					    "slice start is "
					    "outside the "
					    "int64 range");
					FM__TRACE(FM__ERRNO_SYNTAX);
					return -1;
				}
				if (slice->left &&
				    fm__bigint_int64(slice->left->u.i,
						     1, /* clamp */
						     &stop)) {
					ERR(dag,
					    "slice stop is "
					    "outside the "
					    "int64 range");
					FM__TRACE(FM__ERRNO_SYNTAX);
					return -1;
				}
				if (slice->cond &&
				    (0 > start) &&
				    fm__add_int64(start,
						  view->shape[input],
						  &start)) {
					ERR(dag,
					    "slice start is "
					    "outside the "
					    "int64 range");
					FM__TRACE(FM__ERRNO_SYNTAX);
					return -1;
				}
				if (slice->left &&
				    (0 > stop) &&
				    fm__add_int64(stop,
						  view->shape[input],
						  &stop)) {
					ERR(dag,
					    "slice stop is "
					    "outside the "
					    "int64 range");
					FM__TRACE(FM__ERRNO_SYNTAX);
					return -1;
				}
				if (-1 > start) {
					start = -1;
				}
				if (view->shape[input] <= start) {
					start = view->shape[input] - 1;
				}
				if (-1 > stop) {
					stop = -1;
				}
				if (view->shape[input] <= stop) {
					stop = view->shape[input] - 1;
				}
				divisor = (uint64_t)(-(step + 1)) + 1;
				extent = ((stop < start)
					  ? 1 + (int64_t)
					  ((uint64_t)
					   (start - stop - 1) / divisor)
					  : 0);
			}
			if (extent &&
			    (fm__mul_int64(start,
					   view->stride[input],
					   &delta) ||
			     fm__add_int64(offset, delta, &offset))) {
				if (!error) {
					error = "slice offset is outside "
						"the int64 range";
				}
				offset = 0;
			}
			if (fm__mul_int64(view->stride[input],
					  step,
					  &result.stride[output])) {
				if ((1 < extent) && !error) {
					error = "slice stride is outside "
						"the int64 range";
				}
				result.stride[output] = 1;
			}
			result.shape[output++] = extent;
		}
		++input;
		list = list->right;
	}
	while (input < view->ndim) {
		result.shape[output] = view->shape[input];
		result.stride[output] = view->stride[input];
		++input;
		++output;
	}
	result.ndim = output;

	/* empty views do not require representable addresses */

	for (input=0; input<result.ndim; ++input) {
		if (!result.shape[input]) {
			result.offset = 0;
			for (output=0; output<result.ndim; ++output) {
				result.stride[output] = 1;
			}
			(*view) = result;
			return 0;
		}
	}
	if (error) {
		ERR(dag, error);
		FM__TRACE(FM__ERRNO_SYNTAX);
		return -1;
	}
	result.offset = offset;
	(*view) = result;
	return 0;
}

static int
apply_permute(struct fm_geometry *view, const struct fm__dag *list)
{
	struct fm_geometry result;
	const struct fm__dag *dag;
	int seen[FM_MAX_NDIM];
	int64_t index;

	dag = list;
	result = (*view);
	memset(seen, 0, sizeof (seen));
	for (size_t i=0; i<view->ndim; ++i) {
		if (!list || (FM__DAG_OP_EXPR_LIST != list->op)) {
			ERR(dag,
			    "permutation dimension count "
			    "does not match the tensor rank");
			FM__TRACE(FM__ERRNO_SYNTAX);
			return -1;
		}
		if (!list->left || (FM__DAG_TYPE_INT != list->left->type)) {
			FM__TRACE(FM__ERRNO_SOFTWARE);
			return -1;
		}
		if (fm__bigint_int64(list->left->u.i, 0 /* clamp */, &index)) {
			ERR(dag,
			    "permutation index is outside the int64 range");
			FM__TRACE(FM__ERRNO_SYNTAX);
			return -1;
		}
		if ((0 > index) &&
		    fm__add_int64(index, (int64_t)view->ndim, &index)) {
			ERR(dag,
			    "permutation index is outside the int64 range");
			FM__TRACE(FM__ERRNO_SYNTAX);
			return -1;
		}
		if ((0 > index) ||
		    ((int64_t)view->ndim <= index) ||
		    seen[index]) {
			ERR(dag,
			    "permutation indices must "
			    "be unique and within rank");
			FM__TRACE(FM__ERRNO_SYNTAX);
			return -1;
		}
		seen[index] = 1;
		result.shape[i] = view->shape[index];
		result.stride[i] = view->stride[index];
		list = list->right;
	}
	if (list) {
		ERR(dag,
		    "permutation dimension count "
		    "does not match the tensor rank");
		FM__TRACE(FM__ERRNO_SYNTAX);
		return -1;
	}
	(*view) = result;
	return 0;
}

static int
apply_view(struct fm_geometry *geometry, const struct fm__dag *dag)
{
	int64_t temp;
	size_t j;

	if ((FM__DAG_OP_EXPR_VIEW != dag->op) ||
	    !dag->left ||
	    (dag->right && dag->cond)) {
		FM__TRACE(FM__ERRNO_SOFTWARE);
		return -1;
	}
	if (dag->right) {
		return apply_slices(geometry, dag->right);
	}
	if (dag->cond) {
		return apply_permute(geometry, dag->cond);
	}
	for (size_t i=0; i<(geometry->ndim / 2); ++i) {
		j = geometry->ndim - 1 - i;
		temp = geometry->shape[i];
		geometry->shape[i] = geometry->shape[j];
		geometry->shape[j] = temp;
		temp = geometry->stride[i];
		geometry->stride[i] = geometry->stride[j];
		geometry->stride[j] = temp;
	}
	return 0;
}

static int
broadcast_to(struct fm_geometry *geometry_,
	     const struct fm_geometry *operand,
	     const int64_t shape[],
	     size_t ndim,
	     const struct fm__dag *dag)
{
	struct fm_geometry geometry;
	size_t j, leading;
	int64_t numel;
	int empty;

	if (FM_MAX_NDIM < operand->ndim) {
		FM__TRACE(FM__ERRNO_SOFTWARE);
		return -1;
	}
	if (FM_MAX_NDIM < ndim) {
		ERR(dag, "broadcast exceeds maximum tensor rank");
		FM__TRACE(FM__ERRNO_SYNTAX);
		return -1;
	}
	memset(&geometry, 0, sizeof (struct fm_geometry));
	geometry.ndim = ndim;
	empty = 0;
	for (size_t i=0; i<ndim; ++i) {
		if (0 > shape[i]) {
			ERR(dag, "broadcast dimensions must be nonnegative");
			FM__TRACE(FM__ERRNO_SYNTAX);
			return -1;
		}
		geometry.shape[i] = shape[i];
		if (!shape[i]) {
			empty = 1;
		}
	}
	if (geometry.ndim < operand->ndim) {
		ERR(dag, "broadcast target rank is less than input rank");
		FM__TRACE(FM__ERRNO_SYNTAX);
		return -1;
	}
	leading = geometry.ndim - operand->ndim;
	for (size_t i=leading; i<geometry.ndim; ++i) {
		j = i - leading;
		if (geometry.shape[i] == operand->shape[j]) {
			geometry.stride[i] = operand->stride[j];
		}
		else if (1 != operand->shape[j]) {
			ERR(dag,
			    "broadcast can only expand "
			    "dimensions of size one");
			FM__TRACE(FM__ERRNO_SYNTAX);
			return -1;
		}
	}
	numel = empty ? 0 : 1;
	for (size_t i=0; !empty && (i<geometry.ndim); ++i) {
		if (fm__mul_int64(numel, geometry.shape[i], &numel)) {
			ERR(dag,
			    "broadcast element count exceeds int64 range");
			FM__TRACE(FM__ERRNO_SYNTAX);
			return -1;
		}
	}
	geometry.numel = numel;
	geometry.size = operand->size;
	geometry.offset = operand->offset;
	(*geometry_) = geometry;
	return 0;
}

void
fm__geometry_init(void)
{
	if ((uintmax_t)SIZE_MAX < (uintmax_t)INT64_MAX) {
		FM__TRACE(FM__ERRNO_ARCHITECTURE);
		abort();
	}
}

int
fm__geometry_create(struct fm_geometry *geometry, const struct fm__dag *dag)
{
	const struct fm__dag *shape, *stride;
	int64_t extent;
	char buf[256];
	size_t i;

	assert( geometry && dag );

	memset(geometry, 0, sizeof (struct fm_geometry));
	if ((FM__DAG_OP_DECL_TENSOR != dag->op) ||
	    !dag->left ||
	    (FM__DAG_OP_DECL_GEOMETRY != dag->left->op) ||
	    !dag->left->right) {
		FM__TRACE(FM__ERRNO_SOFTWARE);
		return -1;
	}
	shape = dag->left->right;
	stride = dag->left->left;
	if ((0 == (geometry->ndim = fm__utils_list_count(shape))) ||
	    (stride &&
	     (geometry->ndim != fm__utils_list_count(stride)))) {
		FM__TRACE(FM__ERRNO_SOFTWARE);
		return -1;
	}
	if (FM_MAX_NDIM < geometry->ndim) {
		ERR(dag, "tensor rank exceeds the maximum supported rank");
		FM__TRACE(FM__ERRNO_SYNTAX);
		return -1;
	}
	for (i=0; i<geometry->ndim; ++i) {
		if (!fm__dag_is_int(shape->left) ||
		    (stride && !fm__dag_is_int(stride->left))) {
			FM__TRACE(FM__ERRNO_SOFTWARE);
			return -1;
		}
		if (0 > fm__bigint_sign(shape->left->u.i)) {
			ERR(shape->left,
			    "tensor dimension cannot be negative");
			FM__TRACE(FM__ERRNO_SYNTAX);
			return -1;
		}
		if (stride && (0 >= fm__bigint_sign(stride->left->u.i))) {
			ERR(stride->left, "tensor stride must be positive");
			FM__TRACE(FM__ERRNO_SYNTAX);
			return -1;
		}
		if (fm__bigint_int64(shape->left->u.i,
				     0, /* clamp */
				     &geometry->shape[i])) {
			snprintf(buf,
				 sizeof (buf),
				 "dimension index %lu of tensor "
				 "is too large",
				 (unsigned long)i);
			ERR(dag, buf);
			FM__TRACE(FM__ERRNO_SYNTAX);
			return -1;
		}
		if (stride && fm__bigint_int64(stride->left->u.i,
					       0, /* clamp */
					       &geometry->stride[i])) {
			snprintf(buf,
				 sizeof (buf),
				 "stride at dimension index %lu of "
				 "tensor is too large",
				 (unsigned long)i);
			ERR(dag, buf);
			FM__TRACE(FM__ERRNO_SYNTAX);
			return -1;
		}
		shape = shape->right;
		stride = stride ? stride->right : NULL;
	}
	if (!dag->left->left) {
		for (i=0; i<geometry->ndim; ++i) {
			if (!geometry->shape[i]) {
				break;
			}
		}
		if (i < geometry->ndim) {
			for (i=0; i<geometry->ndim; ++i) {
				geometry->stride[i] = 1;
			}
		}
		else {
			geometry->stride[geometry->ndim - 1] = 1;
			for (i=geometry->ndim - 1; i>0; --i) {
				extent = geometry->shape[i];
				if (fm__mul_int64(
					    geometry->stride[i],
					    extent,
					    &geometry->stride[i - 1])) {
					snprintf(buf,
						 sizeof (buf),
						 "default stride at "
						 "dimension index %lu of "
						 "tensor is too large",
						 (unsigned long)(i - 1));
					ERR(dag, buf);
					FM__TRACE(FM__ERRNO_SYNTAX);
					return -1;
				}
			}
		}
	}
	if (tensor_geometry(geometry, dag)) {
		FM__TRACE(0);
		return -1;
	}
	return 0;
}

int
fm__geometry_view(struct fm_geometry *geometry, const struct fm__dag *dag)
{
	int64_t product, extent, low, high, delta;
	int e;

	assert( geometry && dag );

	if ((e = apply_view(geometry, dag))) {
		FM__TRACE(0);
		return e;
	}
	for (size_t i=0; i<geometry->ndim; ++i) {
		if (!geometry->shape[i]) {
			geometry->numel = 0;
			return 0;
		}
	}
	product = 1;
	for (size_t i=0; i<geometry->ndim; ++i) {
		if (fm__mul_int64(product, geometry->shape[i], &product)) {
			ERR(dag,
			    "view element count is outside the int64 range");
			FM__TRACE(FM__ERRNO_SYNTAX);
			return -1;
		}
	}
	geometry->numel = product;
	low = high = geometry->offset;
	for (size_t i=0; i<geometry->ndim; ++i) {
		extent = geometry->shape[i] - 1;
		if (fm__mul_int64(extent, geometry->stride[i], &delta) ||
		    ((0 > delta) ? fm__add_int64(low, delta, &low) :
		     fm__add_int64(high, delta, &high))) {
			ERR(dag,
			    "view storage bounds are outside the int64 range");
			FM__TRACE(FM__ERRNO_SYNTAX);
			return -1;
		}
	}
	if ((0 > low) || (high >= geometry->size)) {
		ERR(dag, "view storage bounds exceed the base tensor");
		FM__TRACE(FM__ERRNO_SYNTAX);
		return -1;
	}
	return 0;
}

void
fm__geometry_scalar(struct fm_geometry *geometry)
{
	assert( geometry );

	memset(geometry, 0, sizeof (*geometry));
	geometry->size = 1;
	geometry->numel = 1;
}

int
fm__geometry_unary(struct fm_geometry *geometry,
		   const struct fm_geometry *operand,
		   const struct fm__dag *dag)
{
	assert( geometry && operand && dag );

	(*geometry) = (*operand);
	return contiguous_geometry(geometry, dag);
}

int
fm__geometry_reshape(struct fm_geometry *geometry_,
		     const struct fm_geometry *operand,
		     const struct fm__dag *dag)
{
	const struct fm__dag *list, *dimension;
	struct fm_geometry geometry;
	int64_t extent, numel;
	size_t inferred;
	int empty;

	assert( geometry_ && operand && dag );

	memset(&geometry, 0, sizeof (struct fm_geometry));
	inferred = FM_MAX_NDIM;
	empty = 0;
	for (list=dag->right; list; list=list->right) {
		if (FM_MAX_NDIM <= geometry.ndim) {
			ERR(dag, "reshape exceeds maximum tensor rank");
			FM__TRACE(FM__ERRNO_SYNTAX);
			return -1;
		}
		dimension = list->left;
		if ((FM__DAG_OP_EXPR_LIST != list->op) ||
		    !dimension || !fm__dag_is_int(dimension) ||
		    !dimension->u.i) {
			ERR(dag,
			    "reshape dimensions "
			    "must be evaluated "
			    "integer expressions");
			FM__TRACE(FM__ERRNO_SYNTAX);
			return -1;
		}
		if (fm__bigint_int64(dimension->u.i, 0 /* clamp */, &extent)) {
			ERR(dag,
			    "reshape dimension is outside the int64 range");
			FM__TRACE(FM__ERRNO_SYNTAX);
			return -1;
		}
		if (-1 > extent) {
			ERR(dag,
			    "reshape dimensions must be nonnegative or -1");
			FM__TRACE(FM__ERRNO_SYNTAX);
			return -1;
		}
		if (-1 == extent) {
			if (FM_MAX_NDIM != inferred) {
				ERR(dag,
				    "reshape permits only one "
				    "inferred dimension");
				FM__TRACE(FM__ERRNO_SYNTAX);
				return -1;
			}
			inferred = geometry.ndim;
		}
		if (!extent) {
			empty = 1;
		}
		geometry.shape[geometry.ndim++] = extent;
	}
	if (empty && (FM_MAX_NDIM != inferred)) {
		ERR(dag,
		    "cannot infer a reshape dimension "
		    "when another dimension is zero");
		FM__TRACE(FM__ERRNO_SYNTAX);
		return -1;
	}
	numel = empty ? 0 : 1;
	for (size_t i=0; !empty && (i<geometry.ndim); ++i) {
		if (i != inferred) {
			if (fm__mul_int64(numel, geometry.shape[i], &numel)) {
				ERR(dag,
				    "reshape element count "
				    "exceeds int64 range");
				FM__TRACE(FM__ERRNO_SYNTAX);
				return -1;
			}
		}
	}
	if (FM_MAX_NDIM != inferred) {
		if (operand->numel % numel) {
			ERR(dag,
			    "reshape inferred dimension must be an integer");
			FM__TRACE(FM__ERRNO_SYNTAX);
			return -1;
		}
		geometry.shape[inferred] = operand->numel / numel;
		numel = operand->numel;
	}
	if (numel != operand->numel) {
		ERR(dag, "reshape must preserve the element count");
		FM__TRACE(FM__ERRNO_SYNTAX);
		return -1;
	}
	if (contiguous_geometry(&geometry, dag)) {
		FM__TRACE(0);
		return -1;
	}
	if (1 == fm__geometry_flatten_stride(operand)) {
		geometry.size = operand->size;
		geometry.offset = operand->offset;
	}
	(*geometry_) = geometry;
	return 0;
}

int
fm__geometry_broadcast(struct fm_geometry *geometry_,
		       const struct fm_geometry *operand,
		       const struct fm__dag *dag)
{
	const struct fm__dag *list, *dimension;
	int64_t extent, shape[FM_MAX_NDIM];
	size_t ndim;

	assert( geometry_ && operand && dag );

	ndim = 0;
	for (list=dag->right; list; list=list->right) {
		if (FM_MAX_NDIM <= ndim) {
			ERR(dag, "broadcast exceeds maximum tensor rank");
			FM__TRACE(FM__ERRNO_SYNTAX);
			return -1;
		}
		dimension = list->left;
		if ((FM__DAG_OP_EXPR_LIST != list->op) ||
		    !dimension ||
		    !fm__dag_is_int(dimension) ||
		    !dimension->u.i) {
			ERR(dag,
			    "broadcast dimensions must be "
			    "evaluated integer expressions");
			FM__TRACE(FM__ERRNO_SYNTAX);
			return -1;
		}
		if (fm__bigint_int64(dimension->u.i, 0 /* clamp */, &extent)) {
			ERR(dag,
			    "broadcast dimension is outside the int64 range");
			FM__TRACE(FM__ERRNO_SYNTAX);
			return -1;
		}
		shape[ndim++] = extent;
	}
	return broadcast_to(geometry_, operand, shape, ndim, dag);
}

int
fm__geometry_binary(struct fm_geometry *geometry,
		    const struct fm_geometry *left,
		    const struct fm_geometry *right,
		    const struct fm__dag *dag)
{
	assert( geometry && left && right && dag );

	if (left->ndim && right->ndim &&
	    ((left->ndim != right->ndim) ||
	     !fm__utils_match_shape(left->shape, right->shape, left->ndim))) {
		ERR(dag, "tensor operands must have identical shapes");
		FM__TRACE(FM__ERRNO_SYNTAX);
		return -1;
	}
	(*geometry) = (left->ndim > right->ndim) ? (*left) : (*right);
	return contiguous_geometry(geometry, dag);
}

int
fm__geometry_matmul_plan(struct fm__matmul_plan *plan_,
			 const struct fm_geometry *left,
			 const struct fm_geometry *right,
			 const struct fm__dag *dag)
{
	size_t left_batch, right_batch, batch;
	struct fm_geometry geometry;
	struct fm__matmul_plan plan;
	int64_t shape[FM_MAX_NDIM];
	int64_t l, r;

	assert( plan_ && left && right && dag );

	if (!left->ndim ||
	    !right->ndim ||
	    (FM_MAX_NDIM < left->ndim) ||
	    (FM_MAX_NDIM < right->ndim)) {
		ERR(dag,
		    "matrix multiplication requires "
		    "nonzero operand ranks within the limit");
		FM__TRACE(FM__ERRNO_SYNTAX);
		return -1;
	}
	if (left->shape[left->ndim - 1] !=
	    right->shape[(1 == right->ndim) ? 0 : right->ndim - 2]) {
		ERR(dag,
		    "matrix multiplication requires "
		    "compatible inner dimensions");
		FM__TRACE(FM__ERRNO_SYNTAX);
		return -1;
	}
	left_batch = (2 < left->ndim) ? left->ndim - 2 : 0;
	right_batch = (2 < right->ndim) ? right->ndim - 2 : 0;
	batch = (left_batch > right_batch) ? left_batch : right_batch;
	memset(&geometry, 0, sizeof (struct fm_geometry));
	geometry.ndim = batch;
	for (size_t i=0; i<batch; ++i) {
		l = ((i < batch - left_batch)
		     ? 1
		     : left->shape[i - (batch - left_batch)]);
		r = ((i < batch - right_batch)
		     ? 1
		     : right->shape[i - (batch - right_batch)]);
		if ((l != r) && (1 != l) && (1 != r)) {
			ERR(dag,
			    "matrix multiplication requires "
			    "broadcastable batch dimensions");
			FM__TRACE(FM__ERRNO_SYNTAX);
			return -1;
		}
		geometry.shape[i] = (1 == l) ? r : l;
	}
	if (1 < left->ndim) {
		geometry.shape[geometry.ndim++] = left->shape[left->ndim - 2];
	}
	if (1 < right->ndim) {
		geometry.shape[geometry.ndim++] =
			right->shape[right->ndim - 1];
	}
	if (contiguous_geometry(&geometry, dag)) {
		FM__TRACE(0);
		return -1;
	}
	plan.output = geometry;
	plan.left = (*left);
	plan.right = (*right);
	if (1 == left->ndim) {
		plan.left.ndim = 2;
		plan.left.shape[1] = left->shape[0];
		plan.left.stride[1] = left->stride[0];
		plan.left.shape[0] = 1;
		plan.left.stride[0] = 0;
	}
	if (1 == right->ndim) {
		plan.right.ndim = 2;
		plan.right.shape[1] = 1;
		plan.right.stride[1] = 0;
	}
	for (size_t i=0; i<batch; ++i) {
		shape[i] = geometry.shape[i];
	}
	shape[batch] = plan.left.shape[plan.left.ndim - 2];
	shape[batch + 1] = plan.left.shape[plan.left.ndim - 1];
	if (broadcast_to(&plan.left, &plan.left, shape, batch + 2, dag)) {
		FM__TRACE(0);
		return -1;
	}
	shape[batch] = plan.right.shape[plan.right.ndim - 2];
	shape[batch + 1] = plan.right.shape[plan.right.ndim - 1];
	if (broadcast_to(&plan.right, &plan.right, shape, batch + 2, dag)) {
		FM__TRACE(0);
		return -1;
	}
	memset(&plan.result, 0, sizeof (struct fm_geometry));
	plan.result.ndim = batch + 2;
	for (size_t i=0; i<batch; ++i) {
		plan.result.shape[i] = geometry.shape[i];
	}
	plan.result.shape[batch] = plan.left.shape[batch];
	plan.result.shape[batch + 1] = plan.right.shape[batch + 1];
	if (contiguous_geometry(&plan.result, dag)) {
		FM__TRACE(0);
		return -1;
	}
	(*plan_) = plan;
	return 0;
}

int
fm__geometry_matmul(struct fm_geometry *geometry,
		    const struct fm_geometry *left,
		    const struct fm_geometry *right,
		    const struct fm__dag *dag)
{
	struct fm__matmul_plan plan;

	assert( geometry && left && right && dag );

	if (fm__geometry_matmul_plan(&plan, left, right, dag)) {
		FM__TRACE(0);
		return -1;
	}
	(*geometry) = plan.output;
	return 0;
}

int
fm__geometry_cond(struct fm_geometry *geometry_,
		  const struct fm_geometry *cond,
		  const struct fm_geometry *left,
		  const struct fm_geometry *right,
		  const struct fm__dag *dag)
{
	struct fm_geometry geometry;

	assert( geometry_ && cond && left && right && dag );

	if (left->ndim && right->ndim &&
	    ((left->ndim != right->ndim) ||
	     !fm__utils_match_shape(left->shape, right->shape, left->ndim))) {
		ERR(dag,
		    "conditional result tensors must have identical shapes");
		FM__TRACE(FM__ERRNO_SYNTAX);
		return -1;
	}
	geometry = (left->ndim > right->ndim) ? (*left) : (*right);
	if (!geometry.ndim && cond->ndim) {
		geometry = (*cond);
	}
	if (cond->ndim &&
	    ((cond->ndim != geometry.ndim) ||
	     !fm__utils_match_shape(cond->shape,
				    geometry.shape,
				    geometry.ndim))) {
		ERR(dag, "tensor condition must match the result shape");
		FM__TRACE(FM__ERRNO_SYNTAX);
		return -1;
	}
	if (contiguous_geometry(&geometry, dag)) {
		FM__TRACE(0);
		return -1;
	}
	(*geometry_) = geometry;
	return 0;
}

int
fm__geometry_fnc(struct fm_geometry *geometry_,
		 const struct fm_geometry * const argv[],
		 size_t argc,
		 const struct fm__dag *dag)
{
	struct fm_geometry geometry;

	assert( geometry_ && argv && dag );

	memset(&geometry, 0, sizeof (struct fm_geometry));
	if (fm__fnc_shape(&geometry, dag, argv, argc) ||
	    ((FM_EXPR_OP_VIEW != FM__FNCS[dag->index].op) &&
	     contiguous_geometry(&geometry, dag))) {
		FM__TRACE(0);
		return -1;
	}
	(*geometry_) = geometry;
	return 0;
}

int64_t
fm__geometry_flatten_stride(const struct fm_geometry *geometry)
{
	int64_t stride, extent, expected;

	assert( geometry && (FM_MAX_NDIM >= geometry->ndim) );

	if (!geometry->numel) {
		return 1;
	}
	stride = extent = 1;
	for (size_t i=geometry->ndim; i>0; --i) {
		if (1 < geometry->shape[i - 1]) {
			if (1 == extent) {
				stride = geometry->stride[i - 1];
			}
			else if (fm__mul_int64(stride, extent, &expected) ||
				 (geometry->stride[i - 1] != expected)) {
				return 0;
			}
			if (fm__mul_int64(extent,
					  geometry->shape[i - 1],
					  &extent)) {
				return 0;
			}
		}
	}
	return stride;
}
