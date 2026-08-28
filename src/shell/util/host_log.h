#pragma once

#include "log.h"

#include <stdbool.h>

typedef enum host_log_level {
    HOST_LOG_LEVEL_ALL = 0,
    HOST_LOG_LEVEL_WARN,
    HOST_LOG_LEVEL_ERROR,
    HOST_LOG_LEVEL_NONE
} host_log_level;

/* Default policy is WARN. */
void host_log_init(void);
void host_log_apply(host_log_level level);

const char *host_log_level_name(host_log_level level);
bool host_log_level_from_string(const char *s, host_log_level *out_level);
