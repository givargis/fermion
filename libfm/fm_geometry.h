/**
 * Copyright (c) 2025-2026 Tony Givargis
 * University of California, Irvine
 * fm_geometry.h
 */

#ifndef FM_GEOMETRY_H
#define FM_GEOMETRY_H

#include "fm_dag.h"

struct fm__matmul_plan {
	struct fm_geometry left;
	struct fm_geometry right;
	struct fm_geometry result;
	struct fm_geometry output;
};

void fm__geometry_init(void);

int fm__geometry_create(struct fm_geometry *geometry,
			const struct fm__dag *dag);

int fm__geometry_view(struct fm_geometry *geometry, const struct fm__dag *dag);

void fm__geometry_scalar(struct fm_geometry *geometry);

int fm__geometry_unary(struct fm_geometry *geometry,
		       const struct fm_geometry *operand,
		       const struct fm__dag *dag);

int fm__geometry_reshape(struct fm_geometry *geometry,
			 const struct fm_geometry *operand,
			 const struct fm__dag *dag);

int fm__geometry_broadcast(struct fm_geometry *geometry,
			   const struct fm_geometry *operand,
			   const struct fm__dag *dag);

int fm__geometry_binary(struct fm_geometry *geometry,
			const struct fm_geometry *left,
			const struct fm_geometry *right,
			const struct fm__dag *dag);

int fm__geometry_matmul_plan(struct fm__matmul_plan *plan,
			     const struct fm_geometry *left,
			     const struct fm_geometry *right,
			     const struct fm__dag *dag);

int fm__geometry_matmul(struct fm_geometry *geometry,
			const struct fm_geometry *left,
			const struct fm_geometry *right,
			const struct fm__dag *dag);

int fm__geometry_cond(struct fm_geometry *geometry,
		      const struct fm_geometry *cond,
		      const struct fm_geometry *left,
		      const struct fm_geometry *right,
		      const struct fm__dag *dag);

int fm__geometry_fnc(struct fm_geometry *geometry,
		     const struct fm_geometry * const argv[],
		     size_t argc,
		     const struct fm__dag *dag);

int64_t /* stride or 0 if a copy is required */
fm__geometry_flatten_stride(const struct fm_geometry *geometry);

static inline int /* bool */
fm__geometry_is_populated(const struct fm_geometry *geometry)
{
	assert( geometry );

	return geometry->ndim || geometry->numel;
}

#endif
