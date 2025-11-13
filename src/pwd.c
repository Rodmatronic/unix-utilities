/*
 * pwd - print current working directory
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>

int main(void) {
    char *buf = NULL;
    size_t size;

    size = 14;

    for (;;) {
        buf = malloc(size);
        if (!buf) {
            perror("malloc");
            return 1;
        }

        if (getcwd(buf, size) != NULL) {
            printf("%s\n", buf);
            free(buf);
            return 0;
        }

        /* getcwd failed */
        if (errno != ERANGE) {
            perror("getcwd");
            free(buf);
            return 1;
        }

        /* buffer too small, grow and retry */
        free(buf);
        size *= 2;
    }
}
