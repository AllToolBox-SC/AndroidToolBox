#include "c_prompt_toolkit_buffer.h"
#include "c_prompt_toolkit_keymap.h"
#include "c_prompt_toolkit.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct replay_state {
    cptk_edit_mode mode;
    int vi_insert_mode;
    int vi_pending_op;
    char vi_register[512];
    cptk_buffer buffer;
    char *history[128];
    size_t history_count;
    long history_nav;
    char history_snapshot[512];
} replay_state;

static void replay_capture_range(const cptk_buffer *buffer, size_t start, size_t end, char *out, size_t out_cap) {
    const char *text;
    size_t len;
    size_t a;
    size_t b;
    size_t n;

    if (!out || out_cap == 0) {
        return;
    }
    out[0] = '\0';
    if (!buffer) {
        return;
    }

    text = cptk_buffer_text(buffer);
    len = cptk_buffer_len(buffer);
    a = (start <= len) ? start : len;
    b = (end <= len) ? end : len;
    if (a > b) {
        size_t t = a;
        a = b;
        b = t;
    }
    n = b - a;
    if (n >= out_cap) {
        n = out_cap - 1;
    }
    if (n > 0) {
        memcpy(out, text + a, n);
    }
    out[n] = '\0';
}

static size_t replay_word_end_exclusive(const cptk_buffer *buffer) {
    size_t end = cptk_buffer_word_end_target(buffer);
    size_t len = cptk_buffer_len(buffer);
    if (end < len) {
        return end + 1;
    }
    return len;
}

static int replay_apply_ranged_action(replay_state *st, cptk_editor_action action) {
    size_t start;
    size_t end;

    if (!st) {
        return -1;
    }

    start = cptk_buffer_cursor(&st->buffer);
    end = start;

    switch (action) {
        case CPTK_EDITOR_ACTION_DELETE_TO_END:
        case CPTK_EDITOR_ACTION_CHANGE_TO_END:
        case CPTK_EDITOR_ACTION_YANK_TO_END:
            end = cptk_buffer_len(&st->buffer);
            break;
        case CPTK_EDITOR_ACTION_DELETE_WORD_FORWARD:
        case CPTK_EDITOR_ACTION_CHANGE_WORD_FORWARD:
        case CPTK_EDITOR_ACTION_YANK_WORD_FORWARD:
            end = cptk_buffer_word_forward_target(&st->buffer);
            break;
        case CPTK_EDITOR_ACTION_DELETE_WORD_BACKWARD:
        case CPTK_EDITOR_ACTION_CHANGE_WORD_BACKWARD:
        case CPTK_EDITOR_ACTION_YANK_WORD_BACKWARD:
            start = cptk_buffer_word_backward_target(&st->buffer);
            end = cptk_buffer_cursor(&st->buffer);
            break;
        case CPTK_EDITOR_ACTION_DELETE_WORD_END:
        case CPTK_EDITOR_ACTION_CHANGE_WORD_END:
        case CPTK_EDITOR_ACTION_YANK_WORD_END:
            end = replay_word_end_exclusive(&st->buffer);
            break;
        case CPTK_EDITOR_ACTION_DELETE_LINE:
        case CPTK_EDITOR_ACTION_CHANGE_LINE:
        case CPTK_EDITOR_ACTION_YANK_LINE:
            start = 0;
            end = cptk_buffer_len(&st->buffer);
            break;
        default:
            return 0;
    }

    replay_capture_range(&st->buffer, start, end, st->vi_register, sizeof(st->vi_register));

    if (action == CPTK_EDITOR_ACTION_YANK_TO_END ||
        action == CPTK_EDITOR_ACTION_YANK_WORD_FORWARD ||
        action == CPTK_EDITOR_ACTION_YANK_WORD_BACKWARD ||
        action == CPTK_EDITOR_ACTION_YANK_WORD_END ||
        action == CPTK_EDITOR_ACTION_YANK_LINE) {
        return 0;
    }

    return cptk_buffer_delete_span(&st->buffer, start, end);
}

static char *replay_strdup(const char *s) {
    size_t len;
    char *out;
    if (!s) {
        return NULL;
    }
    len = strlen(s);
    out = (char *)malloc(len + 1);
    if (!out) {
        return NULL;
    }
    memcpy(out, s, len + 1);
    return out;
}

static void replay_trim(char *s) {
    size_t n;
    size_t i;
    if (!s) {
        return;
    }
    n = strlen(s);
    while (n > 0 && (s[n - 1] == '\r' || s[n - 1] == '\n' || s[n - 1] == ' ' || s[n - 1] == '\t')) {
        s[--n] = '\0';
    }
    i = 0;
    while (s[i] == ' ' || s[i] == '\t') {
        i++;
    }
    if (i > 0) {
        memmove(s, s + i, strlen(s + i) + 1);
    }
}

static int replay_history_add(replay_state *st, const char *line) {
    char *copy;
    if (!st || !line || line[0] == '\0') {
        return 0;
    }
    if (st->history_count >= sizeof(st->history) / sizeof(st->history[0])) {
        return -1;
    }
    copy = replay_strdup(line);
    if (!copy) {
        return -1;
    }
    st->history[st->history_count++] = copy;
    return 0;
}

static void replay_apply_completion(replay_state *st) {
    static const char *k_cmds[] = {
        "onekeyroot",
        "openshell",
        "about",
        "mods",
        "commonly",
        "help-links",
        "man-apps",
        "magisk-mod",
        "exit"
    };
    const char *text;
    size_t len;
    size_t i;

    if (!st) {
        return;
    }
    text = cptk_buffer_text(&st->buffer);
    len = cptk_buffer_len(&st->buffer);
    for (i = 0; i < sizeof(k_cmds) / sizeof(k_cmds[0]); ++i) {
        if (len == 0 || strncmp(k_cmds[i], text, len) == 0) {
            (void)cptk_buffer_set_text(&st->buffer, k_cmds[i]);
            return;
        }
    }
}

static void replay_set_from_history(replay_state *st, const char *text) {
    if (!st) {
        return;
    }
    (void)cptk_buffer_set_text(&st->buffer, text ? text : "");
}

static void replay_apply_action(replay_state *st, cptk_editor_action action, const cptk_key_event *key) {
    if (!st) {
        return;
    }

    if (action == CPTK_EDITOR_ACTION_MOVE_LEFT) {
        cptk_buffer_move_left(&st->buffer);
    } else if (action == CPTK_EDITOR_ACTION_MOVE_RIGHT) {
        cptk_buffer_move_right(&st->buffer);
    } else if (action == CPTK_EDITOR_ACTION_MOVE_WORD_FORWARD) {
        cptk_buffer_move_word_forward(&st->buffer);
    } else if (action == CPTK_EDITOR_ACTION_MOVE_WORD_BACKWARD) {
        cptk_buffer_move_word_backward(&st->buffer);
    } else if (action == CPTK_EDITOR_ACTION_MOVE_WORD_END) {
        cptk_buffer_move_word_end(&st->buffer);
    } else if (action == CPTK_EDITOR_ACTION_MOVE_HOME) {
        cptk_buffer_move_home(&st->buffer);
    } else if (action == CPTK_EDITOR_ACTION_MOVE_END) {
        cptk_buffer_move_end(&st->buffer);
    } else if (action == CPTK_EDITOR_ACTION_BACKSPACE) {
        (void)cptk_buffer_backspace(&st->buffer);
    } else if (action == CPTK_EDITOR_ACTION_DELETE) {
        (void)cptk_buffer_delete(&st->buffer);
    } else if (action == CPTK_EDITOR_ACTION_HISTORY_PREV && st->history_count > 0) {
        if (st->history_nav < 0) {
            size_t snap_len = cptk_buffer_len(&st->buffer);
            if (snap_len >= sizeof(st->history_snapshot)) {
                snap_len = sizeof(st->history_snapshot) - 1;
            }
            memcpy(st->history_snapshot, cptk_buffer_text(&st->buffer), snap_len);
            st->history_snapshot[snap_len] = '\0';
            st->history_nav = (long)st->history_count - 1;
        } else if (st->history_nav > 0) {
            st->history_nav--;
        }
        replay_set_from_history(st, st->history[(size_t)st->history_nav]);
    } else if (action == CPTK_EDITOR_ACTION_HISTORY_NEXT && st->history_count > 0) {
        if (st->history_nav >= 0 && st->history_nav + 1 < (long)st->history_count) {
            st->history_nav++;
            replay_set_from_history(st, st->history[(size_t)st->history_nav]);
        } else if (st->history_nav >= 0) {
            st->history_nav = -1;
            replay_set_from_history(st, st->history_snapshot);
        }
    } else if (action == CPTK_EDITOR_ACTION_COMPLETE) {
        replay_apply_completion(st);
        st->history_nav = -1;
    } else if (
        action == CPTK_EDITOR_ACTION_DELETE_TO_END ||
        action == CPTK_EDITOR_ACTION_DELETE_WORD_FORWARD ||
        action == CPTK_EDITOR_ACTION_DELETE_WORD_BACKWARD ||
        action == CPTK_EDITOR_ACTION_DELETE_WORD_END ||
        action == CPTK_EDITOR_ACTION_DELETE_LINE ||
        action == CPTK_EDITOR_ACTION_CHANGE_TO_END ||
        action == CPTK_EDITOR_ACTION_CHANGE_WORD_FORWARD ||
        action == CPTK_EDITOR_ACTION_CHANGE_WORD_BACKWARD ||
        action == CPTK_EDITOR_ACTION_CHANGE_WORD_END ||
        action == CPTK_EDITOR_ACTION_CHANGE_LINE ||
        action == CPTK_EDITOR_ACTION_YANK_TO_END ||
        action == CPTK_EDITOR_ACTION_YANK_WORD_FORWARD ||
        action == CPTK_EDITOR_ACTION_YANK_WORD_BACKWARD ||
        action == CPTK_EDITOR_ACTION_YANK_WORD_END ||
        action == CPTK_EDITOR_ACTION_YANK_LINE
    ) {
        (void)replay_apply_ranged_action(st, action);
        if (
            action == CPTK_EDITOR_ACTION_CHANGE_TO_END ||
            action == CPTK_EDITOR_ACTION_CHANGE_WORD_FORWARD ||
            action == CPTK_EDITOR_ACTION_CHANGE_WORD_BACKWARD ||
            action == CPTK_EDITOR_ACTION_CHANGE_WORD_END ||
            action == CPTK_EDITOR_ACTION_CHANGE_LINE
        ) {
            st->vi_insert_mode = 1;
        }
        st->history_nav = -1;
    } else if (action == CPTK_EDITOR_ACTION_PASTE_AFTER) {
        if (st->vi_register[0] != '\0') {
            if (cptk_buffer_cursor(&st->buffer) < cptk_buffer_len(&st->buffer)) {
                cptk_buffer_move_right(&st->buffer);
            }
            (void)cptk_buffer_insert_text(&st->buffer, st->vi_register);
            st->history_nav = -1;
        }
    } else if (action == CPTK_EDITOR_ACTION_INSERT_TEXT && key && key->key == CPTK_VT100_KEY_TEXT && key->codepoint >= 32 && key->codepoint < 127) {
        (void)cptk_buffer_insert_ascii(&st->buffer, (char)key->codepoint);
        st->history_nav = -1;
    }
}

static int replay_emit_key(replay_state *st, cptk_vt100_key_code key_code, uint32_t codepoint) {
    cptk_key_event key;
    cptk_editor_action action;

    if (!st) {
        return -1;
    }

    memset(&key, 0, sizeof(key));
    key.key = key_code;
    key.codepoint = codepoint;

    action = cptk_keymap_resolve_ex2(st->mode, &st->vi_insert_mode, &st->vi_pending_op, &key);
    replay_apply_action(st, action, &key);
    return 0;
}

static int replay_handle_line(replay_state *st, const char *line) {
    if (!st || !line) {
        return -1;
    }

    if (strncmp(line, "MODE ", 5) == 0) {
        const char *v = line + 5;
        if (_stricmp(v, "VI") == 0) {
            st->mode = CPTK_EDIT_MODE_VI;
            st->vi_insert_mode = 1;
            st->vi_pending_op = CPTK_VI_OP_NONE;
            return 0;
        }
        if (_stricmp(v, "EMACS") == 0) {
            st->mode = CPTK_EDIT_MODE_EMACS;
            st->vi_insert_mode = 0;
            st->vi_pending_op = CPTK_VI_OP_NONE;
            return 0;
        }
        return -1;
    }

    if (strncmp(line, "HISTORY ", 8) == 0) {
        return replay_history_add(st, line + 8);
    }

    if (strncmp(line, "SET ", 4) == 0) {
        return cptk_buffer_set_text(&st->buffer, line + 4);
    }

    if (strncmp(line, "TEXT ", 5) == 0) {
        const char *p = line + 5;
        while (*p) {
            if (replay_emit_key(st, CPTK_VT100_KEY_TEXT, (uint32_t)(unsigned char)*p) != 0) {
                return -1;
            }
            p++;
        }
        return 0;
    }

    if (strncmp(line, "KEY ", 4) == 0) {
        const char *k = line + 4;
        if (_stricmp(k, "ENTER") == 0) return replay_emit_key(st, CPTK_VT100_KEY_ENTER, 0);
        if (_stricmp(k, "LEFT") == 0) return replay_emit_key(st, CPTK_VT100_KEY_LEFT, 0);
        if (_stricmp(k, "RIGHT") == 0) return replay_emit_key(st, CPTK_VT100_KEY_RIGHT, 0);
        if (_stricmp(k, "HOME") == 0) return replay_emit_key(st, CPTK_VT100_KEY_HOME, 0);
        if (_stricmp(k, "END") == 0) return replay_emit_key(st, CPTK_VT100_KEY_END, 0);
        if (_stricmp(k, "UP") == 0) return replay_emit_key(st, CPTK_VT100_KEY_UP, 0);
        if (_stricmp(k, "DOWN") == 0) return replay_emit_key(st, CPTK_VT100_KEY_DOWN, 0);
        if (_stricmp(k, "BACKSPACE") == 0) return replay_emit_key(st, CPTK_VT100_KEY_BACKSPACE, 0);
        if (_stricmp(k, "DELETE") == 0) return replay_emit_key(st, CPTK_VT100_KEY_DELETE, 0);
        if (_stricmp(k, "TAB") == 0) return replay_emit_key(st, CPTK_VT100_KEY_TAB, 0);
        if (_stricmp(k, "ESC") == 0 || _stricmp(k, "ESCAPE") == 0) return replay_emit_key(st, CPTK_VT100_KEY_ESCAPE, 0);
        if (_stricmp(k, "CTRL_C") == 0) return replay_emit_key(st, CPTK_VT100_KEY_CTRL_C, 0);
        return -1;
    }

    return 0;
}

int main(int argc, char **argv) {
    FILE *fp = NULL;
    replay_state st;
    char line[1024];
    size_t i;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <trace-file>\n", argv[0]);
        return 1;
    }

    memset(&st, 0, sizeof(st));
    st.mode = CPTK_EDIT_MODE_EMACS;
    st.vi_insert_mode = 0;
    st.vi_pending_op = CPTK_VI_OP_NONE;
    st.vi_register[0] = '\0';
    st.history_nav = -1;
    st.history_snapshot[0] = '\0';

    if (cptk_buffer_init(&st.buffer, 128) != 0) {
        fprintf(stderr, "buffer init failed\n");
        return 2;
    }

    #ifdef _WIN32
    if (fopen_s(&fp, argv[1], "rb") != 0) {
        fp = NULL;
    }
    #else
    fp = fopen(argv[1], "rb");
    #endif
    if (!fp) {
        fprintf(stderr, "cannot open trace: %s\n", argv[1]);
        cptk_buffer_free(&st.buffer);
        return 3;
    }

    while (fgets(line, sizeof(line), fp)) {
        replay_trim(line);
        if (line[0] == '\0' || line[0] == '#') {
            continue;
        }
        if (replay_handle_line(&st, line) != 0) {
            fprintf(stderr, "bad trace command: %s\n", line);
            fclose(fp);
            cptk_buffer_free(&st.buffer);
            for (i = 0; i < st.history_count; ++i) {
                free(st.history[i]);
            }
            return 4;
        }
    }

    fclose(fp);

    printf("FINAL text=%s\n", cptk_buffer_text(&st.buffer));
    printf("FINAL cursor=%u\n", (unsigned)cptk_buffer_cursor(&st.buffer));
    if (st.mode == CPTK_EDIT_MODE_VI) {
        printf("FINAL mode=%s\n", st.vi_insert_mode ? "insert" : "normal");
    } else {
        printf("FINAL mode=emacs\n");
    }

    cptk_buffer_free(&st.buffer);
    for (i = 0; i < st.history_count; ++i) {
        free(st.history[i]);
    }
    return 0;
}
