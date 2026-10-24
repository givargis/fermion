/**
 * Copyright (c) 2025-2026 Tony Givargis
 * University of California, Irvine
 * fm_lexer.c
 */

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <assert.h>

#include "fm_errno.h"
#include "fm_lexer.h"

struct fm__lexer {
	size_t lineno;
	size_t column;
	const char *s; /* start */
	const char *p; /* current */
	const char *error;
	struct fm__lexer_token token;
	struct {
		const char *s;
		enum fm__lexer_op op;
	} maps[80];
};

static const struct {
	const char *s;
	enum fm__lexer_op op;
} KEYWORDS[] = {
	{ "true",          FM__LEXER_OP_TRUE      },
	{ "false",         FM__LEXER_OP_FALSE     },
	{ "bool",          FM__LEXER_OP_BOOL      },
	{ "byte",          FM__LEXER_OP_BYTE      },
	{ "short",         FM__LEXER_OP_SHORT     },
	{ "int",           FM__LEXER_OP_INT       },
	{ "long",          FM__LEXER_OP_LONG      },
	{ "fp8_e4m3",      FM__LEXER_OP_FP8_E4M3  },
	{ "fp8_e5m2",      FM__LEXER_OP_FP8_E5M2  },
	{ "bf16",          FM__LEXER_OP_BF16      },
	{ "fp16",          FM__LEXER_OP_FP16      },
	{ "fp32",          FM__LEXER_OP_FP32      },
	{ "fp64",          FM__LEXER_OP_FP64      },
	{ "float",         FM__LEXER_OP_FP32      },
	{ "double",        FM__LEXER_OP_FP64      },
	{ "unsigned",      FM__LEXER_OP_UNSIGNED  }
};

static int
hex2int(int c)
{
	c = tolower((unsigned char)c);
	if (('0' <= c) && ('9' >= c)) {
		return c - '0';
	}
	if (('a' <= c) && ('f' >= c)) {
		return c - 'a' + 10;
	}
	return -1;
}

static int
operator(const char *s, int *op)
{
	switch (*s) {
	case '{': *op = FM__LEXER_OP_OPENBRACE; return 1;
	case '}': *op = FM__LEXER_OP_CLOSEBRACE; return 1;
	case '[': *op = FM__LEXER_OP_OPENBRACKET; return 1;
	case ']': *op = FM__LEXER_OP_CLOSEBRACKET; return 1;
	case '(': *op = FM__LEXER_OP_OPENPAREN; return 1;
	case ')': *op = FM__LEXER_OP_CLOSEPAREN; return 1;
	case '?': *op = FM__LEXER_OP_QUESTION; return 1;
	case ':': *op = FM__LEXER_OP_COLON; return 1;
	case ',': *op = FM__LEXER_OP_COMMA; return 1;
	case ';': *op = FM__LEXER_OP_SEMICOLON; return 1;
	case '`': *op = FM__LEXER_OP_BACKTICK; return 1;
	case '@': *op = FM__LEXER_OP_AT; return 1;
	case '#': *op = FM__LEXER_OP_HASH; return 1;
	case '$': *op = FM__LEXER_OP_DOLLAR; return 1;
	case '+': *op = FM__LEXER_OP_ADD; return 1;
	case '-': *op = FM__LEXER_OP_SUB; return 1;
	case '*': *op = FM__LEXER_OP_MUL; return 1;
	case '/': *op = FM__LEXER_OP_DIV; return 1;
	case '%': *op = FM__LEXER_OP_MOD; return 1;
	case '~': *op = FM__LEXER_OP_NOT; return 1;
	case '!':
		if ('=' == s[1]) {
			*op = FM__LEXER_OP_NE;
			return 2;
		}
		*op = FM__LEXER_OP_LOGNOT;
		return 1;
	case '<':
		if ('<' == s[1]) {
			*op = FM__LEXER_OP_SHL;
			return 2;
		}
		if ('=' == s[1]) {
			*op = FM__LEXER_OP_LE;
			return 2;
		}
		*op = FM__LEXER_OP_LT;
		return 1;
	case '>':
		if ('>' == s[1]) {
			*op = FM__LEXER_OP_SHR;
			return 2;
		}
		if ('=' == s[1]) {
			*op = FM__LEXER_OP_GE;
			return 2;
		}
		*op = FM__LEXER_OP_GT;
		return 1;
	case '&':
		if ('&' == s[1]) {
			*op = FM__LEXER_OP_LOGAND;
			return 2;
		}
		*op = FM__LEXER_OP_AND;
		return 1;
	case '^':
		if ('^' == s[1]) {
			*op = FM__LEXER_OP_LOGXOR;
			return 2;
		}
		*op = FM__LEXER_OP_XOR;
		return 1;
	case '|':
		if ('|' == s[1]) {
			*op = FM__LEXER_OP_LOGOR;
			return 2;
		}
		*op = FM__LEXER_OP_OR;
		return 1;
	case '=':
		if ('=' == s[1]) {
			*op = FM__LEXER_OP_EQ;
			return 2;
		}
		*op = FM__LEXER_OP_ASSIGN;
		return 1;
	}
	return 0;
}

static size_t
hash(const char *b, const char *e)
{
	size_t k;

	k = 0;
	while (b < e) {
		k = k * 31 + (unsigned char)*b++;
	}
	return k;
}

static char *
strdupl(const char *b, const char *e)
{
	size_t n;
	char *s;

	n = e - b;
	if (!(s = malloc(n + 1))) {
		FM__ERRNO(FM__ERRNO_MEMORY);
		return NULL;
	}
	memcpy(s, b, n);
	s[n] = '\0';
	return s;
}

static int /* bool */
identifier(const char *b, const char *e)
{
	if (('_' == *b) || isalpha((unsigned char)*b)) {
		while (++b < e) {
			if (('_' != *b) && !isalnum((unsigned char)*b)) {
				return 0;
			}
		}
		return 1;
	}
	return 0;
}

static const char *
escape(const char *b, const char *e, unsigned char *c)
{
	int h, l;

	if (++b < e) {
		switch (*b++) {
		case '0' : *c = '\0'; break;
		case 'a' : *c = '\a'; break;
		case 'b' : *c = '\b'; break;
		case 'f' : *c = '\f'; break;
		case 'n' : *c = '\n'; break;
		case 'r' : *c = '\r'; break;
		case 't' : *c = '\t'; break;
		case 'v' : *c = '\v'; break;
		case '\\': *c = '\\'; break;
		case '\'': *c = '\''; break;
		case '"' : *c = '"';  break;
		case 'x' :
			if (2 > (e - b)) {
				return NULL;
			}
			h = hex2int((unsigned char)b[0]);
			l = hex2int((unsigned char)b[1]);
			if ((0 > h) || (0 > l)) {
				return NULL;
			}
			*c = (unsigned char)((h << 4) | l);
			return b + 2;
		default:
			return NULL;
		}
		return b;
	}
	return NULL;
}

static const char *
advance(struct fm__lexer *lexer, const char *s)
{
	if ('\r' == *s) {
		if ('\n' == *++s) {
			++s;
		}
		lexer->lineno += 1;
		lexer->column  = 1;
	}
	else if ('\n' == *s++) {
		lexer->lineno += 1;
		lexer->column  = 1;
	}
	else {
		++lexer->column;
	}
	return s;
}

static const char *
eol(struct fm__lexer *lexer, const char *s)
{
	while (*s) {
		if (('\r' == *s) || ('\n' == *s)) {
			return advance(lexer, s);
		}
		s = advance(lexer, s);
	}
	return s;
}

static const char *
comment(struct fm__lexer *lexer, const char *s)
{
	lexer->column += 2;
	while (*s) {
		if (('*' == s[0]) && ('/' == s[1])) {
			lexer->column += 2;
			return s + 2;
		}
		if (('/' == s[0]) && ('*' == s[1])) {
			lexer->error = "'/*' within block comment";
			FM__ERRNO(FM__ERRNO_SYNTAX);
			return NULL;
		}
		s = advance(lexer, s);
	}
	lexer->error = "unterminated block comment";
	FM__ERRNO(FM__ERRNO_SYNTAX);
	return NULL;
}

static const char *
skip(struct fm__lexer *lexer, const char *s)
{
	for (;;) {
		if (isspace((unsigned char)*s)) {
			s = advance(lexer, s);
		}
		else if (('/' == s[0]) && ('/' == s[1])) {
			lexer->column += 2;
			s = eol(lexer, s + 2);
		}
		else if (('/' == s[0]) && ('*' == s[1])) {
			lexer->token.lineno = lexer->lineno;
			lexer->token.column = lexer->column;
			if (!(s = comment(lexer, s + 2))) {
				FM__ERRNO(0);
				return NULL;
			}
		}
		else {
			break;
		}
	}
	return s;
}

static const char *
quoted(struct fm__lexer *lexer, const char *s)
{
	char delim;

	delim = *s;
	++s;
	++lexer->column;
	while (*s) {
		if (('\n' == *s) || ('\r' == *s)) {
			break;
		}
		else if ('\\' == *s) {
			if (('\n' == s[1]) || ('\r' == s[1]) || !s[1]) {
				lexer->error = "invalid escape sequence";
				FM__ERRNO(FM__ERRNO_SYNTAX);
				return NULL;
			}
			++s;
			++lexer->column;
		}
		else if (delim == *s) {
			++lexer->column;
			return s + 1;
		}
		++s;
		++lexer->column;
	}
	lexer->error = ('"' == delim
			? "unterminated string literal"
			: "unterminated character literal");
	FM__ERRNO(FM__ERRNO_SYNTAX);
	return NULL;
}

static int
character(struct fm__lexer *lexer, const char *b, const char *e)
{
	unsigned char c;
	const char *p;

	if (++b >= --e) {
		lexer->error = "invalid character literal";
		FM__ERRNO(FM__ERRNO_SYNTAX);
		return -1;
	}
	if ('\\' == *b) {
		p = escape(b, e, &c);
	}
	else {
		p = b + 1;
		c = (unsigned char)*b;
	}
	if (!p || (p != e)) {
		lexer->error = "invalid character literal";
		FM__ERRNO(FM__ERRNO_SYNTAX);
		return -1;
	}
	if (!(lexer->token.u.i = fm__bigint_int(c))) {
		lexer->error = "out of memory";
		FM__ERRNO(0);
		return -1;
	}
	return 0;
}

static char *
string(struct fm__lexer *lexer, const char *b, const char *e)
{
	unsigned char c;
	const char *p;
	char *q, *s;

	if (!(s = malloc(e - b - 1))) {
		lexer->error = "out of memory";
		FM__ERRNO(FM__ERRNO_MEMORY);
		return NULL;
	}
	++b;
	--e;
	q = s;
	while (b < e) {
		if ('\\' == *b) {
			if (!(p = escape(b, e, &c))) {
				free(s);
				lexer->error = "invalid escape sequence";
				FM__ERRNO(FM__ERRNO_SYNTAX);
				return NULL;
			}
			b = p;
			if (!c) {
				free(s);
				lexer->error = "NUL within string literal";
				FM__ERRNO(FM__ERRNO_SYNTAX);
				return NULL;
			}
		}
		else {
			c = (unsigned char)*b++;
		}
		*q++ = (char)c;
	}
	*q = '\0';
	return s;
}

static const char *
numeric(struct fm__lexer *lexer, const char *s)
{
	const char *p;
	char *e, *s_;

	/* hexadecimal */

	if (('0' == s[0]) && (('x' == s[1]) || ('X' == s[1]))) {
		p = (s += 2);
		while (isxdigit((unsigned char)*s)) {
			++s;
		}
		if (p == s) {
			lexer->error = "invalid integer literal";
			FM__ERRNO(FM__ERRNO_SYNTAX);
			return NULL;
		}
		if (!(s_ = strdupl(p - 2, s))) {
			lexer->error = "out of memory";
			FM__ERRNO(0);
			return NULL;
		}
		if (!(lexer->token.u.i = fm__bigint_string(s_))) {
			free(s_);
			lexer->error = (FM__ERRNO_ARGUMENTS == errno
					? "integer literal conversion failed"
					: "out of memory");
			FM__ERRNO(0);
			return NULL;
		}
		lexer->token.op = FM__LEXER_OP_LINT;
		free(s_);
		lexer->column += (s - p) + 2;
		return s;
	}

	/* floating-point */

	p = s;
	while (*s) {
		if (('.' == *s) || ('e' == *s) || ('E' == *s)) {
			errno = 0;
			lexer->token.op = FM__LEXER_OP_LREAL;
			lexer->token.u.r = strtod(p, &e);
			if ((e <= s) ||
			    ('e' == *e) || ('E' == *e)) {
				lexer->error =
					"invalid floating-point literal";
				FM__ERRNO(0);
				return NULL;
			}
			if (ERANGE == errno) {
				lexer->error =
					"floating-point literal out of range";
				FM__ERRNO(0);
				return NULL;
			}
			lexer->column += (e - p);
			return e;
		}
		if (!isdigit((unsigned char)*s)) {
			break;
		}
		++s;
	}

	/* decimal */

	if (!(s_ = strdupl(p, s))) {
		lexer->error = "out of memory";
		FM__ERRNO(0);
		return NULL;
	}
	if (!(lexer->token.u.i = fm__bigint_string(s_))) {
		free(s_);
		lexer->error = (FM__ERRNO_ARGUMENTS == errno
				? "integer literal conversion failed"
				: "out of memory");
		FM__ERRNO(0);
		return NULL;
	}
	lexer->token.op = FM__LEXER_OP_LINT;
	free(s_);
	lexer->column += (s - p);
	return s;
}

static void
reset(struct fm__lexer *lexer)
{
	if ((FM__LEXER_OP_LSTRING == lexer->token.op) ||
	    (FM__LEXER_OP_IDENTIFIER == lexer->token.op)) {
		free((void *)lexer->token.u.s);
	}
	if (FM__LEXER_OP_LINT == lexer->token.op) {
		fm__bigint_free(lexer->token.u.i);
	}
	memset(&lexer->token, 0, sizeof (struct fm__lexer_token));
}

static void
dictionary(struct fm__lexer *lexer)
{
	const size_t M = sizeof (lexer->maps) / sizeof (lexer->maps[0]);
	const size_t N = sizeof (KEYWORDS) / sizeof (KEYWORDS[0]);
	size_t i, j, k;
	const char *s;

	for (i=0; i<N; ++i) {
		s = KEYWORDS[i].s;
		k = hash(s, s + strlen(s));
		for (j=0; j<M; ++j) {
			k = (k + 1) % M;
			if (!lexer->maps[k].s) {
				lexer->maps[k].s = s;
				lexer->maps[k].op = KEYWORDS[i].op;
				break;
			}
			if (!strcmp(lexer->maps[k].s, s)) {
				break;
			}
		}
		if (M <= j) {
			FM__ERRNO(FM__ERRNO_SOFTWARE);
			abort();
		}
	}
}

static enum fm__lexer_op
lookup(const struct fm__lexer *lexer, const char *b, const char *e)
{
	const size_t M = sizeof (lexer->maps) / sizeof (lexer->maps[0]);
	size_t j, k, n;

	n = e - b;
	k = hash(b, e);
	for (j=0; j<M; ++j) {
		k = (k + 1) % M;
		if (!lexer->maps[k].s) {
			break;
		}
		if ((strlen(lexer->maps[k].s) == n) &&
		    !strncmp(lexer->maps[k].s, b, n)) {
			return lexer->maps[k].op;
		}
	}
	return FM__LEXER_OP_UNKNOWN;
}

void
fm__lexer_init(void)
{
	static const char ASCII[] =
		" !\"#$%&'()*+,-./"
		"0123456789:;<=>?"
		"@ABCDEFGHIJKLMNO"
		"PQRSTUVWXYZ[\\]^_"
		"`abcdefghijklmno"
		"pqrstuvwxyz{|}~";
	size_t i;

	if ((8 != CHAR_BIT) ||
	    ('\a' !=  7) ||
	    ('\b' !=  8) ||
	    ('\t' !=  9) ||
	    ('\n' != 10) ||
	    ('\v' != 11) ||
	    ('\f' != 12) ||
	    ('\r' != 13)) {
		FM__ERRNO(FM__ERRNO_ARCHITECTURE);
		abort();
	}
	for (i=0; i<(sizeof (ASCII) - 1); ++i) {
		if ((32 + i) != (unsigned char)ASCII[i]) {
			FM__ERRNO(FM__ERRNO_ARCHITECTURE);
			abort();
		}
	}
}

fm__lexer_t
fm__lexer_open(const char *s)
{
	struct fm__lexer *lexer;

	assert( s );

	if (!(lexer = malloc(sizeof (struct fm__lexer)))) {
		FM__ERRNO(FM__ERRNO_MEMORY);
		return NULL;
	}
	memset(lexer, 0, sizeof (struct fm__lexer));
	lexer->lineno = 1;
	lexer->column = 1;
	lexer->s = s;
	lexer->p = s;
	dictionary(lexer);
	return lexer;
}

void
fm__lexer_close(fm__lexer_t lexer)
{
	if (lexer) {
		reset(lexer);
		free(lexer);
	}
}

const struct fm__lexer_token *
fm__lexer_next(fm__lexer_t lexer)
{
	const char *s, *p;
	int n, op;

	assert( lexer );

	if (!lexer->p) {
		FM__ERRNO(FM__ERRNO_ARGUMENTS);
		return NULL;
	}
	reset(lexer);
	if (!(s = skip(lexer, lexer->p))) {
		lexer->p = NULL;
		FM__ERRNO(0);
		return NULL;
	}
	lexer->token.lineno = lexer->lineno;
	lexer->token.column = lexer->column;

	/* eof */

	if (!*s) {
		lexer->token.op = FM__LEXER_OP_EOF;
		lexer->p = s;
		return &lexer->token;
	}

	/* character */

	if ('\'' == *s) {
		p = s;
		if (!(s = quoted(lexer, s)) || character(lexer, p, s)) {
			lexer->p = NULL;
			FM__ERRNO(0);
			return NULL;
		}
		lexer->token.op = FM__LEXER_OP_LINT;
		lexer->p = s;
		return &lexer->token;
	}

	/* string */

	if ('\"' == *s) {
		p = s;
		if (!(s = quoted(lexer, s)) ||
		    !(lexer->token.u.s = string(lexer, p, s))) {
			lexer->p = NULL;
			FM__ERRNO(0);
			return NULL;
		}
		lexer->token.op = FM__LEXER_OP_LSTRING;
		lexer->p = s;
		return &lexer->token;
	}

	/* numeric */

	if (isdigit((unsigned char)*s) ||
	    (('.' == s[0]) && isdigit((unsigned char)s[1]))) {
		if (!(s = numeric(lexer, s))) {
			lexer->p = NULL;
			FM__ERRNO(0);
			return NULL;
		}
		lexer->p = s;
		return &lexer->token;
	}

	/* operator */

	if ((n = operator(s, &op))) {
		lexer->token.op = op;
		lexer->p = s + n;
		lexer->column += n;
		return &lexer->token;
	}

	/* keyword? */

	p = s;
	do {
		++s;
	}
	while (*s &&
	       !isspace((unsigned char)*s) &&
	       ('\'' != *s) &&
	       ('\"' != *s) &&
	       !operator(s, &op));
	lexer->column += (s - p);
	if ((op = lookup(lexer, p, s))) {
		if (FM__LEXER_OP_TRUE == op) {
			lexer->token.op = FM__LEXER_OP_LBOOL;
			lexer->token.u.b = 1;
			lexer->p = s;
			return &lexer->token;
		}
		if (FM__LEXER_OP_FALSE == op) {
			lexer->token.op = FM__LEXER_OP_LBOOL;
			lexer->token.u.b = 0;
			lexer->p = s;
			return &lexer->token;
		}
		lexer->token.op = op;
		lexer->p = s;
		return &lexer->token;
	}

	/* identifier? */

	if (!identifier(p, s)) {
		lexer->p = NULL;
		lexer->error = "unrecognized token";
		FM__ERRNO(FM__ERRNO_SYNTAX);
		return NULL;
	}
	if (!(lexer->token.u.s = strdupl(p, s))) {
		lexer->p = NULL;
		lexer->error = "out of memory";
		FM__ERRNO(0);
		return NULL;
	}
	lexer->token.op = FM__LEXER_OP_IDENTIFIER;
	lexer->p = s;
	return &lexer->token;
}

const char *
fm__lexer_error(fm__lexer_t lexer, size_t *lineno, size_t *column)
{
	assert( lexer && lineno && column );

	*lineno = lexer->token.lineno;
	*column = lexer->token.column;
	return lexer->error;
}
