/*
 * find - recursively list files/folders
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

void walk(const char *path)
{
    DIR *dir;
    struct dirent *entry;
    char fullpath[4096];
    struct stat st;

    printf("%s\n", path);

    dir = opendir(path);
    if (!dir)
        return;

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        snprintf(fullpath, sizeof(fullpath), "%s/%s", path, entry->d_name);
        if (lstat(fullpath, &st) == -1)
            continue;

        if (S_ISDIR(st.st_mode))
            walk(fullpath);
        else
            printf("%s\n", fullpath);
    }

    closedir(dir);
}

int main(int argc, char *argv[])
{
    char path[4096];
    const char *arg;
    size_t len;

    if (argc < 2)
        arg = ".";
    else
        arg = argv[1];

    strncpy(path, arg, sizeof(path) - 1);
    path[sizeof(path) - 1] = '\0';

    len = strlen(path);
    while (len > 1 && path[len - 1] == '/')
        path[--len] = '\0';

    walk(path);
    return 0;
}

