/**
 * Copyright (c) 2025-2026 Tony Givargis
 * University of California, Irvine
 * main.c
 */

#include <assert.h>

#include "fm_utils.h"
#include "fm_lexer.h"
#include "fm_parser.h"
#include "fm_bigint.h"

static void
stage(void)
{
	const char *s = "1+2";

	const struct fm__parser_node *node;
	fm__parser_t parser;

	parser = fm__parser_open(s);

	node = fm__parser_root(parser);

	assert( node );

	fm__parser_close(parser);
}

int
main(int argc, char *argv[])
{
	(void)argc;
	(void)argv;

	fm__utils_init();
	fm__lexer_init();
	fm__bigint_init();
	stage();
	return 0;
}
