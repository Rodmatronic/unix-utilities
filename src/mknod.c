/*	$OpenBSD: mknod.c,v 1.31 2019/06/28 13:32:44 deraadt Exp $	*/
/*	$NetBSD: mknod.c,v 1.8 1995/08/11 00:08:18 jtc Exp $	*/

/*
 * Copyright (c) 1997-2016 Theo de Raadt <deraadt@openbsd.org>,
 *	Marc Espie <espie@openbsd.org>,	Todd Miller <millert@openbsd.org>,
 *	Martin Natano <natano@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>

#include "defs.h"

char *__progname;

struct node {
	const char *name;
	mode_t mode;
	dev_t dev;
	char mflag;
};

int optreset;

static int domakenodes(struct node *, int);
static dev_t compute_device(int, char **);
static void usage(int);

int
main(int argc, char *argv[])
{
	struct node *node;
	int ismkfifo;
	int n = 0;
	int mode = DEFFILEMODE;
	int mflag = 0;
	int ch;

	node = reallocarray(NULL, sizeof(struct node), argc);
	if (!node) {
		perror("reallocarray");
		exit(1);
	}

	ismkfifo = strcmp(__progname, "mkfifo") == 0;

	/* we parse all arguments upfront */
	while (argc > 1) {
		while ((ch = getopt(argc, argv, "")) != -1) {
			switch (ch) {
			default:
				usage(ismkfifo);
			}
		}
		argc -= optind;
		argv += optind;

		if (ismkfifo) {
			while (*argv) {
				node[n].mode = mode | S_IFIFO;
				node[n].mflag = mflag;
				node[n].name = *argv;
				node[n].dev = 0;
				n++;
				argv++;
			}
			/* XXX no multiple getopt */
			break;
		} else {
			if (argc < 2)
				usage(ismkfifo);
			node[n].mode = mode;
			node[n].mflag = mflag;
			node[n].name = argv[0];
			if (strlen(argv[1]) != 1) {
				fprintf(stderr, "invalid device type '%s'\n", argv[1]);
			}

			/* XXX computation offset by one for next getopt */
			switch(argv[1][0]) {
			case 'p':
				node[n].mode |= S_IFIFO;
				node[n].dev = 0;
				argv++;
				argc--;
				break;
			case 'b':
				node[n].mode |= S_IFBLK;
				goto common;
			case 'c':
				node[n].mode |= S_IFCHR;
common:
				node[n].dev = compute_device(argc, argv);
				argv+=3;
				argc-=3;
				break;
			default:
				fprintf(stderr, "invalid device type '%s'\n", argv[1]);
				exit(1);
			}
			n++;
		}
		optind = 1;
		optreset = 1;
	}

	if (n == 0)
		usage(ismkfifo);

	return (domakenodes(node, n));
}

static dev_t
compute_device(int argc, char **argv)
{
	dev_t dev;
	char *endp;
	unsigned long major, minor;

	if (argc < 4)
		usage(0);

	errno = 0;
	major = strtoul(argv[2], &endp, 0);
	if (endp == argv[2] || *endp != '\0') {
		fprintf(stderr, "invalid major number '%s'\n", argv[2]);
		exit(1);
	}
	if (errno == ERANGE && major == ULONG_MAX) {
		fprintf(stderr, "major number too large: '%s'\n", argv[2]);
		exit(1);
	}

	errno = 0;
	minor = strtoul(argv[3], &endp, 0);
	if (endp == argv[3] || *endp != '\0') {
		fprintf(stderr, "invalid minor number '%s'\n", argv[3]);
		exit(1);
	}
	if (errno == ERANGE && minor == ULONG_MAX) {
		fprintf(stderr, "minor number too large: '%s'\n", argv[3]);
		exit(1);
	}

	dev = makedev(major, minor);
	if (major(dev) != major || minor(dev) != minor) {
		fprintf(stderr, "major or minor number too large (%lu %lu)\n", major,minor);
		exit(1);
	}

	return dev;
}

static int
domakenodes(struct node *node, int n)
{
	int done_umask = 0;
	int rv = 0;
	int i;

	for (i = 0; i != n; i++) {
		int r;
		/*
		 * If the user specified a mode via `-m', don't allow the umask
		 * to modify it.  If no `-m' flag was specified, the default
		 * mode is the value of the bitwise inclusive or of S_IRUSR,
		 * S_IWUSR, S_IRGRP, S_IWGRP, S_IROTH, and S_IWOTH as
		 * modified by the umask.
		 */
		if (node[i].mflag && !done_umask) {
			(void)umask(0);
			done_umask = 1;
		}

		r = mknod(node[i].name, node[i].mode, node[i].dev);
		if (r == -1) {
			perror(node[i].name);
			rv = 1;
		}
	}

	free(node);
	return rv;
}

static void
usage(int ismkfifo)
{

	if (ismkfifo == 1)
		(void)fprintf(stderr, "usage: %s fifo_name ...\n",
		    __progname);
	else {
		(void)fprintf(stderr,
		    "usage: %s name b|c major minor\n",
		    __progname);
		(void)fprintf(stderr, "       %s name p\n",
		    __progname);
	}
	exit(1);
}
