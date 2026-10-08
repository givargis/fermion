/**
 * Copyright (c) 2025-2026 Tony Givargis
 * University of California, Irvine
 * fm_dag.h
 */

#ifndef FM_DAG_H
#define FM_DAG_H

#include <assert.h>

#include "fm_bigint.h"
#include "fm.h"

#define FM__DAG_MAX_DEPTH 256

enum fm__dag_op {
	FM__DAG_OP_UNKNOWN,
	FM__DAG_OP_EXPR_LINT,         /* u.i */
	FM__DAG_OP_EXPR_LREAL,        /* u.d */
	FM__DAG_OP_EXPR_LBOOL,        /* u.b */
	FM__DAG_OP_EXPR_LSTRING,      /* u.s */
	FM__DAG_OP_EXPR_IDENTIFIER,   /* tensor:u.s | u.? based on type */
	FM__DAG_OP_EXPR_FNC,          /* index, args:R */
	FM__DAG_OP_EXPR_CAST,         /* cast<type>( R ) */
	FM__DAG_OP_EXPR_RESHAPE,      /* reshape( L, [ R ] ) */
	FM__DAG_OP_EXPR_BROADCAST,    /* broadcast( L, [ R ] ) */
	FM__DAG_OP_EXPR_SLICE,        /* slice, C:L:R */
	FM__DAG_OP_EXPR_SLICES,       /* slice:L, link:R */
	FM__DAG_OP_EXPR_VIEW,         /* L [ slices:R | permute:C ] | L */
	FM__DAG_OP_EXPR_POS,          /* + R */
	FM__DAG_OP_EXPR_NEG,          /* - R */
	FM__DAG_OP_EXPR_NOT,          /* ~ R */
	FM__DAG_OP_EXPR_LOGNOT,       /* ! R */
	FM__DAG_OP_EXPR_MUL,          /* L *  R */
	FM__DAG_OP_EXPR_MATMUL,       /* L @  R */
	FM__DAG_OP_EXPR_DIV,          /* L /  R */
	FM__DAG_OP_EXPR_MOD,          /* L %  R */
	FM__DAG_OP_EXPR_ADD,          /* L +  R */
	FM__DAG_OP_EXPR_SUB,          /* L -  R */
	FM__DAG_OP_EXPR_SHL,          /* L << R */
	FM__DAG_OP_EXPR_SHR,          /* L >> R */
	FM__DAG_OP_EXPR_LT,           /* L <  R */
	FM__DAG_OP_EXPR_GT,           /* L >  R */
	FM__DAG_OP_EXPR_LE,           /* L <= R */
	FM__DAG_OP_EXPR_GE,           /* L >= R */
	FM__DAG_OP_EXPR_EQ,           /* L == R */
	FM__DAG_OP_EXPR_NE,           /* L != R */
	FM__DAG_OP_EXPR_AND,          /* L &  R */
	FM__DAG_OP_EXPR_XOR,          /* L ^  R */
	FM__DAG_OP_EXPR_OR,           /* L |  R */
	FM__DAG_OP_EXPR_LOGAND,       /* L && R */
	FM__DAG_OP_EXPR_LOGXOR,       /* L ^^ R */
	FM__DAG_OP_EXPR_LOGOR,        /* L || R */
	FM__DAG_OP_EXPR_COND,         /* C ? L : R */
	FM__DAG_OP_EXPR_LIST,         /* expr:L, link:R */
	FM__DAG_OP_DECL_GEOMETRY,     /* shape:R, stride:L */
	FM__DAG_OP_DECL_INIT_ZEROS,   /* zeros () */
	FM__DAG_OP_DECL_INIT_ONES,    /* ones () */
	FM__DAG_OP_DECL_INIT_UNIFORM, /* uniform ( R ), seed:u.u */
	FM__DAG_OP_DECL_INIT_NORMAL,  /* normal ( R ), seed:u.u */
	FM__DAG_OP_DECL_INIT_IOTA,    /* iota ( R ) */
	FM__DAG_OP_DECL_LET,          /* dag:R */
	FM__DAG_OP_DECL_TENSOR,       /* name:u.s, dtype:type, geom:L, ini:R */
	FM__DAG_OP_DECL_COMPUTE,      /* name:u.s, dtype:type, dag:R */
	FM__DAG_OP_DECL_LIST          /* decl:L, link:R */
};

enum fm__dag_type {
	FM__DAG_TYPE_UNKNOWN,
	FM__DAG_TYPE_INT,
	FM__DAG_TYPE_REAL,
	FM__DAG_TYPE_BOOL,
	FM__DAG_TYPE_STRING,
	FM__DAG_TYPE_TBOOL,    /* bool */
	FM__DAG_TYPE_INT8,     /* byte */
	FM__DAG_TYPE_UINT8,    /* unsigned byte */
	FM__DAG_TYPE_INT16,    /* short */
	FM__DAG_TYPE_UINT16,   /* unsigned short */
	FM__DAG_TYPE_INT32,    /* int */
	FM__DAG_TYPE_UINT32,   /* unsigned int */
	FM__DAG_TYPE_INT64,    /* long */
	FM__DAG_TYPE_UINT64,   /* unsigned long */
	FM__DAG_TYPE_FP8_E4M3, /* fp8_e4m3 */
	FM__DAG_TYPE_FP8_E5M2, /* fp8_e5m2 */
	FM__DAG_TYPE_BF16,     /* bf16 */
	FM__DAG_TYPE_FP16,     /* fp16 */
	FM__DAG_TYPE_FP32,     /* float */
	FM__DAG_TYPE_FP64      /* double */
};

enum fm__dag_slice {
	FM__DAG_SLICE_SCALAR,
	FM__DAG_SLICE_RANGE,
	FM__DAG_SLICE_STEPPED_RANGE
};

struct fm__dag {
	int *stop; /* from parser */
	char *errstr; /* from parser */
	int mark;
	size_t id;
	size_t index;
	size_t lineno;
	size_t column;
	enum fm__dag_op op;
	enum fm__dag_type type;
	enum fm__dag_slice slice;
	struct fm_geometry geometry;
	struct fm__dag *cond;
	struct fm__dag *left;
	struct fm__dag *right;
	union {
		int b; /* bool */
		double d;
		uint64_t u;
		const char *s;
		fm__bigint_t i;
	} u;
};

typedef struct fm__dag_pool *fm__dag_pool_t;

void fm__dag_init(void);

fm__dag_pool_t fm__dag_pool_open(int *stop, char *errstr);

void fm__dag_pool_close(fm__dag_pool_t pool);

struct fm__dag *fm__dag_pool_allocate(fm__dag_pool_t pool);

enum fm__dag_type fm__dag_type(const struct fm__dag *dag);

int fm__dag_eval_expr(struct fm__dag *dag);

static inline int /* bool */
fm__dag_is_int(const struct fm__dag *dag)
{
	assert( dag );

	return FM__DAG_TYPE_INT == dag->type;
}

static inline int /* bool */
fm__dag_is_real(const struct fm__dag *dag)
{
	assert( dag );

	return FM__DAG_TYPE_REAL == dag->type;
}

static inline int /* bool */
fm__dag_is_bool(const struct fm__dag *dag)
{
	assert( dag );

	return FM__DAG_TYPE_BOOL == dag->type;
}

static inline int /* bool */
fm__dag_is_string(const struct fm__dag *dag)
{
	assert( dag );

	return FM__DAG_TYPE_STRING == dag->type;
}

static inline int /* bool */
fm__dag_is_tensor(const struct fm__dag *dag)
{
	assert( dag );

	switch (dag->type) {
	case FM__DAG_TYPE_TBOOL   : return 1;
	case FM__DAG_TYPE_INT8    : return 1;
	case FM__DAG_TYPE_UINT8   : return 1;
	case FM__DAG_TYPE_INT16   : return 1;
	case FM__DAG_TYPE_UINT16  : return 1;
	case FM__DAG_TYPE_INT32   : return 1;
	case FM__DAG_TYPE_UINT32  : return 1;
	case FM__DAG_TYPE_INT64   : return 1;
	case FM__DAG_TYPE_UINT64  : return 1;
	case FM__DAG_TYPE_FP8_E4M3: return 1;
	case FM__DAG_TYPE_FP8_E5M2: return 1;
	case FM__DAG_TYPE_BF16    : return 1;
	case FM__DAG_TYPE_FP16    : return 1;
	case FM__DAG_TYPE_FP32    : return 1;
	case FM__DAG_TYPE_FP64    : return 1;
	default: break;
	}
	return 0;
}

static inline int /* bool */
fm__dag_is_int_tensor(const struct fm__dag *dag)
{
	assert( dag );

	switch (dag->type) {
	case FM__DAG_TYPE_INT8 : return 1;
	case FM__DAG_TYPE_INT16: return 1;
	case FM__DAG_TYPE_INT32: return 1;
	case FM__DAG_TYPE_INT64: return 1;
	default: break;
	}
	return 0;
}

static inline int /* bool */
fm__dag_is_uint_tensor(const struct fm__dag *dag)
{
	assert( dag );

	switch (dag->type) {
	case FM__DAG_TYPE_UINT8 : return 1;
	case FM__DAG_TYPE_UINT16: return 1;
	case FM__DAG_TYPE_UINT32: return 1;
	case FM__DAG_TYPE_UINT64: return 1;
	default: break;
	}
	return 0;
}

static inline int /* bool */
fm__dag_is_fp_tensor(const struct fm__dag *dag)
{
	assert( dag );

	switch (dag->type) {
	case FM__DAG_TYPE_FP8_E4M3: return 1;
	case FM__DAG_TYPE_FP8_E5M2: return 1;
	case FM__DAG_TYPE_BF16    : return 1;
	case FM__DAG_TYPE_FP16    : return 1;
	case FM__DAG_TYPE_FP32    : return 1;
	case FM__DAG_TYPE_FP64    : return 1;
	default: break;
	}
	return 0;
}

static inline int /* bool */
fm__dag_is_numeric_tensor(const struct fm__dag *dag)
{
	assert( dag );

	return (fm__dag_is_int_tensor(dag) ||
		fm__dag_is_uint_tensor(dag) ||
		fm__dag_is_fp_tensor(dag));
}

#endif
