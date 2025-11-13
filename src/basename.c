/*
 * basename file
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

void usage(void);
extern int optind;

char *
basename(char *input)
{
	char *end, *start;

	if (input == NULL || *input == '\0')
		return ".";

	/* Strip trailing slashes */
	end = input + strlen(input) - 1;
	while (end > input && *end == '/')
		*end-- = '\0';
	start = strrchr(input, '/');
	if (start)
		return start + 1;
	else
		return input;
}

int
main(int argc, char *argv[])
{
	int ch;
	char *p;

	while ((ch = getopt(argc, argv, "")) != -1) {
		switch (ch) {
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 1 && argc != 2)
		usage();

	if (**argv == '\0') {
		(void)puts("");
		return 0;
	}
	p = basename(*argv);
	if (p == NULL) {
		perror(*argv);
		exit(1);
	}
	/*
	 * If the suffix operand is present, is not identical to the
	 * characters remaining in string, and is identical to a suffix
	 * of the characters remaining in string, the suffix suffix
	 * shall be removed from string.
	 */
	if (*++argv) {
		size_t suffixlen, stringlen, off;

		suffixlen = strlen(*argv);
		stringlen = strlen(p);

		if (suffixlen < stringlen) {
			off = stringlen - suffixlen;
			if (!strcmp(p + off, *argv))
				p[off] = '\0';
		}
	}
	puts(p);
	return 0;
}

void
usage(void)
{
	fprintf(stderr, "usage: basename string [suffix]\n");
	exit(1);
}
