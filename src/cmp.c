#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>

FILE *file1, *file2;

int eflg;
int lflg = 1;

long line = 1;
long character = 0;
long skip1 = 0;
long skip2 = 0;

void usage(void){
	fprintf(stderr, "usage: cmp [-s] [-l] [FILE1] [FILE2]\n");
	exit(2);
}

int main(int argc, char **argv){
	int character_file1, character_file2;
	int opt;

	while ((opt = getopt(argc, argv, "sl")) != -1){
		switch (opt) {
		case 's':
			lflg--;		/* silent */
			break;
		case 'l':
			lflg++;		/* list all diffs */
			break;
		default:
			usage();
		}
	}

	if (argc - optind != 2)
		usage();

	/* open first file */
	if (argv[optind][0] == '-' && argv[optind][1] == '\0')
		file1 = stdin;
	else if ((file1 = fopen(argv[optind], "r")) == NULL){
		perror(argv[optind]);
		exit(2);
	}

	if ((file2 = fopen(argv[optind + 1], "r")) == NULL){
		perror(argv[optind + 1]);
		exit(2);
	}

	while (skip1--){
		if ((character_file1 = getc(file1)) == EOF)
			exit(1);
	}
	while (skip2--){
		if ((character_file2 = getc(file2)) == EOF)
			exit(1);
	}

	while (1) {
		character++;
		character_file1 = getc(file1);
		character_file2 = getc(file2);

		if (character_file1 == character_file2){
			if (character_file1 == '\n')
				line++;
			if (character_file1 == EOF)
				exit(eflg ? 1 : 0);
			continue;
		}

		if (lflg == 0)	  /* -s */
			exit(1);

		if (character_file1 == EOF || character_file2 == EOF)
			exit(1);

		if (lflg == 1){
			printf("%s %s differ: byte %ld, line %ld\n", argv[optind], argv[optind + 1], character, line);
			exit(1);
		}

		/* -l: list all differences */
		eflg = 1;
		printf("%6ld %3o %3o\n", character, character_file1, character_file2);
	}
}
