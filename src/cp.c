/*
 * cp - copy files
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <getopt.h>

extern int optind;

void usage()
{
	fprintf(stderr, "usage: cp [-nRr] source target\n");
	exit(1);
}

int copy_file(const char *src, const char *dst, int noclobber)
{
	int fold, fnew, n;
	struct stat buf, dirbuf;
	char iobuf[512];

	if ((fold = open(src, 0)) < 0) {
		perror(src);
		return -1;
	}
	stat(src, &buf);

	if (stat(dst, &dirbuf) >= 0) {
		if (buf.st_dev == dirbuf.st_dev && buf.st_ino == dirbuf.st_ino) {
			fprintf(stderr, "cp: copying file to itself.\n");
			close(fold);
			return -1;
		}
		if (noclobber) {
			close(fold);
			return 0;
		}
	}
	if ((fnew = open(dst, O_CREAT | O_WRONLY | O_TRUNC, buf.st_mode & 07777)) < 0) {
		perror(dst);
		close(fold);
		return -1;
	}

	while ((n = read(fold, iobuf, sizeof(iobuf))) > 0) {
		if (write(fnew, iobuf, n) != n) {
			perror(dst);
			close(fold);
			close(fnew);
			return -1;
		}
	}

	close(fold);
	close(fnew);
	return 0;
}

int copy_dir(const char *src, const char *dst, int noclobber, int recursive)
{
	DIR *dp;
	struct dirent *entry;
	struct stat st;
	char srcpath[1024], dstpath[1024];

	if (stat(src, &st) < 0) {
		perror(src);
		return -1;
	}

	if (mkdir(dst, st.st_mode & 07777) < 0 && access(dst, F_OK) != 0) {
		perror(dst);
		return -1;
	}

	if (!(dp = opendir(src))) {
		perror(src);
		return -1;
	}

	while ((entry = readdir(dp)) != NULL) {
		if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
			continue;

		snprintf(srcpath, sizeof(srcpath), "%s/%s", src, entry->d_name);
		snprintf(dstpath, sizeof(dstpath), "%s/%s", dst, entry->d_name);

		if (stat(srcpath, &st) < 0) {
			perror(srcpath);
			continue;
		}

		if (S_ISDIR(st.st_mode)) {
			if (recursive)
				copy_dir(srcpath, dstpath, noclobber, recursive);
		} else {
			copy_file(srcpath, dstpath, noclobber);
		}
	}

	closedir(dp);
	return 0;
}

int main(int argc, char **argv)
{
	struct stat st;
	int noclobber = 0, recursive = 0;
	int opt;

	while ((opt = getopt(argc, argv, "nRr")) != -1) {
		switch (opt) {
		case 'R':
		case 'r':
			recursive = 1;
			break;
		case 'n':
			noclobber = 1;
			break;
		default:
			usage();
		}
	}

	if (argc - optind != 2)
		usage();

	if (stat(argv[optind], &st) < 0) {
		perror(argv[optind]);
		exit(1);
	}

	if (S_ISDIR(st.st_mode)) {
		if (!recursive) {
			fprintf(stderr, "cp: -r not specified; omitting directory '%s'\n", argv[optind]);
			exit(1);
		}
		copy_dir(argv[optind], argv[optind + 1], noclobber, recursive);
	} else {
		copy_file(argv[optind], argv[optind + 1], noclobber);
	}

	exit(0);
}

