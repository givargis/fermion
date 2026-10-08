/**
 * Copyright (c) 2025-2026 Tony Givargis
 * University of California, Irvine
 * fm.h
 */

#ifndef FM_H
#define FM_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FM_VERSION "0.1.0"

#define FM_MAX_NDIM   32
#define FM_ERRSTR_LEN 128

enum fm_node_op {
	FM_NODE_OP_UNKNOWN,
	FM_NODE_OP_TENSOR,
	FM_NODE_OP_COMPUTE
};

enum fm_dtype {
	FM_DTYPE_UNKNOWN,
	FM_DTYPE_BOOL,
	FM_DTYPE_INT8,
	FM_DTYPE_UINT8,
	FM_DTYPE_INT16,
	FM_DTYPE_UINT16,
	FM_DTYPE_INT32,
	FM_DTYPE_UINT32,
	FM_DTYPE_INT64,
	FM_DTYPE_UINT64,
	FM_DTYPE_FP8_E4M3,
	FM_DTYPE_FP8_E5M2,
	FM_DTYPE_BF16,
	FM_DTYPE_FP16,
	FM_DTYPE_FP32,
	FM_DTYPE_FP64
};

enum fm_init_op {
	FM_INIT_OP_UNKNOWN,
	FM_INIT_OP_ZEROS,
	FM_INIT_OP_ONES,
	FM_INIT_OP_UNIFORM,
	FM_INIT_OP_NORMAL,
	FM_INIT_OP_IOTA
};

enum fm_fold_op {
	FM_FOLD_OP_UNKNOWN,
	FM_FOLD_OP_MIN,
	FM_FOLD_OP_MAX,
	FM_FOLD_OP_SUM,
	FM_FOLD_OP_COUNT,
	FM_FOLD_OP_PRODUCT,
	FM_FOLD_OP_ARGMIN,
	FM_FOLD_OP_ARGMAX,
	FM_FOLD_OP_ANY,
	FM_FOLD_OP_ALL
};

enum fm_expr_op {
	FM_EXPR_OP_UNKNOWN,

	FM_EXPR_OP_SCALAR,      /* u.fp64 or u.int64 */
	FM_EXPR_OP_TENSOR,      /* u.node */
	FM_EXPR_OP_VIEW,        /* view(L, geometry) */
	FM_EXPR_OP_CAST,        /* (dtype) R */
	FM_EXPR_OP_COPY,        /* copy(R) */

	FM_EXPR_OP_COND,        /* u.expr ? L : R */

	FM_EXPR_OP_POS,         /* +R */
	FM_EXPR_OP_NEG,         /* -R */
	FM_EXPR_OP_ADD,         /* L + R */
	FM_EXPR_OP_SUB,         /* L - R */
	FM_EXPR_OP_MUL,         /* L * R */
	FM_EXPR_OP_DIV,         /* L / R */
	FM_EXPR_OP_MOD,         /* L % R */

	FM_EXPR_OP_ABS,         /* abs(R) */
	FM_EXPR_OP_SIGNBIT,     /* signbit(R) */
	FM_EXPR_OP_COPYSIGN,    /* copysign(L, R): magnitude, sign */

	FM_EXPR_OP_MIN,         /* min(L, R) */
	FM_EXPR_OP_MAX,         /* max(L, R) */
	FM_EXPR_OP_CLAMP,       /* L=value, R=lower, u.expr=upper */

	FM_EXPR_OP_CEIL,        /* ceil(R) */
	FM_EXPR_OP_FLOOR,       /* floor(R) */
	FM_EXPR_OP_ROUND,       /* round(R) */
	FM_EXPR_OP_TRUNC,       /* trunc(R) */

	FM_EXPR_OP_ISINF,       /* isinf(R) */
	FM_EXPR_OP_ISNAN,       /* isnan(R) */
	FM_EXPR_OP_ISNORMAL,    /* isnormal(R) */
	FM_EXPR_OP_ISFINITE,    /* isfinite(R) */

	FM_EXPR_OP_LT,          /* L < R */
	FM_EXPR_OP_GT,          /* L > R */
	FM_EXPR_OP_LE,          /* L <= R */
	FM_EXPR_OP_GE,          /* L >= R */
	FM_EXPR_OP_EQ,          /* L == R */
	FM_EXPR_OP_NE,          /* L != R */

	FM_EXPR_OP_NOT,         /* ~R */
	FM_EXPR_OP_SHL,         /* L << R */
	FM_EXPR_OP_SHR,         /* L >> R */
	FM_EXPR_OP_AND,         /* L & R */
	FM_EXPR_OP_XOR,         /* L ^ R */
	FM_EXPR_OP_OR,          /* L | R */

	FM_EXPR_OP_LOGNOT,      /* !R */
	FM_EXPR_OP_LOGAND,      /* L && R */
	FM_EXPR_OP_LOGXOR,      /* L ^^ R */
	FM_EXPR_OP_LOGOR,       /* L || R */

	FM_EXPR_OP_EXP,         /* exp(R) */
	FM_EXPR_OP_EXP2,        /* exp2(R) */
	FM_EXPR_OP_LOG,         /* log(R) */
	FM_EXPR_OP_LOG1P,       /* log1p(R) */
	FM_EXPR_OP_SQRT,        /* sqrt(R) */
	FM_EXPR_OP_POW,         /* pow(L, R): base, exponent */

	FM_EXPR_OP_SIN,         /* sin(R) */
	FM_EXPR_OP_COS,         /* cos(R) */
	FM_EXPR_OP_TAN,         /* tan(R) */
	FM_EXPR_OP_SEC,         /* sec(R) */
	FM_EXPR_OP_COT,         /* cot(R) */
	FM_EXPR_OP_ASIN,        /* asin(R) */
	FM_EXPR_OP_ACOS,        /* acos(R) */
	FM_EXPR_OP_ATAN,        /* atan(R) */
	FM_EXPR_OP_ATAN2,       /* atan2(L, R): y, x */

	FM_EXPR_OP_ERF,         /* erf(R) */
	FM_EXPR_OP_ERFC,        /* erfc(R) */
	FM_EXPR_OP_LGAMMA,      /* lgamma(R) */
	FM_EXPR_OP_TGAMMA,      /* tgamma(R) */

	FM_EXPR_OP_NORMAL_CDF,  /* normal_cdf(R) */
	FM_EXPR_OP_NORMAL_SF,   /* normal_sf(R) */

	FM_EXPR_OP_RELU,        /* relu(R) */
	FM_EXPR_OP_GELU,        /* gelu(R) */
	FM_EXPR_OP_SIGMOID,     /* sigmoid(R) */
	FM_EXPR_OP_SILU,        /* silu(R) */
	FM_EXPR_OP_SOFTPLUS,    /* softplus(R) */

	FM_EXPR_OP_MATMUL,      /* L @ R */

	FM_EXPR_OP_CONCAT,      /* concat(L, R, u.index.axis) */
	FM_EXPR_OP_GATHER,      /* L=input, R=indices; u.index.axis */
	FM_EXPR_OP_SCATTER,     /* L=input, R=indices; u.index */

	FM_EXPR_OP_FOLD,        /* R=input, L=optional mask; u.fold */
	FM_EXPR_OP_SCAN,        /* scan(R, u.fold.op, u.fold.axis) */

	FM_EXPR_OP_ARGSORT,     /* argsort(R, u.sort) */
	FM_EXPR_OP_TOPK,        /* topk(R, u.sort) */

	FM_EXPR_OP_END
};

struct fm_geometry {
	size_t ndim;
	int64_t size;
	int64_t numel;
	int64_t offset;
	int64_t shape[FM_MAX_NDIM];
	int64_t stride[FM_MAX_NDIM];
};

struct fm_fold {
	int64_t axis;
	enum fm_fold_op op;
};

struct fm_sort {
	int64_t axis;
	int64_t k;
	int sorted;     /* bool */
	int descending; /* bool */
};

struct fm_index {
	int64_t axis;
	struct fm_expr *source; /* scatter */
};

struct fm_expr {
	size_t id;
	size_t refs;
	size_t lineno;
	size_t column;
	enum fm_expr_op op;
	enum fm_dtype dtype;
	struct fm_expr *left;
	struct fm_expr *right;
	struct fm_geometry geometry;
	union {
		double fp64;
		int64_t int64;
		struct fm_fold fold;
		struct fm_sort sort;
		struct fm_index index;
		struct fm_expr *expr;
		const struct fm_node *node;
	} u;
};

struct fm_node {
	const char *name;
	enum fm_node_op op;
	enum fm_dtype dtype;
	struct fm_geometry geometry;
	struct {
		enum fm_init_op op;
		double low;
		double high;
		int64_t start;
		int64_t step;
		uint64_t seed; /* 0 -> nondeterministic */
	} init;
	struct fm_expr *expr;
	size_t max_expr_id;
	struct fm_node *link;
};

typedef struct fm *fm_t;

fm_t fm_open(const char *program, char errstr[FM_ERRSTR_LEN]);

void fm_close(fm_t fm);

int fm_json(fm_t fm, const char *pathname);

const struct fm_node *fm_head(fm_t fm);

const struct fm_node *fm_lookup(fm_t fm, const char *name);

#ifdef __cplusplus
}
#endif

#endif
