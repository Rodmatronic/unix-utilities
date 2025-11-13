/*	$OpenBSD: env.c,v 1.19 2024/07/28 21:44:42 kn Exp $	*/

/*
 * Copyright (c) 1988, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>

extern int optind;
static void usage(void);

int
main(int argc, char *argv[])
{
	extern char **environ;
	extern int optind;
	char **ep, *p;
	int ch;

	while ((ch = getopt(argc, argv, "iu:-")) != -1)
		switch(ch) {
		case '-':			/* obsolete */
		case 'i':
			if ((environ = calloc(1, sizeof(char *))) == NULL) {
				perror("calloc");
				exit(1);
			}
			break;
		case 'u':
			if (unsetenv(optarg) == -1) {
				perror("unsetenv");
				exit(1);
			}
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	for (; *argv && (p = strchr(*argv, '=')); ++argv) {
		*p++ = '\0';
		if (setenv(*argv, p, 1) == -1) {
			perror("setenv");
			exit(1);
		}
	}

	if (*argv) {
		execvp(*argv, argv);
		perror(*argv);
		exit(1);
	}

	for (ep = environ; *ep; ep++)
		(void)printf("%s\n", *ep);

	return 0;
}

static void
usage(void)
{
	(void)fprintf(stderr, "usage: env [-i] [-u name] [name=value ...] "
	    "[utility [argument ...]]\n");
	exit(1);
}
