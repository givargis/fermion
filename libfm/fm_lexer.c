/**
 * Copyright (c) 2025-2026 Tony Givargis
 * University of California, Irvine
 * fm_lexer.c
 */

#include "fm_utils.h"
#include "fm_lexer.h"

#define MAX_OPERATOR_LENGTH 3 /* at least the longest operator */

#define ERR FM__UTILS_ERR_LEXER

struct fm__lexer {
	int stop;
	char *errstr; /* borrowed */
	char *program;
	size_t lineno;
	size_t column;
	struct {
		const char *name;
		enum fm__lexer_op op;
	} maps[FM__LEXER_OP_END * 2]; /* ~ 2 x (KEYWORDS + OPERATORS) */
	size_t size;
	size_t capacity;
	struct fm__lexer_token *tokens;
};

static const struct {
	const char *name;
	enum fm__lexer_op op;
} KEYWORDS[] = {
	{ "bf16",          FM__LEXER_OP_BF16      },
	{ "bool",          FM__LEXER_OP_BOOL      },
	{ "broadcast",     FM__LEXER_OP_BROADCAST },
	{ "byte",          FM__LEXER_OP_BYTE      },
	{ "cast",          FM__LEXER_OP_CAST      },
	{ "double",        FM__LEXER_OP_DOUBLE    },
	{ "false",         FM__LEXER_OP_FALSE     },
	{ "float",         FM__LEXER_OP_FLOAT     },
	{ "fp16",          FM__LEXER_OP_FP16      },
	{ "fp8_e4m3",      FM__LEXER_OP_FP8_E4M3  },
	{ "fp8_e5m2",      FM__LEXER_OP_FP8_E5M2  },
	{ "int",           FM__LEXER_OP_INT       },
	{ "iota",          FM__LEXER_OP_IOTA      },
	{ "long",          FM__LEXER_OP_LONG      },
	{ "normal",        FM__LEXER_OP_NORMAL    },
	{ "ones",          FM__LEXER_OP_ONES      },
	{ "reshape",       FM__LEXER_OP_RESHAPE   },
	{ "short",         FM__LEXER_OP_SHORT     },
	{ "true",          FM__LEXER_OP_TRUE      },
	{ "uniform",       FM__LEXER_OP_UNIFORM   },
	{ "unsigned",      FM__LEXER_OP_UNSIGNED  },
	{ "zeros",         FM__LEXER_OP_ZEROS     },
	{ ".seed",         FM__LEXER_OP_DOTSEED   }
};

static const struct {
	const char *name;
	enum fm__lexer_op op;
} OPERATORS[] = {
	{ "+",  FM__LEXER_OP_ADD          },
	{ "-",  FM__LEXER_OP_SUB          },
	{ "*",  FM__LEXER_OP_MUL          },
	{ "/",  FM__LEXER_OP_DIV          },
	{ "%",  FM__LEXER_OP_MOD          },
	{ "<<", FM__LEXER_OP_SHL          },
	{ ">>", FM__LEXER_OP_SHR          },
	{ "&",  FM__LEXER_OP_AND          },
	{ "^",  FM__LEXER_OP_XOR          },
	{ "|",  FM__LEXER_OP_OR           },
	{ "~",  FM__LEXER_OP_NOT          },
	{ "&&", FM__LEXER_OP_LOGAND       },
	{ "^^", FM__LEXER_OP_LOGXOR       },
	{ "||", FM__LEXER_OP_LOGOR        },
	{ "!",  FM__LEXER_OP_LOGNOT       },
	{ "<",  FM__LEXER_OP_LT           },
	{ ">",  FM__LEXER_OP_GT           },
	{ "<=", FM__LEXER_OP_LE           },
	{ ">=", FM__LEXER_OP_GE           },
	{ "==", FM__LEXER_OP_EQ           },
	{ "!=", FM__LEXER_OP_NE           },
	{ "?",  FM__LEXER_OP_QUESTION     },
	{ ":",  FM__LEXER_OP_COLON        },
	{ "=",  FM__LEXER_OP_ASSIGN       },
	{ ",",  FM__LEXER_OP_COMMA        },
	{ ";",  FM__LEXER_OP_SEMICOLON    },
	{ "{",  FM__LEXER_OP_OPENBRACE    },
	{ "}",  FM__LEXER_OP_CLOSEBRACE   },
	{ "[",  FM__LEXER_OP_OPENBRACKET  },
	{ "]",  FM__LEXER_OP_CLOSEBRACKET },
	{ "(",  FM__LEXER_OP_OPENPAREN    },
	{ ")",  FM__LEXER_OP_CLOSEPAREN   },
	{ "`",  FM__LEXER_OP_BACKTICK     },
	{ "@",  FM__LEXER_OP_AT           }
};

static size_t
hash(const char *s, size_t n)
{
	size_t i, h;

	for (i=h=0; i<n; ++i) {
		h = h * 31 + (unsigned char)s[i];
	}
	return h;
}

static void
populate(struct fm__lexer *lexer, const char *name, enum fm__lexer_op op)
{
	const size_t N = FM__ARRAY_SIZE(lexer->maps);
	size_t j;

	j = hash(name, strlen(name));
	for (size_t i=0; i<N; ++i) {
		j = (j + 1) % N;
		if (!lexer->maps[j].name) {
			lexer->maps[j].name = name;
			lexer->maps[j].op = op;
			return;
		}
		if (!strcmp(lexer->maps[j].name, name)) {
			FM__TRACE(FM__ERRNO_SOFTWARE);
			abort();
		}
	}
	FM__TRACE(FM__ERRNO_SOFTWARE);
	abort();
}

static enum fm__lexer_op
lookup(fm__lexer_t lexer, const char *b, const char *e)
{
	const size_t N = FM__ARRAY_SIZE(lexer->maps);
	size_t j, n;

	assert( b && (b < e) );

	n = e - b;
	j = hash(b, n);
	for (size_t i=0; i<N; ++i) {
		j = (j + 1) % N;
		if (!lexer->maps[j].name) {
			break;
		}
		if ((strlen(lexer->maps[j].name) == n) &&
		    !strncmp(lexer->maps[j].name, b, n)) {
			return lexer->maps[j].op;
		}
	}
	return FM__LEXER_OP_UNKNOWN;
}

static char *
strdupl(const char *p, const char *s_)
{
	size_t n;
	char *s;

	assert( p < s_ );

	n = s_ - p;
	if (!(s = fm__malloc(n + 1))) {
		FM__TRACE(0);
		return NULL;
	}
	memcpy(s, p, n);
	s[n] = '\0';
	return s;
}

static int
is_identifier(const char *p, const char *s)
{
	assert( p < s );

	if (('_' == (*p)) || isalpha((unsigned char)(*p))) {
		while (++p < s) {
			if (('_' != (*p)) && !isalnum((unsigned char)(*p))) {
				return 0;
			}
		}
		return 1;
	}
	return 0;
}

static struct fm__lexer_token *
allocate(struct fm__lexer *lexer, enum fm__lexer_op op, size_t width)
{
	struct fm__lexer_token *tokens;
	struct fm__lexer_token *token;
	size_t n, m;

	if (lexer->size >= lexer->capacity) {
		m = lexer->capacity ? lexer->capacity : 128;
		if (fm__mul_size(m, 2, &m) ||
		    fm__mul_size(m, sizeof (lexer->tokens[0]), &n)) {
			ERR(lexer, "out of memory");
			FM__TRACE(0);
			return NULL;
		}
		if (!(tokens = fm__realloc(lexer->tokens, n))) {
			ERR(lexer, "out of memory");
			FM__TRACE(0);
			return NULL;
		}
		lexer->tokens = tokens;
		lexer->capacity = m;
	}
	token = &lexer->tokens[lexer->size++];
	memset(token, 0, sizeof (lexer->tokens[0]));
	token->op = op;
	token->lineno = lexer->lineno;
	token->column = lexer->column - width;
	return token;
}

static const char *
eat_eol(struct fm__lexer *lexer, const char *s)
{
	while (*s) {
		if ('\n' == (*s)) {
			lexer->lineno += 1;
			lexer->column  = 1;
			return s + 1;
		}
		++s;
		++lexer->column;
	}
	return s;
}

static const char *
eat_comment(struct fm__lexer *lexer, const char *s)
{
	s += 2;
	lexer->column += 2;
	while (*s) {
		if ('\n' == (*s)) {
			lexer->lineno += 1;
			lexer->column  = 0;
		}
		else if (('/' == s[0]) && ('*' == s[1])) {
			ERR(lexer, "'/*' within block comment");
			FM__TRACE(FM__ERRNO_SYNTAX);
			return NULL;
		}
		else if (('*' == s[0]) && ('/' == s[1])) {
			lexer->column += 2;
			return s + 2;
		}
		++s;
		++lexer->column;
	}
	ERR(lexer, "unterminated block comment");
	FM__TRACE(FM__ERRNO_SYNTAX);
	return NULL;
}

static const char *
eat_quoted(struct fm__lexer *lexer, const char *s)
{
	char delim;

	delim = (*s);
	++s;
	++lexer->column;
	while (*s) {
		if ('\n' == (*s)) {
			break;
		}
		else if ('\\' == (*s)) {
			if (('\n' == s[1]) || ('\r' == s[1]) || !s[1]) {
				ERR(lexer, "invalid escape sequence");
				FM__TRACE(FM__ERRNO_SYNTAX);
				return NULL;
			}
			++s;
			++lexer->column;
		}
		else if (delim == (*s)) {
			++lexer->column;
			return s + 1;
		}
		++s;
		++lexer->column;
	}
	ERR(lexer,
	    ('"' == delim)
	    ? "unterminated string literal"
	    : "unterminated character literal");
	FM__TRACE(FM__ERRNO_SYNTAX);
	return NULL;
}

static const char *
eat_numeric(struct fm__lexer *lexer, const char *s)
{
	struct fm__lexer_token *token;
	const char *p, *s_;

	/* int (hex) */

	if (('0' == s[0]) && (('x' == s[1]) || ('X' == s[1]))) {
		p = s;
		s += 2;
		while (isxdigit((unsigned char)(*s))) {
			++s;
		}
		if (!(s_ = strdupl(p, s))) {
			ERR(lexer, "out of memory");
			FM__TRACE(0);
			return NULL;
		}
		if (!(token = allocate(lexer, FM__LEXER_OP_LINT, 0))) {
			free((void *)s_);
			FM__TRACE(0);
			return NULL;
		}
		if (!(token->u.i = fm__bigint_string(s_))) {
			free((void *)s_);
			ERR(lexer,
			    (FM__ERRNO_MEMORY == errno)
			    ? "out of memory"
			    : "invalid integer constant");
			FM__TRACE(0);
			return NULL;
		}
		free((void *)s_);
		lexer->column += (s - p);
		return s;
	}

	/* real */

	p = s;
	while (*s) {
		if (('.' == (*s)) || ('e' == (*s)) || ('E' == (*s))) {
			char *e;
			double d;
			errno = 0;
			d = strtod(p, &e);
			if (e <= s) {
				ERR(lexer, "invalid floating-point constant");
				FM__TRACE(FM__ERRNO_SYNTAX);
				return NULL;
			}
			if (ERANGE == errno) {
				ERR(lexer,
				    "floating-point constant out of range");
				FM__TRACE(FM__ERRNO_SYNTAX);
				return NULL;
			}
			if (!(token = allocate(lexer,
					       FM__LEXER_OP_LREAL,
					       0))) {
				FM__TRACE(0);
				return NULL;
			}
			token->u.d = d;
			lexer->column += (e - p);
			return e;
		}
		if (!isdigit((unsigned char)(*s))) {
			break;
		}
		++s;
	}

	/* int (dec) */

	if (!(s_ = strdupl(p, s))) {
		ERR(lexer, "out of memory");
		FM__TRACE(0);
		return NULL;
	}
	if (!(token = allocate(lexer, FM__LEXER_OP_LINT, 0))) {
		free((void *)s_);
		FM__TRACE(0);
		return NULL;
	}
	if (!(token->u.i = fm__bigint_string(s_))) {
		free((void *)s_);
		ERR(lexer,
		    (FM__ERRNO_MEMORY == errno)
		    ? "out of memory"
		    : "invalid integer constant");
		FM__TRACE(0);
		return NULL;
	}
	free((void *)s_);
	lexer->column += (s - p);
	return s;
}

static int
process(struct fm__lexer *lexer, const char *b, const char *e)
{
	struct fm__lexer_token *token;
	enum fm__lexer_op op;

	if (b < e) {
		if ((op = lookup(lexer, b, e))) {
			if (FM__LEXER_OP_FALSE == op) {
				if (!(token = allocate(lexer,
						       FM__LEXER_OP_LBOOL,
						       e - b))) {
					FM__TRACE(0);
					return -1;
				}
				token->u.b = 0;
				return 0;
			}
			if (FM__LEXER_OP_TRUE == op) {
				if (!(token = allocate(lexer,
						       FM__LEXER_OP_LBOOL,
						       e - b))) {
					FM__TRACE(0);
					return -1;
				}
				token->u.b = 1;
				return 0;
			}
			if (!allocate(lexer, op, e - b)) {
				FM__TRACE(0);
				return -1;
			}
			return 0;
		}
		if (is_identifier(b, e)) {
			if (!(token = allocate(lexer,
					       FM__LEXER_OP_IDENTIFIER,
					       e - b)) ||
			    !(token->u.s = strdupl(b, e))) {
				ERR(lexer, "out of memory");
				FM__TRACE(0);
				return -1;
			}
			return 0;
		}
		ERR(lexer, "unrecognized token");
		FM__TRACE(FM__ERRNO_SYNTAX);
		return -1;
	}
	return 0;
}

static const char *
operator(struct fm__lexer *lexer, const char *p, const char *s)
{
	enum fm__lexer_op op;
	size_t i;

	i = MAX_OPERATOR_LENGTH;
	while (i) {
		if ((op = lookup(lexer, s, s + i))) {
			if (FM__LEXER_OP_OPERATORS < op) {
				if (process(lexer, p, s) ||
				    !allocate(lexer, op, 0)) {
					FM__TRACE(0);
					return NULL;
				}
				lexer->column += i;
				return s + i;
			}
		}
		--i;
	}
	return s;
}

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

static const char *
escape(const char *b, const char *e, unsigned char *c)
{
	int h, l;

	assert( b && (b < e) && ('\\' == b[0]) && c );

	if (++b < e) {
		switch (*b++) {
		case '0' : (*c) = '\0'; break;
		case 'a' : (*c) = '\a'; break;
		case 'b' : (*c) = '\b'; break;
		case 'f' : (*c) = '\f'; break;
		case 'n' : (*c) = '\n'; break;
		case 'r' : (*c) = '\r'; break;
		case 't' : (*c) = '\t'; break;
		case 'v' : (*c) = '\v'; break;
		case '\\': (*c) = '\\'; break;
		case '\'': (*c) = '\''; break;
		case '"' : (*c) = '"';  break;
		case 'x' :
		case 'X' :
			if (2 > (e - b)) {
				return NULL;
			}
			h = hex2int((unsigned char)b[0]);
			l = hex2int((unsigned char)b[1]);
			if ((0 > h) || (0 > l)) {
				return NULL;
			}
			(*c) = (unsigned char)((h << 4) | l);
			return b + 2;
		default:
			return NULL;
		}
		return b;
	}
	return NULL;
}

static fm__bigint_t
parse_char(struct fm__lexer *lexer, const char *b, const char *e)
{
	unsigned char c;
	fm__bigint_t i;
	const char *p;

	assert( b && (b < e) && ('\'' == b[0]) && ('\'' == e[-1]) );

	if (++b >= --e) {
		ERR(lexer, "invalid character literal");
		FM__TRACE(FM__ERRNO_SYNTAX);
		return NULL;
	}
	if ('\\' == b[0]) {
		p = escape(b, e, &c);
	}
	else {
		p = b + 1;
		c = (unsigned char)b[0];
	}
	if (!p || (p != e)) {
		ERR(lexer, "invalid character literal");
		FM__TRACE(FM__ERRNO_SYNTAX);
		return NULL;
	}
	if (!(i = fm__bigint_int(c))) {
		ERR(lexer, "out of memory");
		FM__TRACE(0);
		return NULL;
	}
	return i;
}

static char *
parse_string(struct fm__lexer *lexer, const char *b, const char *e)
{
	unsigned char c;
	const char *p;
	char *q, *s;

	assert( b && (b < e) && ('"' == b[0]) && ('"' == e[-1]) );

	if (!(s = fm__malloc(e - b - 1))) {
		ERR(lexer, "out of memory");
		FM__TRACE(0);
		return NULL;
	}
	q = s;
	++b;
	--e;
	while (b < e) {
		if ('\\' == b[0]) {
			if (!(p = escape(b, e, &c))) {
				free(s);
				ERR(lexer, "invalid escape sequence");
				FM__TRACE(FM__ERRNO_SYNTAX);
				return NULL;
			}
			b = p;
			if (!c) {
				free(s);
				ERR(lexer, "NUL escape within string literal");
				FM__TRACE(FM__ERRNO_SYNTAX);
				return NULL;
			}
		}
		else {
			c = (unsigned char)(*b++);
		}
		(*q++) = (char)c;
	}
	(*q) = '\0';
	return s;
}

static int
tokenize(struct fm__lexer *lexer)
{
	struct fm__lexer_token *token;
	const char *s, *p;

	s = lexer->program;
	p = lexer->program;
	lexer->lineno = 1;
	lexer->column = 1;
	while (*s) {
		if (isspace((unsigned char)(*s))) {
			if (process(lexer, p, s)) {
				FM__TRACE(0);
				return -1;
			}
			if ('\n' == (*s)) {
				lexer->lineno += 1;
				lexer->column  = 0;
			}
			++s;
			++lexer->column;
			p = s;
		}
		else if (('/' == s[0]) && ('/' == s[1])) {
			if (process(lexer, p, s)) {
				FM__TRACE(0);
				return -1;
			}
			s = eat_eol(lexer, s);
			p = s;
		}
		else if (('/' == s[0]) && ('*' == s[1])) {
			if (process(lexer, p, s)) {
				FM__TRACE(0);
				return -1;
			}
			if (!(s = eat_comment(lexer, s))) {
				FM__TRACE(0);
				return -1;
			}
			p = s;
		}
		else if ('\'' == s[0]) {
			if (process(lexer, p, s)) {
				FM__TRACE(0);
				return -1;
			}
			p = s;
			if (!(s = eat_quoted(lexer, s))) {
				FM__TRACE(0);
				return -1;
			}
			if (!(token = allocate(lexer,
					       FM__LEXER_OP_LINT,
					       s - p)) ||
			    !(token->u.i = parse_char(lexer, p, s))) {
				FM__TRACE(0);
				return -1;
			}
			p = s;
		}
		else if ('"' == s[0]) {
			if (process(lexer, p, s)) {
				FM__TRACE(0);
				return -1;
			}
			p = s;
			if (!(s = eat_quoted(lexer, s))) {
				FM__TRACE(0);
				return -1;
			}
			if (!(token = allocate(lexer,
					       FM__LEXER_OP_LSTRING,
					       s - p)) ||
			    !(token->u.s = parse_string(lexer, p, s))) {
				FM__TRACE(0);
				return -1;
			}
			p = s;
		}
		else if ((p == s) &&
			 ((('.' == s[0]) && isdigit((unsigned char)s[1])) ||
			  isdigit((unsigned char)(*s)))) {
			if (!(s = eat_numeric(lexer, p))) {
				FM__TRACE(0);
				return -1;
			}
			p = s;
		}
		else {
			const char *t;
			if (!(t = operator(lexer, p, s))) {
				FM__TRACE(0);
				return -1;
			}
			if (s == t) {
				++s;
				++lexer->column;
			}
			else {
				s = t;
				p = s;
			}
		}
	}
	if (process(lexer, p, s) || !allocate(lexer, FM__LEXER_OP_EOF, 0)) {
		FM__TRACE(0);
		return -1;
	}
	return 0;
}

static void
unix_copy(struct fm__lexer *lexer, const char *s)
{
	char *p;

	p = lexer->program;
	while (*s) {
		if ('\r' == (*s)) {
			(*p++) = '\n';
			if ('\n' == s[1]) {
				s += 2;
			}
			else {
				++s;
			}
		}
		else {
			(*p++) = (*s++);
		}
	}
	memset(p, '\0', MAX_OPERATOR_LENGTH + 1);
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

	if (('\a' !=  7) ||
	    ('\b' !=  8) ||
	    ('\t' !=  9) ||
	    ('\n' != 10) ||
	    ('\v' != 11) ||
	    ('\f' != 12) ||
	    ('\r' != 13)) {
		FM__TRACE(FM__ERRNO_ARCHITECTURE);
		abort();
	}
	for (size_t i=0; i<(sizeof (ASCII) - 1); ++i) {
		if ((32 + i) != (unsigned char)ASCII[i]) {
			FM__TRACE(FM__ERRNO_ARCHITECTURE);
			abort();
		}
	}
}

fm__lexer_t
fm__lexer_open(const char *program, char errstr[FM_ERRSTR_LEN])
{
	struct fm__lexer *lexer;
	size_t n;

	assert( program && (*program) && errstr );

	if (!(lexer = fm__malloc(sizeof (struct fm__lexer)))) {
		snprintf(errstr, FM_ERRSTR_LEN, "1:1: out of memory");
		FM__TRACE(0);
		return NULL;
	}
	memset(lexer, 0, sizeof (struct fm__lexer));
	lexer->errstr = errstr;
	lexer->program = NULL;
	lexer->tokens = NULL;
	for (size_t i=0; i<FM__ARRAY_SIZE(lexer->maps); ++i) {
		lexer->maps[i].name = NULL;
	}
	for (size_t i=0; i<FM__ARRAY_SIZE(KEYWORDS); ++i) {
		populate(lexer, KEYWORDS[i].name, KEYWORDS[i].op);
	}
	for (size_t i=0; i<FM__ARRAY_SIZE(OPERATORS); ++i) {
		assert( MAX_OPERATOR_LENGTH >= strlen(OPERATORS[i].name) );
		populate(lexer, OPERATORS[i].name, OPERATORS[i].op);
	}
	if (fm__add_size(strlen(program), MAX_OPERATOR_LENGTH + 1, &n) ||
	    !(lexer->program = fm__malloc(n))) {
		fm__lexer_close(lexer);
		FM__TRACE(0);
		return NULL;
	}
	unix_copy(lexer, program);
	if (tokenize(lexer)) {
		fm__lexer_close(lexer);
		FM__TRACE(0);
		return NULL;
	}
	errstr[0] = '\0';
	return lexer;
}

void
fm__lexer_close(fm__lexer_t lexer)
{
	struct fm__lexer_token *token;

	if (lexer) {
		if (lexer->tokens) {
			for (size_t i=0; i<lexer->size; ++i) {
				token = &lexer->tokens[i];
				if (FM__LEXER_OP_LINT == token->op) {
					fm__bigint_free(token->u.i);
				}
				if ((FM__LEXER_OP_LSTRING == token->op) ||
				    (FM__LEXER_OP_IDENTIFIER == token->op)) {
					free((void *)token->u.s);
				}
			}
		}
		free(lexer->tokens);
		free(lexer->program);
		free(lexer);
	}
}

size_t
fm__lexer_size(fm__lexer_t lexer)
{
	assert( lexer );

	return lexer->size;
}

const struct fm__lexer_token *
fm__lexer_lookup(fm__lexer_t lexer, size_t i)
{
	assert( lexer );
	assert( i < lexer->size );

	return &lexer->tokens[i];
}
