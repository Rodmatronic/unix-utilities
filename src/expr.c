/*	$OpenBSD: expr.c,v 1.28 2022/01/28 05:15:05 guenther Exp $	*/
/*	$NetBSD: expr.c,v 1.3.6.1 1996/06/04 20:41:47 cgd Exp $	*/

/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include <unistd.h>
#include <regex.h>
#include <errno.h>

#include "defs.h"

struct val	*make_int(int64_t);
struct val	*make_str(char *);
void		 free_value(struct val *);
int		 is_integer(struct val *, int64_t *, const char **);
int		 to_integer(struct val *, const char **);
void		 to_string(struct val *);
int		 is_zero_or_null(struct val *);
void		 nexttoken(int);
void	 	error(void);
struct val	*eval6(void);
struct val	*eval5(void);
struct val	*eval4(void);
struct val	*eval3(void);
struct val	*eval2(void);
struct val	*eval1(void);
struct val	*eval0(void);

long 
d_strtoll(const char *nptr, char **endptr, int base)
{
	const char *s;
	long  acc, cutoff;
	int c;
	int neg, any, cutlim;

	/*
	 * Skip white space and pick up leading +/- sign if any.
	 * If base is 0, allow 0x for hex and 0 for octal, else
	 * assume decimal; if base is already 16, allow 0x.
	 */
	s = nptr;
	do {
		c = (unsigned char) *s++;
	} while (isspace(c));
	if (c == '-') {
		neg = 1;
		c = *s++;
	} else {
		neg = 0;
		if (c == '+')
			c = *s++;
	}
	if ((base == 0 || base == 16) &&
	    c == '0' && (*s == 'x' || *s == 'X')) {
		c = s[1];
		s += 2;
		base = 16;
	}
	if (base == 0)
		base = c == '0' ? 8 : 10;

	/*
	 * Compute the cutoff value between legal numbers and illegal
	 * numbers.  That is the largest legal value, divided by the
	 * base.  An input number that is greater than this value, if
	 * followed by a legal input character, is too big.  One that
	 * is equal to this value may be valid or not; the limit
	 * between valid and invalid numbers is then based on the last
	 * digit.  For instance, if the range for long s is
	 * [-9223372036854775808..9223372036854775807] and the input base
	 * is 10, cutoff will be set to 922337203685477580 and cutlim to
	 * either 7 (neg==0) or 8 (neg==1), meaning that if we have
	 * accumulated a value > 922337203685477580, or equal but the
	 * next digit is > 7 (or 8), the number is too big, and we will
	 * return a range error.
	 *
	 * Set any if any `digits' consumed; make it negative to indicate
	 * overflow.
	 */
	cutoff = neg ? L_LLONG_MIN : L_LLONG_MAX;
	cutlim = cutoff % base;
	cutoff /= base;
	if (neg) {
		if (cutlim > 0) {
			cutlim -= base;
			cutoff += 1;
		}
		cutlim = -cutlim;
	}
	for (acc = 0, any = 0;; c = (unsigned char) *s++) {
		if (isdigit(c))
			c -= '0';
		else if (isalpha(c))
			c -= isupper(c) ? 'A' - 10 : 'a' - 10;
		else
			break;
		if (c >= base)
			break;
		if (any < 0)
			continue;
		if (neg) {
			if (acc < cutoff || (acc == cutoff && c > cutlim)) {
				any = -1;
				acc = L_LLONG_MIN;
			} else {
				any = 1;
				acc *= base;
				acc -= c;
			}
		} else {
			if (acc > cutoff || (acc == cutoff && c > cutlim)) {
				any = -1;
				acc = L_LLONG_MAX;
			} else {
				any = 1;
				acc *= base;
				acc += c;
			}
		}
	}
	if (endptr != 0)
		*endptr = (char *) (any ? s - 1 : nptr);
	return (acc);
}

long 
l_strtonum(const char *numstr, long minval, long maxval,
    const char **errstrp)
{
	long ll = 0;
	int error = 0;
	char *ep;
	struct errval {
		const char *errstr;
		int err;
	} ev[4] = {
		{ NULL,		0 },
		{ "invalid",	EINVAL },
		{ "too small",	ERANGE },
		{ "too large",	ERANGE },
	};

	ev[0].err = errno;
	errno = 0;
	if (minval > maxval) {
		error = INVALID;
	} else {
		ll = d_strtoll(numstr, &ep, 10);
		if (numstr == ep || *ep != '\0')
			error = INVALID;
		else if ((ll == L_LLONG_MIN && errno == ERANGE) || ll < minval)
			error = TOOSMALL;
		else if ((ll == L_LLONG_MAX && errno == ERANGE) || ll > maxval)
			error = TOOLARGE;
	}
	if (errstrp != NULL)
		*errstrp = ev[error].errstr;
	errno = ev[error].err;
	if (error)
		ll = 0;

	return (ll);
}


enum token {
	OR, AND, EQ, LT, GT, ADD, SUB, MUL, DIV, MOD, MATCH, RP, LP,
	NE, LE, GE, OPERAND, EOI
};

struct val {
	enum {
		integer,
		string
	} type;

	union {
		char	       *s;
		int64_t		i;
	} u;
};

enum token	token;
struct val     *tokval;
char	      **av;

struct val *
make_int(int64_t i)
{
	struct val     *vp;

	vp = malloc(sizeof(*vp));
	if (vp == NULL) {
		perror("malloc");
		exit(1);
	}
	vp->type = integer;
	vp->u.i = i;
	return vp;
}


struct val *
make_str(char *s)
{
	struct val     *vp;

	vp = malloc(sizeof(*vp));
	if (vp == NULL || ((vp->u.s = strdup(s)) == NULL)) {
		perror("malloc");
		exit(1);
	}
	vp->type = string;
	return vp;
}


void
free_value(struct val *vp)
{
	if (vp->type == string)
		free(vp->u.s);
	free(vp);
}


/* determine if vp is an integer; if so, return its value in *r */
int
is_integer(struct val *vp, int64_t *r, const char **errstr)
{
	const char *errstrp;

	if (errstr == NULL)
		errstr = &errstrp;
	*errstr = NULL;

	if (vp->type == integer) {
		*r = vp->u.i;
		return 1;
	}

	/*
	 * POSIX.2 defines an "integer" as an optional unary minus
	 * followed by digits. Other representations are unspecified,
	 * which means that l_strtonum(3) is a viable option here.
	 */
	*r = l_strtonum(vp->u.s, INT64_MIN, INT64_MAX, errstr);
	return *errstr == NULL;
}


/* coerce to vp to an integer */
int
to_integer(struct val *vp, const char **errstr)
{
	int64_t		r;

	if (errstr != NULL)
		*errstr = NULL;

	if (vp->type == integer)
		return 1;

	if (is_integer(vp, &r, errstr)) {
		free(vp->u.s);
		vp->u.i = r;
		vp->type = integer;
		return 1;
	}

	return 0;
}


/* coerce to vp to an string */
void
to_string(struct val *vp)
{
	char tmp[64];
	size_t len;
	char *s;

	if (vp->type == string)
		return;

	snprintf(tmp, sizeof(tmp), "%ld", vp->u.i);
	len = strlen(tmp) + 1;

	s = malloc(len);
	if (s == NULL) {
		perror("malloc");
		exit(1);
	}
	memcpy(s, tmp, len);

	vp->type = string;
	vp->u.s = s;
}

int
is_zero_or_null(struct val *vp)
{
	if (vp->type == integer)
		return vp->u.i == 0;
	else
		return *vp->u.s == 0 || (to_integer(vp, NULL) && vp->u.i == 0);
}

void
nexttoken(int pat)
{
	char	       *p;

	if ((p = *av) == NULL) {
		token = EOI;
		return;
	}
	av++;

	
	if (pat == 0 && p[0] != '\0') {
		if (p[1] == '\0') {
			const char     *x = "|&=<>+-*/%:()";
			char	       *i;	/* index */

			if ((i = strchr(x, *p)) != NULL) {
				token = i - x;
				return;
			}
		} else if (p[1] == '=' && p[2] == '\0') {
			switch (*p) {
			case '<':
				token = LE;
				return;
			case '>':
				token = GE;
				return;
			case '!':
				token = NE;
				return;
			}
		}
	}
	tokval = make_str(p);
	token = OPERAND;
	return;
}

void
error(void)
{
	fprintf(stderr, "syntax error\n");
	exit(1);
}

struct val *
eval6(void)
{
	struct val     *v;

	if (token == OPERAND) {
		nexttoken(0);
		return tokval;
	} else if (token == RP) {
		nexttoken(0);
		v = eval0();
		if (token != LP)
			error();
		nexttoken(0);
		return v;
	}
	error();
	return NULL; /* so compilers don't complain */
}

/* Parse and evaluate match (regex) expressions */
struct val *
eval5(void)
{
	regex_t		rp;
	regmatch_t	rm[2];
	char		errbuf[256];
	int		eval;
	struct val     *l, *r;
	struct val     *v;

	l = eval6();
	while (token == MATCH) {
		nexttoken(1);
		r = eval6();

		/* coerce to both arguments to strings */
		to_string(l);
		to_string(r);

		/* compile regular expression */
		if ((eval = regcomp(&rp, r->u.s, 0)) != 0) {
			regerror(eval, &rp, errbuf, sizeof(errbuf));
			fprintf(stderr, "%s\n", errbuf);
			exit(1);
		}

		/* compare string against pattern --  remember that patterns
		   are anchored to the beginning of the line */
		if (regexec(&rp, l->u.s, 2, rm, 0) == 0 && rm[0].rm_so == 0) {
			if (rm[1].rm_so >= 0) {
				*(l->u.s + rm[1].rm_eo) = '\0';
				v = make_str(l->u.s + rm[1].rm_so);

			} else {
				v = make_int(rm[0].rm_eo - rm[0].rm_so);
			}
		} else {
			if (rp.re_nsub == 0) {
				v = make_int(0);
			} else {
				v = make_str("");
			}
		}

		/* free arguments and pattern buffer */
		free_value(l);
		free_value(r);
		regfree(&rp);

		l = v;
	}

	return l;
}

/* Parse and evaluate multiplication and division expressions */
struct val *
eval4(void)
{
	const char	*errstr;
	struct val	*l, *r;
	enum token	 op;
	volatile int64_t res;

	l = eval5();
	while ((op = token) == MUL || op == DIV || op == MOD) {
		nexttoken(0);
		r = eval5();

		if (!to_integer(l, &errstr)) {
			fprintf(stderr, "number \"%s\" is %s\n", l->u.s, errstr);
			exit(1);
		}
		if (!to_integer(r, &errstr)) {
			fprintf(stderr, "number \"%s\" is %s\n", r->u.s, errstr);
			exit(1);
		}

		if (op == MUL) {
			res = l->u.i * r->u.i;
			if (r->u.i != 0 && l->u.i != res / r->u.i) {
				fprintf(stderr, "overflow\n");
				exit(1);
			}
			l->u.i = res;
		} else {
			if (r->u.i == 0) {
				fprintf(stderr, "division by zero\n");
				exit(1);
			}
			if (op == DIV) {
				if (l->u.i != INT64_MIN || r->u.i != -1)
					l->u.i /= r->u.i;
				else {
					fprintf(stderr, "overflow\n");
				}
			} else {
				if (l->u.i != INT64_MIN || r->u.i != -1)
					l->u.i %= r->u.i;
				else
					l->u.i = 0;
			}
		}

		free_value(r);
	}

	return l;
}

/* Parse and evaluate addition and subtraction expressions */
struct val *
eval3(void)
{
	const char	*errstr;
	struct val	*l, *r;
	enum token	 op;
	volatile int64_t res;

	l = eval4();
	while ((op = token) == ADD || op == SUB) {
		nexttoken(0);
		r = eval4();

		if (!to_integer(l, &errstr))
			fprintf(stderr, "number \"%s\" is %s\n", l->u.s, errstr);
		if (!to_integer(r, &errstr))
			fprintf(stderr, "number \"%s\" is %s\n", r->u.s, errstr);

		if (op == ADD) {
			res = l->u.i + r->u.i;
			if ((l->u.i > 0 && r->u.i > 0 && res <= 0) ||
			    (l->u.i < 0 && r->u.i < 0 && res >= 0))
				fprintf(stderr, "overflow\n");
			l->u.i = res;
		} else {
			res = l->u.i - r->u.i;
			if ((l->u.i >= 0 && r->u.i < 0 && res <= 0) ||
			    (l->u.i < 0 && r->u.i > 0 && res >= 0))
				fprintf(stderr, "overflow\n");
			l->u.i = res;
		}

		free_value(r);
	}

	return l;
}

/* Parse and evaluate comparison expressions */
struct val *
eval2(void)
{
	struct val     *l, *r;
	enum token	op;
	int64_t		v = 0, li, ri;

	l = eval3();
	while ((op = token) == EQ || op == NE || op == LT || op == GT ||
	    op == LE || op == GE) {
		nexttoken(0);
		r = eval3();

		if (is_integer(l, &li, NULL) && is_integer(r, &ri, NULL)) {
			switch (op) {
			case GT:
				v = (li >  ri);
				break;
			case GE:
				v = (li >= ri);
				break;
			case LT:
				v = (li <  ri);
				break;
			case LE:
				v = (li <= ri);
				break;
			case EQ:
				v = (li == ri);
				break;
			case NE:
				v = (li != ri);
				break;
			default:
				break;
			}
		} else {
			to_string(l);
			to_string(r);

			switch (op) {
			case GT:
				v = (strcoll(l->u.s, r->u.s) > 0);
				break;
			case GE:
				v = (strcoll(l->u.s, r->u.s) >= 0);
				break;
			case LT:
				v = (strcoll(l->u.s, r->u.s) < 0);
				break;
			case LE:
				v = (strcoll(l->u.s, r->u.s) <= 0);
				break;
			case EQ:
				v = (strcoll(l->u.s, r->u.s) == 0);
				break;
			case NE:
				v = (strcoll(l->u.s, r->u.s) != 0);
				break;
			default:
				break;
			}
		}

		free_value(l);
		free_value(r);
		l = make_int(v);
	}

	return l;
}

/* Parse and evaluate & expressions */
struct val *
eval1(void)
{
	struct val     *l, *r;

	l = eval2();
	while (token == AND) {
		nexttoken(0);
		r = eval2();

		if (is_zero_or_null(l) || is_zero_or_null(r)) {
			free_value(l);
			free_value(r);
			l = make_int(0);
		} else {
			free_value(r);
		}
	}

	return l;
}

/* Parse and evaluate | expressions */
struct val *
eval0(void)
{
	struct val     *l, *r;

	l = eval1();
	while (token == OR) {
		nexttoken(0);
		r = eval1();

		if (is_zero_or_null(l)) {
			free_value(l);
			l = r;
		} else {
			free_value(r);
		}
	}

	return l;
}


int
main(int argc, char *argv[])
{
	struct val     *vp;

	if (argc > 1 && !strcmp(argv[1], "--"))
		argv++;

	av = argv + 1;

	nexttoken(0);
	vp = eval0();

	if (token != EOI)
		error();

	if (vp->type == integer)
		printf("%ld\n", vp->u.i);
	else
		printf("%s\n", vp->u.s);

	return is_zero_or_null(vp);
}
