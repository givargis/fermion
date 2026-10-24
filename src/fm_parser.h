/**
 * Copyright (c) 2025-2026 Tony Givargis
 * University of California, Irvine
 * fm_parser.h
 */

#ifndef FM_PARSER_H
#define FM_PARSER_H

struct fm__parser_node {
	int id;
};

typedef struct fm__parser *fm__parser_t;

fm__parser_t fm__parser_open(const char *s);

void fm__parser_close(fm__parser_t parser);

const struct fm__parser_node *fm__parser_root(fm__parser_t parser);

#endif
