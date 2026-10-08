/**
 * Copyright (c) 2025-2026 Tony Givargis
 * University of California, Irvine
 * fm_parser.h
 */

#ifndef FM_PARSER_H
#define FM_PARSER_H

#include "fm_dag.h"
#include "fm_lexer.h"

typedef struct fm__parser *fm__parser_t;

fm__parser_t fm__parser_open(fm__lexer_t lexer, char errstr[FM_ERRSTR_LEN]);

void fm__parser_close(fm__parser_t parser);

const struct fm__dag *fm__parser_dag(fm__parser_t parser);

char *fm__parser_errstr(fm__parser_t parser);

#endif
