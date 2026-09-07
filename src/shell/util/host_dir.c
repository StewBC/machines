#include "host_dir.h"

#include <stdlib.h>

#if defined(_WIN32)
#include <errno.h>
#include <io.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

struct host_dir {
    intptr_t handle;
    struct _finddata_t entry;
    int first;
};

host_dir *host_dir_open(const char *path)
{
    host_dir *dir;
    struct stat st;
    char *pattern;
    size_t size;

    if (path == NULL || path[0] == '\0' || stat(path, &st) != 0 ||
        (st.st_mode & _S_IFMT) != _S_IFDIR) {
        return NULL;
    }
    dir = malloc(sizeof(*dir));
    size = strlen(path) + 3u;
    pattern = malloc(size);
    if (dir == NULL || pattern == NULL) {
        free(dir);
        free(pattern);
        return NULL;
    }
    snprintf(pattern, size, "%s%s*", path,
        path[strlen(path) - 1u] == '/' || path[strlen(path) - 1u] == '\\' ? "" : "/");
    dir->handle = _findfirst(pattern, &dir->entry);
    free(pattern);
    /* A valid but empty directory may have no matching entries. */
    if (dir->handle == -1 && errno != ENOENT) {
        free(dir);
        return NULL;
    }
    dir->first = 1;
    return dir;
}

const char *host_dir_read(host_dir *dir)
{
    if (dir->handle == -1) {
        return NULL;
    }
    if (dir->first) {
        dir->first = 0;
    } else if (_findnext(dir->handle, &dir->entry) != 0) {
        return NULL;
    }
    return dir->entry.name;
}

void host_dir_close(host_dir *dir)
{
    if (dir != NULL) {
        if (dir->handle != -1) {
            _findclose(dir->handle);
        }
        free(dir);
    }
}
#else
#include <dirent.h>

struct host_dir {
    DIR *handle;
};

host_dir *host_dir_open(const char *path)
{
    host_dir *dir;
    if (path == NULL) {
        return NULL;
    }
    dir = malloc(sizeof(*dir));
    if (dir != NULL) {
        dir->handle = opendir(path);
        if (dir->handle == NULL) {
            free(dir);
            return NULL;
        }
    }
    return dir;
}

const char *host_dir_read(host_dir *dir)
{
    struct dirent *entry = readdir(dir->handle);
    return entry != NULL ? entry->d_name : NULL;
}

void host_dir_close(host_dir *dir)
{
    if (dir != NULL) {
        closedir(dir->handle);
        free(dir);
    }
}
#endif
