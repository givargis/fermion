/**
 * Copyright (c) 2025-2026 Tony Givargis
 * University of California, Irvine
 * fm.c
 */

#include "fm_expr.h"
#include "fm_geometry.h"
#include "fm_json.h"
#include "fm_map.h"
#include "fm_parser.h"
#include "fm_utils.h"
#include "fm.h"

#define ERR FM__UTILS_ERR_DAG

struct fm {
	fm__map_t map;
	fm__lexer_t lexer;
	fm__parser_t parser;
	struct fm_node *head;
	struct fm_node *tail;
};

static int
check_iota(const struct fm_node *node, const struct fm__dag *dag)
{
	fm__bigint_t start, step, last_index, delta, last, low, high;
	const char *error;
	int64_t min, max;

	if (FM_DTYPE_BOOL == node->dtype) {
		ERR(dag, "iota is unavailable for bool tensors");
		FM__TRACE(FM__ERRNO_SYNTAX);
		return -1;
	}
	if ((FM_DTYPE_FP8_E4M3 == node->dtype) ||
	    (FM_DTYPE_FP8_E5M2 == node->dtype) ||
	    (FM_DTYPE_BF16 == node->dtype) ||
	    (FM_DTYPE_FP16 == node->dtype) ||
	    (FM_DTYPE_FP32 == node->dtype) ||
	    (FM_DTYPE_FP64 == node->dtype)) {
		ERR(dag, "iota is unavailable for floating point tensors");
		FM__TRACE(FM__ERRNO_SYNTAX);
		return -1;
	}
	switch (node->dtype) {
	case FM_DTYPE_INT8:
		min = INT8_MIN;
		max = INT8_MAX;
		error = "iota sequence exceeds the signed 8-bit range";
		break;
	case FM_DTYPE_UINT8:
		min = 0;
		max = UINT8_MAX;
		error = "iota sequence exceeds the unsigned 8-bit range";
		break;
	case FM_DTYPE_INT16:
		min = INT16_MIN;
		max = INT16_MAX;
		error = "iota sequence exceeds the signed 16-bit range";
		break;
	case FM_DTYPE_UINT16:
		min = 0;
		max = UINT16_MAX;
		error = "iota sequence exceeds the unsigned 16-bit range";
		break;
	case FM_DTYPE_INT32:
		min = INT32_MIN;
		max = INT32_MAX;
		error = "iota sequence exceeds the signed 32-bit range";
		break;
	case FM_DTYPE_UINT32:
		min = 0;
		max = UINT32_MAX;
		error = "iota sequence exceeds the unsigned 32-bit range";
		break;
	case FM_DTYPE_INT64:
		min = INT64_MIN;
		max = INT64_MAX;
		error = "iota sequence exceeds the signed 64-bit range";
		break;
	case FM_DTYPE_UINT64:
		min = 0;
		max = INT64_MAX;
		error = "iota sequence exceeds the non-negative signed "
			"64-bit range";
		break;
	default:
		FM__TRACE(FM__ERRNO_SOFTWARE);
		return -1;
	}
	if (node->geometry.numel) {
		start = step = last_index = delta = last = NULL;
		low = high = NULL;
		if (!(start = fm__bigint_int(node->init.start)) ||
		    !(step = fm__bigint_int(node->init.step)) ||
		    !(last_index = fm__bigint_int(node->geometry.numel - 1)) ||
		    !(delta = fm__bigint_mul(step, last_index)) ||
		    !(last = fm__bigint_add(start, delta)) ||
		    !(low = fm__bigint_int(min)) ||
		    !(high = fm__bigint_int(max))) {
			fm__bigint_free(start);
			fm__bigint_free(step);
			fm__bigint_free(last_index);
			fm__bigint_free(delta);
			fm__bigint_free(last);
			fm__bigint_free(low);
			fm__bigint_free(high);
			FM__TRACE(0);
			return -1;
		}
		if ((fm__bigint_cmp(start, low) < 0) ||
		    (fm__bigint_cmp(start, high) > 0) ||
		    (fm__bigint_cmp(last, low) < 0) ||
		    (fm__bigint_cmp(last, high) > 0)) {
			fm__bigint_free(start);
			fm__bigint_free(step);
			fm__bigint_free(last_index);
			fm__bigint_free(delta);
			fm__bigint_free(last);
			fm__bigint_free(low);
			fm__bigint_free(high);
			ERR(dag, error);
			FM__TRACE(FM__ERRNO_SYNTAX);
			return -1;
		}
		fm__bigint_free(start);
		fm__bigint_free(step);
		fm__bigint_free(last_index);
		fm__bigint_free(delta);
		fm__bigint_free(last);
		fm__bigint_free(low);
		fm__bigint_free(high);
	}
	return 0;
}

static int
process_tensor(struct fm *fm, const struct fm__dag *dag)
{
	const struct fm__dag *init;
	struct fm_node *node;

	if (!dag->u.s ||
	    !dag->left ||
	    !dag->left->right ||
	    !fm__dag_is_tensor(dag)) {
		FM__TRACE(FM__ERRNO_SOFTWARE);
		return -1;
	}
	if (!(node = fm__malloc(sizeof (struct fm_node)))) {
		FM__TRACE(0);
		return -1;
	}
	memset(node, 0, sizeof (struct fm_node));
	node->expr = NULL;
	node->link = NULL;
	node->init.low = 0.0;
	node->init.high = 0.0;
	node->op = FM_NODE_OP_TENSOR;
	if (!(node->dtype = fm__utils_type2dtype(dag->type))) {
		free(node);
		FM__TRACE(FM__ERRNO_SOFTWARE);
		return -1;
	}
	if (!(node->name = fm__strdup(dag->u.s))) {
		free(node);
		FM__TRACE(0);
		return -1;
	}
	if (!fm->head) {
		fm->head = fm->tail = node;
	}
	else {
		fm->tail->link = node;
		fm->tail = node;
	}
	if (fm__map_update(fm->map, node->name, node)) {
		FM__TRACE(0);
		return -1;
	}
	node->geometry = dag->geometry;
	if ((init = dag->right)) {
		if (FM__DAG_OP_DECL_INIT_ZEROS == init->op) {
			node->init.op = FM_INIT_OP_ZEROS;
		}
		else if (FM__DAG_OP_DECL_INIT_ONES == init->op) {
			node->init.op = FM_INIT_OP_ONES;
		}
		else if (FM__DAG_OP_DECL_INIT_UNIFORM == init->op) {
			node->init.op = FM_INIT_OP_UNIFORM;
		}
		else if (FM__DAG_OP_DECL_INIT_NORMAL == init->op) {
			node->init.op = FM_INIT_OP_NORMAL;
		}
		else if (FM__DAG_OP_DECL_INIT_IOTA == init->op) {
			node->init.op = FM_INIT_OP_IOTA;
		}
		else {
			FM__TRACE(FM__ERRNO_SOFTWARE);
			return -1;
		}
		if ((FM__DAG_OP_DECL_INIT_UNIFORM == init->op) ||
		    (FM__DAG_OP_DECL_INIT_NORMAL == init->op)) {
			const struct fm__dag *low = NULL;
			const struct fm__dag *high = NULL;
			node->init.low = 0.0;
			node->init.high = 1.0;
			node->init.seed = init->u.u;
			if (init->right) {
				if (!(low = init->right->left) ||
				    !(high = init->right->right->left)) {
					FM__TRACE(FM__ERRNO_SOFTWARE);
					return -1;
				}
			}
			if (low && fm__dag_is_int(low)) {
				node->init.low = fm__bigint_double(low->u.i);
			}
			if (high && fm__dag_is_int(high)) {
				node->init.high = fm__bigint_double(high->u.i);
			}
			if (low && fm__dag_is_real(low)) {
				node->init.low = low->u.d;
			}
			if (high && fm__dag_is_real(high)) {
				node->init.high = high->u.d;
			}
			if (isnan(node->init.low) ||
			    isinf(node->init.low) ||
			    isnan(node->init.high) ||
			    isinf(node->init.high)) {
				ERR(init,
				    "initializer parameters must be finite");
				FM__TRACE(FM__ERRNO_SYNTAX);
				return -1;
			}
			if ((FM__DAG_OP_DECL_INIT_UNIFORM == init->op) &&
			    (node->init.low >= node->init.high)) {
				ERR(init,
				    "uniform initializer requires low < high");
				FM__TRACE(FM__ERRNO_SYNTAX);
				return -1;
			}
			if ((FM__DAG_OP_DECL_INIT_NORMAL == init->op) &&
			    (0.0 >= node->init.high)) {
				ERR(init,
				    "normal initializer "
				    "requires a positive "
				    "standard deviation");
				FM__TRACE(FM__ERRNO_SYNTAX);
				return -1;
			}
		}
		if (FM__DAG_OP_DECL_INIT_IOTA == init->op) {
			const struct fm__dag *start = NULL;
			const struct fm__dag *step = NULL;
			node->init.start = 0;
			node->init.step = 1;
			if (init->right) {
				if (!(start = init->right->left) ||
				    !(step = init->right->right->left)) {
					FM__TRACE(FM__ERRNO_SOFTWARE);
					return -1;
				}
			}
			if (start) {
				if (!fm__dag_is_int(start) ||
				    fm__bigint_int64(start->u.i,
						     0, /* clamp */
						     &node->init.start)) {
					ERR(init,
					    "iota initializer "
					    "requires a signed "
					    "64-bit integer "
					    "start");
					FM__TRACE(FM__ERRNO_SYNTAX);
					return -1;
				}
			}
			if (step) {
				if (!fm__dag_is_int(step) ||
				    fm__bigint_int64(step->u.i,
						     0, /* clamp */
						     &node->init.step)) {
					ERR(init,
					    "iota initializer "
					    "requires a signed "
					    "64-bit integer "
					    "step");
					FM__TRACE(FM__ERRNO_SYNTAX);
					return -1;
				}
			}
			if (check_iota(node, init)) {
				FM__TRACE(0);
				return -1;
			}
		}
	}
	return 0;
}

static int
process_compute(struct fm *fm, const struct fm__dag *dag)
{
	struct fm_node *node;

	if (!dag->u.s || !dag->right || !fm__dag_is_tensor(dag)) {
		FM__TRACE(FM__ERRNO_SOFTWARE);
		return -1;
	}
	if (!(node = fm__malloc(sizeof (struct fm_node)))) {
		FM__TRACE(0);
		return -1;
	}
	memset(node, 0, sizeof (struct fm_node));
	node->expr = NULL;
	node->link = NULL;
	node->init.low = 0.0;
	node->init.high = 0.0;
	node->op = FM_NODE_OP_COMPUTE;
	if (!(node->dtype = fm__utils_type2dtype(dag->type))) {
		free(node);
		FM__TRACE(FM__ERRNO_SOFTWARE);
		return -1;
	}
	if (!(node->name = fm__strdup(dag->u.s))) {
		free(node);
		FM__TRACE(0);
		return -1;
	}
	if (!fm->head) {
		fm->head = fm->tail = node;
	}
	else {
		fm->tail->link = node;
		fm->tail = node;
	}
	if (fm__map_update(fm->map, node->name, node)) {
		FM__TRACE(0);
		return -1;
	}
	if (!(node->expr = fm__expr(fm, dag->right, &node->max_expr_id))) {
		FM__TRACE(0);
		return -1;
	}
	node->geometry = node->expr->geometry;
	return 0;
}

static int
process(struct fm *fm)
{
	const struct fm__dag *dag;

	if (!(dag = fm__parser_dag(fm->parser))) {
		FM__TRACE(FM__ERRNO_SOFTWARE);
		return -1;
	}
	while (dag && (FM__DAG_OP_DECL_LIST == dag->op)) {
		if (FM__DAG_OP_DECL_LET == dag->left->op) {
			/* ignore */
		}
		else if (FM__DAG_OP_DECL_TENSOR == dag->left->op) {
			if (process_tensor(fm, dag->left)) {
				FM__TRACE(0);
				return -1;
			}
		}
		else if (FM__DAG_OP_DECL_COMPUTE == dag->left->op) {
			if (process_compute(fm, dag->left)) {
				FM__TRACE(0);
				return -1;
			}
		}
		else {
			FM__TRACE(FM__ERRNO_SOFTWARE);
			return -1;
		}
		dag = dag->right;
	}
	return 0;
}

static void
init(void)
{
	fm__bigint_init();
	fm__dag_init();
	fm__geometry_init();
	fm__lexer_init();
}

fm_t
fm_open(const char *program, char errstr[FM_ERRSTR_LEN])
{
	struct fm *fm;

	init();
	if (!program || !(*program) || !errstr) {
		if (errstr) {
			snprintf(errstr,
				 FM_ERRSTR_LEN,
				 "1:1: invalid arguments");
		}
		FM__TRACE(FM__ERRNO_ARGUMENTS);
		return NULL;
	}
	errstr[0] = '\0';
	if (!(fm = fm__malloc(sizeof (struct fm)))) {
		snprintf(errstr, FM_ERRSTR_LEN, "1:1: out of memory");
		FM__TRACE(0);
		return NULL;
	}
	memset(fm, 0, sizeof (struct fm));
	fm->map = NULL;
	fm->lexer = NULL;
	fm->parser = NULL;
	fm->head = NULL;
	fm->tail = NULL;
	if (!(fm->map = fm__map_open()) ||
	    !(fm->lexer = fm__lexer_open(program, errstr)) ||
	    !(fm->parser = fm__parser_open(fm->lexer, errstr)) ||
	    process(fm)) {
		if (!errstr[0]) {
			snprintf(errstr,
				 FM_ERRSTR_LEN,
				 "1:1: %s",
				 (FM__ERRNO_MEMORY == errno)
				 ? "out of memory"
				 : (FM__ERRNO_NUMERIC == errno)
				 ? "numeric overflow"
				 : "internal error");
		}
		fm_close(fm);
		FM__TRACE(0);
		return NULL;
	}
	fm__parser_close(fm->parser);
	fm__lexer_close(fm->lexer);
	fm->lexer = NULL;
	fm->parser = NULL;
	errstr[0] = '\0';
	return fm;
}

void
fm_close(fm_t fm)
{
	struct fm_node *node;

	if (fm) {
		while (fm->head) {
			node = fm->head;
			fm->head = node->link;
			fm__expr_free(node->expr);
			free((void *)node->name);
			memset(node, 0, sizeof (struct fm_node));
			free(node);
		}
		fm__parser_close(fm->parser);
		fm__lexer_close(fm->lexer);
		fm__map_close(fm->map);
		memset(fm, 0, sizeof (struct fm));
	}
	free(fm);
}

int
fm_json(fm_t fm, const char *pathname)
{
	FILE *file;

	if (!fm || !pathname || !(*pathname)) {
		FM__TRACE(FM__ERRNO_ARGUMENTS);
		return -1;
	}
	if (!(file = fopen(pathname, "w"))) {
		FM__TRACE(FM__ERRNO_FILE);
		return -1;
	}
	if (fm__json(fm, file)) {
		fclose(file);
		FM__TRACE(0);
		return -1;
	}
	if (fclose(file)) {
		FM__TRACE(FM__ERRNO_FILE);
		return -1;
	}
	return 0;
}

const struct fm_node *
fm_head(fm_t fm)
{
	if (!fm) {
		FM__TRACE(FM__ERRNO_ARGUMENTS);
		return NULL;
	}
	return fm->head;
}

const struct fm_node *
fm_lookup(fm_t fm, const char *name)
{
	if (!fm || !name || !(*name)) {
		FM__TRACE(FM__ERRNO_ARGUMENTS);
		return NULL;
	}
	return fm__map_lookup(fm->map, name);
}
