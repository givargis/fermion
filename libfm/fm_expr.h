/**
 * Copyright (c) 2025-2026 Tony Givargis
 * University of California, Irvine
 * fm_expr.h
 */

#ifndef FM_EXPR_H
#define FM_EXPR_H

#include "fm_dag.h"

struct fm_expr *fm__expr(fm_t lang, const struct fm__dag *dag, size_t *id);

void fm__expr_free(struct fm_expr *expr);

#endif
