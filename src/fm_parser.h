/**
 * Copyright (c) 2025-2026 Tony Givargis
 * University of California, Irvine
 * fm_parser.h
 */

#ifndef FM_PARSER_H
#define FM_PARSER_H

#include "fm_bigint.h"

enum fm__parser_op {
	FM__PARSER_OP_LINT,       /* u.i */
	FM__PARSER_OP_LBOOL,      /* u.b */
	FM__PARSER_OP_LREAL,      /* u.r */
	FM__PARSER_OP_LSTRING,    /* u.s */
	FM__PARSER_OP_IDENTIFIER, /* u.s */
	FM__PARSER_OP_ADD,
	FM__PARSER_OP_SUB,
	FM__PARSER_OP_MUL,
	FM__PARSER_OP_DIV,
	FM__PARSER_OP_MOD
};

struct fm__parser_node {
	enum fm__parser_op op;
	union {
		int b;
		double r;
		const char *s;
		fm__bigint_t i;
		struct {
			int left;
			int right;
		} children;
	} u;
};

typedef struct fm__parser *fm__parser_t;

fm__parser_t fm__parser_open(const char *s);

void fm__parser_close(fm__parser_t parser);

#endif
