#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int line_count = 10;

void copyout(int count);
void usage(void);

int main(int argc, char *argv[]){
	 int opt;
	 int around = 0;
	 char *name;

	 while ((opt = getopt(argc, argv, "n:")) != -1){
		  switch (opt){
		  case 'n':
				line_count = atoi(optarg);
				break;
		  default:
			usage();
		  }
	 }

	 /* Remaining arguments are filenames */
	 if (optind == argc){
		  copyout(line_count);
		  exit(0);
	 }

	 for (; optind < argc; optind++){
		  name = argv[optind];

		  if (freopen(name, "r", stdin) == NULL){
			perror(name);
			continue;
		  }

		  if (around++)
			putchar('\n');

		  if (argc - optind > 1)
			printf("==> %s <==\n", name);

		  copyout(line_count);
		  fflush(stdout);
	 }

	 return 0;
}

void copyout(int count){
	char line_buf[BUFSIZ];

	while (count > 0 && fgets(line_buf, sizeof line_buf, stdin) != 0) {
		printf("%s", line_buf);
		fflush(stdout);
		count--;
	}
}

void usage(void){
	fprintf(stderr, "usage: head [-n lines] [filename...]\n");
	exit(1);
}
