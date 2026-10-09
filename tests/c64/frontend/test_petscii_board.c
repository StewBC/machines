#include "petscii_board.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void expect_true(const char *name, bool value)
{
    if (!value) {
        fprintf(stderr, "%s: expected true\n", name);
        exit(1);
    }
}

static void expect_int(const char *name, int expected, int actual)
{
    if (expected != actual) {
        fprintf(stderr, "%s: expected %d, got %d\n", name, expected, actual);
        exit(1);
    }
}

static const petscii_placed_key *find_label(const petscii_layout *layout, const char *label, int which)
{
    int i;
    int seen = 0;

    for (i = 0; i < layout->count; i++) {
        if (layout->keys[i].label != NULL && strcmp(layout->keys[i].label, label) == 0) {
            if (seen == which) {
                return &layout->keys[i];
            }
            seen += 1;
        }
    }
    fprintf(stderr, "missing key label %s #%d\n", label, which);
    exit(1);
}

static int find_layout_index(const petscii_layout *layout, const petscii_placed_key *key)
{
    return (int)(key - layout->keys);
}

static void test_screen_codes(void)
{
    expect_int("A screen", 0x01, petscii_board_screen_code(0x41));
    expect_int("shifted A screen", 0x41, petscii_board_screen_code(0xC1));
    expect_int("cbm A screen", 0x70, petscii_board_screen_code(0xB0));
    expect_int("home is control", -1, petscii_board_screen_code(0x13));
    expect_int("pi", 0x5E, petscii_board_screen_code(0xFF));
}

static void test_faces(void)
{
    petscii_board board;
    petscii_layout layout;
    const petscii_placed_key *key_a;
    const petscii_placed_key *key_1;
    const petscii_placed_key *home;
    const petscii_placed_key *key_0;

    petscii_board_init(&board);
    petscii_board_layout(&board, 4000, &layout);
    expect_true("board has keys", layout.count > 60);
    expect_int("wide view does not scroll", 0, layout.scroll_max);

    key_a = find_label(&layout, "A", 0);
    expect_int("A faces", 3, key_a->face_count);
    expect_int("A plain kind", PETSCII_FACE_GLYPH, (int)key_a->faces[0].kind);
    expect_int("A plain screen", 0x01, key_a->faces[0].screen);
    expect_true("A plain has no shift", !key_a->faces[0].shift);
    expect_int("A shift screen", 0x41, key_a->faces[1].screen);
    expect_true("A shift flag", key_a->faces[1].shift);
    expect_int("A cbm screen", 0x70, key_a->faces[2].screen);
    expect_true("A cbm flag", key_a->faces[2].commodore);
    expect_true("A cbm is not shift", !key_a->faces[2].shift);

    key_1 = find_label(&layout, "1", 0);
    expect_int("1 faces", 4, key_1->face_count);
    expect_int("1 glyph", PETSCII_FACE_GLYPH, (int)key_1->faces[0].kind);
    expect_int("1 bang", PETSCII_FACE_GLYPH, (int)key_1->faces[1].kind);
    expect_int("1 cbm color kind", PETSCII_FACE_COLOR, (int)key_1->faces[2].kind);
    expect_int("1 cbm orange", 8, key_1->faces[2].color);
    expect_true("1 cbm color uses commodore", key_1->faces[2].commodore);
    expect_int("1 ctrl color kind", PETSCII_FACE_COLOR, (int)key_1->faces[3].kind);
    expect_int("1 ctrl black", 0, key_1->faces[3].color);
    expect_true("1 ctrl color uses control", key_1->faces[3].control);

    home = find_label(&layout, "HOME", 0);
    expect_int("home faces", 2, home->face_count);
    expect_true("home name", home->faces[0].name != NULL && strcmp(home->faces[0].name, "HOME") == 0);
    expect_true("clr name", home->faces[1].name != NULL && strcmp(home->faces[1].name, "CLR") == 0);
    expect_true("clr is shifted", home->faces[1].shift);

    key_0 = find_label(&layout, "0", 0);
    expect_true("0 has a glyph", key_0->face_count >= 2);
    expect_true(
        "0 ctrl is rvs off",
        key_0->faces[key_0->face_count - 1].name != NULL &&
            strcmp(key_0->faces[key_0->face_count - 1].name, "OFF") == 0);
}

static bool op_is(const petscii_key_op *op, petscii_op_kind kind, c64_key key, bool pressed)
{
    return op->kind == kind && op->key == key && op->pressed == pressed;
}

static void test_click_chords(void)
{
    petscii_board board;
    petscii_layout layout;
    const petscii_placed_key *key_a;
    const petscii_placed_key *key_1;
    petscii_key_op ops[PETSCII_OP_MAX];
    petscii_mods host;
    size_t n;
    int key_index = 0;
    int face_index = 0;

    memset(&host, 0, sizeof(host));
    petscii_board_init(&board);
    petscii_board_layout(&board, 4000, &layout);
    key_a = find_label(&layout, "A", 0);
    key_1 = find_label(&layout, "1", 0);

    n = petscii_board_pointer_down(
        &board, &key_a->faces[0], 10, 1000, &host, ops, PETSCII_OP_MAX);
    expect_true("plain A presses A last", n >= 1 && op_is(&ops[n - 1], PETSCII_OP_KEY, C64_KEY_A, true));
    expect_true("plain A forces shift up", op_is(&ops[0], PETSCII_OP_KEY, C64_KEY_LEFT_SHIFT, false));
    expect_true("plain A forces right shift up", op_is(&ops[1], PETSCII_OP_KEY, C64_KEY_RIGHT_SHIFT, false));
    expect_true("plain A forces commodore up", op_is(&ops[3], PETSCII_OP_KEY, C64_KEY_COMMODORE, false));

    n = petscii_board_pointer_up(&board, 10, 1010, &host, ops, PETSCII_OP_MAX);
    expect_int("short click waits for a scan", 0, (int)n);
    expect_true("still holding", board.holding);

    n = petscii_board_pump(&board, 11, 1020, &host, ops, PETSCII_OP_MAX);
    expect_true("release after a guest frame", n >= 1 && op_is(&ops[0], PETSCII_OP_KEY, C64_KEY_A, false));
    expect_true("hold cleared", !board.holding);

    n = petscii_board_pointer_down(
        &board, &key_a->faces[1], 20, 2000, &host, ops, PETSCII_OP_MAX);
    expect_true("shifted A holds left shift", op_is(&ops[0], PETSCII_OP_KEY, C64_KEY_LEFT_SHIFT, true));
    expect_true("shifted A does not hold commodore", op_is(&ops[3], PETSCII_OP_KEY, C64_KEY_COMMODORE, false));
    expect_true("shifted A presses A", op_is(&ops[n - 1], PETSCII_OP_KEY, C64_KEY_A, true));
    n = petscii_board_pointer_up(&board, 20, 2000 + 80, &host, ops, PETSCII_OP_MAX);
    expect_true("80ms releases without a new frame", n >= 1 && op_is(&ops[0], PETSCII_OP_KEY, C64_KEY_A, false));
    expect_true("shift released", op_is(&ops[1], PETSCII_OP_KEY, C64_KEY_LEFT_SHIFT, false));

    n = petscii_board_pointer_down(
        &board, &key_a->faces[2], 30, 3000, &host, ops, PETSCII_OP_MAX);
    expect_true("cbm A holds commodore", op_is(&ops[3], PETSCII_OP_KEY, C64_KEY_COMMODORE, true));
    expect_true("cbm A does not hold shift", op_is(&ops[0], PETSCII_OP_KEY, C64_KEY_LEFT_SHIFT, false));
    petscii_board_cancel(&board, &host, ops, PETSCII_OP_MAX);

    host.shift = true;
    n = petscii_board_pointer_down(
        &board, &key_a->faces[0], 40, 4000, &host, ops, PETSCII_OP_MAX);
    expect_true("host shift does not select the shifted glyph", op_is(&ops[0], PETSCII_OP_KEY, C64_KEY_LEFT_SHIFT, false));
    expect_true("host right shift is cleared for the tap", op_is(&ops[1], PETSCII_OP_KEY, C64_KEY_RIGHT_SHIFT, false));
    n = petscii_board_pointer_up(&board, 41, 4010, &host, ops, PETSCII_OP_MAX);
    expect_true("host shift comes back on right shift", op_is(&ops[2], PETSCII_OP_KEY, C64_KEY_RIGHT_SHIFT, true));
    host.shift = false;

    n = petscii_board_pointer_down(
        &board, &key_1->faces[3], 50, 5000, &host, ops, PETSCII_OP_MAX);
    expect_true("black swatch holds control", op_is(&ops[2], PETSCII_OP_KEY, C64_KEY_CONTROL, true));
    expect_true("black swatch presses 1", op_is(&ops[n - 1], PETSCII_OP_KEY, C64_KEY_1, true));
    expect_true("black swatch is not commodore", op_is(&ops[3], PETSCII_OP_KEY, C64_KEY_COMMODORE, false));
    petscii_board_cancel(&board, &host, ops, PETSCII_OP_MAX);

    petscii_board_layout(&board, 4000, &layout);
    key_a = find_label(&layout, "A", 0);
    expect_true(
        "hit the plain A face",
        petscii_board_hit(
            &layout,
            key_a->face_rects[0].x + 1,
            key_a->face_rects[0].y + 1,
            &key_index,
            &face_index));
    expect_int("hit key", find_layout_index(&layout, key_a), key_index);
    expect_int("hit face", 0, face_index);
}

static void test_shift_lock_restore_and_scroll(void)
{
    petscii_board board;
    petscii_layout layout;
    const petscii_placed_key *lock_key;
    const petscii_placed_key *restore;
    const petscii_placed_key *key_a;
    petscii_key_op ops[PETSCII_OP_MAX];
    petscii_mods host;
    size_t n;

    memset(&host, 0, sizeof(host));
    petscii_board_init(&board);
    petscii_board_layout(&board, 4000, &layout);
    lock_key = find_label(&layout, "LOCK", 0);
    restore = find_label(&layout, "RSTR", 0);

    n = petscii_board_pointer_down(
        &board, &lock_key->faces[0], 1, 0, &host, ops, PETSCII_OP_MAX);
    expect_int("lock is one op", 1, (int)n);
    expect_true("lock presses shift", op_is(&ops[0], PETSCII_OP_KEY, C64_KEY_LEFT_SHIFT, true));
    expect_true("locked", petscii_board_shift_locked(&board));
    expect_true("lock is not a hold", !board.holding);

    petscii_board_layout(&board, 4000, &layout);
    key_a = find_label(&layout, "A", 0);
    n = petscii_board_pointer_down(
        &board, &key_a->faces[0], 2, 100, &host, ops, PETSCII_OP_MAX);
    expect_true("locked unshifted tap drops shift", op_is(&ops[0], PETSCII_OP_KEY, C64_KEY_LEFT_SHIFT, false));
    n = petscii_board_pointer_up(&board, 3, 110, &host, ops, PETSCII_OP_MAX);
    expect_true("lock restores shift", op_is(&ops[1], PETSCII_OP_KEY, C64_KEY_LEFT_SHIFT, true));

    n = petscii_board_pointer_down(
        &board, &restore->faces[0], 4, 200, &host, ops, PETSCII_OP_MAX);
    expect_int("restore one op", 1, (int)n);
    expect_true("restore nmi", ops[0].kind == PETSCII_OP_RESTORE);

    petscii_board_layout(&board, 200, &layout);
    expect_true("narrow view scrolls", layout.scroll_max > 0);
    petscii_board_scroll(&board, 200, 100000);
    expect_int("scroll clamps", layout.scroll_max, board.scroll_x);
    petscii_board_scroll(&board, 200, -100000);
    expect_int("scroll floor", 0, board.scroll_x);
}

static void test_space_and_host_lights(void)
{
    petscii_board board;
    petscii_layout layout;
    const petscii_placed_key *space_key;
    const petscii_placed_key *key_a;
    const petscii_placed_key *shift_key;
    const petscii_placed_key *larr;
    petscii_key_op ops[PETSCII_OP_MAX];
    petscii_mods host;
    bool down[C64_KEY_COUNT];
    size_t n;

    memset(&host, 0, sizeof(host));
    memset(down, 0, sizeof(down));
    petscii_board_init(&board);
    petscii_board_layout(&board, 4000, &layout);
    space_key = find_label(&layout, "SPACE", 0);
    expect_int("space is one face", 1, space_key->face_count);
    larr = find_label(&layout, "LARR", 0);
    expect_true("wide board is centered", larr->rect.x > 100);

    n = petscii_board_pointer_down(
        &board, &space_key->faces[0], 1, 0, &host, ops, PETSCII_OP_MAX);
    expect_true("plain space leaves shift up", op_is(&ops[0], PETSCII_OP_KEY, C64_KEY_LEFT_SHIFT, false));
    expect_true("plain space presses space", op_is(&ops[n - 1], PETSCII_OP_KEY, C64_KEY_SPACE, true));
    petscii_board_cancel(&board, &host, ops, PETSCII_OP_MAX);

    host.shift = true;
    n = petscii_board_pointer_down(
        &board, &space_key->faces[0], 2, 100, &host, ops, PETSCII_OP_MAX);
    expect_true("shift click space holds shift", op_is(&ops[0], PETSCII_OP_KEY, C64_KEY_LEFT_SHIFT, true));
    expect_true("shift click space presses space", op_is(&ops[n - 1], PETSCII_OP_KEY, C64_KEY_SPACE, true));
    petscii_board_cancel(&board, &host, ops, PETSCII_OP_MAX);
    host.shift = false;

    down[C64_KEY_A] = true;
    down[C64_KEY_LEFT_SHIFT] = true;
    petscii_board_set_host_keys(&board, down, false);
    petscii_board_layout(&board, 4000, &layout);
    key_a = find_label(&layout, "A", 0);
    shift_key = find_label(&layout, "SHIFT", 0);
    expect_true("host shift lights shift", petscii_board_face_held(&board, &shift_key->faces[0]));
    expect_true("host shift+A lights the shifted face", petscii_board_face_held(&board, &key_a->faces[1]));
    expect_true("host shift+A does not light plain A", !petscii_board_face_held(&board, &key_a->faces[0]));

    host.shift = true;
    n = petscii_board_pointer_down(
        &board, &key_a->faces[0], 3, 200, &host, ops, PETSCII_OP_MAX);
    expect_true("ball rule keeps plain A unshifted", op_is(&ops[0], PETSCII_OP_KEY, C64_KEY_LEFT_SHIFT, false));
    expect_true("click lights the face you hit", petscii_board_face_held(&board, &key_a->faces[0]));
    expect_true("click hides host shift", !petscii_board_face_held(&board, &shift_key->faces[0]));
    petscii_board_cancel(&board, &host, ops, PETSCII_OP_MAX);
    host.shift = false;

    {
        const petscii_placed_key *ret = find_label(&layout, "RET", 0);
        const petscii_placed_key *down = find_label(&layout, "CRSR", 0);
        const petscii_placed_key *crsr = find_label(&layout, "CRSR", 1);
        const petscii_placed_key *f7 = find_label(&layout, "F7", 0);
        const petscii_placed_key *zee = find_label(&layout, "Z", 0);

        expect_int("return is one face", 1, ret->face_count);
        expect_true("return face is RET", ret->faces[0].name != NULL && strcmp(ret->faces[0].name, "RET") == 0);
        host.shift = true;
        n = petscii_board_pointer_down(
            &board, &ret->faces[0], 4, 300, &host, ops, PETSCII_OP_MAX);
        expect_true("shift click return holds shift", op_is(&ops[0], PETSCII_OP_KEY, C64_KEY_LEFT_SHIFT, true));
        expect_true("shift click return presses return", op_is(&ops[n - 1], PETSCII_OP_KEY, C64_KEY_RETURN, true));
        petscii_board_cancel(&board, &host, ops, PETSCII_OP_MAX);

        expect_int("cursor right has two faces", 2, crsr->face_count);
        expect_true("left cursor is on the left", crsr->faces[0].name != NULL && strcmp(crsr->faces[0].name, "LEFT") == 0);
        expect_true("right cursor is on the right", crsr->faces[1].name != NULL && strcmp(crsr->faces[1].name, "RIGHT") == 0);
        expect_true("left face sits left of right", crsr->face_rects[0].x < crsr->face_rects[1].x);
        /* Names are 5px glyphs plus 1px gaps: DOWN/LEFT are 23, RIGHT is 29. */
        expect_true("DOWN face holds its name", down->face_rects[0].w >= 29);
        expect_true("LEFT face holds its name", crsr->face_rects[0].w >= 29);
        expect_true("RIGHT face holds its name", crsr->face_rects[1].w >= 29);
        expect_true("cursor keys are wider than a letter", crsr->rect.w > zee->rect.w && down->rect.w > zee->rect.w);
        expect_true("cursor keys stop short of F7", crsr->rect.x + crsr->rect.w < f7->rect.x);
    }
    (void)n;
}

int main(void)
{
    test_screen_codes();
    test_faces();
    test_click_chords();
    test_shift_lock_restore_and_scroll();
    test_space_and_host_lights();
    return 0;
}
