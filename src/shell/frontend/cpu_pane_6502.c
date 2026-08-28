#include "cpu_pane_6502.h"

#include <stdio.h>
#include <string.h>

static const uint8_t k_flag_bits[8] = {7, 6, 5, 4, 3, 2, 1, 0};

static const nk_flags k_pane_flags =
    NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_NO_SCROLLBAR;

void cpu_pane_6502_init(cpu_pane_6502_state *state)
{
    if (state == NULL) {
        return;
    }
    memset(state, 0, sizeof(*state));
}

static int cpu_pane_hex_digit_value(char ch)
{
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f') {
        return ch - 'a' + 10;
    }
    if (ch >= 'A' && ch <= 'F') {
        return ch - 'A' + 10;
    }
    return -1;
}

bool cpu_pane_6502_parse_hex(const char *text, size_t max_digits, uint16_t *out)
{
    uint16_t value = 0;
    size_t i;
    size_t length;

    if (text == NULL || out == NULL) {
        return false;
    }
    length = strlen(text);
    if (length == 0 || length > max_digits) {
        return false;
    }
    for (i = 0; i < length; ++i) {
        int digit = cpu_pane_hex_digit_value(text[i]);
        if (digit < 0) {
            return false;
        }
        value = (uint16_t)((value << 4) | (uint16_t)digit);
    }
    *out = value;
    return true;
}

bool cpu_pane_6502_parse_flag(const char *text, uint8_t *out)
{
    if (text == NULL || out == NULL || text[0] == '\0' || text[1] != '\0') {
        return false;
    }
    if (text[0] != '0' && text[0] != '1') {
        return false;
    }
    *out = (uint8_t)(text[0] - '0');
    return true;
}

void cpu_pane_6502_format(
    cpu_pane_6502_state *state,
    const cpu_pane_6502_regs *regs,
    cpu_pane_6502_field except)
{
    size_t i;

    if (state == NULL || regs == NULL) {
        return;
    }
    if (except != CPU_PANE_6502_FIELD_PC) {
        snprintf(state->pc, sizeof(state->pc), "%04X", regs->pc);
    }
    if (except != CPU_PANE_6502_FIELD_SP) {
        snprintf(state->sp, sizeof(state->sp), "%02X", regs->sp);
    }
    if (except != CPU_PANE_6502_FIELD_A) {
        snprintf(state->a, sizeof(state->a), "%02X", regs->a);
    }
    if (except != CPU_PANE_6502_FIELD_X) {
        snprintf(state->x, sizeof(state->x), "%02X", regs->x);
    }
    if (except != CPU_PANE_6502_FIELD_Y) {
        snprintf(state->y, sizeof(state->y), "%02X", regs->y);
    }
    for (i = 0; i < 8; ++i) {
        cpu_pane_6502_field field = (cpu_pane_6502_field)(
            CPU_PANE_6502_FIELD_STATUS_N + (int)i);
        if (except == field) {
            continue;
        }
        state->flags[i][0] = (regs->p & (uint8_t)(1u << k_flag_bits[i])) ? '1' : '0';
        state->flags[i][1] = '\0';
    }
}

void cpu_pane_6502_commit(
    cpu_pane_6502_state *state,
    cpu_pane_6502_field field,
    const cpu_pane_6502_regs *regs,
    const cpu_pane_6502_ops *ops)
{
    uint16_t value;
    uint8_t flag_value;

    if (state == NULL || regs == NULL) {
        return;
    }

    switch (field) {
        case CPU_PANE_6502_FIELD_PC:
            if (cpu_pane_6502_parse_hex(state->pc, 4, &value) &&
                ops != NULL && ops->set_pc != NULL) {
                ops->set_pc(ops->ctx, value);
                return;
            }
            break;
        case CPU_PANE_6502_FIELD_SP:
            if (cpu_pane_6502_parse_hex(state->sp, 2, &value) &&
                ops != NULL && ops->set_sp != NULL) {
                ops->set_sp(ops->ctx, (uint8_t)value);
                return;
            }
            break;
        case CPU_PANE_6502_FIELD_A:
            if (cpu_pane_6502_parse_hex(state->a, 2, &value) &&
                ops != NULL && ops->set_a != NULL) {
                ops->set_a(ops->ctx, (uint8_t)value);
                return;
            }
            break;
        case CPU_PANE_6502_FIELD_X:
            if (cpu_pane_6502_parse_hex(state->x, 2, &value) &&
                ops != NULL && ops->set_x != NULL) {
                ops->set_x(ops->ctx, (uint8_t)value);
                return;
            }
            break;
        case CPU_PANE_6502_FIELD_Y:
            if (cpu_pane_6502_parse_hex(state->y, 2, &value) &&
                ops != NULL && ops->set_y != NULL) {
                ops->set_y(ops->ctx, (uint8_t)value);
                return;
            }
            break;
        case CPU_PANE_6502_FIELD_STATUS_N:
        case CPU_PANE_6502_FIELD_STATUS_V:
        case CPU_PANE_6502_FIELD_STATUS_UNUSED:
        case CPU_PANE_6502_FIELD_STATUS_B:
        case CPU_PANE_6502_FIELD_STATUS_D:
        case CPU_PANE_6502_FIELD_STATUS_I:
        case CPU_PANE_6502_FIELD_STATUS_Z:
        case CPU_PANE_6502_FIELD_STATUS_C: {
            size_t flag_index = (size_t)(field - CPU_PANE_6502_FIELD_STATUS_N);
            if (cpu_pane_6502_parse_flag(state->flags[flag_index], &flag_value) &&
                ops != NULL && ops->set_status != NULL) {
                uint8_t mask = (uint8_t)(1u << k_flag_bits[flag_index]);
                uint8_t status = (uint8_t)(regs->p & (uint8_t)~mask);
                if (flag_value != 0) {
                    status = (uint8_t)(status | mask);
                }
                ops->set_status(ops->ctx, status);
                return;
            }
            break;
        }
        case CPU_PANE_6502_FIELD_NONE:
        default:
            break;
    }
    cpu_pane_6502_format(state, regs, CPU_PANE_6502_FIELD_NONE);
}

static void cpu_pane_6502_draw_edit(
    struct nk_context *ctx,
    cpu_pane_6502_state *state,
    cpu_pane_6502_field field,
    char *buffer,
    int max,
    nk_plugin_filter filter,
    bool editable,
    const cpu_pane_6502_regs *regs,
    const cpu_pane_6502_ops *ops)
{
    nk_flags edit_flags = NK_EDIT_SELECTABLE | NK_EDIT_CLIPBOARD | NK_EDIT_SIG_ENTER;
    nk_flags result;

    if (!editable) {
        edit_flags |= NK_EDIT_READ_ONLY;
    }
    result = nk_edit_string_zero_terminated(ctx, edit_flags, buffer, max, filter);
    if ((result & NK_EDIT_ACTIVE) != 0 && editable) {
        ctx->current->edit.mode = NK_TEXT_EDIT_MODE_REPLACE;
    }
    if ((result & NK_EDIT_ACTIVATED) != 0 && editable) {
        state->active_field = field;
    }
    if ((result & NK_EDIT_COMMITED) != 0) {
        cpu_pane_6502_commit(state, field, regs, ops);
        state->active_field = CPU_PANE_6502_FIELD_NONE;
        nk_edit_unfocus(ctx);
    }
}

static void cpu_pane_6502_draw_pair(
    struct nk_context *ctx,
    cpu_pane_6502_state *state,
    const char *label,
    cpu_pane_6502_field field,
    char *buffer,
    int max,
    float label_w,
    float edit_w,
    bool editable,
    const cpu_pane_6502_regs *regs,
    const cpu_pane_6502_ops *ops)
{
    nk_layout_row_push(ctx, label_w);
    nk_label(ctx, label, NK_TEXT_LEFT);
    nk_layout_row_push(ctx, edit_w);
    cpu_pane_6502_draw_edit(
        ctx, state, field, buffer, max, nk_filter_hex, editable, regs, ops);
}

static void cpu_pane_6502_draw_flag(
    struct nk_context *ctx,
    cpu_pane_6502_state *state,
    const char *label,
    size_t index,
    bool editable,
    const cpu_pane_6502_regs *regs,
    const cpu_pane_6502_ops *ops)
{
    nk_layout_row_push(ctx, 0.055f);
    nk_label(ctx, label, NK_TEXT_LEFT);
    nk_layout_row_push(ctx, 0.070f);
    cpu_pane_6502_draw_edit(
        ctx,
        state,
        (cpu_pane_6502_field)(CPU_PANE_6502_FIELD_STATUS_N + (int)index),
        state->flags[index],
        (int)sizeof(state->flags[index]),
        nk_filter_binary,
        editable,
        regs,
        ops);
}

void cpu_pane_6502_draw(
    struct nk_context *ctx,
    struct nk_rect bounds,
    cpu_pane_6502_state *state,
    const cpu_pane_6502_regs *regs,
    bool has_cpu,
    bool editable,
    const char *empty_status,
    const cpu_pane_6502_ops *ops)
{
    if (ctx == NULL || state == NULL) {
        return;
    }

    if (!editable) {
        state->active_field = CPU_PANE_6502_FIELD_NONE;
    }
    if (has_cpu && regs != NULL) {
        cpu_pane_6502_format(state, regs, state->active_field);
    }
    if (state->cancel_edit) {
        if (has_cpu && regs != NULL) {
            cpu_pane_6502_format(state, regs, CPU_PANE_6502_FIELD_NONE);
        }
        state->active_field = CPU_PANE_6502_FIELD_NONE;
        state->cancel_edit = false;
    }

    if (nk_begin(ctx, "CPU", bounds, k_pane_flags)) {
        if (!has_cpu || regs == NULL) {
            nk_layout_row_dynamic(ctx, 20.0f, 1);
            nk_label(ctx, empty_status != NULL ? empty_status : "Unknown", NK_TEXT_LEFT);
            nk_label(ctx, "PC ----  SP --  A --  X --  Y --", NK_TEXT_LEFT);
            nk_label(ctx, "N -  V -  - -  B -  D -  I -  Z -  C -", NK_TEXT_LEFT);
        } else {
            nk_layout_row_begin(ctx, NK_DYNAMIC, 22.0f, 10);
            cpu_pane_6502_draw_pair(
                ctx, state, "PC", CPU_PANE_6502_FIELD_PC, state->pc,
                (int)sizeof(state->pc), 0.07f, 0.19f, editable, regs, ops);
            cpu_pane_6502_draw_pair(
                ctx, state, "SP", CPU_PANE_6502_FIELD_SP, state->sp,
                (int)sizeof(state->sp), 0.07f, 0.13f, editable, regs, ops);
            cpu_pane_6502_draw_pair(
                ctx, state, "A", CPU_PANE_6502_FIELD_A, state->a,
                (int)sizeof(state->a), 0.05f, 0.12f, editable, regs, ops);
            cpu_pane_6502_draw_pair(
                ctx, state, "X", CPU_PANE_6502_FIELD_X, state->x,
                (int)sizeof(state->x), 0.05f, 0.12f, editable, regs, ops);
            cpu_pane_6502_draw_pair(
                ctx, state, "Y", CPU_PANE_6502_FIELD_Y, state->y,
                (int)sizeof(state->y), 0.05f, 0.12f, editable, regs, ops);
            nk_layout_row_end(ctx);

            nk_layout_row_begin(ctx, NK_DYNAMIC, 22.0f, 16);
            cpu_pane_6502_draw_flag(ctx, state, "N", 0, editable, regs, ops);
            cpu_pane_6502_draw_flag(ctx, state, "V", 1, editable, regs, ops);
            cpu_pane_6502_draw_flag(ctx, state, "-", 2, editable, regs, ops);
            cpu_pane_6502_draw_flag(ctx, state, "B", 3, editable, regs, ops);
            cpu_pane_6502_draw_flag(ctx, state, "D", 4, editable, regs, ops);
            cpu_pane_6502_draw_flag(ctx, state, "I", 5, editable, regs, ops);
            cpu_pane_6502_draw_flag(ctx, state, "Z", 6, editable, regs, ops);
            cpu_pane_6502_draw_flag(ctx, state, "C", 7, editable, regs, ops);
            nk_layout_row_end(ctx);
        }
    }
    nk_end(ctx);
}
