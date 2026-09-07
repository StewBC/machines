#pragma once

typedef struct host_dir host_dir;

/* Unsorted, unbounded directory iteration. No filtering of dot entries.
 * Open returns NULL on failure. Read returns a filename (valid until the next
 * read or close), or NULL at end/error. Close releases the open directory. */
host_dir *host_dir_open(const char *path);
const char *host_dir_read(host_dir *dir);
void host_dir_close(host_dir *dir);
