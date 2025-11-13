/*	$OpenBSD: rm.c,v 1.45 2025/04/20 13:47:54 kn Exp $	*/
/*	$NetBSD: rm.c,v 1.19 1995/09/07 06:48:50 jtc Exp $	*/

/*-
 * Copyright (c) 1990, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *	notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *	notice, this list of conditions and the following disclaimer in the
 *	documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *	may be used to endorse or promote products derived from this software
 *	without specific prior written permission.
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

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <getopt.h>

#include "defs.h"

int dflag, eval, fflag, iflag, Pflag, vflag, stdin_ok;

int	check(char *, char *, struct stat *);
void	checkdot(char **);
void	rm_file(char **);
int	rm_overwrite(char *, struct stat *);
int	pass(int, off_t, char *, size_t);
void	rm_tree(char **);
void	usage(void);

extern int optind;

void
l_strmode(mode_t mode, char *p)
{
	 /* print type */
	switch (mode & S_IFMT) {
	case S_IFDIR:			/* directory */
		*p++ = 'd';
		break;
	case S_IFCHR:			/* character special */
		*p++ = 'c';
		break;
	case S_IFBLK:			/* block special */
		*p++ = 'b';
		break;
	case S_IFREG:			/* regular */
		*p++ = '-';
		break;
#ifdef S_IFIFO
	case S_IFIFO:			/* fifo */
		*p++ = 'p';
		break;
#endif
	default:			/* unknown */
		*p++ = '?';
		break;
	}
	/* usr */
	if (mode & S_IRUSR)
		*p++ = 'r';
	else
		*p++ = '-';
	if (mode & S_IWUSR)
		*p++ = 'w';
	else
		*p++ = '-';
	switch (mode & (S_IXUSR | S_ISUID)) {
	case 0:
		*p++ = '-';
		break;
	case S_IXUSR:
		*p++ = 'x';
		break;
	case S_ISUID:
		*p++ = 'S';
		break;
	case S_IXUSR | S_ISUID:
		*p++ = 's';
		break;
	}
	/* group */
	if (mode & S_IRGRP)
		*p++ = 'r';
	else
		*p++ = '-';
	if (mode & S_IWGRP)
		*p++ = 'w';
	else
		*p++ = '-';
	switch (mode & (S_IXGRP | S_ISGID)) {
	case 0:
		*p++ = '-';
		break;
	case S_IXGRP:
		*p++ = 'x';
		break;
	case S_ISGID:
		*p++ = 'S';
		break;
	case S_IXGRP | S_ISGID:
		*p++ = 's';
		break;
	}
	/* other */
	if (mode & S_IROTH)
		*p++ = 'r';
	else
		*p++ = '-';
	if (mode & S_IWOTH)
		*p++ = 'w';
	else
		*p++ = '-';
	switch (mode & (S_IXOTH | S_ISVTX)) {
	case 0:
		*p++ = '-';
		break;
	case S_IXOTH:
		*p++ = 'x';
		break;
	case S_ISVTX:
		*p++ = 'T';
		break;
	case S_IXOTH | S_ISVTX:
		*p++ = 't';
		break;
	}
	*p++ = ' ';		/* will be a '+' if ACL's implemented */
	*p = '\0';
}

int
main(int argc, char *argv[])
{
	int ch, rflag;

	Pflag = rflag = 0;
	while ((ch = getopt(argc, argv, "dfiPRrv")) != -1)
		switch (ch) {
		case 'd':
			dflag = 1;
			break;
		case 'f':
			fflag = 1;
			iflag = 0;
			break;
		case 'i':
			fflag = 0;
			iflag = 1;
			break;
		case 'P':
			Pflag = 1;
			break;
		case 'R':
		case 'r':
			rflag = 1;
			break;
		case 'v':
			vflag = 1;
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc < 1 && fflag == 0)
		usage();

	checkdot(argv);
	stdin_ok = isatty(STDIN_FILENO);

	if (*argv) {
		if (rflag)
			rm_tree(argv);
		else
			rm_file(argv);
	}

	return (eval);
}

void
rm_tree(char **argv)
{
	struct stat sb;
	DIR *dirp;
	struct dirent *dp;
	char path[L_PATH_MAX];

	for (; *argv; argv++) {
		if (stat(*argv, &sb) != 0) {
			if (!fflag || errno != ENOENT) {
				perror(*argv);
				eval = 1;
			}
			continue;
		}

		if (S_ISDIR(sb.st_mode)) {
			if (!fflag && !check(*argv, *argv, &sb))
				continue;

			dirp = opendir(*argv);
			if (!dirp) {
				if (!fflag || errno != ENOENT) {
					perror(*argv);
					eval = 1;
				}
				continue;
			}

			while ((dp = readdir(dirp)) != NULL) {
				char *childv[2];
				if (!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, ".."))
					continue;

				sprintf(path, "%s/%s", *argv, dp->d_name);
				childv[0] = path;
				childv[1] = NULL;
				rm_tree(childv);
			}
			closedir(dirp);

			if (rmdir(*argv) == 0) {
				if (vflag)
					printf("%s\n", *argv);
			} else if (!fflag || errno != ENOENT) {
				perror(*argv);
				eval = 1;
			}
		} else {
			if (Pflag)
				rm_overwrite(*argv, &sb);
			if (unlink(*argv) == 0) {
				if (vflag)
					printf("%s\n", *argv);
			} else if (!fflag || errno != ENOENT) {
				perror(*argv);
				eval = 1;
			}
		}
	}
}

void
rm_file(char **argv)
{
	struct stat sb;
	int rval;
	char *f;

	while ((f = *argv++) != NULL) {
		if (stat(f, &sb)) {
			if (!fflag || errno != ENOENT) {
				perror(f);
				eval = 1;
			}
			continue;
		}

		if (S_ISDIR(sb.st_mode) && !dflag) {
			fprintf(stderr, "%s: is a directory\n", f);
			eval = 1;
			continue;
		}
		if (!fflag && !check(f, f, &sb))
			continue;
		else if (S_ISDIR(sb.st_mode))
			rval = rmdir(f);
		else {
			if (Pflag)
				rm_overwrite(f, &sb);
			rval = unlink(f);
		}
		if (rval && (!fflag || errno != ENOENT)) {
			perror(f);
			eval = 1;
		} else if (rval == 0 && vflag)
			printf("%s\n", f);
	}
}

int
rm_overwrite(char *file, struct stat *sbp)
{
	struct stat sb, sb2;
	size_t bsize = 1024; /* default block size */
	int fd;
	char *buf = NULL;

	fd = -1;
	if (sbp == NULL) {
		if (stat(file, &sb))
			goto err;
		sbp = &sb;
	}
	if (!S_ISREG(sbp->st_mode))
		return 1;
	if (sbp->st_nlink > 1) {
		fprintf(stderr, "%s (inode %lu): not overwritten due to multiple links\n",
			file, (unsigned long)sbp->st_ino);
		return 0;
	}
	if ((fd = open(file, O_WRONLY|O_NONBLOCK)) == -1)
		goto err;
	if (fstat(fd, &sb2))
		goto err;
	if (sb2.st_dev != sbp->st_dev || sb2.st_ino != sbp->st_ino ||
		!S_ISREG(sb2.st_mode)) {
		errno = EPERM;
		goto err;
	}

	/* Fallback: use st_blksize if available */
#ifdef _STATBUF_ST_BLKSIZE
	bsize = sb2.st_blksize > 0 ? sb2.st_blksize : 1024;
#endif

	if ((buf = malloc(bsize)) == NULL) {
		fprintf(stderr, "%s: malloc\n", file);
		exit(1);
	}

	if (!pass(fd, sbp->st_size, buf, bsize))
		goto err;
	/*if (fsync(fd))
		goto err;*/
	close(fd);
	free(buf);
	return 1;

err:
	perror(file);
	if (fd != -1)
		close(fd);
	free(buf);
	eval = 1;
	return 0;
}

int
pass(int fd, off_t len, char *buf, size_t bsize)
{
	size_t wlen;
	size_t i;

	for (; len > 0; len -= wlen) {
		wlen = len < bsize ? len : bsize;
		for (i = 0; i < wlen; i++)
			buf[i] = rand() & 0xff;
		if (write(fd, buf, wlen) != wlen)
			return 0;
	}
	return 1;
}

int
check(char *path, char *name, struct stat *sp)
{
	int ch, first;
	char modep[15];

	if (iflag)
		fprintf(stderr, "remove %s? ", path);
	else {
		if (!stdin_ok /*|| S_ISLNK(sp->st_mode)*/ || !access(name, W_OK) ||
			errno != EACCES)
			return (1);
		l_strmode(sp->st_mode, modep);
		fprintf(stderr, "override %s %d/%d for %s? ",
			modep + 1, sp->st_uid, sp->st_gid, path);
	}
	fflush(stderr);

	first = ch = getchar();
	while (ch != '\n' && ch != EOF)
		ch = getchar();
	return (first == 'y' || first == 'Y');
}

#define ISDOT(a) ((a)[0] == '.' && (!(a)[1] || ((a)[1] == '.' && !(a)[2])))

void
checkdot(char **argv)
{
	char *p, **save, **t;
	int complained;
	struct stat sb, root;

	stat("/", &root);
	complained = 0;
	for (t = argv; *t;) {
		if (stat(*t, &sb) == 0 &&
			root.st_ino == sb.st_ino && root.st_dev == sb.st_dev) {
			if (!complained++)
				fprintf(stderr, "rm: \"/\" may not be removed\n");
			goto skip;
		}
		p = strrchr(*t, '\0');
		while (--p > *t && *p == '/')
			*p = '\0';

		if ((p = strrchr(*t, '/')) != NULL)
			++p;
		else
			p = *t;

		if (ISDOT(p)) {
			if (!complained++)
				fprintf(stderr, "\".\" and \"..\" may not be removed\n");
skip:
			eval = 1;
			for (save = t; (t[0] = t[1]) != NULL; ++t)
				continue;
			t = save;
		} else
			++t;
	}
}

void
usage(void)
{
	(void)fprintf(stderr, "usage: rm [-dfiPRrv] file ...\n");
	exit(1);
}
