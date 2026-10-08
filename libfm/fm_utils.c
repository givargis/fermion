/**
 * Copyright (c) 2025-2026 Tony Givargis
 * University of California, Irvine
 * fm_utils.c
 */

#include "fm_utils.h"

void
fm__utils_err(int *stop,
	      char errstr[FM_ERRSTR_LEN],
	      size_t lineno,
	      size_t column,
	      const char *message)
{
	if (!(*stop)) {
		snprintf(errstr,
			 FM_ERRSTR_LEN,
			 "%zu:%zu: %s",
			 lineno,
			 column,
			 message);
	}
	(*stop) = 1;
}

int
fm__utils_match_shape(const int64_t *a, const int64_t *b, size_t ndim)
{
	assert( a && b && (FM_MAX_NDIM >= ndim) );

	return 0 == memcmp(a, b, ndim * sizeof (a[0]));
}

size_t
fm__utils_list_count(const struct fm__dag *dag)
{
	size_t n;

	n = 0;
	while (dag && (FM__DAG_OP_EXPR_LIST == dag->op)) {
		dag = dag->right;
		++n;
	}
	return n;
}

enum fm_expr_op
fm__utils_op2op(enum fm__dag_op op)
{
	switch (op) {
	case FM__DAG_OP_EXPR_LINT      : return FM_EXPR_OP_SCALAR;
	case FM__DAG_OP_EXPR_LREAL     : return FM_EXPR_OP_SCALAR;
	case FM__DAG_OP_EXPR_LBOOL     : return FM_EXPR_OP_SCALAR;
	case FM__DAG_OP_EXPR_IDENTIFIER: return FM_EXPR_OP_TENSOR;
	case FM__DAG_OP_EXPR_BROADCAST : return FM_EXPR_OP_VIEW;
	case FM__DAG_OP_EXPR_VIEW      : return FM_EXPR_OP_VIEW;
	case FM__DAG_OP_EXPR_CAST      : return FM_EXPR_OP_CAST;
	case FM__DAG_OP_EXPR_POS       : return FM_EXPR_OP_POS;
	case FM__DAG_OP_EXPR_NEG       : return FM_EXPR_OP_NEG;
	case FM__DAG_OP_EXPR_NOT       : return FM_EXPR_OP_NOT;
	case FM__DAG_OP_EXPR_LOGNOT    : return FM_EXPR_OP_LOGNOT;
	case FM__DAG_OP_EXPR_MUL       : return FM_EXPR_OP_MUL;
	case FM__DAG_OP_EXPR_MATMUL    : return FM_EXPR_OP_MATMUL;
	case FM__DAG_OP_EXPR_MOD       : return FM_EXPR_OP_MOD;
	case FM__DAG_OP_EXPR_DIV       : return FM_EXPR_OP_DIV;
	case FM__DAG_OP_EXPR_ADD       : return FM_EXPR_OP_ADD;
	case FM__DAG_OP_EXPR_SUB       : return FM_EXPR_OP_SUB;
	case FM__DAG_OP_EXPR_SHL       : return FM_EXPR_OP_SHL;
	case FM__DAG_OP_EXPR_SHR       : return FM_EXPR_OP_SHR;
	case FM__DAG_OP_EXPR_LT        : return FM_EXPR_OP_LT;
	case FM__DAG_OP_EXPR_GT        : return FM_EXPR_OP_GT;
	case FM__DAG_OP_EXPR_LE        : return FM_EXPR_OP_LE;
	case FM__DAG_OP_EXPR_GE        : return FM_EXPR_OP_GE;
	case FM__DAG_OP_EXPR_EQ        : return FM_EXPR_OP_EQ;
	case FM__DAG_OP_EXPR_NE        : return FM_EXPR_OP_NE;
	case FM__DAG_OP_EXPR_AND       : return FM_EXPR_OP_AND;
	case FM__DAG_OP_EXPR_XOR       : return FM_EXPR_OP_XOR;
	case FM__DAG_OP_EXPR_OR        : return FM_EXPR_OP_OR;
	case FM__DAG_OP_EXPR_LOGAND    : return FM_EXPR_OP_LOGAND;
	case FM__DAG_OP_EXPR_LOGXOR    : return FM_EXPR_OP_LOGXOR;
	case FM__DAG_OP_EXPR_LOGOR     : return FM_EXPR_OP_LOGOR;
	case FM__DAG_OP_EXPR_COND      : return FM_EXPR_OP_COND;
	default: break;
	}
	return FM_EXPR_OP_UNKNOWN;
}

enum fm_dtype
fm__utils_type2dtype(enum fm__dag_type type)
{
	switch (type) {
	case FM__DAG_TYPE_INT     : return FM_DTYPE_INT64;
	case FM__DAG_TYPE_REAL    : return FM_DTYPE_FP64;
	case FM__DAG_TYPE_BOOL    : return FM_DTYPE_BOOL;
	case FM__DAG_TYPE_TBOOL   : return FM_DTYPE_BOOL;
	case FM__DAG_TYPE_INT8    : return FM_DTYPE_INT8;
	case FM__DAG_TYPE_UINT8   : return FM_DTYPE_UINT8;
	case FM__DAG_TYPE_INT16   : return FM_DTYPE_INT16;
	case FM__DAG_TYPE_UINT16  : return FM_DTYPE_UINT16;
	case FM__DAG_TYPE_INT32   : return FM_DTYPE_INT32;
	case FM__DAG_TYPE_UINT32  : return FM_DTYPE_UINT32;
	case FM__DAG_TYPE_INT64   : return FM_DTYPE_INT64;
	case FM__DAG_TYPE_UINT64  : return FM_DTYPE_UINT64;
	case FM__DAG_TYPE_FP8_E4M3: return FM_DTYPE_FP8_E4M3;
	case FM__DAG_TYPE_FP8_E5M2: return FM_DTYPE_FP8_E5M2;
	case FM__DAG_TYPE_BF16    : return FM_DTYPE_BF16;
	case FM__DAG_TYPE_FP16    : return FM_DTYPE_FP16;
	case FM__DAG_TYPE_FP32    : return FM_DTYPE_FP32;
	case FM__DAG_TYPE_FP64    : return FM_DTYPE_FP64;
	default: break;
	}
	return FM_DTYPE_UNKNOWN;
}
