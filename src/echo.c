/*
 * echo -n [message]
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>

int
main(int argc, char *argv[])
{
	int nflag;

	/* This utility may NOT do getopt(3) option parsing. */
	if (*++argv && !strcmp(*argv, "-n")) {
		++argv;
		nflag = 1;
	}
	else
		nflag = 0;

	while (*argv) {
		fputs(*argv, stdout);
		if (*++argv)
			putchar(' ');
	}
	if (!nflag)
		putchar('\n');

	return 0;
}
