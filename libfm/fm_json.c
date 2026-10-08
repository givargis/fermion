/**
 * Copyright (c) 2025-2026 Tony Givargis
 * University of California, Irvine
 * fm_json.c
 */

#include "fm_core.h"
#include "fm_bitset.h"
#include "fm_json.h"

#define PU(k, v, e)					\
	( 0 > fprintf(file,				\
		      "%s\"%s\": %" PRIuMAX "%s",	\
		      space(level),			\
		      (k),				\
		      (uintmax_t)(v),			\
		      (e)) )

#define PI(k, v, e)					\
	( 0 > fprintf(file,				\
		      "%s\"%s\": %" PRIdMAX "%s",	\
		      space(level),			\
		      (k),				\
		      (intmax_t)(v),			\
		      (e)) )

#define PO(k, v, e)				\
	( 0 > fprintf(file,			\
		      "%s\"%s\": %s%s",		\
		      space(level),		\
		      (k),			\
		      (v),			\
		      (e)) )

#define PS(k, v, e)				\
	( 0 > fprintf(file,			\
		      "%s\"%s\": \"%s\"%s",	\
		      space(level),		\
		      (k),			\
		      (v),			\
		      (e)) )

#define PD(k, v, e)				\
	( 0 > fprintf(file,			\
		      0.0 == (v)		\
		      ? "%s\"%s\": %.1f%s"	\
		      : "%s\"%s\": %.17g%s",	\
		      space(level),		\
		      (k),			\
		      (v),			\
		      (e)) )

static const char * const NODE_OP_STR[] = {
	"NODE_OP_UNKNOWN",
	"NODE_OP_TENSOR",
	"NODE_OP_COMPUTE"
};

static const char * const DTYPE_STR[] = {
	"DTYPE_UNKNOWN",
	"DTYPE_BOOL",
	"DTYPE_INT8",
	"DTYPE_UINT8",
	"DTYPE_INT16",
	"DTYPE_UINT16",
	"DTYPE_INT32",
	"DTYPE_UINT32",
	"DTYPE_INT64",
	"DTYPE_UINT64",
	"DTYPE_FP8_E4M3",
	"DTYPE_FP8_E5M2",
	"DTYPE_BF16",
	"DTYPE_FP16",
	"DTYPE_FP32",
	"DTYPE_FP64"
};

static const char * const INIT_OP_STR[] = {
	"INIT_OP_UNKNOWN",
	"INIT_OP_ZEROS",
	"INIT_OP_ONES",
	"INIT_OP_UNIFORM",
	"INIT_OP_NORMAL",
	"INIT_OP_IOTA"
};

static const char * const FOLD_OP_STR[] = {
	"FOLD_UNKNOWN",
	"FOLD_MIN",
	"FOLD_MAX",
	"FOLD_SUM",
	"FOLD_COUNT",
	"FOLD_PRODUCT",
	"FOLD_ARGMIN",
	"FOLD_ARGMAX",
	"FOLD_ANY",
	"FOLD_ALL"
};

static const char * const EXPR_OP_STR[] = {
	"EXPR_OP_UNKNOWN",
	"EXPR_OP_SCALAR",
	"EXPR_OP_TENSOR",
	"EXPR_OP_VIEW",
	"EXPR_OP_CAST",
	"EXPR_OP_COPY",
	"EXPR_OP_COND",
	"EXPR_OP_POS",
	"EXPR_OP_NEG",
	"EXPR_OP_ADD",
	"EXPR_OP_SUB",
	"EXPR_OP_MUL",
	"EXPR_OP_DIV",
	"EXPR_OP_MOD",
	"EXPR_OP_ABS",
	"EXPR_OP_SIGNBIT",
	"EXPR_OP_COPYSIGN",
	"EXPR_OP_MIN",
	"EXPR_OP_MAX",
	"EXPR_OP_CLAMP",
	"EXPR_OP_CEIL",
	"EXPR_OP_FLOOR",
	"EXPR_OP_ROUND",
	"EXPR_OP_TRUNC",
	"EXPR_OP_ISINF",
	"EXPR_OP_ISNAN",
	"EXPR_OP_ISNORMAL",
	"EXPR_OP_ISFINITE",
	"EXPR_OP_LT",
	"EXPR_OP_GT",
	"EXPR_OP_LE",
	"EXPR_OP_GE",
	"EXPR_OP_EQ",
	"EXPR_OP_NE",
	"EXPR_OP_NOT",
	"EXPR_OP_SHL",
	"EXPR_OP_SHR",
	"EXPR_OP_AND",
	"EXPR_OP_XOR",
	"EXPR_OP_OR",
	"EXPR_OP_LOGNOT",
	"EXPR_OP_LOGAND",
	"EXPR_OP_LOGXOR",
	"EXPR_OP_LOGOR",
	"EXPR_OP_EXP",
	"EXPR_OP_EXP2",
	"EXPR_OP_LOG",
	"EXPR_OP_LOG1P",
	"EXPR_OP_SQRT",
	"EXPR_OP_POW",
	"EXPR_OP_SIN",
	"EXPR_OP_COS",
	"EXPR_OP_TAN",
	"EXPR_OP_SEC",
	"EXPR_OP_COT",
	"EXPR_OP_ASIN",
	"EXPR_OP_ACOS",
	"EXPR_OP_ATAN",
	"EXPR_OP_ATAN2",
	"EXPR_OP_ERF",
	"EXPR_OP_ERFC",
	"EXPR_OP_LGAMMA",
	"EXPR_OP_TGAMMA",
	"EXPR_OP_NORMAL_CDF",
	"EXPR_OP_NORMAL_SF",
	"EXPR_OP_RELU",
	"EXPR_OP_GELU",
	"EXPR_OP_SIGMOID",
	"EXPR_OP_SILU",
	"EXPR_OP_SOFTPLUS",
	"EXPR_OP_MATMUL",
	"EXPR_OP_CONCAT",
	"EXPR_OP_GATHER",
	"EXPR_OP_SCATTER",
	"EXPR_OP_FOLD",
	"EXPR_OP_SCAN",
	"EXPR_OP_ARGSORT",
	"EXPR_OP_TOPK",
	"EXPR_OP_END"
};

static const char *
space(int level)
{
	static const char T[] = "                                            ";
	size_t n;

	if ((n = sizeof (T) - 1) < (size_t)(level * 2)) {
		level = (int)(n / 2);
	}
	return &T[n - level * 2];
}

static int
init_json(const struct fm_node *node, FILE *file, int level)
{
	if (!node->init.op) {
		if (PO("init", "null", "")) {
			FM__TRACE(0);
			return -1;
		}
		return 0;
	}
	if (PO("init", "{", "\n")) {
		FM__TRACE(0);
		return -1;
	}
	++level;
	if (PS("type", INIT_OP_STR[node->init.op], ",\n") ||
	    PD("low", node->init.low, ",\n") ||
	    PD("high", node->init.high, ",\n") ||
	    PI("start", node->init.start, ",\n") ||
	    PI("step", node->init.step, ",\n") ||
	    PU("seed", node->init.seed, "\n")) {
		FM__TRACE(0);
		return -1;
	}
	--level;
	if (0 > fprintf(file, "%s}", space(level))) {
		FM__TRACE(0);
		return -1;
	}
	return 0;
}

static int
geometry_json(const struct fm_geometry *geometry, FILE *file, int level)
{
	if (PO("geometry", "{", "\n")) {
		FM__TRACE(0);
		return -1;
	}
	++level;
	if (PU("ndim", geometry->ndim, ",\n") ||
	    PI("size", geometry->size, ",\n") ||
	    PI("numel", geometry->numel, ",\n") ||
	    PI("offset", geometry->offset, ",\n") ||
	    PO("shape", "[", "")) {
		FM__TRACE(0);
		return -1;
	}
	for (size_t i=0; i<geometry->ndim; ++i) {
		if (0 > fprintf(file,
				"%" PRIdMAX "%s",
				(intmax_t)geometry->shape[i],
				(i + 1) == geometry->ndim ? "" : ", ")) {
			FM__TRACE(0);
			return -1;
		}
	}
	if ((0 > fprintf(file, "],\n")) || PO("stride", "[", "")) {
		FM__TRACE(0);
		return -1;
	}
	for (size_t i=0; i<geometry->ndim; ++i) {
		if (0 > fprintf(file,
				"%" PRIdMAX "%s",
				(intmax_t)geometry->stride[i],
				(i + 1) == geometry->ndim ? "" : ", ")) {
			FM__TRACE(0);
			return -1;
		}
	}
	--level;
	if ((0 > fprintf(file, "]\n")) ||
	    (0 > fprintf(file, "%s}", space(level)))) {
		FM__TRACE(0);
		return -1;
	}
	return 0;
}

static int
expr_json(const struct fm_expr *expr,
	  FILE *file,
	  fm__bitset_t bitset,
	  int level,
	  int *mark)
{
	assert( expr );

	if (fm__bitset_get(bitset, expr->id)) {
		return 0;
	}
	fm__bitset_set(bitset, expr->id);
	if (((FM_EXPR_OP_COND == expr->op) ||
	     (FM_EXPR_OP_CLAMP == expr->op)) &&
	    expr_json(expr->u.expr, file, bitset, level, mark)) {
		FM__TRACE(0);
		return -1;
	}
	if ((FM_EXPR_OP_SCATTER == expr->op) &&
	    expr_json(expr->u.index.source, file, bitset, level, mark)) {
		FM__TRACE(0);
		return -1;
	}
	if ((expr->left &&
	     expr_json(expr->left, file, bitset, level, mark)) ||
	    (expr->right &&
	     expr_json(expr->right, file, bitset, level, mark))) {
		FM__TRACE(0);
		return -1;
	}
	if (!(*mark)) {
		if (0 > fprintf(file, ",\n")) {
			FM__TRACE(0);
			return -1;
		}
	}
	(*mark) = 0;
	if ((0 > fprintf(file, "%s{\n", space(level++))) ||
	    PS("op", EXPR_OP_STR[expr->op], ",\n") ||
	    PU("id", expr->id, ",\n") ||
	    PU("refs", expr->refs, ",\n") ||
	    PU("lineno", expr->lineno, ",\n") ||
	    PU("column", expr->column, ",\n") ||
	    PS("dtype", DTYPE_STR[expr->dtype], ",\n") ||
	    (FM_EXPR_OP_COND == expr->op
	     ? PU("cond", expr->u.expr->id, ",\n")
	     : 0) ||
	    (FM_EXPR_OP_CLAMP == expr->op
	     ? PU("third", expr->u.expr->id, ",\n")
	     : 0) ||
	    (expr->left
	     ? PU("left", expr->left->id, ",\n")
	     : 0) ||
	    (expr->right
	     ? PU("right", expr->right->id, ",\n")
	     : 0) ||
	    geometry_json(&expr->geometry, file, level)) {
		FM__TRACE(0);
		return -1;
	}
	if (FM_EXPR_OP_SCALAR == expr->op) {
		if (FM_DTYPE_FP64 == expr->dtype) {
			if ((0 > fprintf(file, ",\n")) ||
			    PD("fp64", expr->u.fp64, "")) {
				FM__TRACE(0);
				return -1;
			}
		}
		if ((FM_DTYPE_INT64 == expr->dtype) ||
		    (FM_DTYPE_BOOL == expr->dtype)) {
			if ((0 > fprintf(file, ",\n")) ||
			    PI("int64", expr->u.int64, "")) {
				FM__TRACE(0);
				return -1;
			}
		}
	}
	if ((FM_EXPR_OP_FOLD == expr->op) || (FM_EXPR_OP_SCAN == expr->op)) {
		if ((0 > fprintf(file, ",\n")) || PO("fold", "{", "\n")) {
			FM__TRACE(0);
			return -1;
		}
		++level;
		if (PS("op", FOLD_OP_STR[expr->u.fold.op], ",\n") ||
		    PI("axis", expr->u.fold.axis, "\n") ||
		    (0 > fprintf(file, "%s}", space(--level)))) {
			FM__TRACE(0);
			return -1;
		}
	}
	if ((FM_EXPR_OP_ARGSORT == expr->op) ||
	    (FM_EXPR_OP_TOPK == expr->op)) {
		if ((0 > fprintf(file, ",\n")) || PO("sort", "{", "\n")) {
			FM__TRACE(0);
			return -1;
		}
		++level;
		if (PI("axis", expr->u.sort.axis, ",\n")) {
			FM__TRACE(0);
			return -1;
		}
		if (FM_EXPR_OP_TOPK == expr->op) {
			if (PI("k", expr->u.sort.k, ",\n") ||
			    PO("sorted",
			       expr->u.sort.sorted ? "1" : "0",
			       ",\n")) {
				FM__TRACE(0);
				return -1;
			}
		}
		if (PO("descending",
		       expr->u.sort.descending ? "1" : "0",
		       "\n") ||
		    (0 > fprintf(file, "%s}", space(--level)))) {
			FM__TRACE(0);
			return -1;
		}
	}
	if ((FM_EXPR_OP_CONCAT == expr->op) ||
	    (FM_EXPR_OP_GATHER == expr->op)) {
		if ((0 > fprintf(file, ",\n")) || PO("index", "{", "\n")) {
			FM__TRACE(0);
			return -1;
		}
		++level;
		if (PI("axis", expr->u.index.axis, ",\n") ||
		    PO("source", "null", "\n") ||
		    (0 > fprintf(file, "%s}", space(--level)))) {
			FM__TRACE(0);
			return -1;
		}
	}
	if (FM_EXPR_OP_SCATTER == expr->op) {
		if ((0 > fprintf(file, ",\n")) || PO("index", "{", "\n")) {
			FM__TRACE(0);
			return -1;
		}
		++level;
		if (PI("axis", expr->u.index.axis, ",\n") ||
		    PU("source", expr->u.index.source->id, "\n") ||
		    (0 > fprintf(file, "%s}", space(--level)))) {
			FM__TRACE(0);
			return -1;
		}
	}
	if (FM_EXPR_OP_TENSOR == expr->op) {
		if ((0 > fprintf(file, ",\n")) ||
		    PS("node", expr->u.node->name, "")) {
			FM__TRACE(0);
			return -1;
		}
	}
	--level;
	if (0 > fprintf(file, "\n%s}", space(level))) {
		FM__TRACE(0);
		return -1;
	}
	return 0;
}

int
fm__json(fm_t fm, FILE *file)
{
	const struct fm_node *node;
	fm__bitset_t bitset;
	int level;
	int mark;

	assert( fm && file );

	level = 1;
	if ((0 > fprintf(file, "{\n")) ||
	    PS("version", FM_VERSION, ",\n") ||
	    PO("nodes", "[", "\n")) {
		FM__TRACE(0);
		return -1;
	}
	++level;
	node = fm_head(fm);
	while (node) {
		if (0 > fprintf(file, "%s{\n", space(level))) {
			FM__TRACE(0);
			return -1;
		}
		++level;
		if (PS("name", node->name, ",\n") ||
		    PS("op", NODE_OP_STR[node->op], ",\n") ||
		    PS("dtype", DTYPE_STR[node->dtype], ",\n") ||
		    geometry_json(&node->geometry, file, level) ||
		    (0 > fprintf(file, ",\n")) ||
		    init_json(node, file, level) ||
		    (0 > fprintf(file, ",\n"))) {
			FM__TRACE(0);
			return -1;
		}
		if (node->expr) {
			mark = 1;
			bitset = fm__bitset_open(node->max_expr_id + 1);
			if (!bitset || PO("expr", "[", "\n")) {
				fm__bitset_close(bitset);
				FM__TRACE(0);
				return -1;
			}
			++level;
			if (expr_json(node->expr,
				      file,
				      bitset,
				      level,
				      &mark) ||
			    (0 > fprintf(file,
					 "\n%s],\n",
					 space(--level)))) {
				fm__bitset_close(bitset);
				FM__TRACE(0);
				return -1;
			}
			fm__bitset_close(bitset);
		}
		else {
			if (PO("expr", "null", ",\n")) {
				FM__TRACE(0);
				return -1;
			}
		}
		if (PU("max_expr_id", node->max_expr_id, "\n")) {
			FM__TRACE(0);
			return -1;
		}
		--level;
		if (0 > fprintf(file,
				"%s}%s\n",
				space(level),
				node->link ? "," : "")) {
			FM__TRACE(0);
			return -1;
		}
		node = node->link;
	}
	--level;
	if (0 > fprintf(file, "%s]\n}\n", space(level))) {
		FM__TRACE(0);
		return -1;
	}
	return 0;
}
