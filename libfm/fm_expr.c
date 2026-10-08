/**
 * Copyright (c) 2025-2026 Tony Givargis
 * University of California, Irvine
 * fm_expr.c
 */

#include "fm_fnc.h"
#include "fm_geometry.h"
#include "fm_utils.h"
#include "fm_expr.h"

#define ERR FM__UTILS_ERR_DAG

static struct fm_expr *
expr_new(size_t *id, const struct fm__dag *dag)
{
	struct fm_expr *expr;

	assert( id && dag );

	if (SIZE_MAX == (*id)) {
		FM__TRACE(FM__ERRNO_MEMORY);
		return NULL;
	}
	if (!(expr = fm__malloc(sizeof (struct fm_expr)))) {
		FM__TRACE(0);
		return NULL;
	}
	memset(expr, 0, sizeof (struct fm_expr));
	expr->left = NULL;
	expr->right = NULL;
	expr->id = ++(*id);
	expr->refs = 1;
	expr->op = fm__utils_op2op(dag->op);
	if (FM_EXPR_OP_COND == expr->op) {
		expr->u.expr = NULL;
	}
	expr->dtype = fm__utils_type2dtype(dag->type);
	expr->lineno = dag->lineno;
	expr->column = dag->column;
	return expr;
}

static int
matmul_operand(struct fm_expr **operand,
	       const struct fm_geometry *geometry,
	       const struct fm__dag *dag,
	       size_t *id)
{
	struct fm_expr *view;

	if (((*operand)->geometry.ndim == geometry->ndim) &&
	    fm__utils_match_shape((*operand)->geometry.shape,
				  geometry->shape,
				  geometry->ndim)) {
		return 0;
	}
	if (!(view = expr_new(id, dag))) {
		FM__TRACE(0);
		return -1;
	}
	view->op = FM_EXPR_OP_VIEW;
	view->dtype = (*operand)->dtype;
	view->geometry = (*geometry);
	view->left = (*operand);
	(*operand) = view;
	return 0;
}

static int
matmul_expr(struct fm_expr **expr_, const struct fm__dag *dag, size_t *id)
{
	struct fm_expr *expr, *view;
	struct fm__matmul_plan plan;

	expr = (*expr_);
	if (fm__geometry_matmul_plan(&plan,
				     &expr->left->geometry,
				     &expr->right->geometry, dag) ||
	    matmul_operand(&expr->left, &plan.left, dag, id) ||
	    matmul_operand(&expr->right, &plan.right, dag, id)) {
		FM__TRACE(0);
		return -1;
	}
	expr->geometry = plan.result;
	if (plan.result.ndim != plan.output.ndim) {
		if (!(view = expr_new(id, dag))) {
			FM__TRACE(0);
			return -1;
		}
		view->op = FM_EXPR_OP_VIEW;
		view->geometry = plan.output;
		view->left = expr;
		(*expr_) = view;
	}
	return 0;
}

static void
free_argv(struct fm_expr *argv[], size_t argc)
{
	while (argc) {
		fm__expr_free(argv[--argc]);
	}
}

static struct fm_expr *
walk(fm_t lang, const struct fm__dag *dag, size_t *id, size_t depth)
{
	struct fm_expr *expr, *copy, *argv[FM__FNC_MAX_ARGS];
	const struct fm__dag *arg;
	int done, fold;
	size_t argc, i;
	int64_t axis;

	done = 0;
	if (FM__DAG_MAX_DEPTH <= depth) {
		ERR(dag, "maximum evaluation depth exceeded");
		FM__TRACE(FM__ERRNO_SYNTAX);
		return NULL;
	}
	++depth;
	if (fm__dag_is_tensor(dag) &&
	    !fm__geometry_is_populated(&dag->geometry)) {
		FM__TRACE(FM__ERRNO_SOFTWARE);
		return NULL;
	}
	if (FM__DAG_OP_EXPR_LINT == dag->op) {
		if (!(expr = expr_new(id, dag))) {
			FM__TRACE(0);
			return NULL;
		}
		if (fm__bigint_int64(dag->u.i,
				     0, /* clamp */
				     &expr->u.int64)) {
			ERR(dag, "integer value is outside int64 range");
			FM__TRACE(FM__ERRNO_SYNTAX);
			fm__expr_free(expr);
			return NULL;
		}
	}
	else if (FM__DAG_OP_EXPR_LREAL == dag->op) {
		if (!(expr = expr_new(id, dag))) {
			FM__TRACE(0);
			return NULL;
		}
		expr->u.fp64 = dag->u.d;
	}
	else if (FM__DAG_OP_EXPR_LBOOL == dag->op) {
		if (!(expr = expr_new(id, dag))) {
			FM__TRACE(0);
			return NULL;
		}
		expr->u.int64 = dag->u.b ? 1 : 0;
	}
	else if (FM__DAG_OP_EXPR_LSTRING == dag->op) {
		ERR(dag,
		    "string values are not supported in compute expressions");
		FM__TRACE(FM__ERRNO_SYNTAX);
		return NULL;
	}
	else if (FM__DAG_OP_EXPR_IDENTIFIER == dag->op) {
		if (!(expr = expr_new(id, dag))) {
			FM__TRACE(0);
			return NULL;
		}
		if (!expr->dtype) {
			fm__expr_free(expr);
			FM__TRACE(FM__ERRNO_SOFTWARE);
			return NULL;
		}
		if (!(expr->u.node = fm_lookup(lang, dag->u.s))) {
			fm__expr_free(expr);
			FM__TRACE(0);
			return NULL;
		}
		expr->geometry = dag->geometry;
	}
	else if ((FM__DAG_OP_EXPR_VIEW == dag->op) ||
		 (FM__DAG_OP_EXPR_BROADCAST == dag->op)) {
		if (!(expr = expr_new(id, dag))) {
			FM__TRACE(0);
			return NULL;
		}
		if (!expr->dtype) {
			fm__expr_free(expr);
			FM__TRACE(FM__ERRNO_SOFTWARE);
			return NULL;
		}
		if (!(expr->left = walk(lang, dag->left, id, depth))) {
			fm__expr_free(expr);
			FM__TRACE(0);
			return NULL;
		}
		expr->geometry = dag->geometry;
	}
	else if (FM__DAG_OP_EXPR_FNC == dag->op) {
		if (!(expr = expr_new(id, dag))) {
			FM__TRACE(0);
			return NULL;
		}
		if (!expr->dtype ||
		    !dag->right ||
		    !dag->index ||
		    (FM__FNCS_SIZE <= dag->index)) {
			fm__expr_free(expr);
			FM__TRACE(FM__ERRNO_SOFTWARE);
			return NULL;
		}
		fold = ((FM_EXPR_OP_FOLD ==
			 FM__FNCS[dag->index].op) ||
			(FM_EXPR_OP_SCAN ==
			 FM__FNCS[dag->index].op));
		arg = dag->right;
		argc = 0;
		while (arg) {
			if ((FM__DAG_OP_EXPR_LIST != arg->op) ||
			    !arg->left ||
			    (FM__FNC_MAX_ARGS <= argc)) {
				free_argv(argv, argc);
				fm__expr_free(expr);
				FM__TRACE(FM__ERRNO_SOFTWARE);
				return NULL;
			}
			if (fold && (1 == argc)) {
				argv[argc++] = NULL;
				arg = arg->right;
				continue;
			}
			argv[argc] = walk(lang, arg->left, id, depth);
			if (!argv[argc]) {
				free_argv(argv, argc);
				fm__expr_free(expr);
				FM__TRACE(0);
				return NULL;
			}
			++argc;
			arg = arg->right;
		}
		i = fm__fnc_types_count(dag->index) - 1;
		if ((argc != i) &&
		    !((argc == (i + 1)) &&
		      FM__FNCS[dag->index].masked &&
		      (FM_EXPR_OP_FOLD ==
		       FM__FNCS[dag->index].op) &&
		      (FM_DTYPE_BOOL == argv[i]->dtype))) {
			free_argv(argv, argc);
			fm__expr_free(expr);
			FM__TRACE(FM__ERRNO_SOFTWARE);
			return NULL;
		}
		if (fold && !(argv[1] = walk(lang,
					     dag->right->right->left,
					     id,
					     depth))) {
			free_argv(argv, argc);
			fm__expr_free(expr);
			FM__TRACE(0);
			return NULL;
		}
		expr->geometry = dag->geometry;
		if (FM__FNCS[dag->index].op) {
			if ((FM_EXPR_OP_CONCAT ==
			     FM__FNCS[dag->index].op) ||
			    (FM_EXPR_OP_GATHER ==
			     FM__FNCS[dag->index].op) ||
			    (FM_EXPR_OP_SCATTER ==
			     FM__FNCS[dag->index].op)) {
				i = (FM_EXPR_OP_SCATTER ==
				     FM__FNCS[dag->index].op) ? 3 : 2;
				if ((argc != (i + 1)) ||
				    (argv[i]->id != (*id)) ||
				    (1 != argv[i]->refs)) {
					free_argv(argv, argc);
					fm__expr_free(expr);
					FM__TRACE(FM__ERRNO_SOFTWARE);
					return NULL;
				}
				axis = argv[i]->u.int64;
				if (0 > axis) {
					axis += (int64_t)
						argv[0]->geometry.ndim;
				}
				expr->u.index.axis = axis;
				expr->u.index.source = NULL;
				expr->left = argv[0];
				expr->right = argv[1];
				argv[0] = NULL;
				argv[1] = NULL;
				if (FM_EXPR_OP_SCATTER ==
				    FM__FNCS[dag->index].op) {
					expr->u.index.source = argv[2];
					argv[2] = NULL;
				}
				fm__expr_free(argv[i]);
				argv[i] = NULL;
				--(*id);
			}
			else if ((FM_EXPR_OP_ARGSORT ==
				  FM__FNCS[dag->index].op) ||
				 (FM_EXPR_OP_TOPK ==
				  FM__FNCS[dag->index].op)) {
				for (i=1; i<argc; ++i) {
					if ((FM_EXPR_OP_SCALAR !=
					     argv[i]->op) ||
					    (argv[i]->id !=
					     ((*id) - (argc - 1 - i))) ||
					    (1 != argv[i]->refs)) {
						free_argv(argv, argc);
						fm__expr_free(expr);
						FM__TRACE(FM__ERRNO_SOFTWARE);
						return NULL;
					}
				}
				i = (FM_EXPR_OP_TOPK ==
				     FM__FNCS[dag->index].op) ? 2 : 1;
				axis = argv[i]->u.int64;
				if (0 > axis) {
					axis += (int64_t)
						argv[0]->geometry.ndim;
				}
				expr->u.sort.axis = axis;
				expr->u.sort.descending =
					argv[i + 1]->u.int64 != 0;
				if (2 == i) {
					expr->u.sort.k = argv[1]->u.int64;
					expr->u.sort.sorted =
						argv[4]->u.int64 != 0;
				}
				expr->right = argv[0];
				argv[0] = NULL;
				for (i=argc; i>1; --i) {
					fm__expr_free(argv[i - 1]);
					argv[i - 1] = NULL;
					--(*id);
				}
			}
			else if ((FM_EXPR_OP_CLAMP ==
				  FM__FNCS[dag->index].op) &&
				 (3 == argc)) {
				expr->left = argv[0];
				expr->right = argv[1];
				expr->u.expr = argv[2];
			}
			else if ((1 != argc) && (2 != argc) &&
				 !(fold && (3 == argc))) {
				free_argv(argv, argc);
				fm__expr_free(expr);
				FM__TRACE(FM__ERRNO_SOFTWARE);
				return NULL;
			}
			if (FM_EXPR_OP_VIEW == FM__FNCS[dag->index].op) {
				if (((1 != argc) && (2 != argc)) ||
				    ((2 == argc) &&
				     ((argv[1]->id != (*id)) ||
				      (1 != argv[1]->refs)))) {
					free_argv(argv, argc);
					fm__expr_free(expr);
					FM__TRACE(FM__ERRNO_SOFTWARE);
					return NULL;
				}
				expr->left = argv[0];
				argv[0] = NULL;
				if ((FM__FNC_SHAPE_FLATTEN ==
				     FM__FNCS[dag->index].shape) &&
				    !fm__geometry_flatten_stride(
					    &expr->left->geometry)) {
					if (!(copy = expr_new(id, dag))) {
						free_argv(argv, argc);
						fm__expr_free(expr);
						FM__TRACE(0);
						return NULL;
					}
					copy->op = FM_EXPR_OP_COPY;
					copy->dtype = expr->left->dtype;
					if (fm__geometry_unary(
						    &copy->geometry,
						    &expr->left->geometry,
						    dag)) {
						fm__expr_free(copy);
						free_argv(argv, argc);
						fm__expr_free(expr);
						FM__TRACE(0);
						return NULL;
					}
					copy->right = expr->left;
					expr->left = copy;
				}
				if (2 == argc) {
					fm__expr_free(argv[1]);
					argv[1] = NULL;
					--(*id);
				}
			}
			else if ((FM_EXPR_OP_FOLD ==
				  FM__FNCS[dag->index].op) ||
				 (FM_EXPR_OP_SCAN ==
				  FM__FNCS[dag->index].op)) {
				if (((2 != argc) && (3 != argc)) ||
				    (FM_EXPR_OP_SCALAR != argv[1]->op) ||
				    (FM_DTYPE_INT64 != argv[1]->dtype) ||
				    !(expr->u.fold.op =
				      fm__fnc_fold(dag->index)) ||
				    (argv[1]->id != (*id)) ||
				    (1 != argv[1]->refs)) {
					free_argv(argv, argc);
					fm__expr_free(expr);
					FM__TRACE(FM__ERRNO_SOFTWARE);
					return NULL;
				}
				axis = argv[1]->u.int64;
				if (0 > axis) {
					axis += (int64_t)
						argv[0]->geometry.ndim;
				}
				expr->u.fold.axis = axis;
				if (3 == argc) {
					expr->left = argv[2];
					argv[2] = NULL;
				}
				expr->right = argv[0];
				argv[0] = NULL;
				fm__expr_free(argv[1]);
				argv[1] = NULL;
				--(*id);
			}
			else if (1 == argc) {
				expr->right = argv[0];
			}
			else if (2 == argc) {
				expr->left = argv[0];
				expr->right = argv[1];
			}
			expr->op = FM__FNCS[dag->index].op;
		}
		else {
			free_argv(argv, argc);
			fm__expr_free(expr);
			FM__TRACE(FM__ERRNO_SOFTWARE);
			return NULL;
		}
		done = 1;
	}
	else if ((FM__DAG_OP_EXPR_CAST == dag->op) ||
		 (FM__DAG_OP_EXPR_POS == dag->op) ||
		 (FM__DAG_OP_EXPR_NEG == dag->op) ||
		 (FM__DAG_OP_EXPR_NOT == dag->op) ||
		 (FM__DAG_OP_EXPR_LOGNOT == dag->op)) {
		if (!(expr = expr_new(id, dag))) {
			FM__TRACE(0);
			return NULL;
		}
		if (!expr->dtype) {
			fm__expr_free(expr);
			FM__TRACE(FM__ERRNO_SOFTWARE);
			return NULL;
		}
		if (!(expr->right = walk(lang, dag->right, id, depth))) {
			fm__expr_free(expr);
			FM__TRACE(0);
			return NULL;
		}
		expr->geometry = dag->geometry;
		done = fm__dag_is_tensor(dag);
	}
	else if (FM__DAG_OP_EXPR_RESHAPE == dag->op) {
		if (!(expr = expr_new(id, dag))) {
			FM__TRACE(0);
			return NULL;
		}
		expr->op = FM_EXPR_OP_VIEW;
		if (!(expr->left = walk(lang, dag->left, id, depth))) {
			fm__expr_free(expr);
			FM__TRACE(0);
			return NULL;
		}
		if (1 != fm__geometry_flatten_stride(&expr->left->geometry)) {
			if (!(copy = expr_new(id, dag))) {
				fm__expr_free(expr);
				FM__TRACE(0);
				return NULL;
			}
			copy->op = FM_EXPR_OP_COPY;
			copy->dtype = expr->left->dtype;
			if (fm__geometry_unary(&copy->geometry,
					       &expr->left->geometry,
					       dag)) {
				fm__expr_free(copy);
				fm__expr_free(expr);
				FM__TRACE(0);
				return NULL;
			}
			copy->right = expr->left;
			expr->left = copy;
		}
		expr->geometry = dag->geometry;
	}
	else if ((FM__DAG_OP_EXPR_MUL == dag->op) ||
		 (FM__DAG_OP_EXPR_MOD == dag->op) ||
		 (FM__DAG_OP_EXPR_DIV == dag->op) ||
		 (FM__DAG_OP_EXPR_ADD == dag->op) ||
		 (FM__DAG_OP_EXPR_SUB == dag->op) ||
		 (FM__DAG_OP_EXPR_SHL == dag->op) ||
		 (FM__DAG_OP_EXPR_SHR == dag->op) ||
		 (FM__DAG_OP_EXPR_LT == dag->op) ||
		 (FM__DAG_OP_EXPR_GT == dag->op) ||
		 (FM__DAG_OP_EXPR_LE == dag->op) ||
		 (FM__DAG_OP_EXPR_GE == dag->op) ||
		 (FM__DAG_OP_EXPR_EQ == dag->op) ||
		 (FM__DAG_OP_EXPR_NE == dag->op) ||
		 (FM__DAG_OP_EXPR_AND == dag->op) ||
		 (FM__DAG_OP_EXPR_XOR == dag->op) ||
		 (FM__DAG_OP_EXPR_OR == dag->op) ||
		 (FM__DAG_OP_EXPR_LOGAND == dag->op) ||
		 (FM__DAG_OP_EXPR_LOGXOR == dag->op) ||
		 (FM__DAG_OP_EXPR_LOGOR == dag->op)) {
		if (!(expr = expr_new(id, dag))) {
			FM__TRACE(0);
			return NULL;
		}
		if (!expr->dtype) {
			fm__expr_free(expr);
			FM__TRACE(FM__ERRNO_SOFTWARE);
			return NULL;
		}
		if (!(expr->left = walk(lang, dag->left, id, depth)) ||
		    !(expr->right = walk(lang, dag->right, id, depth))) {
			fm__expr_free(expr);
			FM__TRACE(0);
			return NULL;
		}
		expr->geometry = dag->geometry;
		done = fm__dag_is_tensor(dag);
	}
	else if (FM__DAG_OP_EXPR_MATMUL == dag->op) {
		if (!(expr = expr_new(id, dag))) {
			FM__TRACE(0);
			return NULL;
		}
		if (!expr->dtype) {
			fm__expr_free(expr);
			FM__TRACE(FM__ERRNO_SOFTWARE);
			return NULL;
		}
		if (!(expr->left = walk(lang, dag->left, id, depth)) ||
		    !(expr->right = walk(lang, dag->right, id, depth))) {
			fm__expr_free(expr);
			FM__TRACE(0);
			return NULL;
		}
		if (matmul_expr(&expr, dag, id)) {
			fm__expr_free(expr);
			FM__TRACE(0);
			return NULL;
		}
		done = 1;
	}
	else if (FM__DAG_OP_EXPR_COND == dag->op) {
		if (!(expr = expr_new(id, dag))) {
			FM__TRACE(0);
			return NULL;
		}
		if (!expr->dtype) {
			fm__expr_free(expr);
			FM__TRACE(FM__ERRNO_SOFTWARE);
			return NULL;
		}
		if (!(expr->u.expr = walk(lang, dag->cond, id, depth)) ||
		    !(expr->left = walk(lang, dag->left, id, depth)) ||
		    !(expr->right = walk(lang, dag->right, id, depth))) {
			fm__expr_free(expr);
			FM__TRACE(0);
			return NULL;
		}
		expr->geometry = dag->geometry;
		done = fm__dag_is_tensor(dag);
	}
	else {
		FM__TRACE(FM__ERRNO_SOFTWARE);
		return NULL;
	}
	if (!done && /* geometry finalized */
	    (FM_EXPR_OP_TENSOR != expr->op) &&
	    (FM_EXPR_OP_VIEW != expr->op)) {
		fm__geometry_scalar(&expr->geometry);
	}
	return expr;
}

struct fm_expr *
fm__expr(fm_t lang, const struct fm__dag *dag, size_t *id)
{
	struct fm_expr *expr;

	assert( lang && dag && id );

	(*id) = 0;
	if (!(expr = walk(lang, dag, id, 0))) {
		(*id) = 0;
		FM__TRACE(0);
		return NULL;
	}
	return expr;
}

void
fm__expr_free(struct fm_expr *expr)
{
	if (expr) {
		assert( expr->refs );
		if (--expr->refs) {
			return;
		}
		fm__expr_free(expr->left);
		fm__expr_free(expr->right);
		if ((FM_EXPR_OP_COND == expr->op) ||
		    (FM_EXPR_OP_CLAMP == expr->op)) {
			fm__expr_free(expr->u.expr);
		}
		if (FM_EXPR_OP_SCATTER == expr->op) {
			fm__expr_free(expr->u.index.source);
		}
		free(expr);
	}
}
