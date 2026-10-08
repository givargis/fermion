/**
 * Copyright (c) 2025-2026 Tony Givargis
 * University of California, Irvine
 * fm_parser.c
 */

#include "fm_fnc.h"
#include "fm_geometry.h"
#include "fm_map.h"
#include "fm_utils.h"
#include "fm_parser.h"

#define ERR(p, m, e)					\
	do {						\
		if ((p)->stop) {			\
			FM__TRACE(0);			\
		}					\
		else {					\
			FM__UTILS_ERR_PARSER((p), (m)); \
			FM__TRACE(e);			\
		}					\
	}						\
	while (0)

#define MKD(p, d, o)						\
	do {							\
		(d) = fm__dag_pool_allocate((p)->pool);		\
		if (!(d)) {					\
			ERR(p, "out of memory", 0);		\
			return NULL;				\
		}						\
		(d)->op = (o);					\
		if ((FM__DAG_OP_DECL_TENSOR == (d)->op) ||	\
		    (FM__DAG_OP_DECL_COMPUTE == (d)->op)) {	\
			(d)->u.s = NULL;			\
		}						\
		(d)->lineno = next(p)->lineno;			\
		(d)->column = next(p)->column;			\
	}							\
	while (0)

struct fm__parser {
	int stop;
	char *errstr; /* borrowed */
	size_t i, n;
	size_t depth;
	uint64_t seed;
	fm__map_t map;
	fm__lexer_t lexer;
	fm__dag_pool_t pool;
	struct fm__dag *dag;
};

static struct fm__dag SEED_DAG = { 0 };

static enum fm__dag_type
set_type(struct fm__dag *dag, enum fm__dag_type type)
{
	dag->type = type;
	if (FM__DAG_TYPE_INT == type) {
		dag->u.i = NULL;
	}
	else if ((FM__DAG_TYPE_STRING == type) ||
		 (FM__DAG_OP_DECL_TENSOR == dag->op) ||
		 (FM__DAG_OP_DECL_COMPUTE == dag->op) ||
		 ((FM__DAG_OP_EXPR_IDENTIFIER == dag->op) &&
		  fm__dag_is_tensor(dag))) {
		dag->u.s = NULL;
	}
	return type;
}

static const struct fm__lexer_token *
next(const struct fm__parser *parser)
{
	if (!parser->stop && (parser->i < parser->n)) {
		return fm__lexer_lookup(parser->lexer, parser->i);
	}
	return fm__lexer_lookup(parser->lexer, parser->n - 1);
}

static int /* int */
match(const struct fm__parser *parser, enum fm__lexer_op op)
{
	return op == next(parser)->op;
}

static void
forward(struct fm__parser *parser)
{
	if (parser->i < parser->n) {
		++parser->i;
	}
}

static void
reverse(struct fm__parser *parser)
{
	if (parser->i) {
		--parser->i;
	}
}

static int
push(struct fm__parser *parser)
{
	if (FM__DAG_MAX_DEPTH <= parser->depth) {
		ERR(parser,
		    "maximum expression depth exceeded",
		    FM__ERRNO_SYNTAX);
		return -1;
	}
	++parser->depth;
	return 0;
}

static void
pop(struct fm__parser *parser)
{
	--parser->depth;
}

static int /* bool */
is_int_expr_list(const struct fm__dag *dag)
{
	while (dag &&
	       (FM__DAG_OP_EXPR_LIST == dag->op) &&
	       (FM__DAG_TYPE_INT == dag->left->type)) {
		dag = dag->right;
	}
	return NULL == dag;
}

static int
eval_expr_list(struct fm__dag *dag)
{
	while (dag && (FM__DAG_OP_EXPR_LIST == dag->op)) {
		if (fm__dag_eval_expr(dag->left)) {
			FM__TRACE(0);
			return -1;
		}
		dag = dag->right;
	}
	return 0;
}

static int
fold_compute_constants(struct fm__parser *parser,
		       struct fm__dag *dag,
		       size_t depth)
{
	enum fm__dag_op op;

	if (!dag) {
		return 0;
	}
	if (FM__DAG_MAX_DEPTH <= depth) {
		ERR(parser,
		    "maximum expression depth exceeded",
		    FM__ERRNO_SYNTAX);
		return -1;
	}
	op = FM__DAG_OP_UNKNOWN;
	if (fm__dag_is_int(dag)) {
		op = FM__DAG_OP_EXPR_LINT;
	}
	else if (fm__dag_is_real(dag)) {
		op = FM__DAG_OP_EXPR_LREAL;
	}
	else if (fm__dag_is_bool(dag)) {
		op = FM__DAG_OP_EXPR_LBOOL;
	}
	if (FM__DAG_OP_UNKNOWN != op) {
		if (fm__dag_eval_expr(dag)) {
			ERR(parser,
			    (FM__ERRNO_MEMORY == errno)
			    ? "out of memory"
			    : "expression evaluation failed",
			    0);
			return -1;
		}
		dag->op = op;
		dag->cond = NULL;
		dag->left = NULL;
		dag->right = NULL;
		return 0;
	}
	if (fold_compute_constants(parser, dag->cond, depth + 1) ||
	    fold_compute_constants(parser, dag->left, depth + 1) ||
	    fold_compute_constants(parser, dag->right, depth + 1)) {
		FM__TRACE(0);
		return -1;
	}
	return 0;
}

static int
reshape_geometry(struct fm__parser *parser, struct fm__dag *dag)
{
	struct fm_geometry geometry;

	if (!fm__geometry_is_populated(&dag->left->geometry)) {
		ERR(parser,
		    "missing tensor operand geometry",
		    FM__ERRNO_SYNTAX);
		return -1;
	}
	if (!is_int_expr_list(dag->right)) {
		ERR(parser,
		    "reshape dimensions must "
		    "contain only integer expressions",
		    FM__ERRNO_SYNTAX);
		return -1;
	}
	if (eval_expr_list(dag->right) ||
	    fm__geometry_reshape(&geometry, &dag->left->geometry, dag)) {
		ERR(parser,
		    (FM__ERRNO_MEMORY == errno)
		    ? "out of memory"
		    : "invalid tensor geometry",
		    0);
		return -1;
	}
	dag->geometry = geometry;
	return 0;
}

static int
broadcast_geometry(struct fm__parser *parser, struct fm__dag *dag)
{
	struct fm_geometry geometry;

	if (!fm__geometry_is_populated(&dag->left->geometry)) {
		ERR(parser,
		    "missing tensor operand geometry",
		    FM__ERRNO_SYNTAX);
		return -1;
	}
	if (!is_int_expr_list(dag->right)) {
		ERR(parser,
		    "broadcast dimensions must contain "
		    "only integer expressions",
		    FM__ERRNO_SYNTAX);
		return -1;
	}
	if (eval_expr_list(dag->right) ||
	    fm__geometry_broadcast(&geometry,
				   &dag->left->geometry,
				   dag)) {
		ERR(parser,
		    (FM__ERRNO_MEMORY == errno)
		    ? "out of memory"
		    : "invalid tensor geometry",
		    0);
		return -1;
	}
	dag->geometry = geometry;
	return 0;
}

static int
unary_geometry(struct fm__parser *parser, struct fm__dag *dag)
{
	struct fm_geometry geometry;

	if (!fm__dag_is_tensor(dag)) {
		return 0;
	}
	if (fm__dag_is_tensor(dag->right) &&
	    !fm__geometry_is_populated(&dag->right->geometry)) {
		ERR(parser,
		    "missing tensor operand geometry",
		    FM__ERRNO_SYNTAX);
		return -1;
	}
	if (fm__geometry_unary(&geometry, &dag->right->geometry, dag)) {
		ERR(parser,
		    (FM__ERRNO_MEMORY == errno)
		    ? "out of memory"
		    : "invalid tensor geometry",
		    0);
		return -1;
	}
	dag->geometry = geometry;
	return 0;
}

static int
binary_geometry(struct fm__parser *parser, struct fm__dag *dag)
{
	struct fm_geometry geometry;

	if (!fm__dag_is_tensor(dag)) {
		return 0;
	}
	if ((fm__dag_is_tensor(dag->left) &&
	     !fm__geometry_is_populated(&dag->left->geometry)) ||
	    (fm__dag_is_tensor(dag->right) &&
	     !fm__geometry_is_populated(&dag->right->geometry))) {
		ERR(parser,
		    "missing tensor operand geometry",
		    FM__ERRNO_SYNTAX);
		return -1;
	}
	if (fm__geometry_binary(&geometry,
				&dag->left->geometry,
				&dag->right->geometry,
				dag)) {
		ERR(parser,
		    (FM__ERRNO_MEMORY == errno)
		    ? "out of memory"
		    : "invalid tensor geometry",
		    0);
		return -1;
	}
	dag->geometry = geometry;
	return 0;
}

static int
matmul_geometry(struct fm__parser *parser, struct fm__dag *dag)
{
	struct fm_geometry geometry;

	if (!fm__geometry_is_populated(&dag->left->geometry) ||
	    !fm__geometry_is_populated(&dag->right->geometry)) {
		ERR(parser,
		    "missing tensor operand geometry",
		    FM__ERRNO_SYNTAX);
		return -1;
	}
	if (fm__geometry_matmul(&geometry,
				&dag->left->geometry,
				&dag->right->geometry,
				dag)) {
		ERR(parser,
		    (FM__ERRNO_MEMORY == errno)
		    ? "out of memory"
		    : "invalid tensor geometry",
		    0);
		return -1;
	}
	dag->geometry = geometry;
	return 0;
}

static int
cond_geometry(struct fm__parser *parser, struct fm__dag *dag)
{
	struct fm_geometry geometry;

	if (!fm__dag_is_tensor(dag)) {
		return 0;
	}
	if ((fm__dag_is_tensor(dag->cond) &&
	     !fm__geometry_is_populated(&dag->cond->geometry)) ||
	    (fm__dag_is_tensor(dag->left) &&
	     !fm__geometry_is_populated(&dag->left->geometry)) ||
	    (fm__dag_is_tensor(dag->right) &&
	     !fm__geometry_is_populated(&dag->right->geometry))) {
		ERR(parser,
		    "missing tensor operand geometry",
		    FM__ERRNO_SYNTAX);
		return -1;
	}
	if (fm__geometry_cond(&geometry,
			      &dag->cond->geometry,
			      &dag->left->geometry,
			      &dag->right->geometry,
			      dag)) {
		ERR(parser,
		    (FM__ERRNO_MEMORY == errno)
		    ? "out of memory"
		    : "invalid tensor geometry",
		    0);
		return -1;
	}
	dag->geometry = geometry;
	return 0;
}

static int
fnc_geometry(struct fm__parser *parser, struct fm__dag *dag)
{
	const struct fm_geometry *argv[FM__FNC_MAX_ARGS];
	struct fm_geometry geometry;
	const struct fm__dag *arg;
	size_t argc;

	argc = 0;
	for (arg=dag->right; arg; arg=arg->right) {
		if ((FM__FNC_MAX_ARGS <= argc) || !arg->left ||
		    (FM__DAG_OP_EXPR_LIST != arg->op)) {
			ERR(parser,
			    "invalid function argument list",
			    FM__ERRNO_SYNTAX);
			return -1;
		}
		if (fm__dag_is_tensor(arg->left) &&
		    !fm__geometry_is_populated(&arg->left->geometry)) {
			ERR(parser,
			    "missing tensor operand geometry",
			    FM__ERRNO_SYNTAX);
			return -1;
		}
		argv[argc++] = &arg->left->geometry;
	}
	if (fm__geometry_fnc(&geometry, argv, argc, dag)) {
		ERR(parser,
		    (FM__ERRNO_MEMORY == errno)
		    ? "out of memory"
		    : "invalid tensor geometry",
		    0);
		return -1;
	}
	dag->geometry = geometry;
	return 0;
}

static int
validate_geometry(struct fm__parser *parser,
		  const struct fm__dag *dag,
		  size_t depth)
{
	if (!dag) {
		return 0;
	}
	if ((FM__DAG_OP_EXPR_LIST == dag->op) ||
	    (FM__DAG_OP_DECL_LIST == dag->op)) {
		do {
			if (validate_geometry(parser, dag->left, depth)) {
				FM__TRACE(0);
				return -1;
			}
			dag = dag->right;
		}
		while (dag);
		return 0;
	}
	if ((FM__DAG_OP_DECL_LET != dag->op) &&
	    (FM__DAG_OP_DECL_TENSOR != dag->op) &&
	    (FM__DAG_OP_DECL_COMPUTE != dag->op) &&
	    (FM__DAG_OP_DECL_GEOMETRY != dag->op) &&
	    (FM__DAG_OP_DECL_INIT_ZEROS != dag->op) &&
	    (FM__DAG_OP_DECL_INIT_ONES != dag->op) &&
	    (FM__DAG_OP_DECL_INIT_UNIFORM != dag->op) &&
	    (FM__DAG_OP_DECL_INIT_NORMAL != dag->op) &&
	    (FM__DAG_OP_DECL_INIT_IOTA != dag->op)) {
		if (FM__DAG_MAX_DEPTH <= depth) {
			ERR(parser,
			    "maximum expression depth exceeded",
			    FM__ERRNO_SYNTAX);
			return -1;
		}
		++depth;
	}
	if (validate_geometry(parser, dag->left, depth) ||
	    validate_geometry(parser, dag->right, depth) ||
	    validate_geometry(parser, dag->cond, depth)) {
		FM__TRACE(0);
		return -1;
	}
	if (fm__dag_is_tensor(dag) &&
	    !fm__geometry_is_populated(&dag->geometry)) {
		ERR(parser, "missing tensor geometry", FM__ERRNO_SYNTAX);
		return -1;
	}
	return 0;
}

/**----------------------------------------------------------------------------
 * (E0)  expr_fnc
 * (E1)  expr_primary
 * (E2)  expr_slice
 * (E3)  expr_slices
 * (E4)  expr_view
 * (E5)  expr_unary
 * (E6)  expr_multiplicative
 * (E7)  expr_additive
 * (E8)  expr_shift
 * (E9)  expr_relational
 * (E10) expr_equality
 * (E11) expr_and
 * (E12) expr_xor
 * (E13) expr_or
 * (E14) expr_logic_and
 * (E15) expr_logic_xor
 * (E16) expr_logic_or
 * (E17) expr
 * (E18) expr_list
 *---------------------------------------------------------------------------*/

/**
 * (E0)
 *
 * expr_fnc: ...
 */

static size_t /* index */
expr_fnc(const char *s)
{
	for (size_t i=1; i<FM__FNCS_SIZE; ++i) {
		if (!strcmp(FM__FNCS[i].name, s)) {
			return i;
		}
	}
	return 0;
}

/**
 * (E1)
 *
 * expr_primary: LINT
 *             | LREAL
 *             | LBOOL
 *             | LSTRING
 *             | IDENTIFIER
 *             | IDENTIFIER '(' expr_list ')'
 *             | 'cast' '<' decl_dtype '>' '(' expr ')'
 *             | 'reshape' '(' expr ',' '[' expr_list? ']' ')'
 *             | 'broadcast' '(' expr ',' '[' expr_list? ']' ')'
 *             | '(' expr ')'
 */

static struct fm__dag *expr(struct fm__parser *parser);
static struct fm__dag *expr_list(struct fm__parser *parser);
static enum fm__dag_type decl_dtype(struct fm__parser *parser);

static struct fm__dag *
expr_primary(struct fm__parser *parser)
{
	struct fm__dag *dag, *decl, *arg;
	const char *s;
	size_t i;
	int64_t value;

	dag = NULL;
	if (match(parser, FM__LEXER_OP_LINT)) {
		MKD(parser, dag, FM__DAG_OP_EXPR_LINT);
		set_type(dag, FM__DAG_TYPE_INT);
		if (!(dag->u.i = fm__bigint_clone(next(parser)->u.i))) {
			ERR(parser, "out of memory", 0);
			return NULL;
		}
		forward(parser);
	}
	else if (match(parser, FM__LEXER_OP_LREAL)) {
		MKD(parser, dag, FM__DAG_OP_EXPR_LREAL);
		set_type(dag, FM__DAG_TYPE_REAL);
		dag->u.d = next(parser)->u.d;
		forward(parser);
	}
	else if (match(parser, FM__LEXER_OP_LBOOL)) {
		MKD(parser, dag, FM__DAG_OP_EXPR_LBOOL);
		set_type(dag, FM__DAG_TYPE_BOOL);
		dag->u.b = next(parser)->u.b;
		forward(parser);
	}
	else if (match(parser, FM__LEXER_OP_LSTRING)) {
		MKD(parser, dag, FM__DAG_OP_EXPR_LSTRING);
		set_type(dag, FM__DAG_TYPE_STRING);
		if (!(dag->u.s = fm__strdup(next(parser)->u.s))) {
			ERR(parser, "out of memory", 0);
			return NULL;
		}
		forward(parser);
	}
	else if (match(parser, FM__LEXER_OP_IDENTIFIER)) {
		s = next(parser)->u.s;
		if ((i = expr_fnc(s))) {
			MKD(parser, dag, FM__DAG_OP_EXPR_FNC);
			forward(parser);
			if (!match(parser, FM__LEXER_OP_OPENPAREN)) {
				ERR(parser, "missing '('", FM__ERRNO_SYNTAX);
				return NULL;
			}
			forward(parser);
			dag->index = i;
			dag->right = expr_list(parser);
			if (!(set_type(dag, fm__dag_type(dag)))) {
				ERR(parser,
				    "invalid arguments to function",
				    FM__ERRNO_SYNTAX);
				return NULL;
			}
			if (!match(parser, FM__LEXER_OP_CLOSEPAREN)) {
				ERR(parser, "missing ')'", FM__ERRNO_SYNTAX);
				return NULL;
			}
			forward(parser);
			for (arg=dag->right, i=1; arg; arg=arg->right, ++i) {
				if (FM__FNC_TYPE_INT_SCALAR ==
				    FM__FNCS[dag->index].types[i]) {
					if (fm__dag_eval_expr(arg->left)) {
						ERR(parser,
						    (FM__ERRNO_MEMORY ==
						     errno)
						    ? "out of memory"
						    : ("expression evaluation "
						       "failed"),
						    0);
						return NULL;
					}
					arg->left->op = FM__DAG_OP_EXPR_LINT;
				}
			}
			if (fm__dag_is_int(dag)) {
				if (fm__fnc_static(dag, &value)) {
					ERR(parser,
					    (FM__ERRNO_MEMORY == errno)
					    ? "out of memory"
					    : "expression evaluation failed",
					    0);
					return NULL;
				}
				if (!(dag->u.i = fm__bigint_int(value))) {
					ERR(parser, "out of memory", 0);
					return NULL;
				}
				dag->op = FM__DAG_OP_EXPR_LINT;
				dag->index = 0;
				dag->right = NULL;
			}
			else if (fnc_geometry(parser, dag)) {
				FM__TRACE(0);
				return NULL;
			}
		}
		else {
			if (!(decl = fm__map_lookup(parser->map, s))) {
				ERR(parser,
				    "undeclared tensor",
				    FM__ERRNO_SYNTAX);
				return NULL;
			}
			MKD(parser, dag, FM__DAG_OP_EXPR_IDENTIFIER);
			forward(parser);
			if ((FM__DAG_OP_DECL_TENSOR == decl->op) ||
			    (FM__DAG_OP_DECL_COMPUTE == decl->op)) {
				set_type(dag, decl->type);
				if (!fm__geometry_is_populated(
					    &decl->geometry)) {
					ERR(parser,
					    "missing tensor "
					    "declaration geometry",
					    FM__ERRNO_SYNTAX);
					return NULL;
				}
				dag->geometry = decl->geometry;
				if (!(dag->u.s = fm__strdup(s))) {
					ERR(parser, "out of memory", 0);
					return NULL;
				}
			}
			else {
				decl = decl->right; /* let -> expr */
				set_type(dag, decl->type);
				if (FM__DAG_TYPE_INT == dag->type) {
					dag->op = FM__DAG_OP_EXPR_LINT;
					dag->u.i = fm__bigint_clone(decl->u.i);
					if (!dag->u.i) {
						ERR(parser,
						    "out of memory",
						    0);
						return NULL;
					}
				}
				else if (FM__DAG_TYPE_REAL == dag->type) {
					dag->op = FM__DAG_OP_EXPR_LREAL;
					dag->u.d = decl->u.d;
				}
				else if (FM__DAG_TYPE_BOOL == dag->type) {
					dag->op = FM__DAG_OP_EXPR_LBOOL;
					dag->u.b = decl->u.b;
				}
				else {
					assert( FM__DAG_TYPE_STRING ==
						dag->type );
					dag->op = FM__DAG_OP_EXPR_LSTRING;
					dag->u.s = fm__strdup(decl->u.s);
					if (!dag->u.s) {
						ERR(parser,
						    "out of memory",
						    0);
						return NULL;
					}
				}
			}
		}
	}
	else if (match(parser, FM__LEXER_OP_CAST)) {
		MKD(parser, dag, FM__DAG_OP_EXPR_CAST);
		forward(parser);
		if (!match(parser, FM__LEXER_OP_LT)) {
			ERR(parser,
			    "missing '<' after 'cast'",
			    FM__ERRNO_SYNTAX);
			return NULL;
		}
		forward(parser);
		if (!(set_type(dag, decl_dtype(parser)))) {
			ERR(parser,
			    "missing dtype in cast operator",
			    FM__ERRNO_SYNTAX);
			return NULL;
		}
		if (!match(parser, FM__LEXER_OP_GT)) {
			ERR(parser,
			    "missing '>' after cast dtype",
			    FM__ERRNO_SYNTAX);
			return NULL;
		}
		forward(parser);
		if (!match(parser, FM__LEXER_OP_OPENPAREN)) {
			ERR(parser,
			    "missing '(' after cast dtype",
			    FM__ERRNO_SYNTAX);
			return NULL;
		}
		forward(parser);
		if (!(dag->right = expr(parser))) {
			ERR(parser,
			    "missing expression in cast operator",
			    FM__ERRNO_SYNTAX);
			return NULL;
		}
		if (!match(parser, FM__LEXER_OP_CLOSEPAREN)) {
			ERR(parser,
			    "missing ')' after cast expression",
			    FM__ERRNO_SYNTAX);
			return NULL;
		}
		forward(parser);
		if (!fm__dag_type(dag)) {
			ERR(parser,
			    "cast operand must be a numeric scalar or tensor",
			    FM__ERRNO_SYNTAX);
			return NULL;
		}
		if (unary_geometry(parser, dag)) {
			FM__TRACE(0);
			return NULL;
		}
	}
	else if (match(parser, FM__LEXER_OP_RESHAPE)) {
		MKD(parser, dag, FM__DAG_OP_EXPR_RESHAPE);
		forward(parser);
		if (!match(parser, FM__LEXER_OP_OPENPAREN)) {
			ERR(parser,
			    "missing '(' after reshape operator",
			    FM__ERRNO_SYNTAX);
			return NULL;
		}
		forward(parser);
		if (!(dag->left = expr(parser))) {
			ERR(parser,
			    "missing expression in reshape operator",
			    FM__ERRNO_SYNTAX);
			return NULL;
		}
		if (!match(parser, FM__LEXER_OP_COMMA)) {
			ERR(parser,
			    "missing ',' after reshape expression",
			    FM__ERRNO_SYNTAX);
			return NULL;
		}
		forward(parser);
		if (!match(parser, FM__LEXER_OP_OPENBRACKET)) {
			ERR(parser,
			    "missing '[' in reshape operator",
			    FM__ERRNO_SYNTAX);
			return NULL;
		}
		forward(parser);
		dag->right = expr_list(parser);
		if (!match(parser, FM__LEXER_OP_CLOSEBRACKET)) {
			ERR(parser,
			    "missing ']' in reshape operator",
			    FM__ERRNO_SYNTAX);
			return NULL;
		}
		forward(parser);
		if (!match(parser, FM__LEXER_OP_CLOSEPAREN)) {
			ERR(parser,
			    "missing ')' in reshape operator",
			    FM__ERRNO_SYNTAX);
			return NULL;
		}
		forward(parser);
		if (!(set_type(dag, fm__dag_type(dag)))) {
			ERR(parser,
			    "reshape operand must be a tensor",
			    FM__ERRNO_SYNTAX);
			return NULL;
		}
		if (reshape_geometry(parser, dag)) {
			FM__TRACE(0);
			return NULL;
		}
	}
	else if (match(parser, FM__LEXER_OP_BROADCAST)) {
		MKD(parser, dag, FM__DAG_OP_EXPR_BROADCAST);
		forward(parser);
		if (!match(parser, FM__LEXER_OP_OPENPAREN)) {
			ERR(parser,
			    "missing '(' after broadcast operator",
			    FM__ERRNO_SYNTAX);
			return NULL;
		}
		forward(parser);
		if (!(dag->left = expr(parser))) {
			ERR(parser,
			    "missing expression in broadcast operator",
			    FM__ERRNO_SYNTAX);
			return NULL;
		}
		if (!match(parser, FM__LEXER_OP_COMMA)) {
			ERR(parser,
			    "missing ',' after broadcast expression",
			    FM__ERRNO_SYNTAX);
			return NULL;
		}
		forward(parser);
		if (!match(parser, FM__LEXER_OP_OPENBRACKET)) {
			ERR(parser,
			    "missing '[' in broadcast operator",
			    FM__ERRNO_SYNTAX);
			return NULL;
		}
		forward(parser);
		dag->right = expr_list(parser);
		if (!match(parser, FM__LEXER_OP_CLOSEBRACKET)) {
			ERR(parser,
			    "missing ']' in broadcast operator",
			    FM__ERRNO_SYNTAX);
			return NULL;
		}
		forward(parser);
		if (!match(parser, FM__LEXER_OP_CLOSEPAREN)) {
			ERR(parser,
			    "missing ')' in broadcast operator",
			    FM__ERRNO_SYNTAX);
			return NULL;
		}
		forward(parser);
		if (!(set_type(dag, fm__dag_type(dag)))) {
			ERR(parser,
			    "broadcast operand must be a tensor",
			    FM__ERRNO_SYNTAX);
			return NULL;
		}
		if (broadcast_geometry(parser, dag)) {
			FM__TRACE(0);
			return NULL;
		}
	}
	else if (match(parser, FM__LEXER_OP_OPENPAREN)) {
		forward(parser);
		if (!(dag = expr(parser))) {
			ERR(parser,
			    "invalid expression after '('",
			    FM__ERRNO_SYNTAX);
			return NULL;
		}
		if (!match(parser, FM__LEXER_OP_CLOSEPAREN)) {
			ERR(parser, "missing ')'", FM__ERRNO_SYNTAX);
			return NULL;
		}
		forward(parser);
	}
	return dag;
}

/**
 * (E2)
 *
 * expr_slice: expr
 *           | expr? ':' expr? ( ':' expr? )?
 */

static struct fm__dag *
expr_slice(struct fm__parser *parser)
{
	struct fm__dag *dag;

	MKD(parser, dag, FM__DAG_OP_EXPR_SLICE);
	dag->cond = expr(parser);
	if (match(parser, FM__LEXER_OP_COLON)) {
		dag->slice = FM__DAG_SLICE_RANGE;
		forward(parser);
		dag->left = expr(parser);
		if (match(parser, FM__LEXER_OP_COLON)) {
			dag->slice = FM__DAG_SLICE_STEPPED_RANGE;
			forward(parser);
			dag->right = expr(parser);
		}
	}
	if (parser->stop) {
		FM__TRACE(0);
		return NULL;
	}
	if (dag->cond && (FM__DAG_TYPE_INT != dag->cond->type)) {
		if (FM__DAG_SLICE_SCALAR != dag->slice) {
			ERR(parser,
			    "tensor slice start must "
			    "be an integer expression",
			    FM__ERRNO_SYNTAX);
		}
		else {
			ERR(parser,
			    "tensor index must be an integer expression",
			    FM__ERRNO_SYNTAX);
		}
		return NULL;
	}
	if (dag->left && (FM__DAG_TYPE_INT != dag->left->type)) {
		ERR(parser,
		    "tensor slice stop must be an integer expression",
		    FM__ERRNO_SYNTAX);
		return NULL;
	}
	if (dag->right && (FM__DAG_TYPE_INT != dag->right->type)) {
		ERR(parser,
		    "tensor slice step must be an integer expression",
		    FM__ERRNO_SYNTAX);
		return NULL;
	}
	if ((dag->cond && fm__dag_eval_expr(dag->cond)) ||
	    (dag->left && fm__dag_eval_expr(dag->left)) ||
	    (dag->right && fm__dag_eval_expr(dag->right))) {
		ERR(parser,
		    (FM__ERRNO_MEMORY == errno)
		    ? "out of memory"
		    : "invalid tensor geometry",
		    0);
		return NULL;
	}
	return ((dag->cond ||
		 (FM__DAG_SLICE_SCALAR != dag->slice)) ? dag : NULL);
}

/**
 * (E3)
 *
 * expr_slices: expr_slice
 *            | expr_slices ',' expr_slice
 */

static struct fm__dag *
expr_slices(struct fm__parser *parser)
{
	struct fm__dag *dag, *dag_, *head, *tail;
	int separator;

	separator = 0;
	head = tail = NULL;
	while ((dag = expr_slice(parser))) {
		MKD(parser, dag_, FM__DAG_OP_EXPR_SLICES);
		dag_->left = dag;
		if (tail) {
			tail->right = dag_;
		}
		else {
			head = dag_;
		}
		tail = dag_;
		if ((head != tail) && !separator) {
			ERR(parser,
			    "missing ',' in tensor slice list",
			    FM__ERRNO_SYNTAX);
			return NULL;
		}
		if ((separator = match(parser, FM__LEXER_OP_COMMA))) {
			forward(parser);
		}
	}
	if (head && separator) {
		ERR(parser,
		    "dangling ',' after tensor slice list",
		    FM__ERRNO_SYNTAX);
		return NULL;
	}
	return head;
}

/**
 * (E4)
 *
 * expr_view: expr_view '[' expr_slices ']'
 *          | expr_view '{' expr_list '}'
 *          | expr_view '`'
 *          | expr_primary
 */

static struct fm__dag *
expr_view_(struct fm__parser *parser, struct fm__dag *dag)
{
	struct fm__dag *prev;

	for (;;) {
		prev = dag;
		if (match(parser, FM__LEXER_OP_OPENBRACKET)) {
			MKD(parser, dag, FM__DAG_OP_EXPR_VIEW);
			dag->left = prev;
			forward(parser);
			if (!(dag->right = expr_slices(parser))) {
				ERR(parser,
				    "missing tensor slice specification",
				    FM__ERRNO_SYNTAX);
				return NULL;
			}
			if (!match(parser, FM__LEXER_OP_CLOSEBRACKET)) {
				ERR(parser,
				    "missing ']' after tensor slice list",
				    FM__ERRNO_SYNTAX);
				return NULL;
			}
			forward(parser);
			if (!(set_type(dag, fm__dag_type(dag)))) {
				ERR(parser,
				    "invalid tensor view expression",
				    FM__ERRNO_SYNTAX);
				return NULL;
			}
		}
		else if (match(parser, FM__LEXER_OP_OPENBRACE)) {
			MKD(parser, dag, FM__DAG_OP_EXPR_VIEW);
			dag->left = prev;
			forward(parser);
			if (!(dag->cond = expr_list(parser))) {
				ERR(parser,
				    "missing tensor permute specification",
				    FM__ERRNO_SYNTAX);
				return NULL;
			}
			if (!match(parser, FM__LEXER_OP_CLOSEBRACE)) {
				ERR(parser,
				    "missing '}' after tensor permute list",
				    FM__ERRNO_SYNTAX);
				return NULL;
			}
			forward(parser);
			if (!(set_type(dag, fm__dag_type(dag)))) {
				ERR(parser,
				    "invalid tensor view expression",
				    FM__ERRNO_SYNTAX);
				return NULL;
			}
		}
		else if (match(parser, FM__LEXER_OP_BACKTICK)) {
			MKD(parser, dag, FM__DAG_OP_EXPR_VIEW);
			dag->left = prev;
			forward(parser);
			if (!(set_type(dag, fm__dag_type(dag)))) {
				ERR(parser,
				    "invalid tensor view expression",
				    FM__ERRNO_SYNTAX);
				return NULL;
			}
		}
		else {
			break;
		}
		if (!fm__geometry_is_populated(&prev->geometry)) {
			ERR(parser,
			    "missing tensor operand geometry",
			    FM__ERRNO_SYNTAX);
			return NULL;
		}
		if (fold_compute_constants(parser, dag->right, 0) ||
		    fold_compute_constants(parser, dag->cond, 0)) {
			return NULL;
		}
		dag->geometry = prev->geometry;
		if (fm__geometry_view(&dag->geometry, dag)) {
			ERR(parser,
			    (FM__ERRNO_MEMORY == errno)
			    ? "out of memory"
			    : "expression evaluation failed",
			    0);
			return NULL;
		}
	}
	return dag;
}

static struct fm__dag *
expr_view(struct fm__parser *parser)
{
	struct fm__dag *dag;

	if (!(dag = expr_primary(parser))) {
		return NULL;
	}
	return expr_view_(parser, dag);
}

/**
 * (E5)
 *
 * expr_unary: '+' expr_unary
 *           | '-' expr_unary
 *           | '~' expr_unary
 *           | '!' expr_unary
 *           | expr_view
 */

static struct fm__dag *expr_unary(struct fm__parser *parser);

static struct fm__dag *
expr_unary_(struct fm__parser *parser)
{
	struct fm__dag *dag;

	dag = NULL;
	if (match(parser, FM__LEXER_OP_ADD)) {
		MKD(parser, dag, FM__DAG_OP_EXPR_POS);
		forward(parser);
		if (!(dag->right = expr_unary(parser))) {
			ERR(parser,
			    "missing operand for unary '+'",
			    FM__ERRNO_SYNTAX);
			return NULL;
		}
		if (!(set_type(dag, fm__dag_type(dag)))) {
			ERR(parser,
			    "invalid unary '+' operand type",
			    FM__ERRNO_SYNTAX);
			return NULL;
		}
	}
	else if (match(parser, FM__LEXER_OP_SUB)) {
		MKD(parser, dag, FM__DAG_OP_EXPR_NEG);
		forward(parser);
		if (!(dag->right = expr_unary(parser))) {
			ERR(parser,
			    "missing operand for unary '-'",
			    FM__ERRNO_SYNTAX);
			return NULL;
		}
		if (!(set_type(dag, fm__dag_type(dag)))) {
			ERR(parser,
			    "invalid unary '-' operand type",
			    FM__ERRNO_SYNTAX);
			return NULL;
		}
	}
	else if (match(parser, FM__LEXER_OP_NOT)) {
		MKD(parser, dag, FM__DAG_OP_EXPR_NOT);
		forward(parser);
		if (!(dag->right = expr_unary(parser))) {
			ERR(parser,
			    "missing operand for unary '~'",
			    FM__ERRNO_SYNTAX);
			return NULL;
		}
		if (!(set_type(dag, fm__dag_type(dag)))) {
			ERR(parser,
			    "invalid unary '~' operand type",
			    FM__ERRNO_SYNTAX);
			return NULL;
		}
	}
	else if (match(parser, FM__LEXER_OP_LOGNOT)) {
		MKD(parser, dag, FM__DAG_OP_EXPR_LOGNOT);
		forward(parser);
		if (!(dag->right = expr_unary(parser))) {
			ERR(parser,
			    "missing operand for unary '!'",
			    FM__ERRNO_SYNTAX);
			return NULL;
		}
		if (!(set_type(dag, fm__dag_type(dag)))) {
			ERR(parser,
			    "invalid unary '!' operand type",
			    FM__ERRNO_SYNTAX);
			return NULL;
		}
	}
	else {
		return expr_view(parser);
	}
	if (unary_geometry(parser, dag)) {
		FM__TRACE(0);
		return NULL;
	}
	return dag;
}

static struct fm__dag *
expr_unary(struct fm__parser *parser)
{
	struct fm__dag *dag;

	if (push(parser)) {
		FM__TRACE(0);
		return NULL;
	}
	dag = expr_unary_(parser);
	pop(parser);
	return dag;
}

/**
 * (E6)
 *
 * expr_multiplicative: expr_multiplicative '*' expr_unary
 *                    | expr_multiplicative '@' expr_unary
 *                    | expr_multiplicative '/' expr_unary
 *                    | expr_multiplicative '%' expr_unary
 *                    | expr_unary
 */

static struct fm__dag *
expr_multiplicative_(struct fm__parser *parser, struct fm__dag *dag)
{
	struct fm__dag *prev;

	for (;;) {
		prev = dag;
		if (match(parser, FM__LEXER_OP_MUL)) {
			MKD(parser, dag, FM__DAG_OP_EXPR_MUL);
			dag->left = prev;
			forward(parser);
			if (!(dag->right = expr_unary(parser))) {
				ERR(parser,
				    "missing right operand for '*'",
				    FM__ERRNO_SYNTAX);
				return NULL;
			}
			if (!(set_type(dag, fm__dag_type(dag)))) {
				ERR(parser,
				    "invalid operand types for '*'",
				    FM__ERRNO_SYNTAX);
				return NULL;
			}
		}
		else if (match(parser, FM__LEXER_OP_AT)) {
			MKD(parser, dag, FM__DAG_OP_EXPR_MATMUL);
			dag->left = prev;
			forward(parser);
			if (!(dag->right = expr_unary(parser))) {
				ERR(parser,
				    "missing right operand for '@'",
				    FM__ERRNO_SYNTAX);
				return NULL;
			}
			if (!(set_type(dag, fm__dag_type(dag)))) {
				ERR(parser,
				    "invalid operand types for '@'",
				    FM__ERRNO_SYNTAX);
				return NULL;
			}
		}
		else if (match(parser, FM__LEXER_OP_DIV)) {
			MKD(parser, dag, FM__DAG_OP_EXPR_DIV);
			dag->left = prev;
			forward(parser);
			if (!(dag->right = expr_unary(parser))) {
				ERR(parser,
				    "missing right operand for '/'",
				    FM__ERRNO_SYNTAX);
				return NULL;
			}
			if (!(set_type(dag, fm__dag_type(dag)))) {
				ERR(parser,
				    "invalid operand types for '/'",
				    FM__ERRNO_SYNTAX);
				return NULL;
			}
		}
		else if (match(parser, FM__LEXER_OP_MOD)) {
			MKD(parser, dag, FM__DAG_OP_EXPR_MOD);
			dag->left = prev;
			forward(parser);
			if (!(dag->right = expr_unary(parser))) {
				ERR(parser,
				    "missing right operand for '%'",
				    FM__ERRNO_SYNTAX);
				return NULL;
			}
			if (!(set_type(dag, fm__dag_type(dag)))) {
				ERR(parser,
				    "invalid operand types for '%'",
				    FM__ERRNO_SYNTAX);
				return NULL;
			}
		}
		else {
			break;
		}
		if ((FM__DAG_OP_EXPR_MATMUL == dag->op)
		    ? matmul_geometry(parser, dag)
		    : binary_geometry(parser, dag)) {
			FM__TRACE(0);
			return NULL;
		}
	}
	return dag;
}

static struct fm__dag *
expr_multiplicative(struct fm__parser *parser)
{
	struct fm__dag *dag;

	if (!(dag = expr_unary(parser))) {
		return NULL;
	}
	return expr_multiplicative_(parser, dag);
}

/**
 * (E7)
 *
 * expr_additive: expr_additive '+' expr_multiplicative
 *              | expr_additive '-' expr_multiplicative
 *              | expr_multiplicative
 */

static struct fm__dag *
expr_additive_(struct fm__parser *parser, struct fm__dag *dag)
{
	struct fm__dag *prev;

	for (;;) {
		prev = dag;
		if (match(parser, FM__LEXER_OP_ADD)) {
			MKD(parser, dag, FM__DAG_OP_EXPR_ADD);
			dag->left = prev;
			forward(parser);
			if (!(dag->right = expr_multiplicative(parser))) {
				ERR(parser,
				    "missing right operand for '+'",
				    FM__ERRNO_SYNTAX);
				return NULL;
			}
			if (!(set_type(dag, fm__dag_type(dag)))) {
				ERR(parser,
				    "invalid operand types for '+'",
				    FM__ERRNO_SYNTAX);
				return NULL;
			}
		}
		else if (match(parser, FM__LEXER_OP_SUB)) {
			MKD(parser, dag, FM__DAG_OP_EXPR_SUB);
			dag->left = prev;
			forward(parser);
			if (!(dag->right = expr_multiplicative(parser))) {
				ERR(parser,
				    "missing right operand for '-'",
				    FM__ERRNO_SYNTAX);
				return NULL;
			}
			if (!(set_type(dag, fm__dag_type(dag)))) {
				ERR(parser,
				    "invalid operand types for '-'",
				    FM__ERRNO_SYNTAX);
				return NULL;
			}
		}
		else {
			break;
		}
		if (binary_geometry(parser, dag)) {
			FM__TRACE(0);
			return NULL;
		}
	}
	return dag;
}

static struct fm__dag *
expr_additive(struct fm__parser *parser)
{
	struct fm__dag *dag;

	if (!(dag = expr_multiplicative(parser))) {
		return NULL;
	}
	return expr_additive_(parser, dag);
}

/**
 * (E8)
 *
 * expr_shift: expr_shift '<<' expr_additive
 *           | expr_shift '>>' expr_additive
 *           | expr_additive
 */

static struct fm__dag *
expr_shift_(struct fm__parser *parser, struct fm__dag *dag)
{
	struct fm__dag *prev;

	for (;;) {
		prev = dag;
		if (match(parser, FM__LEXER_OP_SHL)) {
			MKD(parser, dag, FM__DAG_OP_EXPR_SHL);
			dag->left = prev;
			forward(parser);
			if (!(dag->right = expr_additive(parser))) {
				ERR(parser,
				    "missing right operand for '<<'",
				    FM__ERRNO_SYNTAX);
				return NULL;
			}
			if (!(set_type(dag, fm__dag_type(dag)))) {
				ERR(parser,
				    "invalid operand types for '<<'",
				    FM__ERRNO_SYNTAX);
				return NULL;
			}
		}
		else if (match(parser, FM__LEXER_OP_SHR)) {
			MKD(parser, dag, FM__DAG_OP_EXPR_SHR);
			dag->left = prev;
			forward(parser);
			if (!(dag->right = expr_additive(parser))) {
				ERR(parser,
				    "missing right operand for '>>'",
				    FM__ERRNO_SYNTAX);
				return NULL;
			}
			if (!(set_type(dag, fm__dag_type(dag)))) {
				ERR(parser,
				    "invalid operand types for '>>'",
				    FM__ERRNO_SYNTAX);
				return NULL;
			}
		}
		else {
			break;
		}
		if (binary_geometry(parser, dag)) {
			FM__TRACE(0);
			return NULL;
		}
	}
	return dag;
}

static struct fm__dag *
expr_shift(struct fm__parser *parser)
{
	struct fm__dag *dag;

	if (!(dag = expr_additive(parser))) {
		return NULL;
	}
	return expr_shift_(parser, dag);
}

/**
 * (E9)
 *
 * expr_relational: expr_relational '<'  expr_shift
 *                | expr_relational '>'  expr_shift
 *                | expr_relational '<=' expr_shift
 *                | expr_relational '>=' expr_shift
 *                | expr_shift
 */

static struct fm__dag *
expr_relational_(struct fm__parser *parser, struct fm__dag *dag)
{
	struct fm__dag *prev;

	for (;;) {
		prev = dag;
		if (match(parser, FM__LEXER_OP_LT)) {
			MKD(parser, dag, FM__DAG_OP_EXPR_LT);
			dag->left = prev;
			forward(parser);
			if (!(dag->right = expr_shift(parser))) {
				ERR(parser,
				    "missing right operand for '<'",
				    FM__ERRNO_SYNTAX);
				return NULL;
			}
			if (!(set_type(dag, fm__dag_type(dag)))) {
				ERR(parser,
				    "invalid operand types for '<'",
				    FM__ERRNO_SYNTAX);
				return NULL;
			}
		}
		else if (match(parser, FM__LEXER_OP_GT)) {
			MKD(parser, dag, FM__DAG_OP_EXPR_GT);
			dag->left = prev;
			forward(parser);
			if (!(dag->right = expr_shift(parser))) {
				ERR(parser,
				    "missing right operand for '>'",
				    FM__ERRNO_SYNTAX);
				return NULL;
			}
			if (!(set_type(dag, fm__dag_type(dag)))) {
				ERR(parser,
				    "invalid operand types for '>'",
				    FM__ERRNO_SYNTAX);
				return NULL;
			}
		}
		else if (match(parser, FM__LEXER_OP_LE)) {
			MKD(parser, dag, FM__DAG_OP_EXPR_LE);
			dag->left = prev;
			forward(parser);
			if (!(dag->right = expr_shift(parser))) {
				ERR(parser,
				    "missing right operand for '<='",
				    FM__ERRNO_SYNTAX);
				return NULL;
			}
			if (!(set_type(dag, fm__dag_type(dag)))) {
				ERR(parser,
				    "invalid operand types for '<='",
				    FM__ERRNO_SYNTAX);
				return NULL;
			}
		}
		else if (match(parser, FM__LEXER_OP_GE)) {
			MKD(parser, dag, FM__DAG_OP_EXPR_GE);
			dag->left = prev;
			forward(parser);
			if (!(dag->right = expr_shift(parser))) {
				ERR(parser,
				    "missing right operand for '>='",
				    FM__ERRNO_SYNTAX);
				return NULL;
			}
			if (!(set_type(dag, fm__dag_type(dag)))) {
				ERR(parser,
				    "invalid operand types for '>='",
				    FM__ERRNO_SYNTAX);
				return NULL;
			}
		}
		else {
			break;
		}
		if (binary_geometry(parser, dag)) {
			FM__TRACE(0);
			return NULL;
		}
	}
	return dag;
}

static struct fm__dag *
expr_relational(struct fm__parser *parser)
{
	struct fm__dag *dag;

	if (!(dag = expr_shift(parser))) {
		return NULL;
	}
	return expr_relational_(parser, dag);
}

/**
 * (E10)
 *
 * expr_equality: expr_equality '==' expr_relational
 *              | expr_equality '!=' expr_relational
 *              | expr_relational
 */

static struct fm__dag *
expr_equality_(struct fm__parser *parser, struct fm__dag *dag)
{
	struct fm__dag *prev;

	for (;;) {
		prev = dag;
		if (match(parser, FM__LEXER_OP_EQ)) {
			MKD(parser, dag, FM__DAG_OP_EXPR_EQ);
			dag->left = prev;
			forward(parser);
			if (!(dag->right = expr_relational(parser))) {
				ERR(parser,
				    "missing right operand for '=='",
				    FM__ERRNO_SYNTAX);
				return NULL;
			}
			if (!(set_type(dag, fm__dag_type(dag)))) {
				ERR(parser,
				    "invalid operand types for '=='",
				    FM__ERRNO_SYNTAX);
				return NULL;
			}
		}
		else if (match(parser, FM__LEXER_OP_NE)) {
			MKD(parser, dag, FM__DAG_OP_EXPR_NE);
			dag->left = prev;
			forward(parser);
			if (!(dag->right = expr_relational(parser))) {
				ERR(parser,
				    "missing right operand for '!='",
				    FM__ERRNO_SYNTAX);
				return NULL;
			}
			if (!(set_type(dag, fm__dag_type(dag)))) {
				ERR(parser,
				    "invalid operand types for '!='",
				    FM__ERRNO_SYNTAX);
				return NULL;
			}
		}
		else {
			break;
		}
		if (binary_geometry(parser, dag)) {
			FM__TRACE(0);
			return NULL;
		}
	}
	return dag;
}

static struct fm__dag *
expr_equality(struct fm__parser *parser)
{
	struct fm__dag *dag;

	if (!(dag = expr_relational(parser))) {
		return NULL;
	}
	return expr_equality_(parser, dag);
}

/**
 * (E11)
 *
 * expr_and: expr_and '&' expr_equality
 *         | expr_equality
 */

static struct fm__dag *
expr_and_(struct fm__parser *parser, struct fm__dag *dag)
{
	struct fm__dag *prev;

	for (;;) {
		prev = dag;
		if (match(parser, FM__LEXER_OP_AND)) {
			MKD(parser, dag, FM__DAG_OP_EXPR_AND);
			dag->left = prev;
			forward(parser);
			if (!(dag->right = expr_equality(parser))) {
				ERR(parser,
				    "missing right operand for '&'",
				    FM__ERRNO_SYNTAX);
				return NULL;
			}
			if (!(set_type(dag, fm__dag_type(dag)))) {
				ERR(parser,
				    "invalid operand types for '&'",
				    FM__ERRNO_SYNTAX);
				return NULL;
			}
		}
		else {
			break;
		}
		if (binary_geometry(parser, dag)) {
			FM__TRACE(0);
			return NULL;
		}
	}
	return dag;
}

static struct fm__dag *
expr_and(struct fm__parser *parser)
{
	struct fm__dag *dag;

	if (!(dag = expr_equality(parser))) {
		return NULL;
	}
	return expr_and_(parser, dag);
}

/**
 * (E12)
 *
 * expr_xor: expr_xor '^' expr_and
 *         | expr_and
 */

static struct fm__dag *
expr_xor_(struct fm__parser *parser, struct fm__dag *dag)
{
	struct fm__dag *prev;

	for (;;) {
		prev = dag;
		if (match(parser, FM__LEXER_OP_XOR)) {
			MKD(parser, dag, FM__DAG_OP_EXPR_XOR);
			dag->left = prev;
			forward(parser);
			if (!(dag->right = expr_and(parser))) {
				ERR(parser,
				    "missing right operand for '^'",
				    FM__ERRNO_SYNTAX);
				return NULL;
			}
			if (!(set_type(dag, fm__dag_type(dag)))) {
				ERR(parser,
				    "invalid operand types for '^'",
				    FM__ERRNO_SYNTAX);
				return NULL;
			}
		}
		else {
			break;
		}
		if (binary_geometry(parser, dag)) {
			FM__TRACE(0);
			return NULL;
		}
	}
	return dag;
}

static struct fm__dag *
expr_xor(struct fm__parser *parser)
{
	struct fm__dag *dag;

	if (!(dag = expr_and(parser))) {
		return NULL;
	}
	return expr_xor_(parser, dag);
}

/**
 * (E13)
 *
 * expr_or: expr_or '|' expr_xor
 *        | expr_xor
 */

static struct fm__dag *
expr_or_(struct fm__parser *parser, struct fm__dag *dag)
{
	struct fm__dag *prev;

	for (;;) {
		prev = dag;
		if (match(parser, FM__LEXER_OP_OR)) {
			MKD(parser, dag, FM__DAG_OP_EXPR_OR);
			dag->left = prev;
			forward(parser);
			if (!(dag->right = expr_xor(parser))) {
				ERR(parser,
				    "missing right operand for '|'",
				    FM__ERRNO_SYNTAX);
				return NULL;
			}
			if (!(set_type(dag, fm__dag_type(dag)))) {
				ERR(parser,
				    "invalid operand types for '|'",
				    FM__ERRNO_SYNTAX);
				return NULL;
			}
		}
		else {
			break;
		}
		if (binary_geometry(parser, dag)) {
			FM__TRACE(0);
			return NULL;
		}
	}
	return dag;
}

static struct fm__dag *
expr_or(struct fm__parser *parser)
{
	struct fm__dag *dag;

	if (!(dag = expr_xor(parser))) {
		return NULL;
	}
	return expr_or_(parser, dag);
}

/**
 * (E14)
 *
 * expr_logic_and: expr_logic_and '&&' expr_or
 *               | expr_or
 */

static struct fm__dag *
expr_logic_and_(struct fm__parser *parser, struct fm__dag *dag)
{
	struct fm__dag *prev;

	for (;;) {
		prev = dag;
		if (match(parser, FM__LEXER_OP_LOGAND)) {
			MKD(parser, dag, FM__DAG_OP_EXPR_LOGAND);
			dag->left = prev;
			forward(parser);
			if (!(dag->right = expr_or(parser))) {
				ERR(parser,
				    "missing right operand for '&&'",
				    FM__ERRNO_SYNTAX);
				return NULL;
			}
			if (!(set_type(dag, fm__dag_type(dag)))) {
				ERR(parser,
				    "invalid operand types for '&&'",
				    FM__ERRNO_SYNTAX);
				return NULL;
			}
		}
		else {
			break;
		}
		if (binary_geometry(parser, dag)) {
			FM__TRACE(0);
			return NULL;
		}
	}
	return dag;
}

static struct fm__dag *
expr_logic_and(struct fm__parser *parser)
{
	struct fm__dag *dag;

	if (!(dag = expr_or(parser))) {
		return NULL;
	}
	return expr_logic_and_(parser, dag);
}

/**
 * (E15)
 *
 * expr_logic_xor: expr_logic_xor '^^' expr_logic_and
 *               | expr_logic_and
 */

static struct fm__dag *
expr_logic_xor_(struct fm__parser *parser, struct fm__dag *dag)
{
	struct fm__dag *prev;

	for (;;) {
		prev = dag;
		if (match(parser, FM__LEXER_OP_LOGXOR)) {
			MKD(parser, dag, FM__DAG_OP_EXPR_LOGXOR);
			dag->left = prev;
			forward(parser);
			if (!(dag->right = expr_logic_and(parser))) {
				ERR(parser,
				    "missing right operand for '^^'",
				    FM__ERRNO_SYNTAX);
				return NULL;
			}
			if (!(set_type(dag, fm__dag_type(dag)))) {
				ERR(parser,
				    "invalid operand types for '^^'",
				    FM__ERRNO_SYNTAX);
				return NULL;
			}
		}
		else {
			break;
		}
		if (binary_geometry(parser, dag)) {
			FM__TRACE(0);
			return NULL;
		}
	}
	return dag;
}

static struct fm__dag *
expr_logic_xor(struct fm__parser *parser)
{
	struct fm__dag *dag;

	if (!(dag = expr_logic_and(parser))) {
		return NULL;
	}
	return expr_logic_xor_(parser, dag);
}

/**
 * (E16)
 *
 * expr_logic_or: expr_logic_or '||' expr_logic_xor
 *              | expr_logic_xor
 */

static struct fm__dag *
expr_logic_or_(struct fm__parser *parser, struct fm__dag *dag)
{
	struct fm__dag *prev;

	for (;;) {
		prev = dag;
		if (match(parser, FM__LEXER_OP_LOGOR)) {
			MKD(parser, dag, FM__DAG_OP_EXPR_LOGOR);
			dag->left = prev;
			forward(parser);
			if (!(dag->right = expr_logic_xor(parser))) {
				ERR(parser,
				    "missing right operand for '||'",
				    FM__ERRNO_SYNTAX);
				return NULL;
			}
			if (!(set_type(dag, fm__dag_type(dag)))) {
				ERR(parser,
				    "invalid operand types for '||'",
				    FM__ERRNO_SYNTAX);
				return NULL;
			}
		}
		else {
			break;
		}
		if (binary_geometry(parser, dag)) {
			FM__TRACE(0);
			return NULL;
		}
	}
	return dag;
}

static struct fm__dag *
expr_logic_or(struct fm__parser *parser)
{
	struct fm__dag *dag;

	if (!(dag = expr_logic_xor(parser))) {
		return NULL;
	}
	return expr_logic_or_(parser, dag);
}

/**
 * (E17)
 *
 * expr: expr_logic_or
 *     | expr_logic_or '?' expr ':' expr
 */

static struct fm__dag *
expr_(struct fm__parser *parser)
{
	struct fm__dag *dag, *dag_;

	assert( parser );

	if (!(dag = expr_logic_or(parser))) {
		return NULL;
	}
	if (match(parser, FM__LEXER_OP_QUESTION)) {
		MKD(parser, dag_, FM__DAG_OP_EXPR_COND);
		dag_->cond = dag;
		forward(parser);
		if (!(dag_->left = expr(parser))) {
			ERR(parser,
			    "missing expression after '?'",
			    FM__ERRNO_SYNTAX);
			return NULL;
		}
		if (!match(parser, FM__LEXER_OP_COLON)) {
			ERR(parser,
			    "missing ':' in conditional expression",
			    FM__ERRNO_SYNTAX);
			return NULL;
		}
		forward(parser);
		if (!(dag_->right = expr(parser))) {
			ERR(parser,
			    "missing expression after ':'",
			    FM__ERRNO_SYNTAX);
			return NULL;
		}
		if (!(set_type(dag_, fm__dag_type(dag_)))) {
			ERR(parser,
			    "incompatible conditional operand types",
			    FM__ERRNO_SYNTAX);
			return NULL;
		}
		if (cond_geometry(parser, dag_)) {
			FM__TRACE(0);
			return NULL;
		}
		dag = dag_;
	}
	return dag;
}

static struct fm__dag *
expr(struct fm__parser *parser)
{
	struct fm__dag *dag;

	if (push(parser)) {
		FM__TRACE(0);
		return NULL;
	}
	dag = expr_(parser);
	pop(parser);
	return dag;
}

/**
 * (E18)
 *
 * expr_list: expr
 *          | expr_list ',' expr
 */

static struct fm__dag *
expr_list(struct fm__parser *parser)
{
	struct fm__dag *dag, *dag_, *head, *tail;
	int separator;

	separator = 0;
	head = tail = NULL;
	while ((dag = expr(parser))) {
		MKD(parser, dag_, FM__DAG_OP_EXPR_LIST);
		dag_->left = dag;
		if (tail) {
			tail->right = dag_;
		}
		else {
			head = dag_;
		}
		tail = dag_;
		if ((head != tail) && !separator) {
			ERR(parser,
			    "missing ',' in expression list",
			    FM__ERRNO_SYNTAX);
			return NULL;
		}
		if ((separator = match(parser, FM__LEXER_OP_COMMA))) {
			forward(parser);
		}
	}
	if (head && separator) {
		ERR(parser,
		    "dangling ',' after expression list",
		    FM__ERRNO_SYNTAX);
		return NULL;
	}
	return head;
}

/**----------------------------------------------------------------------------
 * (D0) decl_dtype
 * (D1) decl_init
 * (D2) decl_assign
 * (D3) decl_tensor
 * (D4) decl_seed
 * (D5) decl_list
 *---------------------------------------------------------------------------*/

/**
 * (D0)
 *
 * decl_dtype: 'bool'
 *           | 'unsigned' 'byte'
 *           | 'unsigned' 'short'
 *           | 'unsigned' 'int'
 *           | 'unsigned' 'long'
 *           | 'byte'
 *           | 'short'
 *           | 'int'
 *           | 'long'
 *           | 'fp8_e4m3'
 *           | 'fp8_e5m2'
 *           | 'bf16'
 *           | 'fp16'
 *           | 'float'
 *           | 'double'
 */

static enum fm__dag_type
decl_dtype(struct fm__parser *parser)
{
	enum fm__dag_type type;
	int flag;

	type = FM__DAG_TYPE_UNKNOWN;
	if ((flag = match(parser, FM__LEXER_OP_UNSIGNED))) {
		forward(parser);
	}
	if (match(parser, FM__LEXER_OP_BOOL)) {
		if (flag) {
			ERR(parser,
			    "invalid 'unsigned' qualifier for type 'bool'",
			    FM__ERRNO_SYNTAX);
			return FM__DAG_TYPE_UNKNOWN;
		}
		type = FM__DAG_TYPE_TBOOL;
		forward(parser);
	}
	else if (match(parser, FM__LEXER_OP_BYTE)) {
		type = flag ? FM__DAG_TYPE_UINT8 : FM__DAG_TYPE_INT8;
		forward(parser);
	}
	else if (match(parser, FM__LEXER_OP_SHORT)) {
		type = (flag ? FM__DAG_TYPE_UINT16 : FM__DAG_TYPE_INT16);
		forward(parser);
	}
	else if (match(parser, FM__LEXER_OP_INT)) {
		type = (flag ? FM__DAG_TYPE_UINT32 : FM__DAG_TYPE_INT32);
		forward(parser);
	}
	else if (match(parser, FM__LEXER_OP_LONG)) {
		type = (flag ? FM__DAG_TYPE_UINT64 : FM__DAG_TYPE_INT64);
		forward(parser);
	}
	else if (match(parser, FM__LEXER_OP_FP8_E4M3)) {
		if (flag) {
			ERR(parser,
			    "invalid 'unsigned' qualifier "
			    "for type 'fp8_e4m3'",
			    FM__ERRNO_SYNTAX);
			return FM__DAG_TYPE_UNKNOWN;
		}
		type = FM__DAG_TYPE_FP8_E4M3;
		forward(parser);
	}
	else if (match(parser, FM__LEXER_OP_FP8_E5M2)) {
		if (flag) {
			ERR(parser,
			    "invalid 'unsigned' "
			    "qualifier for type 'fp8_e5m2'",
			    FM__ERRNO_SYNTAX);
			return FM__DAG_TYPE_UNKNOWN;
		}
		type = FM__DAG_TYPE_FP8_E5M2;
		forward(parser);
	}
	else if (match(parser, FM__LEXER_OP_BF16)) {
		if (flag) {
			ERR(parser,
			    "invalid 'unsigned' qualifier for type 'bf16'",
			    FM__ERRNO_SYNTAX);
			return FM__DAG_TYPE_UNKNOWN;
		}
		type = FM__DAG_TYPE_BF16;
		forward(parser);
	}
	else if (match(parser, FM__LEXER_OP_FP16)) {
		if (flag) {
			ERR(parser,
			    "invalid 'unsigned' qualifier for type 'fp16'",
			    FM__ERRNO_SYNTAX);
			return FM__DAG_TYPE_UNKNOWN;
		}
		type = FM__DAG_TYPE_FP16;
		forward(parser);
	}
	else if (match(parser, FM__LEXER_OP_FLOAT)) {
		if (flag) {
			ERR(parser,
			    "invalid 'unsigned' qualifier for type 'float'",
			    FM__ERRNO_SYNTAX);
			return FM__DAG_TYPE_UNKNOWN;
		}
		type = FM__DAG_TYPE_FP32;
		forward(parser);
	}
	else if (match(parser, FM__LEXER_OP_DOUBLE)) {
		if (flag) {
			ERR(parser,
			    "invalid 'unsigned' qualifier for type 'double'",
			    FM__ERRNO_SYNTAX);
			return FM__DAG_TYPE_UNKNOWN;
		}
		type = FM__DAG_TYPE_FP64;
		forward(parser);
	}
	else {
		if (flag) {
			ERR(parser, "missing type name", FM__ERRNO_SYNTAX);
			return FM__DAG_TYPE_UNKNOWN;
		}
	}
	return type;
}

/**
 * (D1)
 *
 * decl_init: 'zeros' '(' ')'
 *          | 'ones' '(' ')'
 *          | 'uniform' '(' expr_list? ')'
 *          | 'normal' '(' expr_list? ')'
 *          | 'iota' '(' expr_list? ')'
 */

static struct fm__dag *
decl_init(struct fm__parser *parser)
{
	struct fm__dag *dag;

	dag = NULL;
	if (match(parser, FM__LEXER_OP_ZEROS)) {
		MKD(parser, dag, FM__DAG_OP_DECL_INIT_ZEROS);
		forward(parser);
	}
	else if (match(parser, FM__LEXER_OP_ONES)) {
		MKD(parser, dag, FM__DAG_OP_DECL_INIT_ONES);
		forward(parser);
	}
	else if (match(parser, FM__LEXER_OP_UNIFORM)) {
		MKD(parser, dag, FM__DAG_OP_DECL_INIT_UNIFORM);
		dag->u.u = parser->seed;
		if (parser->seed) {
			if (!++parser->seed) {
				parser->seed = 1;
			}
		}
		forward(parser);
	}
	else if (match(parser, FM__LEXER_OP_NORMAL)) {
		MKD(parser, dag, FM__DAG_OP_DECL_INIT_NORMAL);
		dag->u.u = parser->seed;
		if (parser->seed) {
			if (!++parser->seed) {
				parser->seed = 1;
			}
		}
		forward(parser);
	}
	else if (match(parser, FM__LEXER_OP_IOTA)) {
		MKD(parser, dag, FM__DAG_OP_DECL_INIT_IOTA);
		forward(parser);
	}
	if (dag) {
		if (!match(parser, FM__LEXER_OP_OPENPAREN)) {
			ERR(parser,
			    "missing '(' after tensor initializer",
			    FM__ERRNO_SYNTAX);
			return NULL;
		}
		forward(parser);
		if ((FM__DAG_OP_DECL_INIT_UNIFORM == dag->op) ||
		    (FM__DAG_OP_DECL_INIT_NORMAL == dag->op) ||
		    (FM__DAG_OP_DECL_INIT_IOTA == dag->op)) {
			dag->right = expr_list(parser);
		}
		if (!match(parser, FM__LEXER_OP_CLOSEPAREN)) {
			ERR(parser,
			    "missing ')' after tensor initializer",
			    FM__ERRNO_SYNTAX);
			return NULL;
		}
		forward(parser);
		if (dag->right &&
		    ((2 != fm__utils_list_count(dag->right)) ||
		     !(fm__dag_is_int(dag->right->left) ||
		       fm__dag_is_real(dag->right->left)) ||
		     !(fm__dag_is_int(dag->right->right->left) ||
		       fm__dag_is_real(dag->right->right->left)))) {
			ERR(parser,
			    "invalid arguments to tensor initializer",
			    FM__ERRNO_SYNTAX);
			return NULL;
		}
		if (dag->right && eval_expr_list(dag->right)) {
			ERR(parser,
			    (FM__ERRNO_MEMORY == errno)
			    ? "out of memory"
			    : "expression evaluation failed",
			    0);
			return NULL;
		}
	}
	return dag;
}

/**
 * (D2)
 *
 * decl_assign: IDENTIFIER '=' expr
 */

static struct fm__dag *
decl_assign(struct fm__parser *parser)
{
	struct fm__dag *dag;
	size_t checkpoint;
	const char *s;

	dag = NULL;
	checkpoint = parser->i;
	if (match(parser, FM__LEXER_OP_IDENTIFIER)) {
		s = next(parser)->u.s;
		if (fm__map_lookup(parser->map, s)) {
			ERR(parser,
			    "redefinition of tensor",
			    FM__ERRNO_SYNTAX);
			return NULL;
		}
		if (expr_fnc(s)) {
			ERR(parser,
			    "tensor is a reserved function name",
			    FM__ERRNO_SYNTAX);
			return NULL;
		}
		forward(parser);
		if (!match(parser, FM__LEXER_OP_ASSIGN)) {
			parser->i = checkpoint;
			return NULL;
		}
		reverse(parser);
		MKD(parser, dag, FM__DAG_OP_DECL_LET);
		forward(parser);
		forward(parser);
		if (!(dag->right = expr(parser))) {
			ERR(parser,
			    "missing expression for tensor",
			    FM__ERRNO_SYNTAX);
			return NULL;
		}
		if (fm__dag_is_tensor(dag->right)) {
			if (fold_compute_constants(parser, dag->right, 0)) {
				FM__TRACE(0);
				return NULL;
			}
			dag->op = FM__DAG_OP_DECL_COMPUTE;
			set_type(dag, dag->right->type);
			if (!fm__geometry_is_populated
			    (&dag->right->geometry)) {
				ERR(parser,
				    "missing tensor expression geometry",
				    FM__ERRNO_SYNTAX);
				return NULL;
			}
			dag->geometry = dag->right->geometry;
			if (!(dag->u.s = fm__strdup(s))) {
				ERR(parser, "out of memory", 0);
				return NULL;
			}
		}
		else if (fm__dag_eval_expr(dag->right)) {
			ERR(parser,
			    (FM__ERRNO_MEMORY == errno)
			    ? "out of memory"
			    : "expression evaluation failed",
			    0);
			return NULL;
		}
		if (fm__map_update(parser->map, s, dag)) {
			ERR(parser, "out of memory", 0);
			return NULL;
		}
	}
	return dag;
}

/**
 * (D3)
 *
 * decl_tensor: IDENTIFIER ':' decl_dtype '[' expr_list ']'
 *              ( '[' expr_list ']' )?
 *              ( '=' decl_init )?
 */

static struct fm__dag *
decl_tensor(struct fm__parser *parser)
{
	struct fm__dag *dag;
	size_t checkpoint;
	const char *s;

	dag = NULL;
	checkpoint = parser->i;
	if (match(parser, FM__LEXER_OP_IDENTIFIER)) {
		s = next(parser)->u.s;
		if (fm__map_lookup(parser->map, s)) {
			ERR(parser,
			    "redefinition of tensor",
			    FM__ERRNO_SYNTAX);
			return NULL;
		}
		if (expr_fnc(s)) {
			ERR(parser,
			    "tensor is a reserved function name",
			    FM__ERRNO_SYNTAX);
			return NULL;
		}
		forward(parser);
		if (!match(parser, FM__LEXER_OP_COLON)) {
			parser->i = checkpoint;
			return NULL;
		}
		reverse(parser);
		MKD(parser, dag, FM__DAG_OP_DECL_TENSOR);
		forward(parser);
		forward(parser);
		if (!(set_type(dag, decl_dtype(parser)))) {
			ERR(parser,
			    "missing dtype for tensor",
			    FM__ERRNO_SYNTAX);
			return NULL;
		}
		MKD(parser, dag->left, FM__DAG_OP_DECL_GEOMETRY);
		if (!(dag->u.s = fm__strdup(s))) {
			ERR(parser, "out of memory", 0);
			return NULL;
		}
		if (!match(parser, FM__LEXER_OP_OPENBRACKET)) {
			ERR(parser,
			    "missing '[' before shape for tensor",
			    FM__ERRNO_SYNTAX);
			return NULL;
		}
		forward(parser);
		if (!(dag->left->right = expr_list(parser))) {
			ERR(parser,
			    "missing shape for tensor",
			    FM__ERRNO_SYNTAX);
			return NULL;
		}
		if (!match(parser, FM__LEXER_OP_CLOSEBRACKET)) {
			ERR(parser,
			    "missing ']' after shape for tensor",
			    FM__ERRNO_SYNTAX);
			return NULL;
		}
		forward(parser);
		if (match(parser, FM__LEXER_OP_OPENBRACKET)) {
			forward(parser);
			if (!(dag->left->left = expr_list(parser))) {
				ERR(parser,
				    "missing stride for tensor",
				    FM__ERRNO_SYNTAX);
				return NULL;
			}
			if (!match(parser, FM__LEXER_OP_CLOSEBRACKET)) {
				ERR(parser,
				    "missing ']' after stride for tensor",
				    FM__ERRNO_SYNTAX);
				return NULL;
			}
			forward(parser);
		}
		if (match(parser, FM__LEXER_OP_ASSIGN)) {
			forward(parser);
			if (!(dag->right = decl_init(parser))) {
				ERR(parser,
				    "missing initializer for tensor",
				    FM__ERRNO_SYNTAX);
				return NULL;
			}
		}
		if (!is_int_expr_list(dag->left->right)) {
			ERR(parser,
			    "tensor shape must contain only "
			    "integer expressions",
			    FM__ERRNO_SYNTAX);
			return NULL;
		}
		if (!is_int_expr_list(dag->left->left)) {
			ERR(parser,
			    "tensor stride must contain only "
			    "integer expressions",
			    FM__ERRNO_SYNTAX);
			return NULL;
		}
		if (eval_expr_list(dag->left->right) ||
		    eval_expr_list(dag->left->left)) {
			ERR(parser,
			    (FM__ERRNO_MEMORY == errno)
			    ? "out of memory"
			    : "invalid tensor geometry",
			    0);
			return NULL;
		}
		if (dag->left->left &&
		    (fm__utils_list_count(dag->left->right) !=
		     fm__utils_list_count(dag->left->left))) {
			ERR(parser,
			    "tensor shape and stride "
			    "must have the same length",
			    FM__ERRNO_SYNTAX);
			return NULL;
		}
		if (fm__geometry_create(&dag->geometry, dag)) {
			ERR(parser,
			    (FM__ERRNO_MEMORY == errno)
			    ? "out of memory"
			    : "invalid tensor geometry",
			    0);
			return NULL;
		}
		if (fm__map_update(parser->map, s, dag)) {
			ERR(parser, "out of memory", 0);
			return NULL;
		}
	}
	return dag;
}

/**
 * (D4)
 *
 * decl_seed: '.seed' expr
 */

static struct fm__dag *
decl_seed(struct fm__parser *parser)
{
	struct fm__dag *dag;

	if (match(parser, FM__LEXER_OP_DOTSEED)) {
		forward(parser);
		if (!(dag = expr(parser))) {
			ERR(parser,
			    "expected expression after '.seed'",
			    FM__ERRNO_SYNTAX);
			return NULL;
		}
		if (!fm__dag_is_int(dag)) {
			ERR(parser,
			    "'.seed' requires a "
			    "compile-time integer expression",
			    FM__ERRNO_SYNTAX);
			return NULL;
		}
		if (fm__dag_eval_expr(dag)) {
			ERR(parser,
			    (FM__ERRNO_MEMORY == errno)
			    ? "out of memory"
			    : "expression evaluation failed",
			    0);
			return NULL;
		}
		if (0 > fm__bigint_sign(dag->u.i)) {
			ERR(parser,
			    "'.seed' value must be nonnegative",
			    FM__ERRNO_SYNTAX);
			return NULL;
		}
		if (fm__bigint_uint64(dag->u.i,
				      0, /* clamp */
				      &parser->seed)) {
			ERR(parser,
			    "'.seed' value exceeds the maximum of "
			    "18446744073709551615",
			    FM__ERRNO_SYNTAX);
			return NULL;
		}
		return &SEED_DAG;
	}
	return NULL;
}

/**
 * (D5)
 *
 * decl_list: ( decl_assign ';' |
 *              decl_tensor ';' |
 *              decl_seed   ';' )+
 */

static struct fm__dag *
decl_list(struct fm__parser *parser)
{
	struct fm__dag *dag, *dag_, *head, *tail;
	int seen, separator;

	seen = 0;
	separator = 0;
	head = tail = NULL;
	while ((dag = decl_assign(parser)) ||
	       (dag = decl_tensor(parser)) ||
	       (dag = decl_seed(parser))) {
		if (seen && !separator) {
			ERR(parser,
			    "missing ';' in declaration list",
			    FM__ERRNO_SYNTAX);
			return NULL;
		}
		seen = 1;
		if (&SEED_DAG != dag) {
			MKD(parser, dag_, FM__DAG_OP_DECL_LIST);
			dag_->left = dag;
			if (tail) {
				tail->right = dag_;
			}
			else {
				head = dag_;
			}
			tail = dag_;
		}
		if ((separator = match(parser, FM__LEXER_OP_SEMICOLON))) {
			forward(parser);
		}
	}
	if (parser->stop) {
		FM__TRACE(0);
		return NULL;
	}
	if (seen && !separator) {
		ERR(parser,
		    "missing ';' in declaration list",
		    FM__ERRNO_SYNTAX);
		return NULL;
	}
	return head;
}

static struct fm__dag *
top(struct fm__parser *parser)
{
	struct fm__dag *dag;

	dag = decl_list(parser);
	if (parser->stop) {
		FM__TRACE(0);
		return NULL;
	}
	if (!dag) {
		ERR(parser, "empty translation unit", FM__ERRNO_SYNTAX);
		return NULL;
	}
	if (!match(parser, FM__LEXER_OP_EOF)) {
		ERR(parser,
		    "unexpected content after declaration list",
		    FM__ERRNO_SYNTAX);
		return NULL;
	}
	return dag;
}

fm__parser_t
fm__parser_open(fm__lexer_t lexer, char errstr[FM_ERRSTR_LEN])
{
	struct fm__parser *parser;

	assert( lexer && errstr );

	if (!(parser = fm__malloc(sizeof (struct fm__parser)))) {
		snprintf(errstr, FM_ERRSTR_LEN, "1:1: out of memory");
		FM__TRACE(0);
		return NULL;
	}
	memset(parser, 0, sizeof (struct fm__parser));
	parser->errstr = errstr;
	parser->map = NULL;
	parser->lexer = lexer;
	if (!(parser->pool = fm__dag_pool_open(&parser->stop, errstr)) ||
	    !(parser->map = fm__map_open()) ||
	    !(parser->n = fm__lexer_size(parser->lexer)) ||
	    !(parser->dag = top(parser)) ||
	    validate_geometry(parser, parser->dag, 0)) {
		fm__parser_close(parser);
		FM__TRACE(0);
		return NULL;
	}
	errstr[0] = '\0';
	return parser;
}

void
fm__parser_close(fm__parser_t parser)
{
	if (parser) {
		fm__map_close(parser->map);
		fm__dag_pool_close(parser->pool);
		memset(parser, 0, sizeof (struct fm__parser));
	}
	free(parser);
}

const struct fm__dag *
fm__parser_dag(fm__parser_t parser)
{
	assert( parser );

	return parser->dag;
}

char *
fm__parser_errstr(fm__parser_t parser)
{
	assert( parser );

	return parser->errstr;
}
