/**
 * Copyright (c) 2025-2026 Tony Givargis
 * University of California, Irvine
 * fm_bigint.c
 */

#include "fm_core.h"
#include "fm_bigint.h"

#define MAX_PARTS_ ( 16384 )
#define MAX_PARTS  ( MAX_PARTS_ - 1 )
#define MAX_BITS   ( MAX_PARTS * 63 )

#define MAX(a, b) ( ((a) > (b)) ? (a) : (b) )
#define DUP(a, b) ( (0 == ((a) % (b))) ? ((a) / (b)) : ((a) / (b) + 1) )

#define IS_ZERO(a)     ( 0 == (a)->width )
#define IS_NEGATIVE(a) ( (a)->sign )

struct fm__bigint {
	int sign;
	int width;
	uint64_t *parts;
};

static uint64_t C_[]  = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 1,
	0xde0b6b3a7640000 /* 10^18 */
};

static const struct fm__bigint C__[] = {
	{ 0,0, NULL    }, { 0,1, &C_[ 1] }, { 0,1, &C_[ 2] }, { 0,1, &C_[ 3] },
	{ 0,1, &C_[ 4] }, { 0,1, &C_[ 5] }, { 0,1, &C_[ 6] }, { 0,1, &C_[ 7] },
	{ 0,1, &C_[ 8] }, { 0,1, &C_[ 9] }, { 0,1, &C_[10] }, { 0,1, &C_[11] },
	{ 0,1, &C_[12] }, { 0,1, &C_[13] }, { 0,1, &C_[14] }, { 0,1, &C_[15] },
	{ 0,1, &C_[16] }, { 1,1, &C_[17] }, { 0,1, &C_[18] }
};

static const struct fm__bigint *C[] = {
	&C__[ 0], &C__[ 1], &C__[ 2], &C__[ 3], &C__[ 4],
	&C__[ 5], &C__[ 6], &C__[ 7], &C__[ 8], &C__[ 9],
	&C__[10], &C__[11], &C__[12], &C__[13], &C__[14],
	&C__[15], &C__[16], &C__[17], &C__[18]
};

static void
destroy(struct fm__bigint *z)
{
	if (z) {
		free(z->parts);
		free(z);
	}
}

static struct fm__bigint *
create(int width)
{
	struct fm__bigint *z;

	if (MAX_PARTS_ < width) {
		FM__TRACE(FM__ERRNO_NUMERIC);
		return NULL;
	}
	if (!(z = fm__malloc(sizeof (struct fm__bigint)))) {
		FM__TRACE(0);
		return NULL;
	}
	memset(z, 0, sizeof (struct fm__bigint));
	z->parts = NULL;
	if ((z->width = width)) {
		z->parts = fm__malloc(z->width * sizeof (z->parts[0]));
		if (!z->parts) {
			destroy(z);
			FM__TRACE(0);
			return NULL;
		}
	}
	return z;
}

static struct fm__bigint *
clone(const struct fm__bigint *a)
{
	struct fm__bigint *z;

	if (!(z = create(a->width))) {
		FM__TRACE(0);
		return NULL;
	}
	z->sign = a->sign;
	if (z->width) {
		memcpy(z->parts, a->parts, z->width * sizeof (z->parts[0]));
	}
	return z;
}

static void
normalize(struct fm__bigint *z)
{
	while (z->width && !z->parts[z->width - 1]) {
		--z->width;
	}
	if (!z->width) {
		free(z->parts);
		z->parts = NULL;
		z->sign = 0;
	}
}

static struct fm__bigint *
finalize(struct fm__bigint *z)
{
	if (z) {
		if (MAX_PARTS < z->width) {
			destroy(z);
			FM__TRACE(FM__ERRNO_NUMERIC);
			return NULL;
		}
	}
	return z;
}

static int
magnitude(const struct fm__bigint *a, const struct fm__bigint *b)
{
	if (a->width > b->width) {
		return 1;
	}
	if (a->width < b->width) {
		return -1;
	}
	for (int i=a->width-1; i>=0; --i) {
		if (a->parts[i] > b->parts[i]) {
			return 1;
		}
		if (a->parts[i] < b->parts[i]) {
			return -1;
		}
	}
	return 0;
}

static int
cmp(const struct fm__bigint *a, const struct fm__bigint *b)
{
	if (a->sign > b->sign) {
		return -1;
	}
	if (a->sign < b->sign) {
		return +1;
	}
	if (a->width > b->width) {
		return a->sign ? -1 : +1;
	}
	if (a->width < b->width) {
		return a->sign ? +1 : -1;
	}
	for (int i=a->width-1; i>=0; --i) {
		if (a->parts[i] > b->parts[i]) {
			return a->sign ? -1 : +1;
		}
		if (a->parts[i] < b->parts[i]) {
			return a->sign ? +1 : -1;
		}
	}
	return 0;
}

static struct fm__bigint *
uadd(const struct fm__bigint *a, const struct fm__bigint *b)
{
	struct fm__bigint *z;
	uint64_t a_, b_, z_;
	int c;

	c = 0;
	if (!(z = create(MAX(a->width, b->width) + 1))) {
		FM__TRACE(0);
		return NULL;
	}
	for (int i=0; i<z->width; ++i) {
		a_ = (i < a->width) ? a->parts[i] : 0;
		b_ = (i < b->width) ? b->parts[i] : 0;
		z_ = a_ + b_ + (uint64_t)c;
		c = (z_ < a_) || (c && (z_ == a_));
		z->parts[i] = z_;
	}
	normalize(z);
	return z;
}

static struct fm__bigint *
usub(const struct fm__bigint *a, const struct fm__bigint *b)
{
	struct fm__bigint *z;
	uint64_t a_, b_, z_;
	int c;

	c = 0;
	if (!(z = create(a->width))) {
		FM__TRACE(0);
		return NULL;
	}
	for (int i=0; i<z->width; ++i) {
		a_ = a->parts[i];
		b_ = (i < b->width) ? b->parts[i] : 0;
		z_ = a_ - b_ - (uint64_t)c;
		c = (a_ < b_) || (c && (a_ == b_));
		z->parts[i] = z_;
	}
	normalize(z);
	return z;
}

static struct fm__bigint *
add(const struct fm__bigint *a, const struct fm__bigint *b)
{
	struct fm__bigint *z;
	int d;

	if (a->sign == b->sign) {
		if (!(z = uadd(a, b))) {
			FM__TRACE(0);
			return NULL;
		}
		z->sign = a->sign;
	}
	else {
		if (!(d = magnitude(a, b))) {
			if (!(z = create(0))) {
				FM__TRACE(0);
				return NULL;
			}
		}
		else if (0 < d) {
			if (!(z = usub(a, b))) {
				FM__TRACE(0);
				return NULL;
			}
			z->sign = a->sign;
		}
		else {
			if (!(z = usub(b, a))) {
				FM__TRACE(0);
				return NULL;
			}
			z->sign = b->sign;
		}
	}
	return z;
}

static struct fm__bigint *
sub(const struct fm__bigint *a, const struct fm__bigint *b_)
{
	struct fm__bigint *z, b;

	b = (*b_);
	b.sign = IS_ZERO(b_) ? 0 : (b_->sign ? 0 : 1);
	if (!(z = add(a, &b))) {
		FM__TRACE(0);
		return NULL;
	}
	return z;
}

static void
mul128(uint64_t a, uint64_t b, uint64_t *h, uint64_t *l)
{
#ifdef __SIZEOF_INT128__
	__uint128_t t = (__uint128_t)a * (__uint128_t)b;

	assert( h && l );

	(*h) = (uint64_t)(t >> 64);
	(*l) = (uint64_t)t;
#else
	uint64_t ah = a >> 32;
	uint64_t al = a & 0xffffffff;
	uint64_t bh = b >> 32;
	uint64_t bl = b & 0xffffffff;
	uint64_t t1 = bl * al;
	uint64_t t2 = bl * ah;
	uint64_t t3 = bh * al;
	uint64_t t4 = bh * ah;

	assert( h && l );

	t2 += t1 >> 32;
	t3 += t2;
	if (t3 < t2) {
		t4 += FM__U64(1) << 32;
	}
	t4 += t3 >> 32;
	(*h) = t4;
	(*l) = (t3 << 32) | (t1 & 0xffffffff);
#endif
}

static struct fm__bigint *
mul(const struct fm__bigint *a, const struct fm__bigint *b)
{
	struct fm__bigint *z;
	uint64_t h, l;
	int k;

	if (!(z = create(a->width + b->width))) {
		FM__TRACE(0);
		return NULL;
	}
	if (z->width) {
		memset(z->parts, 0, z->width * sizeof (z->parts[0]));
	}
	for (int i=0; i<a->width; ++i) {
		for (int j=0; j<b->width; ++j) {
			mul128(a->parts[i], b->parts[j], &h, &l);
			z->parts[i + j] += l;
			if (z->parts[i + j] < l) {
				k = i + j + 1;
				while (!(++z->parts[k++]));
			}
			z->parts[i + j + 1] += h;
			if (z->parts[i + j + 1] < h) {
				k = i + j + 2;
				while (!(++z->parts[k++]));
			}
		}
	}
	z->sign = a->sign ^ b->sign;
	normalize(z);
	return z;
}

static int
slow_divmod(const struct fm__bigint *a,
	    const struct fm__bigint *b,
	    struct fm__bigint **q,
	    struct fm__bigint **r)
{
	struct fm__bigint *q_, *r_, *b2, *q2;

	/*
	 * TODO: replace recursive division with an iterative algorithm.
	 * Recursion grows with the quotient bit count and can overflow the
	 * stack for large constants (for example, (1 << 200000) / 1). Both
	 * division and remainder use this path.
	 */

	if (0 > cmp(a, b)) {
		(*r) = NULL;
		if (!((*q) = create(0)) || !((*r) = clone(a))) {
			destroy(*q);
			destroy(*r);
			(*q) = NULL;
			(*r) = NULL;
			FM__TRACE(0);
			return -1;
		}
	}
	else {
		q_ = r_ = NULL;
		if (!(b2 = mul(b, C[2])) ||
		    slow_divmod(a, b2, &q_, &r_) ||
		    !(q2 = mul(q_, C[2]))) {
			destroy(q_);
			destroy(r_);
			destroy(b2);
			FM__TRACE(0);
			return -1;
		}
		destroy(b2);
		if (0 > cmp(r_, b)) {
			(*q) = q2;
			(*r) = r_;
			destroy(q_);
		}
		else {
			(*q) = add(q2, C[1]);
			(*r) = sub(r_, b);
			destroy(q_);
			destroy(r_);
			destroy(q2);
			if (!(*q) || !(*r)) {
				destroy(*q);
				destroy(*r);
				(*q) = NULL;
				(*r) = NULL;
				FM__TRACE(0);
				return -1;
			}
		}
	}
	normalize(*q);
	normalize(*r);
	return 0;
}

static int
divmod(const struct fm__bigint *a_,
       const struct fm__bigint *b_,
       struct fm__bigint **q,
       struct fm__bigint **r)
{
	struct fm__bigint a, b;

	if (IS_ZERO(b_)) {
		FM__TRACE(FM__ERRNO_NUMERIC);
		return -1;
	}
	a = (*a_);
	b = (*b_);
	a.sign = 0;
	b.sign = 0;
	if (slow_divmod(&a, &b, q, r)) {
		FM__TRACE(0);
		return -1;
	}
	if (IS_NEGATIVE(a_) ^ IS_NEGATIVE(b_)) {
		(*q)->sign = 1;
	}
	if (IS_NEGATIVE(a_)) {
		(*r)->sign = 1;
	}
	normalize(*q);
	normalize(*r);
	return 0;
}

static struct fm__bigint *
shl(const struct fm__bigint *a, int n)
{
	struct fm__bigint *z;
	uint64_t a_;
	int q, r;

	q = n / 64;
	r = n % 64;
	if (!(z = create(a->width + q + 1))) {
		FM__TRACE(0);
		return NULL;
	}
	memset(z->parts, 0, z->width * sizeof (z->parts[0]));
	for (int i=0; i<a->width; ++i) {
		a_ = a->parts[i];
		z->parts[i + q] |= a_ << r;
		if (r && ((i + q + 1) < z->width)) {
			z->parts[i + q + 1] |= a_ >> (64 - r);
		}
	}
	z->sign = a->sign;
	normalize(z);
	return z;
}

static struct fm__bigint *
shr(const struct fm__bigint *a, int n)
{
	struct fm__bigint *z;
	uint64_t *parts;
	int q, r, flag;
	uint64_t a_;

	q = n / 64;
	r = n % 64;

	/* arithmetic shift with floored division */

	flag = 0;
	if (IS_NEGATIVE(a)) {
		for (int i=0; i<a->width; ++i) {
			if (i == q) {
				break;
			}
			if (a->parts[i]) {
				flag = 1;
				break;
			}
		}
		if ((a->width > q) && (0 < r)) {
			if ((a->parts[q] & ((FM__U64(1) << r) - 1))) {
				flag = 1;
			}
		}
	}

	/* logic shift */

	if (!(z = create(MAX(0, a->width - q)))) {
		FM__TRACE(0);
		return NULL;
	}
	if (z->width) {
		memset(z->parts, 0, z->width * sizeof (z->parts[0]));
	}
	for (int i=q; i<a->width; ++i) {
		a_ = a->parts[i];
		z->parts[i - q] |= a_ >> r;
		if (r && ((i + 1) < a->width)) {
			z->parts[i - q] |= a->parts[i + 1] << (64 - r);
		}
	}
	z->sign = a->sign;
	normalize(z);

	/* arithmetic shift with floored division */

	if (flag) {
		if (IS_ZERO(z)) {
			destroy(z);
			if (!(z = clone(C[17]))) {
				FM__TRACE(0);
				return NULL;
			}
		}
		else {
			flag = 1;
			for (int i=0; i<z->width; ++i) {
				if (++z->parts[i]) {
					flag = 0;
					break;
				}
			}
			if (flag) {
				parts = fm__realloc(z->parts,
						    (z->width + 1) *
						    sizeof (uint64_t));
				if (!parts) {
					destroy(z);
					FM__TRACE(0);
					return NULL;
				}
				z->parts = parts;
				z->parts[z->width++] = 1;
			}
		}
	}
	return z;
}

static struct fm__bigint *
complement(const struct fm__bigint *a, int width)
{
	struct fm__bigint *t1, *t2;
	uint64_t t1_;

	assert( IS_NEGATIVE(a) );

	if (!(t1 = usub(a, C[1]))) {
		FM__TRACE(0);
		return NULL;
	}
	if (!(t2 = create(width))) {
		destroy(t1);
		FM__TRACE(0);
		return NULL;
	}
	for (int i=0; i<width; ++i) {
		t1_ = (i < t1->width) ? t1->parts[i] : 0;
		t2->parts[i] = ~t1_;
	}
	destroy(t1);
	return t2;
}

static struct fm__bigint *
uand(const struct fm__bigint *a, const struct fm__bigint *b, int width)
{
	struct fm__bigint *z;
	uint64_t a_, b_;

	assert( !IS_NEGATIVE(a) && !IS_NEGATIVE(b) );

	if (!(z = create(width))) {
		FM__TRACE(0);
		return NULL;
	}
	for (int i=0; i<width; ++i) {
		a_ = (i < a->width) ? a->parts[i] : 0;
		b_ = (i < b->width) ? b->parts[i] : 0;
		z->parts[i] = a_ & b_;
	}
	return z;
}

static struct fm__bigint *
convert_int(int64_t i)
{
	struct fm__bigint *z;

	if (!(z = create(1))) {
		FM__TRACE(0);
		return NULL;
	}
	z->parts[0] = (0 > i) ? ~(uint64_t)i + 1 : (uint64_t)i;
	z->sign = (0 > i) ? 1 : 0;
	normalize(z);
	return z;
}

static struct fm__bigint *
convert_real(double r)
{
	struct fm__bigint *z;
	uint64_t mantissa;
	int exp, sign;
	int i, n;

	/* NaN/INF values */

	if (isnan(r) || isinf(r)) {
		FM__TRACE(FM__ERRNO_NUMERIC);
		return NULL;
	}

	/* IEEE-754 */

	sign = (0.0 > r) ? 1 : 0;
	r = frexp(fabs(r), &exp);
	mantissa = (uint64_t)(r * (FM__U64(1) << 53));
	exp -= 53;

	/* fractional values */

	if (0 >= (exp + 53)) {
		if (!(z = convert_int(0))) {
			FM__TRACE(0);
			return NULL;
		}
		return z;
	}

	/* create */

	if (!(z = create(DUP(exp + 53, 64)))) {
		FM__TRACE(0);
		return NULL;
	}
	memset(z->parts, 0, z->width * sizeof (z->parts[0]));

	/* coordinate */

	if (0 > exp) {
		mantissa = ((64 > (size_t)-exp) ? (mantissa >> -exp) : 0);
		exp = 0;
	}

	/* populate */

	i = exp / 64;
	n = exp % 64;
	z->parts[i] = mantissa << n;
	if ((i + 1) < z->width) {
		z->parts[i + 1] = (!n ? 0 : (mantissa >> (64 - n)));
	}

	/* finalize */

	z->sign = sign;
	normalize(z);
	return z;
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

static int
dec2int(int c)
{
	if (('0' <= c) && ('9' >= c)) {
		return c - '0';
	}
	return -1;
}

static struct fm__bigint *
convert_string(const char *s)
{
	struct fm__bigint *a, *b, *z;
	int (*p2v)(int);
	const char *s_;
	int v, m, sign;
	double r;
	char *e;

	m = 10;
	sign = 0;
	p2v = dec2int;
	if (('-' == s[0]) || ('+' == s[0])) {
		sign = ('-' == s[0]);
		++s;
		if (!(*s)) {
			FM__TRACE(FM__ERRNO_NUMERIC);
			return NULL;
		}
	}
	if ((0 > dec2int((unsigned char)*s)) && ('.' != *s)) {
		FM__TRACE(FM__ERRNO_NUMERIC);
		return NULL;
	}
	if (('0' == s[0]) && (('x' == s[1]) || ('X' == s[1]))) {
		m = 16;
		p2v = hex2int;
		s += 2;
		if (!(*s)) {
			FM__TRACE(FM__ERRNO_NUMERIC);
			return NULL;
		}
	}
	else {
		s_ = s;
		while (*s) {
			if (('.' == (*s)) || ('e' == (*s)) || ('E' == (*s))) {
				s = s_;
				errno = 0;
				r = strtod(s, &e);
				if ((e == s) ||
				    (EINVAL == errno) ||
				    (ERANGE == errno) ||
				    (*e)) {
					FM__TRACE(FM__ERRNO_NUMERIC);
					return NULL;
				}
				if (!(z = convert_real(r))) {
					FM__TRACE(0);
					return NULL;
				}
				z->sign = sign;
				normalize(z);
				return z;
			}
			++s;
		}
		s = s_;
	}
	if (!(z = convert_int(0))) {
		FM__TRACE(0);
		return NULL;
	}
	while (*s) {
		if (0 > (v = p2v((unsigned char)(*s)))) {
			destroy(z);
			FM__TRACE(FM__ERRNO_NUMERIC);
			return NULL;
		}
		if (!(a = mul(z, C[m])) || !(b = add(a, C[v]))) {
			destroy(a);
			destroy(z);
			FM__TRACE(0);
			return NULL;
		}
		destroy(a);
		destroy(z);
		z = b;
		++s;
	}
	z->sign = sign;
	normalize(z);
	return z;
}

void
fm__bigint_init(void)
{
	if (((uint64_t)INT_MAX < (uint64_t)MAX_PARTS * 64) ||
	    ((uint64_t)SIZE_MAX < (uint64_t)MAX_PARTS * 64) ||
	    (ULLONG_MAX != UINT64_MAX) ||
	    (2 != FLT_RADIX) ||
	    (53 != DBL_MANT_DIG) ||
	    ('b' != ('a' + 1)) ||
	    ('c' != ('a' + 2)) ||
	    ('d' != ('a' + 3)) ||
	    ('e' != ('a' + 4)) ||
	    ('f' != ('a' + 5))) {
		FM__TRACE(FM__ERRNO_ARCHITECTURE);
		abort();
	}
}

void
fm__bigint_free(fm__bigint_t z)
{
	destroy((struct fm__bigint *)z);
}

fm__bigint_t
fm__bigint_int(int64_t i)
{
	return finalize(convert_int(i));
}

fm__bigint_t
fm__bigint_string(const char *s)
{
	assert( s && (*s) );

	return finalize(convert_string(s));
}

fm__bigint_t
fm__bigint_clone(fm__bigint_t a)
{
	assert( a );

	return clone(a);
}

fm__bigint_t
fm__bigint_neg(fm__bigint_t a)
{
	struct fm__bigint *z;

	assert( a );

	if (!(z = clone(a))) {
		FM__TRACE(0);
		return NULL;
	}
	if (!IS_ZERO(z)) {
		z->sign = z->sign ? 0 : 1;
	}
	return finalize(z);
}

fm__bigint_t
fm__bigint_add(fm__bigint_t a, fm__bigint_t b)
{
	assert( a && b );

	return finalize(add(a, b));
}

fm__bigint_t
fm__bigint_sub(fm__bigint_t a, fm__bigint_t b)
{
	assert( a && b );

	return finalize(sub(a, b));
}

fm__bigint_t
fm__bigint_mul(fm__bigint_t a, fm__bigint_t b)
{
	assert( a && b );

	return finalize(mul(a, b));
}

fm__bigint_t
fm__bigint_div(fm__bigint_t a, fm__bigint_t b)
{
	struct fm__bigint *q, *r;

	assert( a && b );

	if (divmod(a, b, &q, &r)) {
		FM__TRACE(0);
		return NULL;
	}
	destroy(r);
	return finalize(q);
}

fm__bigint_t
fm__bigint_mod(fm__bigint_t a, fm__bigint_t b)
{
	struct fm__bigint *q, *r;

	assert( a && b );

	if (divmod(a, b, &q, &r)) {
		FM__TRACE(0);
		return NULL;
	}
	destroy(q);
	return finalize(r);
}

fm__bigint_t
fm__bigint_shl(fm__bigint_t a, fm__bigint_t b)
{
	uint64_t n;

	assert( a && b );

	if (IS_NEGATIVE(b)) {
		FM__TRACE(FM__ERRNO_NUMERIC);
		return NULL;
	}
	if (IS_ZERO(a)) {
		return create(0);
	}
	if ((1 < b->width) ||
	    (MAX_BITS <= (n = IS_ZERO(b) ? 0 : b->parts[0]))) {
		FM__TRACE(FM__ERRNO_NUMERIC);
		return NULL;
	}
	return finalize(shl(a, (int)n));
}

fm__bigint_t
fm__bigint_shr(fm__bigint_t a, fm__bigint_t b)
{
	uint64_t n;

	assert( a && b );

	if (IS_NEGATIVE(b)) {
		FM__TRACE(FM__ERRNO_NUMERIC);
		return NULL;
	}
	if (IS_ZERO(a) ||
	    (1 < b->width) ||
	    (fm__bigint_bits(a) <= (n = IS_ZERO(b) ? 0 : b->parts[0]))) {
		if (IS_NEGATIVE(a)) {
			return clone(C[17]); /* -1 */
		}
		else {
			return create(0);
		}
	}
	return finalize(shr(a, (int)n));
}

fm__bigint_t
fm__bigint_not(fm__bigint_t a)
{
	struct fm__bigint *t1, *z;

	assert( a );

	if (!(t1 = clone(a))) {
		FM__TRACE(0);
		return NULL;
	}
	if (!IS_ZERO(t1)) {
		t1->sign = t1->sign ? 0 : 1;
	}
	if (!(z = sub(t1, C[1]))) {
		destroy(t1);
		FM__TRACE(0);
		return NULL;
	}
	destroy(t1);
	return finalize(z);
}

fm__bigint_t
fm__bigint_and(fm__bigint_t a, fm__bigint_t b)
{
	struct fm__bigint *t1, *t2, *z;
	int width;

	assert( a && b );

	width = MAX(a->width, b->width) + 1;
	t1 = NULL;
	if (IS_NEGATIVE(a)) {
		if (!(t1 = complement(a, width))) {
			FM__TRACE(0);
			return NULL;
		}
	}
	t2 = NULL;
	if (IS_NEGATIVE(b)) {
		if (!(t2 = complement(b, width))) {
			destroy(t1);
			FM__TRACE(0);
			return NULL;
		}
	}
	if (!(z = uand(t1 ? t1 : a, t2 ? t2 : b, width))) {
		destroy(t1);
		destroy(t2);
		FM__TRACE(0);
		return NULL;
	}
	destroy(t1);
	destroy(t2);
	if (z->parts[width - 1] & (FM__U64(1) << 63)) {
		z->sign = 1;
		if (!(t1 = complement(z, width))) {
			destroy(z);
			FM__TRACE(0);
			return NULL;
		}
		t1->sign = 1;
		destroy(z);
		z = t1;
	}
	normalize(z);
	return finalize(z);
}

fm__bigint_t
fm__bigint_or(fm__bigint_t a, fm__bigint_t b)
{
	fm__bigint_t t1, t2, t3, z;

	assert( a && b );

	t1 = t2 = t3 = NULL;
	if (!(t1 = fm__bigint_not(a)) ||
	    !(t2 = fm__bigint_not(b)) ||
	    !(t3 = fm__bigint_and(t1, t2)) ||
	    !(z = fm__bigint_not(t3))) {
		fm__bigint_free(t1);
		fm__bigint_free(t2);
		fm__bigint_free(t3);
		FM__TRACE(0);
		return NULL;
	}
	fm__bigint_free(t1);
	fm__bigint_free(t2);
	fm__bigint_free(t3);
	return finalize((struct fm__bigint *)z);
}

fm__bigint_t
fm__bigint_xor(fm__bigint_t a, fm__bigint_t b)
{
	fm__bigint_t t1, t2, t3, z;

	assert( a && b );

	t1 = t2 = t3 = NULL;
	if (!(t1 = fm__bigint_and(a, b)) ||
	    !(t2 = fm__bigint_or(a, b)) ||
	    !(t3 = fm__bigint_not(t1)) ||
	    !(z = fm__bigint_and(t2, t3))) {
		fm__bigint_free(t1);
		fm__bigint_free(t2);
		fm__bigint_free(t3);
		FM__TRACE(0);
		return NULL;
	}
	fm__bigint_free(t1);
	fm__bigint_free(t2);
	fm__bigint_free(t3);
	return finalize((struct fm__bigint *)z);
}

int
fm__bigint_cmp(fm__bigint_t a, fm__bigint_t b)
{
	assert( a && b );

	return cmp(a, b);
}

int
fm__bigint_sign(fm__bigint_t a)
{
	assert( a );

	return IS_ZERO(a) ? 0 : (IS_NEGATIVE(a) ? -1 : +1);
}

size_t
fm__bigint_bits(fm__bigint_t a)
{
	uint64_t x;

	assert( a );

	if (IS_ZERO(a)) {
		return 0;
	}
	x = a->parts[a->width - 1];
	return (size_t)
		((a->width - 1) * 64 + (x ? 64 - __builtin_clzll(x) : 0));
}

double
fm__bigint_double(fm__bigint_t a)
{
	uint64_t q, mask;
	int bits, shift;
	int limb, bit;
	int sticky;
	double z;

	assert( a );

	if (IS_ZERO(a)) {
		return 0.0;
	}
	bits = fm__bigint_bits(a);
	if (53 >= bits) {
		z = (double)a->parts[0];
		return IS_NEGATIVE(a) ? -z : z;
	}

	/* retain the most-significant 53 bits */

	shift = bits - 53;
	limb = shift / 64;
	bit = shift % 64;
	q = a->parts[limb] >> bit;
	if (bit && ((limb + 1) < a->width)) {
		q |= a->parts[limb + 1] << (64 - bit);
	}

	/* round to nearest, ties to even */

	limb = (shift - 1) / 64;
	bit = (shift - 1) % 64;
	sticky = 0;
	for (int i=0; i<limb; ++i) {
		sticky |= (0 != a->parts[i]);
	}
	if (bit) {
		mask = (FM__U64(1) << bit) - 1;
		sticky |= (0 != (a->parts[limb] & mask));
	}
	if ((a->parts[limb] & (FM__U64(1) << bit)) && (sticky || (q & 1))) {
		++q;
	}
	z = ldexp((double)q, shift);
	return IS_NEGATIVE(a) ? -z : z;
}

int
fm__bigint_uint64(fm__bigint_t a, int clamp /* bool */, uint64_t *i)
{
	assert( a && i );

	if (IS_ZERO(a)) {
		(*i) = 0;
		return 0;
	}
	if (IS_NEGATIVE(a) || (64 < fm__bigint_bits(a))) {
		if (clamp) {
			(*i) = IS_NEGATIVE(a) ? 0 : UINT64_MAX;
			return 0;
		}
		FM__TRACE(FM__ERRNO_NUMERIC);
		return -1;
	}
	(*i) = a->parts[0];
	return 0;
}

int
fm__bigint_int64(fm__bigint_t a, int clamp /* bool */, int64_t *i)
{
	size_t bits;

	assert( a && i );

	if (IS_ZERO(a)) {
		(*i) = 0;
		return 0;
	}
	bits = fm__bigint_bits(a);
	if ((bits > (63 + (size_t)IS_NEGATIVE(a))) ||
	    (IS_NEGATIVE(a) &&
	     (bits == (63 + 1)) &&
	     (a->parts[0] > (FM__U64(1) << 63)))) {
		if (clamp) {
			(*i) = (IS_NEGATIVE(a) ? INT64_MIN : INT64_MAX);
			return 0;
		}
		FM__TRACE(FM__ERRNO_NUMERIC);
		return -1;
	}
	if (IS_NEGATIVE(a)) {
		(*i) = ((a->parts[0] == (FM__U64(1) << 63))
			? INT64_MIN
			: -(int64_t)a->parts[0]);
	}
	else {
		(*i) = (int64_t)a->parts[0];
	}
	return 0;
}
