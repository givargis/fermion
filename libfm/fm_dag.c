/**
 * Copyright (c) 2025-2026 Tony Givargis
 * University of California, Irvine
 * fm_dag.c
 */

#include "fm_fnc.h"
#include "fm_utils.h"
#include "fm_dag.h"

#define DAGS_PER_CHUNK 128

#define ERR FM__UTILS_ERR_DAG

struct fm__dag_pool {
	int *stop;
	char *errstr;
	size_t id;
	size_t size;
	struct fm__dag_pool_chunk {
		struct fm__dag *dags;
		struct fm__dag_pool_chunk *link;
	} *chunks;
};

static int
real(const struct fm__dag *dag, double *z_, int *stop)
{
	double z;

	if (FM__DAG_TYPE_REAL == dag->type) {
		z = dag->u.d;
	}
	else {
		z = fm__bigint_double(dag->u.i);
	}
	if (isnan(z) || isinf(z)) {
		(*stop) = 1;
		ERR(dag, "nonfinite value");
		FM__TRACE(FM__ERRNO_SYNTAX);
		return -1;
	}
	(*z_) = z;
	return 0;
}

static int
eval_expr(struct fm__dag *dag, size_t depth, int *stop)
{
	double l, r;

	if (!dag) {
		FM__TRACE(FM__ERRNO_SOFTWARE);
		return -1;
	}
	if (dag->mark) {
		return 0;
	}
	if (FM__DAG_MAX_DEPTH <= depth) {
		(*stop) = 1;
		ERR(dag, "maximum evaluation depth exceeded");
		FM__TRACE(FM__ERRNO_SYNTAX);
		return -1;
	}
	++depth;
	if ((FM__DAG_OP_EXPR_LINT == dag->op) ||
	    (FM__DAG_OP_EXPR_LREAL == dag->op) ||
	    (FM__DAG_OP_EXPR_LBOOL == dag->op) ||
	    (FM__DAG_OP_EXPR_LSTRING == dag->op) ||
	    (FM__DAG_OP_EXPR_IDENTIFIER == dag->op)) {
		if (fm__dag_is_int(dag) ||
		    fm__dag_is_real(dag) ||
		    fm__dag_is_bool(dag) ||
		    fm__dag_is_string(dag)) {
			dag->mark = 1;
			return 0;
		}
	}
	else if (FM__DAG_OP_EXPR_FNC == dag->op) {
		/* runtime only */
	}
	else if (FM__DAG_OP_EXPR_CAST == dag->op) {
		/* runtime only */
	}
	else if (FM__DAG_OP_EXPR_RESHAPE == dag->op) {
		/* runtime only */
	}
	else if (FM__DAG_OP_EXPR_BROADCAST == dag->op) {
		/* runtime only */
	}
	else if (FM__DAG_OP_EXPR_SLICE == dag->op) {
		/* runtime only */
	}
	else if (FM__DAG_OP_EXPR_SLICES == dag->op) {
		/* runtime only */
	}
	else if (FM__DAG_OP_EXPR_VIEW == dag->op) {
		/* runtime only */
	}
	else if (FM__DAG_OP_EXPR_POS == dag->op) {
		if (eval_expr(dag->right, depth, stop)) {
			FM__TRACE(0);
			return -1;
		}
		if (fm__dag_is_int(dag) && fm__dag_is_int(dag->right)) {
			if (!(dag->u.i = fm__bigint_clone(dag->right->u.i))) {
				FM__TRACE(0);
				return -1;
			}
			dag->mark = 1;
			return 0;
		}
		if (fm__dag_is_real(dag) && fm__dag_is_real(dag->right)) {
			dag->u.d = dag->right->u.d;
			dag->mark = 1;
			return 0;
		}
	}
	else if (FM__DAG_OP_EXPR_NEG == dag->op) {
		if (eval_expr(dag->right, depth, stop)) {
			FM__TRACE(0);
			return -1;
		}
		if (fm__dag_is_int(dag) && fm__dag_is_int(dag->right)) {
			if (!(dag->u.i = fm__bigint_neg(dag->right->u.i))) {
				FM__TRACE(0);
				return -1;
			}
			dag->mark = 1;
			return 0;
		}
		if (fm__dag_is_real(dag) && fm__dag_is_real(dag->right)) {
			dag->u.d = -dag->right->u.d;
			dag->mark = 1;
			return 0;
		}
	}
	else if (FM__DAG_OP_EXPR_NOT == dag->op) {
		if (eval_expr(dag->right, depth, stop)) {
			FM__TRACE(0);
			return -1;
		}
		if (fm__dag_is_int(dag) && fm__dag_is_int(dag->right)) {
			if (!(dag->u.i = fm__bigint_not(dag->right->u.i))) {
				FM__TRACE(0);
				return -1;
			}
			dag->mark = 1;
			return 0;
		}
	}
	else if (FM__DAG_OP_EXPR_LOGNOT == dag->op) {
		if (eval_expr(dag->right, depth, stop)) {
			FM__TRACE(0);
			return -1;
		}
		if (fm__dag_is_bool(dag) && fm__dag_is_bool(dag->right)) {
			dag->u.b = !dag->right->u.b;
			dag->mark = 1;
			return 0;
		}
	}
	else if (FM__DAG_OP_EXPR_MATMUL == dag->op) {
		/* runtime only */
	}
	else if (FM__DAG_OP_EXPR_MUL == dag->op) {
		if (eval_expr(dag->left, depth, stop) ||
		    eval_expr(dag->right, depth, stop)) {
			FM__TRACE(0);
			return -1;
		}
		if (fm__dag_is_int(dag) &&
		    fm__dag_is_int(dag->left) &&
		    fm__dag_is_int(dag->right)) {
			if (!(dag->u.i = fm__bigint_mul(dag->left->u.i,
							dag->right->u.i))) {
				FM__TRACE(0);
				return -1;
			}
			dag->mark = 1;
			return 0;
		}
		if (fm__dag_is_real(dag) &&
		    (fm__dag_is_real(dag->left) ||
		     fm__dag_is_real(dag->right))) {
			if (real(dag->left, &l, stop) ||
			    real(dag->right, &r, stop)) {
				FM__TRACE(0);
				return -1;
			}
			dag->u.d = l * r;
			if (isnan(dag->u.d) || isinf(dag->u.d)) {
				(*stop) = 1;
				ERR(dag, "nonfinite value");
				FM__TRACE(FM__ERRNO_SYNTAX);
				return -1;
			}
			dag->mark = 1;
			return 0;
		}
	}
	else if (FM__DAG_OP_EXPR_DIV == dag->op) {
		if (eval_expr(dag->left, depth, stop) ||
		    eval_expr(dag->right, depth, stop)) {
			FM__TRACE(0);
			return -1;
		}
		if (fm__dag_is_int(dag) &&
		    fm__dag_is_int(dag->left) &&
		    fm__dag_is_int(dag->right)) {
			if (!fm__bigint_sign(dag->right->u.i)) {
				(*stop) = 1;
				ERR(dag, "divide by zero");
				FM__TRACE(FM__ERRNO_SYNTAX);
				return -1;
			}
			if (!(dag->u.i = fm__bigint_div(dag->left->u.i,
							dag->right->u.i))) {
				FM__TRACE(0);
				return -1;
			}
			dag->mark = 1;
			return 0;
		}
		if (fm__dag_is_real(dag) &&
		    (fm__dag_is_real(dag->left) ||
		     fm__dag_is_real(dag->right))) {
			if (real(dag->left, &l, stop) ||
			    real(dag->right, &r, stop)) {
				FM__TRACE(0);
				return -1;
			}
			dag->u.d = l / r;
			if (isnan(dag->u.d) || isinf(dag->u.d)) {
				(*stop) = 1;
				ERR(dag, "nonfinite value");
				FM__TRACE(FM__ERRNO_SYNTAX);
				return -1;
			}
			dag->mark = 1;
			return 0;
		}
	}
	else if (FM__DAG_OP_EXPR_MOD == dag->op) {
		if (eval_expr(dag->left, depth, stop) ||
		    eval_expr(dag->right, depth, stop)) {
			FM__TRACE(0);
			return -1;
		}
		if (fm__dag_is_int(dag) &&
		    fm__dag_is_int(dag->left) &&
		    fm__dag_is_int(dag->right)) {
			if (!fm__bigint_sign(dag->right->u.i)) {
				(*stop) = 1;
				ERR(dag, "divide by zero");
				FM__TRACE(FM__ERRNO_SYNTAX);
				return -1;
			}
			if (!(dag->u.i = fm__bigint_mod(dag->left->u.i,
							dag->right->u.i))) {
				FM__TRACE(0);
				return -1;
			}
			dag->mark = 1;
			return 0;
		}
	}
	else if (FM__DAG_OP_EXPR_ADD == dag->op) {
		if (eval_expr(dag->left, depth, stop) ||
		    eval_expr(dag->right, depth, stop)) {
			FM__TRACE(0);
			return -1;
		}
		if (fm__dag_is_int(dag) &&
		    fm__dag_is_int(dag->left) &&
		    fm__dag_is_int(dag->right)) {
			if (!(dag->u.i = fm__bigint_add(dag->left->u.i,
							dag->right->u.i))) {
				FM__TRACE(0);
				return -1;
			}
			dag->mark = 1;
			return 0;
		}
		if (fm__dag_is_real(dag) &&
		    (fm__dag_is_real(dag->left) ||
		     fm__dag_is_real(dag->right))) {
			if (real(dag->left, &l, stop) ||
			    real(dag->right, &r, stop)) {
				FM__TRACE(0);
				return -1;
			}
			dag->u.d = l + r;
			if (isnan(dag->u.d) || isinf(dag->u.d)) {
				(*stop) = 1;
				ERR(dag, "nonfinite value");
				FM__TRACE(FM__ERRNO_SYNTAX);
				return -1;
			}
			dag->mark = 1;
			return 0;
		}
	}
	else if (FM__DAG_OP_EXPR_SUB == dag->op) {
		if (eval_expr(dag->left, depth, stop) ||
		    eval_expr(dag->right, depth, stop)) {
			FM__TRACE(0);
			return -1;
		}
		if (fm__dag_is_int(dag) &&
		    fm__dag_is_int(dag->left) &&
		    fm__dag_is_int(dag->right)) {
			if (!(dag->u.i = fm__bigint_sub(dag->left->u.i,
							dag->right->u.i))) {
				FM__TRACE(0);
				return -1;
			}
			dag->mark = 1;
			return 0;
		}
		if (fm__dag_is_real(dag) &&
		    (fm__dag_is_real(dag->left) ||
		     fm__dag_is_real(dag->right))) {
			if (real(dag->left, &l, stop) ||
			    real(dag->right, &r, stop)) {
				FM__TRACE(0);
				return -1;
			}
			dag->u.d = l - r;
			if (isnan(dag->u.d) || isinf(dag->u.d)) {
				(*stop) = 1;
				ERR(dag, "nonfinite value");
				FM__TRACE(FM__ERRNO_SYNTAX);
				return -1;
			}
			dag->mark = 1;
			return 0;
		}
	}
	else if (FM__DAG_OP_EXPR_SHL == dag->op) {
		if (eval_expr(dag->left, depth, stop) ||
		    eval_expr(dag->right, depth, stop)) {
			FM__TRACE(0);
			return -1;
		}
		if (fm__dag_is_int(dag) &&
		    fm__dag_is_int(dag->left) &&
		    fm__dag_is_int(dag->right)) {
			if (!(dag->u.i = fm__bigint_shl(dag->left->u.i,
							dag->right->u.i))) {
				FM__TRACE(0);
				return -1;
			}
			dag->mark = 1;
			return 0;
		}
	}
	else if (FM__DAG_OP_EXPR_SHR == dag->op) {
		if (eval_expr(dag->left, depth, stop) ||
		    eval_expr(dag->right, depth, stop)) {
			FM__TRACE(0);
			return -1;
		}
		if (fm__dag_is_int(dag) &&
		    fm__dag_is_int(dag->left) &&
		    fm__dag_is_int(dag->right)) {
			if (!(dag->u.i = fm__bigint_shr(dag->left->u.i,
							dag->right->u.i))) {
				FM__TRACE(0);
				return -1;
			}
			dag->mark = 1;
			return 0;
		}
	}
	else if (FM__DAG_OP_EXPR_LT == dag->op) {
		if (eval_expr(dag->left, depth, stop) ||
		    eval_expr(dag->right, depth, stop)) {
			FM__TRACE(0);
			return -1;
		}
		if (fm__dag_is_bool(dag) &&
		    fm__dag_is_int(dag->left) &&
		    fm__dag_is_int(dag->right)) {
			dag->u.b = (0 > fm__bigint_cmp(dag->left->u.i,
						       dag->right->u.i));
			dag->mark = 1;
			return 0;
		}
		if (fm__dag_is_bool(dag) &&
		    (fm__dag_is_real(dag->left) ||
		     fm__dag_is_real(dag->right))) {
			if (real(dag->left, &l, stop) ||
			    real(dag->right, &r, stop)) {
				FM__TRACE(0);
				return -1;
			}
			dag->u.b = (l < r);
			dag->mark = 1;
			return 0;
		}
		if (fm__dag_is_bool(dag) &&
		    fm__dag_is_bool(dag->left) &&
		    fm__dag_is_bool(dag->right)) {
			dag->u.b = (dag->left->u.b < dag->right->u.b);
			dag->mark = 1;
			return 0;
		}
		if (fm__dag_is_bool(dag) &&
		    fm__dag_is_string(dag->left) &&
		    fm__dag_is_string(dag->right)) {
			dag->u.b = (0 > strcmp(dag->left->u.s,
					       dag->right->u.s));
			dag->mark = 1;
			return 0;
		}
	}
	else if (FM__DAG_OP_EXPR_GT == dag->op) {
		if (eval_expr(dag->left, depth, stop) ||
		    eval_expr(dag->right, depth, stop)) {
			FM__TRACE(0);
			return -1;
		}
		if (fm__dag_is_bool(dag) &&
		    fm__dag_is_int(dag->left) &&
		    fm__dag_is_int(dag->right)) {
			dag->u.b = (0 < fm__bigint_cmp(dag->left->u.i,
						       dag->right->u.i));
			dag->mark = 1;
			return 0;
		}
		if (fm__dag_is_bool(dag) &&
		    (fm__dag_is_real(dag->left) ||
		     fm__dag_is_real(dag->right))) {
			if (real(dag->left, &l, stop) ||
			    real(dag->right, &r, stop)) {
				FM__TRACE(0);
				return -1;
			}
			dag->u.b = (l > r);
			dag->mark = 1;
			return 0;
		}
		if (fm__dag_is_bool(dag) &&
		    fm__dag_is_bool(dag->left) &&
		    fm__dag_is_bool(dag->right)) {
			dag->u.b = (dag->left->u.b > dag->right->u.b);
			dag->mark = 1;
			return 0;
		}
		if (fm__dag_is_bool(dag) &&
		    fm__dag_is_string(dag->left) &&
		    fm__dag_is_string(dag->right)) {
			dag->u.b = (0 < strcmp(dag->left->u.s,
					       dag->right->u.s));
			dag->mark = 1;
			return 0;
		}
	}
	else if (FM__DAG_OP_EXPR_LE == dag->op) {
		if (eval_expr(dag->left, depth, stop) ||
		    eval_expr(dag->right, depth, stop)) {
			FM__TRACE(0);
			return -1;
		}
		if (fm__dag_is_bool(dag) &&
		    fm__dag_is_int(dag->left) &&
		    fm__dag_is_int(dag->right)) {
			dag->u.b = (0 >= fm__bigint_cmp(dag->left->u.i,
							dag->right->u.i));
			dag->mark = 1;
			return 0;
		}
		if (fm__dag_is_bool(dag) &&
		    (fm__dag_is_real(dag->left) ||
		     fm__dag_is_real(dag->right))) {
			if (real(dag->left, &l, stop) ||
			    real(dag->right, &r, stop)) {
				FM__TRACE(0);
				return -1;
			}
			dag->u.b = (l <= r);
			dag->mark = 1;
			return 0;
		}
		if (fm__dag_is_bool(dag) &&
		    fm__dag_is_bool(dag->left) &&
		    fm__dag_is_bool(dag->right)) {
			dag->u.b = (dag->left->u.b <= dag->right->u.b);
			dag->mark = 1;
			return 0;
		}
		if (fm__dag_is_bool(dag) &&
		    fm__dag_is_string(dag->left) &&
		    fm__dag_is_string(dag->right)) {
			dag->u.b = (0 >= strcmp(dag->left->u.s,
						dag->right->u.s));
			dag->mark = 1;
			return 0;
		}
	}
	else if (FM__DAG_OP_EXPR_GE == dag->op) {
		if (eval_expr(dag->left, depth, stop) ||
		    eval_expr(dag->right, depth, stop)) {
			FM__TRACE(0);
			return -1;
		}
		if (fm__dag_is_bool(dag) &&
		    fm__dag_is_int(dag->left) &&
		    fm__dag_is_int(dag->right)) {
			dag->u.b = (0 <= fm__bigint_cmp(dag->left->u.i,
							dag->right->u.i));
			dag->mark = 1;
			return 0;
		}
		if (fm__dag_is_bool(dag) &&
		    (fm__dag_is_real(dag->left) ||
		     fm__dag_is_real(dag->right))) {
			if (real(dag->left, &l, stop) ||
			    real(dag->right, &r, stop)) {
				FM__TRACE(0);
				return -1;
			}
			dag->u.b = (l >= r);
			dag->mark = 1;
			return 0;
		}
		if (fm__dag_is_bool(dag) &&
		    fm__dag_is_bool(dag->left) &&
		    fm__dag_is_bool(dag->right)) {
			dag->u.b = (dag->left->u.b >= dag->right->u.b);
			dag->mark = 1;
			return 0;
		}
		if (fm__dag_is_bool(dag) &&
		    fm__dag_is_string(dag->left) &&
		    fm__dag_is_string(dag->right)) {
			dag->u.b = (0 <= strcmp(dag->left->u.s,
						dag->right->u.s));
			dag->mark = 1;
			return 0;
		}
	}
	else if (FM__DAG_OP_EXPR_EQ == dag->op) {
		if (eval_expr(dag->left, depth, stop) ||
		    eval_expr(dag->right, depth, stop)) {
			FM__TRACE(0);
			return -1;
		}
		if (fm__dag_is_bool(dag) &&
		    fm__dag_is_int(dag->left) &&
		    fm__dag_is_int(dag->right)) {
			dag->u.b = (0 == fm__bigint_cmp(dag->left->u.i,
							dag->right->u.i));
			dag->mark = 1;
			return 0;
		}
		if (fm__dag_is_bool(dag) &&
		    (fm__dag_is_real(dag->left) ||
		     fm__dag_is_real(dag->right))) {
			if (real(dag->left, &l, stop) ||
			    real(dag->right, &r, stop)) {
				FM__TRACE(0);
				return -1;
			}
			dag->u.b = (l == r);
			dag->mark = 1;
			return 0;
		}
		if (fm__dag_is_bool(dag) &&
		    fm__dag_is_bool(dag->left) &&
		    fm__dag_is_bool(dag->right)) {
			dag->u.b = (dag->left->u.b == dag->right->u.b);
			dag->mark = 1;
			return 0;
		}
		if (fm__dag_is_bool(dag) &&
		    fm__dag_is_string(dag->left) &&
		    fm__dag_is_string(dag->right)) {
			dag->u.b = (0 == strcmp(dag->left->u.s,
						dag->right->u.s));
			dag->mark = 1;
			return 0;
		}
	}
	else if (FM__DAG_OP_EXPR_NE == dag->op) {
		if (eval_expr(dag->left, depth, stop) ||
		    eval_expr(dag->right, depth, stop)) {
			FM__TRACE(0);
			return -1;
		}
		if (fm__dag_is_bool(dag) &&
		    fm__dag_is_int(dag->left) &&
		    fm__dag_is_int(dag->right)) {
			dag->u.b = (0 != fm__bigint_cmp(dag->left->u.i,
							dag->right->u.i));
			dag->mark = 1;
			return 0;
		}
		if (fm__dag_is_bool(dag) &&
		    (fm__dag_is_real(dag->left) ||
		     fm__dag_is_real(dag->right))) {
			if (real(dag->left, &l, stop) ||
			    real(dag->right, &r, stop)) {
				FM__TRACE(0);
				return -1;
			}
			dag->u.b = (l != r);
			dag->mark = 1;
			return 0;
		}
		if (fm__dag_is_bool(dag) &&
		    fm__dag_is_bool(dag->left) &&
		    fm__dag_is_bool(dag->right)) {
			dag->u.b = (dag->left->u.b != dag->right->u.b);
			dag->mark = 1;
			return 0;
		}
		if (fm__dag_is_bool(dag) &&
		    fm__dag_is_string(dag->left) &&
		    fm__dag_is_string(dag->right)) {
			dag->u.b = (0 != strcmp(dag->left->u.s,
						dag->right->u.s));
			dag->mark = 1;
			return 0;
		}
	}
	else if (FM__DAG_OP_EXPR_AND == dag->op) {
		if (eval_expr(dag->left, depth, stop) ||
		    eval_expr(dag->right, depth, stop)) {
			FM__TRACE(0);
			return -1;
		}
		if (fm__dag_is_int(dag) &&
		    fm__dag_is_int(dag->left) &&
		    fm__dag_is_int(dag->right)) {
			if (!(dag->u.i = fm__bigint_and(dag->left->u.i,
							dag->right->u.i))) {
				FM__TRACE(0);
				return -1;
			}
			dag->mark = 1;
			return 0;
		}
	}
	else if (FM__DAG_OP_EXPR_XOR == dag->op) {
		if (eval_expr(dag->left, depth, stop) ||
		    eval_expr(dag->right, depth, stop)) {
			FM__TRACE(0);
			return -1;
		}
		if (fm__dag_is_int(dag) &&
		    fm__dag_is_int(dag->left) &&
		    fm__dag_is_int(dag->right)) {
			if (!(dag->u.i = fm__bigint_xor(dag->left->u.i,
							dag->right->u.i))) {
				FM__TRACE(0);
				return -1;
			}
			dag->mark = 1;
			return 0;
		}
	}
	else if (FM__DAG_OP_EXPR_OR == dag->op) {
		if (eval_expr(dag->left, depth, stop) ||
		    eval_expr(dag->right, depth, stop)) {
			FM__TRACE(0);
			return -1;
		}
		if (fm__dag_is_int(dag) &&
		    fm__dag_is_int(dag->left) &&
		    fm__dag_is_int(dag->right)) {
			if (!(dag->u.i = fm__bigint_or(dag->left->u.i,
						       dag->right->u.i))) {
				FM__TRACE(0);
				return -1;
			}
			dag->mark = 1;
			return 0;
		}
	}
	else if (FM__DAG_OP_EXPR_LOGAND == dag->op) {
		if (eval_expr(dag->left, depth, stop)) {
			FM__TRACE(0);
			return -1;
		}
		if (fm__dag_is_bool(dag) &&
		    fm__dag_is_bool(dag->left)) {
			if (!dag->left->u.b) {
				dag->u.b = 0; /* short circuit */
				dag->mark = 1;
				return 0;
			}
			if (eval_expr(dag->right, depth, stop)) {
				FM__TRACE(0);
				return -1;
			}
			if (fm__dag_is_bool(dag->right)) {
				dag->u.b = dag->right->u.b;
				dag->mark = 1;
				return 0;
			}
		}
	}
	else if (FM__DAG_OP_EXPR_LOGXOR == dag->op) {
		if (eval_expr(dag->left, depth, stop) ||
		    eval_expr(dag->right, depth, stop)) {
			FM__TRACE(0);
			return -1;
		}
		if (fm__dag_is_bool(dag) &&
		    fm__dag_is_bool(dag->left) &&
		    fm__dag_is_bool(dag->right)) {
			dag->u.b = dag->left->u.b ^ dag->right->u.b;
			dag->u.b = dag->u.b ? 1 : 0;
			dag->mark = 1;
			return 0;
		}
	}
	else if (FM__DAG_OP_EXPR_LOGOR == dag->op) {
		if (eval_expr(dag->left, depth, stop)) {
			FM__TRACE(0);
			return -1;
		}
		if (fm__dag_is_bool(dag) &&
		    fm__dag_is_bool(dag->left)) {
			if (dag->left->u.b) {
				dag->u.b = 1; /* short circuit */
				dag->mark = 1;
				return 0;
			}
			if (eval_expr(dag->right, depth, stop)) {
				FM__TRACE(0);
				return -1;
			}
			if (fm__dag_is_bool(dag->right)) {
				dag->u.b = dag->right->u.b;
				dag->mark = 1;
				return 0;
			}
		}
	}
	else if (FM__DAG_OP_EXPR_COND == dag->op) {
		if (eval_expr(dag->cond, depth, stop)) {
			FM__TRACE(0);
			return -1;
		}
		if (fm__dag_is_bool(dag->cond)) {
			const struct fm__dag *dag_ = NULL;
			if (dag->cond->u.b) {
				if (eval_expr(dag->left, depth, stop)) {
					FM__TRACE(0);
					return -1;
				}
				dag_ = dag->left;
			}
			else {
				if (eval_expr(dag->right, depth, stop)) {
					FM__TRACE(0);
					return -1;
				}
				dag_ = dag->right;
			}
			if (dag_ &&
			    fm__dag_is_int(dag) &&
			    fm__dag_is_int(dag_)) {
				dag->u.i = fm__bigint_clone(dag_->u.i);
				if (!dag->u.i) {
					FM__TRACE(0);
					return -1;
				}
				dag->mark = 1;
				return 0;
			}
			if (dag_ &&
			    fm__dag_is_real(dag) &&
			    (fm__dag_is_int(dag_) ||
			     fm__dag_is_real(dag_))) {
				if (real(dag_, &dag->u.d, stop)) {
					FM__TRACE(0);
					return -1;
				}
				dag->mark = 1;
				return 0;
			}
			if (dag_ &&
			    fm__dag_is_bool(dag) &&
			    fm__dag_is_bool(dag_)) {
				dag->u.b = dag_->u.b;
				dag->mark = 1;
				return 0;
			}
			if (dag_ &&
			    fm__dag_is_string(dag) &&
			    fm__dag_is_string(dag_)) {
				if (!(dag->u.s = fm__strdup(dag_->u.s))) {
					FM__TRACE(0);
					return -1;
				}
				dag->mark = 1;
				return 0;
			}
		}
	}
	FM__TRACE(FM__ERRNO_SOFTWARE);
	return -1;
}

static int /* bool */
is_valid_slice(const struct fm__dag *dag)
{
	if (!dag || (FM__DAG_OP_EXPR_SLICE != dag->op)) {
		return 0;
	}
	switch (dag->slice) {
	case FM__DAG_SLICE_SCALAR:
		if (!dag->cond || dag->left || dag->right) {
			return 0;
		}
		break;
	case FM__DAG_SLICE_RANGE:
		if (dag->right) {
			return 0;
		}
		break;
	case FM__DAG_SLICE_STEPPED_RANGE:
		break;
	default:
		return 0;
	}
	if ((dag->cond && !fm__dag_is_int(dag->cond)) ||
	    (dag->left && !fm__dag_is_int(dag->left)) ||
	    (dag->right && !fm__dag_is_int(dag->right))) {
		return 0;
	}
	return 1;
}

static int /* bool */
is_valid_slices(const struct fm__dag *dag)
{
	if (dag) {
		while (dag) {
			if ((FM__DAG_OP_EXPR_SLICES != dag->op) ||
			    !is_valid_slice(dag->left)) {
				return 0;
			}
			dag = dag->right;
		}
		return 1;
	}
	return 0;
}

static int /* bool */
is_valid_permute(const struct fm__dag *dag)
{
	if (dag) {
		while (dag) {
			if ((FM__DAG_OP_EXPR_LIST != dag->op) ||
			    !dag->left ||
			    !fm__dag_is_int(dag->left)) {
				return 0;
			}
			dag = dag->right;
		}
		return 1;
	}
	return 0;
}

static int /* bool */
is_valid_tensor_scalar(const struct fm__dag *tensor,
		       const struct fm__dag *scalar)
{
	if (!fm__dag_is_tensor(tensor)) {
		return 0;
	}
	if (FM__DAG_TYPE_TBOOL == tensor->type) {
		return fm__dag_is_bool(scalar);
	}
	if (fm__dag_is_int(scalar)) {
		return 1;
	}
	if (fm__dag_is_real(scalar) && fm__dag_is_fp_tensor(tensor)) {
		return 1;
	}
	return 0;
}

static int /* bool */
is_valid_int_tensor_scalar(const struct fm__dag *tensor,
			   const struct fm__dag *scalar)
{
	return (fm__dag_is_int_tensor(tensor) ||
		fm__dag_is_uint_tensor(tensor)) &&
		fm__dag_is_int(scalar);
}

void
fm__dag_init(void)
{
	if ((SIZE_MAX / sizeof (struct fm__dag)) < DAGS_PER_CHUNK) {
		FM__TRACE(FM__ERRNO_ARCHITECTURE);
		abort();
	}
}

fm__dag_pool_t
fm__dag_pool_open(int *stop, char *errstr)
{
	struct fm__dag_pool *pool;

	if (!(pool = fm__malloc(sizeof (struct fm__dag_pool)))) {
		FM__TRACE(0);
		return NULL;
	}
	memset(pool, 0, sizeof (struct fm__dag_pool));
	pool->stop = stop;
	pool->errstr = errstr;
	pool->chunks = NULL;
	return pool;
}

void
fm__dag_pool_close(fm__dag_pool_t pool)
{
	struct fm__dag_pool_chunk *chunk;
	struct fm__dag *dag;

	if (pool) {
		while (pool->chunks) {
			chunk = pool->chunks;
			pool->chunks = chunk->link;
			for (size_t i=0; i<DAGS_PER_CHUNK; ++i) {
				dag = &chunk->dags[i];
				if (FM__DAG_TYPE_INT == dag->type) {
					fm__bigint_free(dag->u.i);
				}
				if ((FM__DAG_TYPE_STRING == dag->type) ||
				    (FM__DAG_OP_DECL_TENSOR == dag->op) ||
				    (FM__DAG_OP_DECL_COMPUTE == dag->op) ||
				    ((FM__DAG_OP_EXPR_IDENTIFIER == dag->op) &&
				     fm__dag_is_tensor(dag))) {
					free((void *)dag->u.s);
				}
			}
			free(chunk->dags);
			free(chunk);
		}
		free(pool);
	}
}

struct fm__dag *
fm__dag_pool_allocate(fm__dag_pool_t pool)
{
	struct fm__dag_pool_chunk *chunk;
	struct fm__dag *dag;
	size_t n;

	assert( pool );

	if (SIZE_MAX == pool->id) {
		FM__TRACE(FM__ERRNO_MEMORY);
		return NULL;
	}
	if (!pool->chunks || (DAGS_PER_CHUNK <= pool->size)) {
		chunk = fm__malloc(sizeof (struct fm__dag_pool_chunk));
		if (!chunk) {
			FM__TRACE(0);
			return NULL;
		}
		memset(chunk, 0, sizeof (struct fm__dag_pool_chunk));
		n = DAGS_PER_CHUNK * sizeof (struct fm__dag);
		if (!(chunk->dags = fm__malloc(n))) {
			free(chunk);
			FM__TRACE(0);
			return NULL;
		}
		memset(chunk->dags, 0, n);
		chunk->link = pool->chunks;
		pool->chunks = chunk;
		pool->size = 0;
	}
	dag = &pool->chunks->dags[pool->size++];
	dag->errstr = pool->errstr;
	dag->stop = pool->stop;
	dag->cond = NULL;
	dag->left = NULL;
	dag->right = NULL;
	dag->id = ++pool->id;
	return dag;
}

enum fm__dag_type
fm__dag_type(const struct fm__dag *dag)
{
	assert( dag );

	if (FM__DAG_OP_EXPR_LINT == dag->op) {
		return FM__DAG_TYPE_INT;
	}
	else if (FM__DAG_OP_EXPR_LREAL == dag->op) {
		return FM__DAG_TYPE_REAL;
	}
	else if (FM__DAG_OP_EXPR_LBOOL == dag->op) {
		return FM__DAG_TYPE_BOOL;
	}
	else if (FM__DAG_OP_EXPR_LSTRING == dag->op) {
		return FM__DAG_TYPE_STRING;
	}
	else if (FM__DAG_OP_EXPR_IDENTIFIER == dag->op) {
		return dag->type;
	}
	else if (FM__DAG_OP_EXPR_FNC == dag->op) {
		if ((0 < dag->index) && (FM__FNCS_SIZE > dag->index)) {
			return fm__fnc_type(dag);
		}
	}
	else if (FM__DAG_OP_EXPR_CAST == dag->op) {
		if (dag->right &&
		    fm__dag_is_tensor(dag) &&
		    (fm__dag_is_int(dag->right) ||
		     fm__dag_is_real(dag->right) ||
		     fm__dag_is_bool(dag->right) ||
		     fm__dag_is_tensor(dag->right))) {
			return dag->type;
		}
	}
	else if ((FM__DAG_OP_EXPR_RESHAPE == dag->op) ||
		 (FM__DAG_OP_EXPR_BROADCAST == dag->op)) {
		if (dag->left && fm__dag_is_tensor(dag->left)) {
			return dag->left->type;
		}
	}
	else if (FM__DAG_OP_EXPR_SLICE == dag->op) {
		return FM__DAG_TYPE_UNKNOWN;
	}
	else if (FM__DAG_OP_EXPR_SLICES == dag->op) {
		return FM__DAG_TYPE_UNKNOWN;
	}
	else if (FM__DAG_OP_EXPR_VIEW == dag->op) {
		if (dag->left &&
		    fm__dag_is_tensor(dag->left) &&
		    ((!dag->right && !dag->cond) ||
		     (dag->right &&
		      !dag->cond &&
		      is_valid_slices(dag->right)) ||
		     (dag->cond &&
		      !dag->right &&
		      is_valid_permute(dag->cond)))) {
			return dag->left->type;
		}
	}
	else if (FM__DAG_OP_EXPR_POS == dag->op) {
		if (dag->right &&
		    (fm__dag_is_int(dag->right) ||
		     fm__dag_is_real(dag->right))) {
			return dag->right->type;
		}
		if (dag->right && fm__dag_is_numeric_tensor(dag->right)) {
			return dag->right->type;
		}
	}
	else if (FM__DAG_OP_EXPR_NEG == dag->op) {
		if (dag->right &&
		    (fm__dag_is_int(dag->right) ||
		     fm__dag_is_real(dag->right) ||
		     fm__dag_is_int_tensor(dag->right) ||
		     fm__dag_is_fp_tensor(dag->right))) {
			return dag->right->type;
		}
	}
	else if (FM__DAG_OP_EXPR_NOT == dag->op) {
		if (dag->right &&
		    (fm__dag_is_int(dag->right) ||
		     fm__dag_is_int_tensor(dag->right) ||
		     fm__dag_is_uint_tensor(dag->right))) {
			return dag->right->type;
		}
	}
	else if (FM__DAG_OP_EXPR_LOGNOT == dag->op) {
		if (dag->right && fm__dag_is_bool(dag->right)) {
			return FM__DAG_TYPE_BOOL;
		}
		if (dag->right && fm__dag_is_tensor(dag->right)) {
			return FM__DAG_TYPE_TBOOL;
		}
	}
	else if ((FM__DAG_OP_EXPR_MUL == dag->op) ||
		 (FM__DAG_OP_EXPR_DIV == dag->op) ||
		 (FM__DAG_OP_EXPR_ADD == dag->op) ||
		 (FM__DAG_OP_EXPR_SUB == dag->op)) {
		if (dag->left && dag->right) {
			if (fm__dag_is_int(dag->left) &&
			    fm__dag_is_int(dag->right)) {
				return FM__DAG_TYPE_INT;
			}
			if ((fm__dag_is_int(dag->left) ||
			     fm__dag_is_real(dag->left)) &&
			    (fm__dag_is_int(dag->right) ||
			     fm__dag_is_real(dag->right))) {
				return FM__DAG_TYPE_REAL;
			}
			if (fm__dag_is_numeric_tensor(dag->left) &&
			    fm__dag_is_numeric_tensor(dag->right) &&
			    (dag->left->type == dag->right->type)) {
				return dag->left->type;
			}
			if (fm__dag_is_numeric_tensor(dag->left) &&
			    is_valid_tensor_scalar(dag->left, dag->right)) {
				return dag->left->type;
			}
			if (fm__dag_is_numeric_tensor(dag->right) &&
			    is_valid_tensor_scalar(dag->right, dag->left)) {
				return dag->right->type;
			}
		}
	}
	else if (FM__DAG_OP_EXPR_MATMUL == dag->op) {
		if (dag->left &&
		    dag->right &&
		    fm__dag_is_numeric_tensor(dag->left) &&
		    fm__dag_is_numeric_tensor(dag->right) &&
		    (dag->left->type == dag->right->type)) {
			return dag->left->type;
		}
	}
	else if ((FM__DAG_OP_EXPR_MOD == dag->op) ||
		 (FM__DAG_OP_EXPR_AND == dag->op) ||
		 (FM__DAG_OP_EXPR_XOR == dag->op) ||
		 (FM__DAG_OP_EXPR_OR == dag->op) ||
		 (FM__DAG_OP_EXPR_SHL == dag->op) ||
		 (FM__DAG_OP_EXPR_SHR == dag->op)) {
		if (dag->left && dag->right) {
			if (fm__dag_is_int(dag->left) &&
			    fm__dag_is_int(dag->right)) {
				return FM__DAG_TYPE_INT;
			}
			if ((fm__dag_is_int_tensor(dag->left) ||
			     fm__dag_is_uint_tensor(dag->left)) &&
			    (dag->left->type == dag->right->type)) {
				return dag->left->type;
			}
			if (is_valid_int_tensor_scalar(dag->left,
						       dag->right)) {
				return dag->left->type;
			}
			if (is_valid_int_tensor_scalar(dag->right,
						       dag->left)) {
				return dag->right->type;
			}
		}
	}
	else if ((FM__DAG_OP_EXPR_LT == dag->op) ||
		 (FM__DAG_OP_EXPR_GT == dag->op) ||
		 (FM__DAG_OP_EXPR_LE == dag->op) ||
		 (FM__DAG_OP_EXPR_GE == dag->op) ||
		 (FM__DAG_OP_EXPR_EQ == dag->op) ||
		 (FM__DAG_OP_EXPR_NE == dag->op)) {
		if (dag->left && dag->right) {
			if ((fm__dag_is_bool(dag->left) &&
			     fm__dag_is_bool(dag->right)) ||
			    (fm__dag_is_string(dag->left) &&
			     fm__dag_is_string(dag->right))) {
				return FM__DAG_TYPE_BOOL;
			}
			if ((fm__dag_is_int(dag->left) ||
			     fm__dag_is_real(dag->left)) &&
			    (fm__dag_is_int(dag->right) ||
			     fm__dag_is_real(dag->right))) {
				return FM__DAG_TYPE_BOOL;
			}
			if (fm__dag_is_tensor(dag->left) &&
			    fm__dag_is_tensor(dag->right) &&
			    (dag->left->type == dag->right->type)) {
				return FM__DAG_TYPE_TBOOL;
			}
			if (is_valid_tensor_scalar(dag->left, dag->right) ||
			    is_valid_tensor_scalar(dag->right, dag->left)) {
				return FM__DAG_TYPE_TBOOL;
			}
		}
	}
	else if ((FM__DAG_OP_EXPR_LOGAND == dag->op) ||
		 (FM__DAG_OP_EXPR_LOGXOR == dag->op) ||
		 (FM__DAG_OP_EXPR_LOGOR == dag->op)) {
		if (dag->left && dag->right) {
			if (fm__dag_is_bool(dag->left) &&
			    fm__dag_is_bool(dag->right)) {
				return FM__DAG_TYPE_BOOL;
			}
			if (fm__dag_is_tensor(dag->left) &&
			    fm__dag_is_tensor(dag->right) &&
			    (dag->left->type == dag->right->type)) {
				return FM__DAG_TYPE_TBOOL;
			}
			if ((fm__dag_is_tensor(dag->left) &&
			     fm__dag_is_bool(dag->right)) ||
			    (fm__dag_is_bool(dag->left) &&
			     fm__dag_is_tensor(dag->right))) {
				return FM__DAG_TYPE_TBOOL;
			}
		}
	}
	else if (FM__DAG_OP_EXPR_COND == dag->op) {
		if (dag->cond && dag->left && dag->right) {
			if (fm__dag_is_bool(dag->cond)) {
				if (fm__dag_is_int(dag->left) &&
				    fm__dag_is_int(dag->right)) {
					return FM__DAG_TYPE_INT;
				}
				if (fm__dag_is_bool(dag->left) &&
				    fm__dag_is_bool(dag->right)) {
					return FM__DAG_TYPE_BOOL;
				}
				if (fm__dag_is_string(dag->left) &&
				    fm__dag_is_string(dag->right)) {
					return FM__DAG_TYPE_STRING;
				}
				if ((fm__dag_is_int(dag->left) ||
				     fm__dag_is_real(dag->left)) &&
				    (fm__dag_is_int(dag->right) ||
				     fm__dag_is_real(dag->right))) {
					return FM__DAG_TYPE_REAL;
				}
				if (fm__dag_is_tensor(dag->left) &&
				    fm__dag_is_tensor(dag->right) &&
				    (dag->left->type == dag->right->type)) {
					return dag->left->type;
				}
				if (is_valid_tensor_scalar(dag->left,
							   dag->right)) {
					return dag->left->type;
				}
				if (is_valid_tensor_scalar(dag->right,
							   dag->left)) {
					return dag->right->type;
				}
			}
			if (FM__DAG_TYPE_TBOOL == dag->cond->type) {
				if (fm__dag_is_tensor(dag->left) &&
				    fm__dag_is_tensor(dag->right) &&
				    (dag->left->type == dag->right->type)) {
					return dag->left->type;
				}
				if (is_valid_tensor_scalar(dag->left,
							   dag->right)) {
					return dag->left->type;
				}
				if (is_valid_tensor_scalar(dag->right,
							   dag->left)) {
					return dag->right->type;
				}
			}
		}
	}
	else if (FM__DAG_OP_EXPR_LIST == dag->op) {
		/* a list has no known type */
	}
	return FM__DAG_TYPE_UNKNOWN;
}

int
fm__dag_eval_expr(struct fm__dag *dag)
{
	int stop;

	assert( dag );

	stop = 0;
	if (eval_expr(dag, 0, &stop)) {
		if (!stop) {
			if (FM__ERRNO_MEMORY == errno) {
				ERR(dag, "out of memory");
				FM__TRACE(0);
			}
			else {
				ERR(dag,
				    "unable to evaluate constant expression");
				FM__TRACE(FM__ERRNO_SYNTAX);
			}
		}
		FM__TRACE(0);
		return -1;
	}
	return 0;
}
