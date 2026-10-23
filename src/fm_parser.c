/**
 * Copyright (c) 2025-2026 Tony Givargis
 * University of California, Irvine
 * fm_parser.h
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "fm_trace.h"
#include "fm_lexer.h"
#include "fm_parser.h"

#define STACK_SIZE 128

struct fm__parser {
	int stop; /* bool */
	size_t top;
	fm__lexer_t lexer;
	struct fm__lexer_token stack[STACK_SIZE];
};

static int
parse(struct fm__parser *parser)
{
	const struct fm__lexer_token *token;
	size_t lineno, column;
	const char *e;

	do {
		if (!(token = fm__lexer_next(parser->lexer))) {
			e = fm__lexer_error(parser->lexer, &lineno, &column);
			printf("error: %lu:%lu: %s\n",
			       (unsigned long)lineno,
			       (unsigned long)column,
			       e);
			FM__TRACE(NULL);
			return -1;
		}
		printf("%lu:%lu: op=%d u.s=[%s]\n",
		       (unsigned long)token->lineno,
		       (unsigned long)token->column,
		       token->op,
		       token->op == FM__LEXER_OP_IDENTIFIER ? token->u.s : "");
	}
	while (FM__LEXER_OP_EOF != token->op);
	return 0;
}

fm__parser_t
fm__parser_open(const char *s)
{
	struct fm__parser *parser;

	assert( s );

	if (!(parser = malloc(sizeof (struct fm__parser)))) {
		FM__TRACE("out of memory");
		return NULL;
	}
	memset(parser, 0, sizeof (struct fm__parser));
	if (!(parser->lexer = fm__lexer_open(s)) || parse(parser)) {
		fm__parser_close(parser);
		FM__TRACE(NULL);
		return NULL;
	}
	return parser;
}

void
fm__parser_close(fm__parser_t parser)
{
	if (parser) {
		fm__lexer_close(parser->lexer);
		free(parser);
	}
}
