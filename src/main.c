/**
 * Copyright (c) 2025-2026 Tony Givargis
 * University of California, Irvine
 * main.c
 */

#include "fm_utils.h"
#include "fm_lexer.h"
#include "fm_bigint.h"

static void
stage(void)
{
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
