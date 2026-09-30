// 6502 assembler
// Stefan Wessels, 2025
// This is free and unencumbered software released into the public domain.

#pragma once

#define ASM_ERR_MAX_STR_LEN 256

typedef enum {
    ASM_ERR_DEFINE,
    ASM_ERR_RESOLVE,
    ASM_ERR_FATAL,
    /* Logged on either pass. Does not fail assembly. */
    ASM_ERR_WARNING
} ASM_ERR_CLASS;

void asm_err(ASSEMBLER *as, ASM_ERR_CLASS cls, const char *format, ...);
void asm_warn(ASSEMBLER *as, const char *format, ...);
