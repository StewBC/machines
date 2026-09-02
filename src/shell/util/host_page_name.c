#include "host_page_name.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

bool host_page_name_stem_now(char stem[16])
{
    time_t now;
    struct tm tm_value;

    if (stem == NULL) {
        return false;
    }
    now = time(NULL);
    if (now == (time_t)-1) {
        return false;
    }
#if defined(_WIN32)
    if (localtime_s(&tm_value, &now) != 0) {
        return false;
    }
#else
    {
        struct tm *tmp = localtime(&now);
        if (tmp == NULL) {
            return false;
        }
        tm_value = *tmp;
    }
#endif
    if (snprintf(
            stem,
            16,
            "%04d%02d%02d-%02d%02d%02d",
            tm_value.tm_year + 1900,
            tm_value.tm_mon + 1,
            tm_value.tm_mday,
            tm_value.tm_hour,
            tm_value.tm_min,
            tm_value.tm_sec) != 15) {
        return false;
    }
    return true;
}

bool host_page_name_build_path(
    const host_page_name_state *st,
    const char *dir,
    const char *ext,
    char *path,
    size_t path_sz,
    char out_stem[16],
    uint8_t *out_xx)
{
    char stem[16];
    unsigned xx;

    if (st == NULL || dir == NULL || dir[0] == '\0' || ext == NULL ||
        ext[0] == '\0' || path == NULL || path_sz == 0u || out_stem == NULL ||
        out_xx == NULL) {
        return false;
    }
    if (!host_page_name_stem_now(stem)) {
        return false;
    }
    if (st->last_stem[0] != '\0' && strcmp(stem, st->last_stem) == 0) {
        if (st->seq >= 99u) {
            return false;
        }
        xx = (unsigned)st->seq + 1u;
    } else {
        xx = 0u;
    }
    if (snprintf(
            path,
            path_sz,
            "%s/%s%02u.%s",
            dir,
            stem,
            xx,
            ext) >= (int)path_sz) {
        return false;
    }
    memcpy(out_stem, stem, sizeof(stem));
    *out_xx = (uint8_t)xx;
    return true;
}

void host_page_name_commit(
    host_page_name_state *st,
    const char stem[16],
    uint8_t xx)
{
    if (st == NULL || stem == NULL) {
        return;
    }
    memcpy(st->last_stem, stem, sizeof(st->last_stem));
    st->last_stem[sizeof(st->last_stem) - 1u] = '\0';
    st->seq = xx;
}
