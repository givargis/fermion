/**
 * Copyright (c) 2025-2026 Tony Givargis
 * University of California, Irvine
 * fm_lexer.h
 */

#ifndef FM_LEXER_H
#define FM_LEXER_H

#include "fm_bigint.h"

enum fm__lexer_op {
	FM__LEXER_OP_UNKNOWN,
	FM__LEXER_OP_EOF,
	FM__LEXER_OP_LINT, /* u.i */
	FM__LEXER_OP_LREAL, /* u.d */
	FM__LEXER_OP_LBOOL, /* u.b */
	FM__LEXER_OP_LSTRING, /* u.s */
	FM__LEXER_OP_IDENTIFIER, /* u.s */
	FM__LEXER_OP_KEYWORDS, /* sentinel */
	FM__LEXER_OP_BF16,
	FM__LEXER_OP_BOOL,
	FM__LEXER_OP_BROADCAST,
	FM__LEXER_OP_BYTE,
	FM__LEXER_OP_CAST,
	FM__LEXER_OP_DOUBLE,
	FM__LEXER_OP_FALSE, /* -> LBOOL */
	FM__LEXER_OP_FLOAT,
	FM__LEXER_OP_FP16,
	FM__LEXER_OP_FP8_E4M3,
	FM__LEXER_OP_FP8_E5M2,
	FM__LEXER_OP_INT,
	FM__LEXER_OP_IOTA,
	FM__LEXER_OP_LONG,
	FM__LEXER_OP_NORMAL,
	FM__LEXER_OP_ONES,
	FM__LEXER_OP_RESHAPE,
	FM__LEXER_OP_SHORT,
	FM__LEXER_OP_TRUE, /* -> LBOOL */
	FM__LEXER_OP_UNIFORM,
	FM__LEXER_OP_UNSIGNED,
	FM__LEXER_OP_ZEROS,
	FM__LEXER_OP_DOTSEED,
	FM__LEXER_OP_OPERATORS, /* sentinel */
	FM__LEXER_OP_ADD,
	FM__LEXER_OP_SUB,
	FM__LEXER_OP_MUL,
	FM__LEXER_OP_DIV,
	FM__LEXER_OP_MOD,
	FM__LEXER_OP_SHL,
	FM__LEXER_OP_SHR,
	FM__LEXER_OP_AND,
	FM__LEXER_OP_XOR,
	FM__LEXER_OP_OR,
	FM__LEXER_OP_NOT,
	FM__LEXER_OP_LOGAND,
	FM__LEXER_OP_LOGXOR,
	FM__LEXER_OP_LOGOR,
	FM__LEXER_OP_LOGNOT,
	FM__LEXER_OP_LT,
	FM__LEXER_OP_GT,
	FM__LEXER_OP_LE,
	FM__LEXER_OP_GE,
	FM__LEXER_OP_EQ,
	FM__LEXER_OP_NE,
	FM__LEXER_OP_QUESTION,
	FM__LEXER_OP_COLON,
	FM__LEXER_OP_ASSIGN,
	FM__LEXER_OP_COMMA,
	FM__LEXER_OP_SEMICOLON,
	FM__LEXER_OP_OPENBRACE,
	FM__LEXER_OP_CLOSEBRACE,
	FM__LEXER_OP_OPENBRACKET,
	FM__LEXER_OP_CLOSEBRACKET,
	FM__LEXER_OP_OPENPAREN,
	FM__LEXER_OP_CLOSEPAREN,
	FM__LEXER_OP_BACKTICK,
	FM__LEXER_OP_AT,
	FM__LEXER_OP_END
};

struct fm__lexer_token {
	size_t lineno;
	size_t column;
	enum fm__lexer_op op;
	union {
		int b; /* bool */
		double d;
		const char *s;
		fm__bigint_t i;
	} u;
};

typedef struct fm__lexer *fm__lexer_t;

void fm__lexer_init(void);

fm__lexer_t fm__lexer_open(const char *program, char errstr[FM_ERRSTR_LEN]);

void fm__lexer_close(fm__lexer_t lexer);

size_t fm__lexer_size(fm__lexer_t lexer);

const struct fm__lexer_token *fm__lexer_lookup(fm__lexer_t lexer, size_t i);

#endif
