#define _GNU_SOURCE
#include <dirent.h>
#include <err.h>
#include <fcntl.h>
#include <grp.h>
#include <pwd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#define BUF_SIZE 256
#define MAX_ENTRIES 256

struct entry {
	char *name;
	unsigned char type;
	struct stat st;
	int has_stat;
};

struct options {
	int show_hidden;	/* -a, -A */
	int long_format;	/* -l, -L */
	int single_column;	/* -1 */
	int no_sort;		/* -f, -F */
	char *path;
};

static int entry_cmp(const void *a, const void *b){
	const struct entry *ea = (const struct entry *)a;
	const struct entry *eb = (const struct entry *)b;
	return strcmp(ea->name, eb->name);
}

int get_term_width(void){
	struct winsize ws;
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0)
		return ws.ws_col;
	return 80;
}

char get_type_char(mode_t mode){
	if (S_ISDIR(mode)) return 'd';
	if (S_ISLNK(mode)) return 'l';
	if (S_ISBLK(mode)) return 'b';
	if (S_ISCHR(mode)) return 'c';
	if (S_ISFIFO(mode)) return 'p';
	if (S_ISSOCK(mode)) return 's';
	return '-';
}

void print_perms(mode_t mode){
	printf("%c", get_type_char(mode));
	printf("%c", (mode & S_IRUSR) ? 'r' : '-');
	printf("%c", (mode & S_IWUSR) ? 'w' : '-');
	printf("%c", (mode & S_IXUSR) ? 'x' : '-');
	printf("%c", (mode & S_IRGRP) ? 'r' : '-');
	printf("%c", (mode & S_IWGRP) ? 'w' : '-');
	printf("%c", (mode & S_IXGRP) ? 'x' : '-');
	printf("%c", (mode & S_IROTH) ? 'r' : '-');
	printf("%c", (mode & S_IWOTH) ? 'w' : '-');
	printf("%c", (mode & S_IXOTH) ? 'x' : '-');
}

void print_long(struct entry *e, const char *dirpath){
	struct passwd *pw;
	struct group *gr;
	char timebuf[64];
	struct tm *tm;
	char *fullpath;
	size_t pathlen;

	/* Build full path */
	pathlen = strlen(dirpath) + strlen(e->name) + 2;
	fullpath = malloc(pathlen);
	if (!fullpath) {
		warn("malloc");
		return;
	}

	if (strcmp(dirpath, "/") == 0)
		snprintf(fullpath, pathlen, "/%s", e->name);
	else
		snprintf(fullpath, pathlen, "%s/%s", dirpath, e->name);

	if (!e->has_stat) {
		if (lstat(fullpath, &e->st) == -1) {
			warn("stat '%s'", fullpath);
			free(fullpath);
			return;
		}
		e->has_stat = 1;
	}

	free(fullpath);

	/* Print permissions */
	print_perms(e->st.st_mode);
	printf(" %3lu", (unsigned long)e->st.st_nlink);

	/* Print owner */
	pw = getpwuid(e->st.st_uid);
	if (pw)
		printf(" %-8s", pw->pw_name);
	else
		printf(" %-8u", e->st.st_uid);

	/* Print group */
	gr = getgrgid(e->st.st_gid);
	if (gr)
		printf(" %-8s", gr->gr_name);
	else
		printf(" %-8u", e->st.st_gid);

	printf(" %8lld", (long long)e->st.st_size);

	tm = localtime(&e->st.st_mtime);
	if (tm)
		strftime(timebuf, sizeof(timebuf), "%b %e %H:%M", tm);
	else
		snprintf(timebuf, sizeof(timebuf), "???");
	printf(" %s", timebuf);

	printf(" %s\n", e->name);
}

void print_multi_column(struct entry *entries, int count){
	int i, max_len = 0, cols, rows, col, row;
	int term_width = get_term_width();
	int col_width;

	if (count == 0)
		return;

	/* Find maximum name length */
	for (i = 0; i < count; i++) {
		int len = strlen(entries[i].name);
		if (len > max_len)
			max_len = len;
	}

	/* Add padding between columns */
	col_width = max_len + 2;

	cols = term_width / col_width;
	if (cols < 1)
		cols = 1;

	rows = (count + cols - 1) / cols;

	for (row = 0; row < rows; row++) {
		for (col = 0; col < cols; col++) {
			i = col * rows + row;
			if (i >= count)
				break;

			printf("%-*s", col_width, entries[i].name);
		}
		printf("\n");
	}
}

void print_entries(struct entry *entries, int count, struct options *opts){
	int i;

	if (count == 0)
		return;

	if (opts->long_format) {
		for (i = 0; i < count; i++)
			print_long(&entries[i], opts->path);
	} else if (opts->single_column) {
		for (i = 0; i < count; i++)
			printf("%s\n", entries[i].name);
	} else {
		print_multi_column(entries, count);
	}
}

void usage(void){
	fprintf(stderr, "usage: ls [-1aAfFlL] [directory]\n");
	exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]){
	int fd, nread, opt;
	char *buf;
	char d_type;
	struct dirent *d;
	struct entry *entries;
	int entry_count = 0;
	struct options opts = {0};

	while ((opt = getopt(argc, argv, "1aAfFlL")) != -1) {
		switch (opt) {
		case '1':
			opts.single_column = 1;
			break;
		case 'a':
		case 'A':
			opts.show_hidden = 1;
			break;
		case 'f':
		case 'F':
			opts.no_sort = 1;
			opts.show_hidden = 1;  /* -f implies -a */
			break;
		case 'l':
		case 'L':
			opts.long_format = 1;
			break;
		default:
			usage();
		}
	}

	/* Get directory path */
	opts.path = (optind < argc) ? argv[optind] : ".";
	buf = malloc(BUF_SIZE);
	if (!buf)
		err(EXIT_FAILURE, "malloc");

	entries = malloc(MAX_ENTRIES * sizeof(struct entry));
	if (!entries)
		err(EXIT_FAILURE, "malloc");

	fd = open(opts.path, O_RDONLY | O_DIRECTORY);
	if (fd == -1)
		err(EXIT_FAILURE, "cannot access '%s'", opts.path);

	for (;;) {
		nread = syscall(SYS_getdents, fd, buf, BUF_SIZE);
		if (nread == -1)
			err(EXIT_FAILURE, "getdents");

		if (nread == 0)
			break;

		for (size_t bpos = 0; bpos < nread;) {
			d = (struct dirent *)(buf + bpos);
			d_type = *(buf + bpos + d->d_reclen - 1);

			/* Skip . and .. unless -a/-f option */
			if (!opts.show_hidden && (strcmp(d->d_name, ".") == 0 || strcmp(d->d_name, "..") == 0)){
				bpos += d->d_reclen;
				continue;
			}

			if (entry_count >= MAX_ENTRIES) {
				fprintf(stderr, "Too many entries\n");
				close(fd);
				exit(EXIT_FAILURE);
			}

			entries[entry_count].name = strdup(d->d_name);
			if (!entries[entry_count].name)
				err(EXIT_FAILURE, "strdup");
			entries[entry_count].type = d_type;
			entries[entry_count].has_stat = 0;
			entry_count++;

			bpos += d->d_reclen;
		}
	}

	close(fd);

	if (!opts.no_sort)
		qsort(entries, entry_count, sizeof(struct entry), entry_cmp);

	print_entries(entries, entry_count, &opts);

	for (int i = 0; i < entry_count; i++)
		free(entries[i].name);

	free(entries);
	free(buf);

	return EXIT_SUCCESS;
}
