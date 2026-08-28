#include "disasm_6502.h"

#include <stdio.h>
#include <string.h>

static int expect_string(const char *actual, const char *expected, const char *label)
{
    if (strcmp(actual, expected) != 0) {
        fprintf(stderr, "%s: expected '%s', got '%s'\n", label, expected, actual);
        return 1;
    }

    return 0;
}

static int expect_nmos_byte(uint8_t opcode, const char *label)
{
    uint8_t bytes[1];
    disasm_6502_line line;
    char want[16];

    bytes[0] = opcode;
    line = disasm_6502_decode_line(0x2000, bytes, 1, NULL, DISASM_6502_NMOS);
    snprintf(want, sizeof(want), ".BYTE $%02X", opcode);
    if (expect_string(line.text, want, label) != 0) {
        return 1;
    }
    if (!line.forced_byte || line.length != 1) {
        fprintf(stderr, "%s: NMOS metadata mismatch\n", label);
        return 1;
    }
    if (disasm_6502_opcode_is_valid(opcode, DISASM_6502_NMOS)) {
        fprintf(stderr, "%s: NMOS should treat opcode as invalid\n", label);
        return 1;
    }
    return 0;
}

static int expect_65c02(
    const uint8_t *bytes,
    size_t length,
    const char *text,
    uint8_t want_length,
    const char *label)
{
    disasm_6502_line line;

    line = disasm_6502_decode_line(0x2000, bytes, length, NULL, DISASM_6502_65C02);
    if (expect_string(line.text, text, label) != 0) {
        return 1;
    }
    if (line.forced_byte || line.length != want_length) {
        fprintf(stderr, "%s: 65C02 metadata mismatch (len %u forced %d)\n",
            label, line.length, (int)line.forced_byte);
        return 1;
    }
    if (!disasm_6502_opcode_is_valid(bytes[0], DISASM_6502_65C02)) {
        fprintf(stderr, "%s: 65C02 should treat opcode as valid\n", label);
        return 1;
    }
    if (disasm_6502_instruction_length(bytes[0], DISASM_6502_65C02) != want_length) {
        fprintf(stderr, "%s: 65C02 length helper mismatch\n", label);
        return 1;
    }
    return 0;
}

int main(void)
{
    int failures = 0;
    uint8_t lda[] = {0xa9, 0x7f};
    uint8_t bra[] = {0x80, 0xfe};
    uint8_t phx[] = {0xda};
    uint8_t stz[] = {0x64, 0x10};
    uint8_t ina[] = {0x1a};
    uint8_t lda_ind[] = {0xb2, 0x80};
    uint8_t jmp_indx[] = {0x7c, 0x00, 0x30};
    uint8_t bad[] = {0x02};
    disasm_6502_line line;

    failures += expect_nmos_byte(0x80, "nmos BRA");
    failures += expect_nmos_byte(0xda, "nmos PHX");
    failures += expect_nmos_byte(0x64, "nmos STZ");
    failures += expect_nmos_byte(0x1a, "nmos INC A");
    failures += expect_nmos_byte(0xb2, "nmos LDA (zp)");
    failures += expect_nmos_byte(0x7c, "nmos JMP (abs,X)");

    failures += expect_65c02(bra, sizeof(bra), "BRA $2000", 2, "65c02 BRA");
    failures += expect_65c02(phx, sizeof(phx), "PHX", 1, "65c02 PHX");
    failures += expect_65c02(stz, sizeof(stz), "STZ $10", 2, "65c02 STZ zp");
    failures += expect_65c02(ina, sizeof(ina), "INC A", 1, "65c02 INC A");
    failures += expect_65c02(lda_ind, sizeof(lda_ind), "LDA ($80)", 2, "65c02 LDA (zp)");
    failures += expect_65c02(jmp_indx, sizeof(jmp_indx), "JMP ($3000,X)", 3, "65c02 JMP (abs,X)");

    line = disasm_6502_decode_line(0x0801, lda, sizeof(lda), NULL, DISASM_6502_65C02);
    failures += expect_string(line.text, "LDA #$7F", "65c02 still decodes NMOS LDA");
    line = disasm_6502_decode_line(0x0801, lda, sizeof(lda), NULL, DISASM_6502_NMOS);
    failures += expect_string(line.text, "LDA #$7F", "nmos LDA unchanged");

    line = disasm_6502_decode_line(0x2000, bad, sizeof(bad), NULL, DISASM_6502_65C02);
    failures += expect_string(line.text, ".BYTE $02", "65c02 $02 still byte");
    if (!line.forced_byte || disasm_6502_opcode_is_valid(0x02, DISASM_6502_65C02)) {
        fprintf(stderr, "$02 should stay invalid on 65C02\n");
        failures++;
    }

    return failures == 0 ? 0 : 1;
}
