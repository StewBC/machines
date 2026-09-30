// 6502 assembler
// Stefan Wessels, 2025
// This is free and unencumbered software released into the public domain.

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "dynarray.h"

/* Zero is an error, so a cleared entry still fails assembly. */
#define ASM_SEVERITY_ERROR 0
#define ASM_SEVERITY_WARNING 1

typedef struct {
    size_t line_number;
    size_t message_length;
    size_t suppressed;
    uint32_t file_name_hash;
    int severity;
    char *err_str;
} ERROR_ENTRY;

typedef struct {
    AM65_DYNARRAY log_array;
    size_t longest_error_message_length;
} ERRORLOG;

void errlog(ERRORLOG *log, ERROR_ENTRY *e);
/* Entries that fail assembly. Warnings are not included. */
size_t errlog_error_count(const ERRORLOG *log);
void errlog_init(ERRORLOG *log);
void errlog_clean(ERRORLOG *log);
void errlog_shutdown(ERRORLOG *log);
