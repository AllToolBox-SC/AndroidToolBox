#include "c_prompt_toolkit.h"
#include "c_prompt_toolkit_buffer.h"
#include "c_prompt_toolkit_keymap.h"
#include "menu.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

#ifdef _WIN32
#include <conio.h>
#include <io.h>
#include <windows.h>
/* Ensure GetAsyncKeyState and related APIs link correctly */
#pragma comment(lib, "user32.lib")

/* Default console attributes used by some rendering helpers when the caller
 * does not provide an explicit attribute. Keep local to this library. */
static WORD g_default_console_attr = (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
#endif

#define ANSI_RESET "\x1b[0m"
#define ANSI_WHITE "\x1b[38;2;230;237;243m"
#define ANSI_BOLD "\x1b[1m"
#define ANSI_UNDERLINE "\x1b[4m"
/* Additional ANSI colors used by the menu renderer */
#define ANSI_BLUE "\x1b[38;2;138;180;248m"
#define ANSI_GREEN "\x1b[38;2;122;209;168m"

/* forward decl for single-choice renderer used below */
static void cptk_menu_render_single_helper(HANDLE out_handle, int start_row, const char *const *items, size_t item_count, size_t focused, const char *digit_buf);
/* forward decls for row-update helpers (defined later) */
static void cptk_menu_update_single_row(HANDLE out_handle, int start_row, const char *const *items, size_t item_count, size_t idx, int is_focused);
static void cptk_menu_update_multi_item(HANDLE out_handle, int start_row, int width, int avail, const char *const *items, size_t item_count, const unsigned char *sel_mask, int idx, int focused, const short *opt_row_start, const short *opt_row_end);

/* Debug logging helper: when ATB_MENU_DEBUG is set (non-zero), write
 * formatted messages to stderr and append to file ATB_MENU_DEBUG_FILE
 * (defaults to atb_menu_debug.log). */
static void cptk_debug_log(const char *fmt, ...) {
    const char *dbg = getenv("ATB_MENU_DEBUG");
    if (!dbg || dbg[0] == '\0' || strcmp(dbg, "0") == 0) {
        return;
    }
    /* Allow suppressing stderr output while still appending to file.
     * Set ATB_MENU_DEBUG_STDERR=0 to disable stderr prints. */
    const char *stderr_env = getenv("ATB_MENU_DEBUG_STDERR");
    int do_stderr = 1;
    if (stderr_env && stderr_env[0] != '\0' && strcmp(stderr_env, "0") == 0) {
        do_stderr = 0;
    }

    va_list ap;
    va_start(ap, fmt);
    if (do_stderr) {
        vfprintf(stderr, fmt, ap);
    }
    va_end(ap);

    const char *file = getenv("ATB_MENU_DEBUG_FILE");
    if (!file || file[0] == '\0') file = "atb_menu_debug.log";
    FILE *f = fopen(file, "a");
    if (f) {
        va_start(ap, fmt);
        vfprintf(f, fmt, ap);
        va_end(ap);
        fclose(f);
    }
}

#define CPTK_VERSION_NUMBER 0x00010000u
#define CPTK_MIN_HISTORY_CAP 16u
#define CPTK_LINE_INIT_CAP 256u

struct cptk_context {
    cptk_callbacks callbacks;
    void *user_data;
    cptk_log_level log_level;
    cptk_edit_mode edit_mode;
    cptk_loop loop;

    char **history;
    size_t history_count;
    size_t history_capacity;
};

static const char *k_status_msg_ok = "ok";
static const char *k_status_msg_einval = "invalid argument";
static const char *k_status_msg_enomem = "out of memory";
static const char *k_status_msg_eio = "io error";
static const char *k_status_msg_enotsup = "not supported";
static const char *k_status_msg_einternal = "internal error";

static void cptk_log(cptk_context *ctx, cptk_log_level level, const char *message) {
    if (!ctx || !message) {
        return;
    }
    if (level > ctx->log_level) {
        return;
    }
    if (ctx->callbacks.on_log) {
        ctx->callbacks.on_log(level, message, ctx->user_data);
    }
}

static char *cptk_strdup(const char *s) {
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

static int cptk_reserve_history(cptk_context *ctx, size_t need) {
    char **next;
    size_t next_cap;
    if (ctx->history_capacity >= need) {
        return 0;
    }
    next_cap = (ctx->history_capacity == 0) ? CPTK_MIN_HISTORY_CAP : ctx->history_capacity;
    while (next_cap < need) {
        next_cap *= 2u;
    }
    next = (char **)realloc(ctx->history, next_cap * sizeof(char *));
    if (!next) {
        return -1;
    }
    ctx->history = next;
    ctx->history_capacity = next_cap;
    return 0;
}

void cptk_default_prompt_options(cptk_prompt_options *options) {
    if (!options) {
        return;
    }
    memset(options, 0, sizeof(*options));
    options->prompt_text = "ATB> ";
    options->multiline = 0;
    options->enable_mouse = 1;
    options->enable_history = 1;
    options->enable_completion = 1;
    options->render_bottom_toolbar = 0;
    options->history_limit = 200;
    options->edit_mode = CPTK_EDIT_MODE_EMACS;
}

const char *cptk_version_string(void) {
    return "c_prompt_toolkit/0.2.0-draft";
}

uint32_t cptk_version_number(void) {
    return CPTK_VERSION_NUMBER;
}

cptk_context *cptk_context_create(void) {
    cptk_context *ctx = (cptk_context *)calloc(1, sizeof(cptk_context));
    int loop_rc;
    if (!ctx) {
        return NULL;
    }
    ctx->log_level = CPTK_LOG_INFO;
    ctx->edit_mode = CPTK_EDIT_MODE_EMACS;
    loop_rc = cptk_loop_init(&ctx->loop);
    if (loop_rc != 0) {
        free(ctx);
        return NULL;
    }
    return ctx;
}

void cptk_context_destroy(cptk_context *ctx) {
    size_t i;
    if (!ctx) {
        return;
    }
    cptk_loop_close(&ctx->loop);
    for (i = 0; i < ctx->history_count; ++i) {
        free(ctx->history[i]);
    }
    free(ctx->history);
    free(ctx);
}

cptk_status cptk_context_set_user_data(cptk_context *ctx, void *user_data) {
    if (!ctx) {
        return CPTK_STATUS_EINVAL;
    }
    ctx->user_data = user_data;
    return CPTK_STATUS_OK;
}

void *cptk_context_get_user_data(cptk_context *ctx) {
    if (!ctx) {
        return NULL;
    }
    return ctx->user_data;
}

cptk_status cptk_context_set_callbacks(cptk_context *ctx, const cptk_callbacks *callbacks) {
    if (!ctx || !callbacks) {
        return CPTK_STATUS_EINVAL;
    }
    ctx->callbacks = *callbacks;
    return CPTK_STATUS_OK;
}

cptk_status cptk_context_set_log_level(cptk_context *ctx, cptk_log_level level) {
    if (!ctx) {
        return CPTK_STATUS_EINVAL;
    }
    ctx->log_level = level;
    return CPTK_STATUS_OK;
}

cptk_status cptk_context_set_edit_mode(cptk_context *ctx, cptk_edit_mode mode) {
    if (!ctx) {
        return CPTK_STATUS_EINVAL;
    }
    ctx->edit_mode = mode;
    return CPTK_STATUS_OK;
}

cptk_status cptk_history_add(cptk_context *ctx, const char *line_utf8) {
    char *copy;
    if (!ctx || !line_utf8) {
        return CPTK_STATUS_EINVAL;
    }
    if (line_utf8[0] == '\0') {
        return CPTK_STATUS_OK;
    }

    if (cptk_reserve_history(ctx, ctx->history_count + 1u) != 0) {
        return CPTK_STATUS_ENOMEM;
    }

    copy = cptk_strdup(line_utf8);
    if (!copy) {
        return CPTK_STATUS_ENOMEM;
    }
    ctx->history[ctx->history_count++] = copy;
    return CPTK_STATUS_OK;
}

size_t cptk_history_count(const cptk_context *ctx) {
    if (!ctx) {
        return 0;
    }
    return ctx->history_count;
}

const char *cptk_history_get(const cptk_context *ctx, size_t index) {
    if (!ctx || index >= ctx->history_count) {
        return NULL;
    }
    return ctx->history[index];
}

#ifdef _WIN32
static int cptk_is_interactive(void) {
    return _isatty(_fileno(stdin)) && _isatty(_fileno(stdout));
}

static int cptk_enable_vt_mode(HANDLE in_handle, HANDLE out_handle, DWORD *old_in, DWORD *old_out, int *vt_input_enabled) {
    DWORD in_mode = 0;
    DWORD out_mode = 0;

    if (!GetConsoleMode(in_handle, &in_mode)) {
        return 0;
    }
    if (!GetConsoleMode(out_handle, &out_mode)) {
        return 0;
    }

    *old_in = in_mode;
    *old_out = out_mode;

    in_mode |= ENABLE_EXTENDED_FLAGS;
    in_mode |= ENABLE_WINDOW_INPUT;
    /* When trying to enable VT input, avoid also setting ENABLE_MOUSE_INPUT
     * on the console mode, because terminals that emit VT mouse bytes do not
     * require Windows to deliver MOUSE_EVENT records and mixing both modes
     * can cause delivery issues. Try VT input first without ENABLE_MOUSE_INPUT.
     */
    DWORD vt_attempt = in_mode & ~((DWORD)ENABLE_MOUSE_INPUT);
    vt_attempt &= ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT);

    *vt_input_enabled = 0;
    if (SetConsoleMode(in_handle, vt_attempt | ENABLE_VIRTUAL_TERMINAL_INPUT)) {
        *vt_input_enabled = 1;
    } else {
        /* Fallback: enable mouse input path for Console INPUT_RECORDs */
        in_mode |= ENABLE_MOUSE_INPUT;
        in_mode &= ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT);
        (void)SetConsoleMode(in_handle, in_mode);
    }

    out_mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    (void)SetConsoleMode(out_handle, out_mode);
    /* Ask the terminal to enable mouse reporting (Xterm/SGR/urxvt variants).
     * This causes mouse/touch events to be delivered as VT sequences which
     * cptk_read_vt_key will parse into mouse events. */
    printf("\x1b[?1000h");   /* X10 mouse */
    printf("\x1b[?1003h");   /* All motion */
    printf("\x1b[?1015h");   /* urxvt */
    printf("\x1b[?1006h");   /* SGR */
    fflush(stdout);
    return 1;
}

static void cptk_restore_vt_mode(HANDLE in_handle, HANDLE out_handle, DWORD old_in, DWORD old_out) {
    /* Disable terminal mouse reporting before restoring console modes. */
    printf("\x1b[?1000l");
    printf("\x1b[?1015l");
    printf("\x1b[?1006l");
    printf("\x1b[?1003l");
    fflush(stdout);
    (void)SetConsoleMode(in_handle, old_in);
    (void)SetConsoleMode(out_handle, old_out);
}

static void cptk_emit_callback_event(cptk_context *ctx, const cptk_key_event *key_event, const cptk_mouse_event *mouse_event) {
    if (!ctx || !ctx->callbacks.on_event) {
        return;
    }
    /* Pass through provided pointers (may be NULL) to the registered callback. */
    ctx->callbacks.on_event(key_event, mouse_event, ctx->user_data);
}

static int cptk_read_vt_key(HANDLE in_handle, cptk_vt100_parser *parser, cptk_key_event *out_key, cptk_mouse_event *out_mouse) {
    DWORD read = 0;
    unsigned char b = 0;
    cptk_vt100_event ev;

    if (out_mouse) {
        memset(out_mouse, 0, sizeof(*out_mouse));
    }

    /* Decide which input path to use: if the console input mode has
     * ENABLE_VIRTUAL_TERMINAL_INPUT set, prefer the VT byte stream (ReadFile)
     * and do NOT consume Console INPUT_RECORDs. Otherwise, use the Console
     * INPUT_RECORD APIs to detect mouse movement/clicks (legacy conhost).
     */
    {
        DWORD console_mode = 0;
        int use_vt_bytes = 0;
        if (in_handle && GetConsoleMode(in_handle, &console_mode)) {
            if (console_mode & ENABLE_VIRTUAL_TERMINAL_INPUT) use_vt_bytes = 1;
        }

        /* env override for testing */
        const char *force_console = getenv("ATB_FORCE_CONSOLE_INPUT");
        const char *force_vt = getenv("ATB_FORCE_VT_BYTES");
        if (force_console && force_console[0] && strcmp(force_console, "0") != 0) {
            use_vt_bytes = 0;
        }
        if (force_vt && force_vt[0] && strcmp(force_vt, "0") != 0) {
            use_vt_bytes = 1;
        }

        if (!use_vt_bytes && in_handle && out_mouse) {
            INPUT_RECORD rec;
            DWORD num = 0;
            if (PeekConsoleInputA(in_handle, &rec, 1, &num) && num > 0) {
                if (rec.EventType == MOUSE_EVENT) {
                        if (ReadConsoleInputA(in_handle, &rec, 1, &num)) {
                    MOUSE_EVENT_RECORD *me = &rec.Event.MouseEvent;
                    out_mouse->x = me->dwMousePosition.X;
                    out_mouse->y = me->dwMousePosition.Y;
                    if (me->dwEventFlags & MOUSE_MOVED) {
                        out_mouse->button = 0x20; /* motion bit used by VT paths */
                        out_mouse->is_release = 0;
                        out_mouse->present = 1;
                        cptk_debug_log("[CPTK_DBG] console_mouse MOVE x=%d y=%d\n", (int)out_mouse->x, (int)out_mouse->y);
                        /* Clear any leftover key info when reporting a mouse event */
                        if (out_key) memset(out_key, 0, sizeof(*out_key));
                        return 1;
                    }
                    /* Button press/release mapping */
                    if (me->dwEventFlags == 0) {
                        if (me->dwButtonState == 0) {
                            out_mouse->button = 3; /* release */
                            out_mouse->is_release = 1;
                        } else {
                            if (me->dwButtonState & FROM_LEFT_1ST_BUTTON_PRESSED) out_mouse->button = 0;
                            else if (me->dwButtonState & FROM_LEFT_2ND_BUTTON_PRESSED) out_mouse->button = 1;
                            else if (me->dwButtonState & RIGHTMOST_BUTTON_PRESSED) out_mouse->button = 2;
                            else out_mouse->button = 0;
                            out_mouse->is_release = 0;
                        }
                        out_mouse->present = 1;
                        cptk_debug_log("[CPTK_DBG] console_mouse BTN x=%d y=%d btn=%d rel=%d\n", (int)out_mouse->x, (int)out_mouse->y, (int)out_mouse->button, (int)out_mouse->is_release);
                        /* Ensure no stale key event is passed alongside this mouse event. */
                        if (out_key) memset(out_key, 0, sizeof(*out_key));
                        return 1;
                    }
                }
                }
            }
        }
        /* else: fall through to VT byte parsing below */
    }

    while (ReadFile(in_handle, &b, 1, &read, NULL) && read == 1) {
        if (cptk_vt100_feed_byte(parser, b, &ev)) {
            if (ev.is_mouse) {
                if (out_mouse) {
                    out_mouse->x = ev.mouse_x > 0 ? (ev.mouse_x - 1) : 0;
                    out_mouse->y = ev.mouse_y > 0 ? (ev.mouse_y - 1) : 0;
                    out_mouse->button = ev.mouse_button;
                    out_mouse->is_release = ev.mouse_is_release;
                    out_mouse->present = 1;
                    cptk_debug_log("[CPTK_DBG] vt_mouse x=%d y=%d btn=%d rel=%d\n", (int)out_mouse->x, (int)out_mouse->y, (int)out_mouse->button, (int)out_mouse->is_release);
                }
                /* Ensure key output is zeroed for mouse-only events so callers
                 * do not act on stale key data. */
                if (out_key) memset(out_key, 0, sizeof(*out_key));
                return 1;
            }
            if (out_key) {
                memset(out_key, 0, sizeof(*out_key));
                out_key->key = ev.key;
                out_key->codepoint = ev.codepoint;
            }
            return 1;
        }
    }
    return 0;
}

static int cptk_read_legacy_key(cptk_key_event *out_key) {
    wint_t wc = _getwch();
    memset(out_key, 0, sizeof(*out_key));

    if (wc == 27) {
        out_key->key = CPTK_VT100_KEY_ESCAPE;
        return 1;
    }
    if (wc == 3) {
        out_key->key = CPTK_VT100_KEY_CTRL_C;
        return 1;
    }
    if (wc == 13) {
        out_key->key = CPTK_VT100_KEY_ENTER;
        return 1;
    }
    if (wc == 8) {
        out_key->key = CPTK_VT100_KEY_BACKSPACE;
        return 1;
    }
    if (wc == 9) {
        out_key->key = CPTK_VT100_KEY_TAB;
        return 1;
    }

    if (wc == 0 || wc == 224) {
        wint_t ext = _getwch();
        switch (ext) {
            case 72: out_key->key = CPTK_VT100_KEY_UP; return 1;
            case 80: out_key->key = CPTK_VT100_KEY_DOWN; return 1;
            case 75: out_key->key = CPTK_VT100_KEY_LEFT; return 1;
            case 77: out_key->key = CPTK_VT100_KEY_RIGHT; return 1;
            case 71: out_key->key = CPTK_VT100_KEY_HOME; return 1;
            case 79: out_key->key = CPTK_VT100_KEY_END; return 1;
            case 83: out_key->key = CPTK_VT100_KEY_DELETE; return 1;
            default: return 0;
        }
    }

    if (wc >= 32 && wc <= 126) {
        out_key->key = CPTK_VT100_KEY_TEXT;
        out_key->codepoint = (uint32_t)wc;
        return 1;
    }

    return 0;
}

static void cptk_render_prompt_line(const char *prompt, const char *line, size_t cursor) {
    size_t prompt_len = strlen(prompt);
    size_t line_len = strlen(line);
    size_t right_count = line_len - cursor;

    fputs("\r\x1b[2K", stdout);
    fputs(prompt, stdout);
    fputs(line, stdout);
    if (right_count > 0) {
        fprintf(stdout, "\x1b[%uD", (unsigned)right_count);
    }
    fflush(stdout);

    (void)prompt_len;
}

static void cptk_apply_completion(cptk_context *ctx, cptk_buffer *buffer) {
    cptk_buffer_state state;
    cptk_completion_item local_items[16];
    cptk_completion_list list;
    const char *insert_text;

    if (!ctx || !buffer || !ctx->callbacks.on_completion) {
        return;
    }

    memset(&state, 0, sizeof(state));
    state.text_utf8 = cptk_buffer_text(buffer);
    state.text_len = cptk_buffer_len(buffer);
    state.cursor = cptk_buffer_cursor(buffer);

    memset(&list, 0, sizeof(list));
    list.items = local_items;
    list.capacity = sizeof(local_items) / sizeof(local_items[0]);

    if (ctx->callbacks.on_completion(&state, &list, ctx->user_data) != 0 || list.count == 0) {
        return;
    }

    insert_text = list.items[0].text;
    if (!insert_text) {
        return;
    }

    (void)cptk_buffer_set_text(buffer, insert_text);
}

static void cptk_set_line_from_history(const char *src, cptk_buffer *buffer) {
    if (!buffer) {
        return;
    }
    (void)cptk_buffer_set_text(buffer, src ? src : "");
}

static void cptk_vi_capture_range(const cptk_buffer *buffer, size_t start, size_t end, char *out, size_t out_cap) {
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

static size_t cptk_vi_word_end_exclusive(const cptk_buffer *buffer) {
    size_t end = cptk_buffer_word_end_target(buffer);
    size_t len = cptk_buffer_len(buffer);
    if (end < len) {
        return end + 1;
    }
    return len;
}
static int cptk_vi_apply_ranged_action(cptk_buffer *line, cptk_editor_action action, char *vi_register, size_t vi_register_cap) {
    size_t start;
    size_t end;

    if (!line) {
        return 0;
    }
    start = cptk_buffer_cursor(line);
    end = start;

    switch (action) {
        case CPTK_EDITOR_ACTION_DELETE_TO_END:
        case CPTK_EDITOR_ACTION_CHANGE_TO_END:
        case CPTK_EDITOR_ACTION_YANK_TO_END:
            end = cptk_buffer_len(line);
            break;
        case CPTK_EDITOR_ACTION_DELETE_WORD_FORWARD:
        case CPTK_EDITOR_ACTION_CHANGE_WORD_FORWARD:
        case CPTK_EDITOR_ACTION_YANK_WORD_FORWARD:
            end = cptk_buffer_word_forward_target(line);
            break;
        case CPTK_EDITOR_ACTION_DELETE_WORD_BACKWARD:
        case CPTK_EDITOR_ACTION_CHANGE_WORD_BACKWARD:
        case CPTK_EDITOR_ACTION_YANK_WORD_BACKWARD:
            start = cptk_buffer_word_backward_target(line);
            end = cptk_buffer_cursor(line);
            break;
        case CPTK_EDITOR_ACTION_DELETE_WORD_END:
        case CPTK_EDITOR_ACTION_CHANGE_WORD_END:
        case CPTK_EDITOR_ACTION_YANK_WORD_END:
            end = cptk_vi_word_end_exclusive(line);
            break;
        case CPTK_EDITOR_ACTION_DELETE_LINE:
        case CPTK_EDITOR_ACTION_CHANGE_LINE:
        case CPTK_EDITOR_ACTION_YANK_LINE:
            start = 0;
            end = cptk_buffer_len(line);
            break;
        default:
            return 0;
    }

    if (vi_register && vi_register_cap > 0) {
        cptk_vi_capture_range(line, start, end, vi_register, vi_register_cap);
    }

    if (action == CPTK_EDITOR_ACTION_YANK_TO_END ||
        action == CPTK_EDITOR_ACTION_YANK_WORD_FORWARD ||
        action == CPTK_EDITOR_ACTION_YANK_WORD_BACKWARD ||
        action == CPTK_EDITOR_ACTION_YANK_WORD_END ||
        action == CPTK_EDITOR_ACTION_YANK_LINE) {
        return 0;
    }

    return cptk_buffer_delete_span(line, start, end);
}
#endif

static cptk_status cptk_prompt_pipe_read(
    cptk_context *ctx,
    const cptk_prompt_options *options,
    char *out_line_utf8,
    size_t out_capacity
) {
    char tmp[4096];
    size_t n;
    (void)ctx;
    if (options && options->prompt_text) {
        fputs(options->prompt_text, stdout);
        fflush(stdout);
    }
    if (!fgets(tmp, sizeof(tmp), stdin)) {
        return CPTK_STATUS_EIO;
    }
    n = strlen(tmp);
    while (n > 0 && (tmp[n - 1] == '\n' || tmp[n - 1] == '\r')) {
        tmp[--n] = '\0';
    }
    if (n + 1 > out_capacity) {
        return CPTK_STATUS_EINVAL;
    }
    memcpy(out_line_utf8, tmp, n + 1);
    return CPTK_STATUS_OK;
}

cptk_status cptk_prompt_run(
    cptk_context *ctx,
    const cptk_prompt_options *options,
    char *out_line_utf8,
    size_t out_capacity
) {
    cptk_prompt_options local_options;
    cptk_edit_mode active_mode;

    if (!ctx || !out_line_utf8 || out_capacity == 0) {
        return CPTK_STATUS_EINVAL;
    }

    cptk_default_prompt_options(&local_options);
    if (options) {
        local_options = *options;
    }
    if (!local_options.prompt_text) {
        local_options.prompt_text = "ATB> ";
    }
    active_mode = local_options.edit_mode;

#ifdef _WIN32
    if (cptk_is_interactive()) {
        HANDLE in_handle = GetStdHandle(STD_INPUT_HANDLE);
        HANDLE out_handle = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD old_in = 0;
        DWORD old_out = 0;
        int vt_enabled = 0;
        cptk_vt100_parser parser;
        cptk_key_event key;

        cptk_buffer line;
        long history_nav = -1;
        char history_snapshot[CPTK_LINE_INIT_CAP];
        char vi_register[CPTK_LINE_INIT_CAP];
        int vi_insert_mode = (active_mode == CPTK_EDIT_MODE_VI) ? 1 : 0;
        int vi_pending_op = CPTK_VI_OP_NONE;
        int loop_started = 0;

        if (cptk_buffer_init(&line, CPTK_LINE_INIT_CAP) != 0) {
            return CPTK_STATUS_ENOMEM;
        }
        history_snapshot[0] = '\0';
        vi_register[0] = '\0';

        if (!cptk_enable_vt_mode(in_handle, out_handle, &old_in, &old_out, &vt_enabled)) {
            cptk_buffer_free(&line);
            return CPTK_STATUS_EIO;
        }

        cptk_vt100_parser_init(&parser);
        cptk_loop_start(&ctx->loop);
        loop_started = 1;
        cptk_render_prompt_line(local_options.prompt_text, cptk_buffer_text(&line), cptk_buffer_cursor(&line));

        while (1) {
            int got_key = 0;
            cptk_editor_action action;
            cptk_mouse_event mouse_ev;

            cptk_loop_poll(&ctx->loop);

            memset(&mouse_ev, 0, sizeof(mouse_ev));
            if (vt_enabled) {
                got_key = cptk_read_vt_key(in_handle, &parser, &key, &mouse_ev);
            }
            if (!got_key) {
                got_key = cptk_read_legacy_key(&key);
            }
            if (!got_key) {
                continue;
            }

            /* Emit both key and mouse events (mouse_ev may be all-zero).
             * The callback can inspect the pointers and react accordingly.
             */
            cptk_emit_callback_event(ctx, &key, mouse_ev.present ? &mouse_ev : NULL);

            action = cptk_keymap_resolve_ex2(active_mode, &vi_insert_mode, &vi_pending_op, &key);

            if (action == CPTK_EDITOR_ACTION_CANCEL) {
                if (loop_started) {
                    cptk_loop_stop(&ctx->loop);
                }
                cptk_restore_vt_mode(in_handle, out_handle, old_in, old_out);
                cptk_buffer_free(&line);
                return CPTK_STATUS_EIO;
            }

            if (action == CPTK_EDITOR_ACTION_ACCEPT) {
                break;
            }

            if (action == CPTK_EDITOR_ACTION_MOVE_LEFT) {
                cptk_buffer_move_left(&line);
            } else if (action == CPTK_EDITOR_ACTION_MOVE_RIGHT) {
                cptk_buffer_move_right(&line);
            } else if (action == CPTK_EDITOR_ACTION_MOVE_WORD_FORWARD) {
                cptk_buffer_move_word_forward(&line);
            } else if (action == CPTK_EDITOR_ACTION_MOVE_WORD_BACKWARD) {
                cptk_buffer_move_word_backward(&line);
            } else if (action == CPTK_EDITOR_ACTION_MOVE_WORD_END) {
                cptk_buffer_move_word_end(&line);
            } else if (action == CPTK_EDITOR_ACTION_MOVE_HOME) {
                cptk_buffer_move_home(&line);
            } else if (action == CPTK_EDITOR_ACTION_MOVE_END) {
                cptk_buffer_move_end(&line);
            } else if (action == CPTK_EDITOR_ACTION_BACKSPACE) {
                (void)cptk_buffer_backspace(&line);
            } else if (action == CPTK_EDITOR_ACTION_DELETE) {
                (void)cptk_buffer_delete(&line);
            } else if (action == CPTK_EDITOR_ACTION_HISTORY_PREV && local_options.enable_history && ctx->history_count > 0) {
                if (history_nav < 0) {
                    size_t snap_len = cptk_buffer_len(&line);
                    if (snap_len >= sizeof(history_snapshot)) {
                        snap_len = sizeof(history_snapshot) - 1;
                    }
                    memcpy(history_snapshot, cptk_buffer_text(&line), snap_len);
                    history_snapshot[snap_len] = '\0';
                    history_nav = (long)ctx->history_count - 1;
                } else if (history_nav > 0) {
                    history_nav--;
                }
                cptk_set_line_from_history(ctx->history[(size_t)history_nav], &line);
            } else if (action == CPTK_EDITOR_ACTION_HISTORY_NEXT && local_options.enable_history && ctx->history_count > 0) {
                if (history_nav >= 0 && history_nav + 1 < (long)ctx->history_count) {
                    history_nav++;
                    cptk_set_line_from_history(ctx->history[(size_t)history_nav], &line);
                } else if (history_nav >= 0) {
                    history_nav = -1;
                    cptk_set_line_from_history(history_snapshot, &line);
                }
            } else if (action == CPTK_EDITOR_ACTION_COMPLETE && local_options.enable_completion) {
                cptk_apply_completion(ctx, &line);
                history_nav = -1;
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
                if (cptk_vi_apply_ranged_action(&line, action, vi_register, sizeof(vi_register)) != 0) {
                    if (loop_started) {
                        cptk_loop_stop(&ctx->loop);
                    }
                    cptk_restore_vt_mode(in_handle, out_handle, old_in, old_out);
                    cptk_buffer_free(&line);
                    return CPTK_STATUS_ENOMEM;
                }
                if (
                    action == CPTK_EDITOR_ACTION_CHANGE_TO_END ||
                    action == CPTK_EDITOR_ACTION_CHANGE_WORD_FORWARD ||
                    action == CPTK_EDITOR_ACTION_CHANGE_WORD_BACKWARD ||
                    action == CPTK_EDITOR_ACTION_CHANGE_WORD_END ||
                    action == CPTK_EDITOR_ACTION_CHANGE_LINE
                ) {
                    vi_insert_mode = 1;
                }
                history_nav = -1;
            } else if (action == CPTK_EDITOR_ACTION_PASTE_AFTER) {
                if (vi_register[0] != '\0') {
                    if (cptk_buffer_cursor(&line) < cptk_buffer_len(&line)) {
                        cptk_buffer_move_right(&line);
                    }
                    if (cptk_buffer_insert_text(&line, vi_register) != 0) {
                        if (loop_started) {
                            cptk_loop_stop(&ctx->loop);
                        }
                        cptk_restore_vt_mode(in_handle, out_handle, old_in, old_out);
                        cptk_buffer_free(&line);
                        return CPTK_STATUS_ENOMEM;
                    }
                    history_nav = -1;
                }
            } else if (action == CPTK_EDITOR_ACTION_INSERT_TEXT && key.codepoint >= 32 && key.codepoint < 127) {
                if (cptk_buffer_insert_ascii(&line, (char)key.codepoint) != 0) {
                    if (loop_started) {
                        cptk_loop_stop(&ctx->loop);
                    }
                    cptk_restore_vt_mode(in_handle, out_handle, old_in, old_out);
                    cptk_buffer_free(&line);
                    return CPTK_STATUS_ENOMEM;
                }
                history_nav = -1;
            }

            cptk_render_prompt_line(local_options.prompt_text, cptk_buffer_text(&line), cptk_buffer_cursor(&line));
        }

        fputs("\n", stdout);
        fflush(stdout);

        if (cptk_buffer_len(&line) + 1 > out_capacity) {
            if (loop_started) {
                cptk_loop_stop(&ctx->loop);
            }
            cptk_restore_vt_mode(in_handle, out_handle, old_in, old_out);
            cptk_buffer_free(&line);
            return CPTK_STATUS_EINVAL;
        }

        if (cptk_buffer_copy_to(&line, out_line_utf8, out_capacity) != 0) {
            if (loop_started) {
                cptk_loop_stop(&ctx->loop);
            }
            cptk_restore_vt_mode(in_handle, out_handle, old_in, old_out);
            cptk_buffer_free(&line);
            return CPTK_STATUS_EINVAL;
        }
        if (local_options.enable_history) {
            (void)cptk_history_add(ctx, cptk_buffer_text(&line));
        }

        if (loop_started) {
            cptk_loop_stop(&ctx->loop);
        }
        cptk_restore_vt_mode(in_handle, out_handle, old_in, old_out);
        cptk_buffer_free(&line);
        return CPTK_STATUS_OK;
    }
#endif

    return cptk_prompt_pipe_read(ctx, &local_options, out_line_utf8, out_capacity);
}

cptk_status cptk_menu_choice(
    cptk_context *ctx,
    const char *title,
    const char *const *items,
    size_t item_count,
    size_t default_index,
    size_t *out_selected_index
) {
    size_t i;
    char line[64];
    cptk_prompt_options opt;
    cptk_status st;
    unsigned long idx;

    if (!ctx || !items || item_count == 0 || !out_selected_index) {
        return CPTK_STATUS_EINVAL;
    }

    if (default_index >= item_count) {
        default_index = 0;
    }

    if (title && title[0]) {
        printf("%s\n", title);
    }

    for (i = 0; i < item_count; ++i) {
        printf("  %u. %s\n", (unsigned)(i + 1), items[i]);
    }

    cptk_default_prompt_options(&opt);
    opt.prompt_text = "";
    opt.enable_completion = 0;
    opt.enable_history = 0;

    st = cptk_prompt_run(ctx, &opt, line, sizeof(line));
    if (st != CPTK_STATUS_OK) {
        return st;
    }

    idx = strtoul(line, NULL, 10);
    if (idx == 0 || idx > item_count) {
        *out_selected_index = default_index;
    } else {
        *out_selected_index = (size_t)(idx - 1);
    }
    return CPTK_STATUS_OK;
}

cptk_status cptk_menu_choice_interactive(
    cptk_context *ctx,
    const char *title,
    const char *const *items,
    size_t item_count,
    size_t default_index,
    size_t *out_selected_index
) {
#ifdef _WIN32
    if (!ctx || !items || item_count == 0 || !out_selected_index) {
        return CPTK_STATUS_EINVAL;
    }

    if (!cptk_is_interactive()) {
        return cptk_menu_choice(ctx, title, items, item_count, default_index, out_selected_index);
    }

    HANDLE in_handle = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE out_handle = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD old_in = 0, old_out = 0;
    int vt_enabled = 0;
    cptk_vt100_parser parser;
    size_t i;
    CONSOLE_SCREEN_BUFFER_INFO csbi_before;
    int start_row = 0;
    short option_rows[256];

    if (!cptk_enable_vt_mode(in_handle, out_handle, &old_in, &old_out, &vt_enabled)) {
        return cptk_menu_choice(ctx, title, items, item_count, default_index, out_selected_index);
    }

    cptk_vt100_parser_init(&parser);

    if (title && title[0]) {
        printf("%s\n", title);
    }

    if (GetConsoleScreenBufferInfo(out_handle, &csbi_before)) {
        start_row = (int)csbi_before.dwCursorPosition.Y;
    } else {
        start_row = 0;
    }

    /* prepare option rows mapping */
    for (i = 0; i < item_count; ++i) {
        option_rows[i] = (short)(start_row + (int)i);
    }

    /* interactive focus index and typed digit buffer */
    size_t focused = default_index < item_count ? default_index : 0;
    char digit_buf[32];
    double last_digit_t = 0.0;
    digit_buf[0] = '\0';
    unsigned long last_motion_tick = 0;
    menu_mouse_click_state *click_state = menu_get_global_mouse_click_state();
    unsigned int double_click_ms = menu_get_double_click_threshold_ms();

    if (click_state) {
        menu_mouse_click_state_reset(click_state);
    }

    /* initial render using ANSI highlighting */
    cptk_menu_render_single_helper(out_handle, start_row, items, item_count, focused, digit_buf);

    while (1) {
        cptk_key_event key;
        cptk_mouse_event mouse_ev;
        int got = 0;

        memset(&mouse_ev, 0, sizeof(mouse_ev));
        got = cptk_read_vt_key(in_handle, &parser, &key, &mouse_ev);
        if (!got) {
            /* fallback to legacy read so keyboard still works */
            got = cptk_read_legacy_key(&key);
        }
        if (!got) {
            continue;
        }

        /* Mouse events: handle motion, press and release. Many terminals
         * encode motion with the 0x20 bit set in the button value; ignore
         * those for selection but use them to update hover/focus. Accept
         * selection on left-button press or on release events for robustness. */
        if (mouse_ev.present) {
            int b = (int)mouse_ev.button;
            int is_motion = (b & 0x20) != 0;
            int btn = b & 0x03; /* 0=left,1=middle,2=right,3=release (historical) */
            int idx = (int)mouse_ev.y - start_row;

            if (is_motion) {
                /* Throttle motion updates to reduce flicker on rapid mouse moves */
                unsigned long now = GetTickCount();
                if (now - last_motion_tick < 30) {
                    continue;
                }
                last_motion_tick = now;
                if (idx >= 0 && (size_t)idx < item_count) {
                    if ((size_t)idx != focused) {
                        size_t prev = focused;
                        focused = (size_t)idx;
                        /* Update only the two affected rows to reduce flicker */
                        cptk_menu_update_single_row(out_handle, start_row, items, item_count, prev, 0);
                        cptk_menu_update_single_row(out_handle, start_row, items, item_count, focused, 1);
                        /* Re-print prompt line */
                        CONSOLE_SCREEN_BUFFER_INFO csbi2;
                        COORD ppos;
                        if (GetConsoleScreenBufferInfo(out_handle, &csbi2)) {
                            DWORD written2 = 0;
                            int width2 = (int)csbi2.dwSize.X;
                            ppos.X = 0;
                            ppos.Y = (SHORT)(start_row + (int)item_count);
                            if (vt_enabled) {
                                /* Use ANSI to clear/print prompt when VT is active */
                                printf("\x1b[%d;1H\x1b[2K", ppos.Y + 1);
                                printf("%s%s%s%s", ANSI_GREEN, ANSI_BOLD, digit_buf ? digit_buf : "", ANSI_RESET);
                            } else {
                                FillConsoleOutputCharacterA(out_handle, ' ', (DWORD)width2, ppos, &written2);
                                FillConsoleOutputAttribute(out_handle, (WORD)g_default_console_attr, (DWORD)width2, ppos, &written2);
                                SetConsoleCursorPosition(out_handle, ppos);
                                printf("%s", digit_buf ? digit_buf : "");
                            }
                        }
                        fflush(stdout);
                    }
                }
                continue;
            }

            /* Non-motion mouse:
             * - left press: focus only
             * - left release: only confirm on double-click of same item
             */
            if (idx >= 0 && (size_t)idx < item_count) {
                if (!mouse_ev.is_release && btn == 0) {
                    /* Left-button press: update focus visually but do not accept. */
                    if ((size_t)idx != focused) {
                        size_t prev = focused;
                        focused = (size_t)idx;
                        cptk_menu_update_single_row(out_handle, start_row, items, item_count, prev, 0);
                        cptk_menu_update_single_row(out_handle, start_row, items, item_count, focused, 1);
                        /* Re-print prompt row */
                        CONSOLE_SCREEN_BUFFER_INFO csbi2;
                        COORD ppos;
                        if (GetConsoleScreenBufferInfo(out_handle, &csbi2)) {
                            DWORD written2 = 0;
                            int width2 = (int)csbi2.dwSize.X;
                            ppos.X = 0;
                            ppos.Y = (SHORT)(start_row + (int)item_count);
                            if (vt_enabled) {
                                printf("\x1b[%d;1H\x1b[2K", ppos.Y + 1);
                                printf("%s%s%s%s", ANSI_GREEN, ANSI_BOLD, digit_buf ? digit_buf : "", ANSI_RESET);
                            } else {
                                FillConsoleOutputCharacterA(out_handle, ' ', (DWORD)width2, ppos, &written2);
                                FillConsoleOutputAttribute(out_handle, (WORD)g_default_console_attr, (DWORD)width2, ppos, &written2);
                                SetConsoleCursorPosition(out_handle, ppos);
                                printf("%s", digit_buf ? digit_buf : "");
                            }
                        }
                        fflush(stdout);
                    }
                } else if (mouse_ev.is_release) {
                    unsigned long long now_ms = menu_now_ms();

                    if ((size_t)idx != focused) {
                        size_t prev = focused;
                        focused = (size_t)idx;
                        cptk_menu_update_single_row(out_handle, start_row, items, item_count, prev, 0);
                        cptk_menu_update_single_row(out_handle, start_row, items, item_count, focused, 1);
                        /* Re-print prompt row */
                        CONSOLE_SCREEN_BUFFER_INFO csbi2;
                        COORD ppos;
                        if (GetConsoleScreenBufferInfo(out_handle, &csbi2)) {
                            DWORD written2 = 0;
                            int width2 = (int)csbi2.dwSize.X;
                            ppos.X = 0;
                            ppos.Y = (SHORT)(start_row + (int)item_count);
                            if (vt_enabled) {
                                printf("\x1b[%d;1H\x1b[2K", ppos.Y + 1);
                                printf("%s%s%s%s", ANSI_GREEN, ANSI_BOLD, digit_buf ? digit_buf : "", ANSI_RESET);
                            } else {
                                FillConsoleOutputCharacterA(out_handle, ' ', (DWORD)width2, ppos, &written2);
                                FillConsoleOutputAttribute(out_handle, (WORD)g_default_console_attr, (DWORD)width2, ppos, &written2);
                                SetConsoleCursorPosition(out_handle, ppos);
                                printf("%s", digit_buf ? digit_buf : "");
                            }
                        }
                        fflush(stdout);
                    }

                    if (click_state && menu_mouse_double_click_hit(click_state, idx, now_ms, double_click_ms)) {
                        *out_selected_index = (size_t)idx;
                        cptk_restore_vt_mode(in_handle, out_handle, old_in, old_out);
                        return CPTK_STATUS_OK;
                    }
                    if (click_state) {
                        menu_mouse_click_state_update(click_state, idx, now_ms);
                    }
                }
            }
            continue;
        }

        /* Numeric typing like simple menu: collect digits and update prompt */
        if (key.key == CPTK_VT100_KEY_TEXT && key.codepoint >= '0' && key.codepoint <= '9') {
            double now = (double)(clock()) / (double)CLOCKS_PER_SEC;
            if (now - last_digit_t > 0.2) digit_buf[0] = '\0';
            size_t len = strlen(digit_buf);
            if (len + 1 < sizeof(digit_buf)) {
                digit_buf[len] = (char)key.codepoint;
                digit_buf[len + 1] = '\0';
            }
            last_digit_t = now;
            /* Only update the prompt line to avoid redrawing the whole list */
            CONSOLE_SCREEN_BUFFER_INFO csbi2;
            COORD ppos;
            if (GetConsoleScreenBufferInfo(out_handle, &csbi2)) {
                DWORD written2 = 0;
                int width2 = (int)csbi2.dwSize.X;
                ppos.X = 0;
                ppos.Y = (SHORT)(start_row + (int)item_count);
                if (vt_enabled) {
                    printf("\x1b[%d;1H\x1b[2K", ppos.Y + 1);
                    printf("%s%s%s%s", ANSI_GREEN, ANSI_BOLD, digit_buf ? digit_buf : "", ANSI_RESET);
                } else {
                    FillConsoleOutputCharacterA(out_handle, ' ', (DWORD)width2, ppos, &written2);
                    FillConsoleOutputAttribute(out_handle, (WORD)g_default_console_attr, (DWORD)width2, ppos, &written2);
                    SetConsoleCursorPosition(out_handle, ppos);
                    printf("%s", digit_buf ? digit_buf : "");
                }
            }
            fflush(stdout);
            continue;
        }

        /* Navigation keys */
        if (key.key == CPTK_VT100_KEY_UP) {
            if (focused > 0) {
                size_t prev = focused;
                focused--;
                cptk_menu_update_single_row(out_handle, start_row, items, item_count, prev, 0);
                cptk_menu_update_single_row(out_handle, start_row, items, item_count, focused, 1);
                /* restore prompt */
                CONSOLE_SCREEN_BUFFER_INFO csbi2; COORD ppos;
                if (GetConsoleScreenBufferInfo(out_handle, &csbi2)) {
                    DWORD written2 = 0; int width2 = (int)csbi2.dwSize.X; ppos.X = 0; ppos.Y = (SHORT)(start_row + (int)item_count);
                    if (vt_enabled) {
                        printf("\x1b[%d;1H\x1b[2K", ppos.Y + 1);
                        printf("%s%s%s%s", ANSI_GREEN, ANSI_BOLD, digit_buf ? digit_buf : "", ANSI_RESET);
                    } else {
                        FillConsoleOutputCharacterA(out_handle, ' ', (DWORD)width2, ppos, &written2);
                        FillConsoleOutputAttribute(out_handle, (WORD)g_default_console_attr, (DWORD)width2, ppos, &written2);
                        SetConsoleCursorPosition(out_handle, ppos);
                        printf("%s", digit_buf ? digit_buf : "");
                    }
                }
                fflush(stdout);
            }
            continue;
        }
        if (key.key == CPTK_VT100_KEY_DOWN) {
            if (focused + 1 < item_count) {
                size_t prev = focused;
                focused++;
                cptk_menu_update_single_row(out_handle, start_row, items, item_count, prev, 0);
                cptk_menu_update_single_row(out_handle, start_row, items, item_count, focused, 1);
                CONSOLE_SCREEN_BUFFER_INFO csbi2; COORD ppos;
                if (GetConsoleScreenBufferInfo(out_handle, &csbi2)) {
                    DWORD written2 = 0; int width2 = (int)csbi2.dwSize.X; ppos.X = 0; ppos.Y = (SHORT)(start_row + (int)item_count);
                    if (vt_enabled) {
                        printf("\x1b[%d;1H\x1b[2K", ppos.Y + 1);
                        printf("%s%s%s%s", ANSI_GREEN, ANSI_BOLD, digit_buf ? digit_buf : "", ANSI_RESET);
                    } else {
                        FillConsoleOutputCharacterA(out_handle, ' ', (DWORD)width2, ppos, &written2);
                        FillConsoleOutputAttribute(out_handle, (WORD)g_default_console_attr, (DWORD)width2, ppos, &written2);
                        SetConsoleCursorPosition(out_handle, ppos);
                        printf("%s", digit_buf ? digit_buf : "");
                    }
                }
                fflush(stdout);
            }
            continue;
        }

        if (key.key == CPTK_VT100_KEY_ENTER) {
            unsigned long v = 0;
            /* Require Enter key to be released before accepting to avoid
             * long-press causing multiple activations. */
#ifdef _WIN32
            while (GetAsyncKeyState(VK_RETURN) & 0x8000) {
                /* wait for key release */
                Sleep(10);
            }
#endif
            if (digit_buf[0]) {
                v = strtoul(digit_buf, NULL, 10);
                if (v >= 1 && v <= item_count) {
                    *out_selected_index = (size_t)(v - 1);
                } else {
                    *out_selected_index = focused;
                }
            } else {
                *out_selected_index = focused;
            }
            cptk_restore_vt_mode(in_handle, out_handle, old_in, old_out);
            return CPTK_STATUS_OK;
        }
        /* ignore other keys */
        continue;
    }

#else
    (void)ctx; (void)title; (void)items; (void)item_count; (void)default_index; (void)out_selected_index;
    return CPTK_STATUS_ENOTSUP;
#endif
}

/* Helper: render multi-choice menu with wrapping and fill opt_row_start/opt_row_end.
 * Returns the number of printed rows.
 */
static size_t cptk_menu_render_multi_helper(
    HANDLE out_handle,
    int start_row,
    int width,
    int avail,
    const char *const *items,
    size_t item_count,
    const unsigned char *sel_mask,
    int focused,
    short *opt_row_start,
    short *opt_row_end
) {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    DWORD written = 0;
    COORD pos;
    size_t total_rows = 0;

    /* First compute mapping (how many wrapped lines each item needs) */
    for (size_t i = 0; i < item_count; ++i) {
        const char *lbl = items[i] ? items[i] : "";
        size_t len = strlen(lbl);
        size_t p = 0;
        int lines = 0;
        while (p < len) {
            size_t can = (size_t)avail;
            if (len - p <= can) {
                p = len;
                lines++;
                break;
            }
            size_t j;
            for (j = can; j > 0; --j) {
                if (lbl[p + j - 1] == ' ') break;
            }
            if (j == 0) j = can;
            p += j;
            while (p < len && lbl[p] == ' ') p++;
            lines++;
        }
        opt_row_start[i] = (short)total_rows;
        opt_row_end[i] = (short)(total_rows + lines - 1);
        total_rows += (size_t)lines;
    }

    /* Clear target region. Prefer ANSI per-line clearing when VT is active. */
    if (GetConsoleScreenBufferInfo(out_handle, &csbi)) {
        pos.X = 0;
        pos.Y = (SHORT)start_row;
        DWORD out_mode = 0;
        int vt_out = 0;
        if (GetConsoleMode(out_handle, &out_mode) && (out_mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING)) vt_out = 1;
        if (vt_out) {
            int y;
            for (y = start_row; y < start_row + (int)total_rows; ++y) {
                printf("\x1b[%d;1H\x1b[2K", y + 1);
            }
            printf("\x1b[%d;1H", start_row + 1);
        } else {
            FillConsoleOutputCharacterA(out_handle, ' ', (DWORD)(width * (int)total_rows), pos, &written);
            FillConsoleOutputAttribute(out_handle, (WORD)g_default_console_attr, (DWORD)(width * (int)total_rows), pos, &written);
            SetConsoleCursorPosition(out_handle, pos);
        }
    }

    /* Print items with wrapping and update mapping (opt_row_start/opt_row_end already relative) */
    for (size_t idx = 0; idx < item_count; ++idx) {
        const char *lbl = items[idx] ? items[idx] : "";
        size_t len = strlen(lbl);
        size_t p = 0;
        int is_focused = (int)idx == focused;
        char sel_ch = sel_mask && sel_mask[idx] ? 'x' : ' ';

        while (p < len) {
            size_t can = (size_t)avail;
            if (len - p <= can) {
                if (is_focused) {
                    /* Highlight focused line without pointer */
                    printf("[%c] %u. %.*s\n", sel_ch, (unsigned)(idx + 1), (int)(len - p), lbl + p);
                } else {
                    printf("  [%c] %u. %.*s\n", sel_ch, (unsigned)(idx + 1), (int)(len - p), lbl + p);
                }
                p = len;
                break;
            }
            size_t j;
            for (j = can; j > 0; --j) {
                if (lbl[p + j - 1] == ' ') break;
            }
            if (j == 0) j = can;
            if (is_focused) {
                printf("[%c] %u. %.*s\n", sel_ch, (unsigned)(idx + 1), (int)j, lbl + p);
            } else {
                printf("  [%c] %u. %.*s\n", sel_ch, (unsigned)(idx + 1), (int)j, lbl + p);
            }
            p += j;
            while (p < len && lbl[p] == ' ') p++;
        }
    }
    fflush(stdout);
    return total_rows;
}

/* Helper: render single-choice menu with simple highlighting using ANSI sequences.
 * Re-draws item_count lines starting at start_row and leaves cursor at prompt row.
 */
static void cptk_menu_render_single_helper(
    HANDLE out_handle,
    int start_row,
    const char *const *items,
    size_t item_count,
    size_t focused,
    const char *digit_buf
) {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    COORD pos;
    size_t i;

    if (!GetConsoleScreenBufferInfo(out_handle, &csbi)) {
        /* fallback: just print without positioning */
        for (i = 0; i < item_count; ++i) {
            if (i == focused) {
                /* Highlight focused line but do not print leading pointer */
                printf("%s%s%s%u. %s%s\n", ANSI_WHITE, ANSI_BOLD, ANSI_UNDERLINE, (unsigned)(i + 1), items[i], ANSI_RESET);
            } else {
                printf("  %u. %s\n", (unsigned)(i + 1), items[i]);
            }
        }
        /* Print only typed digits (no "Select>" label) */
        if (digit_buf && digit_buf[0]) printf("%s", digit_buf);
        fflush(stdout);
        return;
    }
    /* Clear the whole region (options + prompt) to remove previous output.
     * Prefer ANSI clearing when the terminal supports VT output to avoid
     * stomping SGR state via Win32 attribute writes. */
    {
        DWORD out_mode = 0;
        int vt_out = 0;
        if (GetConsoleMode(out_handle, &out_mode) && (out_mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING)) {
            vt_out = 1;
        }
        if (vt_out) {
            int y;
            for (y = start_row; y <= start_row + (int)item_count; ++y) {
                /* Move to line (1-based) and erase entire line */
                printf("\x1b[%d;1H\x1b[2K", y + 1);
            }
            /* Place cursor at start */
            printf("\x1b[%d;1H", start_row + 1);
        } else {
            DWORD written = 0;
            int width = (int)csbi.dwSize.X;
            COORD top = {0, (SHORT)start_row};
            DWORD count = (DWORD)width * (DWORD)(item_count + 1);
            FillConsoleOutputCharacterA(out_handle, ' ', count, top, &written);
            FillConsoleOutputAttribute(out_handle, (WORD)g_default_console_attr, count, top, &written);
        }
    }

    for (i = 0; i < item_count; ++i) {
        pos.X = 0;
        pos.Y = (SHORT)(start_row + (int)i);
        SetConsoleCursorPosition(out_handle, pos);
        if (i == focused) {
            /* Highlight focused line without pointer */
            printf("%s%s%s%u. %s%s\n", ANSI_WHITE, ANSI_BOLD, ANSI_UNDERLINE, (unsigned)(i + 1), items[i], ANSI_RESET);
        } else {
            /* If terminal supports VT, print non-focused items with ANSI colors
             * to match the rest of the UI. Otherwise fall back to plain text. */
            DWORD out_mode2 = 0;
            int vt_out2 = 0;
            if (GetConsoleMode(out_handle, &out_mode2) && (out_mode2 & ENABLE_VIRTUAL_TERMINAL_PROCESSING)) vt_out2 = 1;
            if (vt_out2) {
                printf("  %s%s%u.%s %s%s%s\n", ANSI_BLUE, ANSI_BOLD, (unsigned)(i + 1), ANSI_RESET, ANSI_WHITE, items[i], ANSI_RESET);
            } else {
                printf("  %u. %s\n", (unsigned)(i + 1), items[i]);
            }
        }
    }

    /* prompt row */
    pos.X = 0;
    pos.Y = (SHORT)(start_row + (int)item_count);
    /* For VT terminals, use ANSI to clear/print the prompt with color. */
    {
        DWORD out_mode3 = 0;
        int vt_out3 = 0;
        if (GetConsoleMode(out_handle, &out_mode3) && (out_mode3 & ENABLE_VIRTUAL_TERMINAL_PROCESSING)) vt_out3 = 1;
        if (vt_out3) {
            printf("\x1b[%d;1H\x1b[2K", pos.Y + 1);
            if (digit_buf && digit_buf[0]) printf("%s%s%s%s", ANSI_GREEN, ANSI_BOLD, digit_buf, ANSI_RESET);
        } else {
            SetConsoleCursorPosition(out_handle, pos);
            if (digit_buf && digit_buf[0]) printf("%s", digit_buf);
        }
    }
    fflush(stdout);
}

/* Update a single option row (no prompt redraw). This avoids re-rendering the
 * whole menu on hover changes, reducing flicker. */
static void cptk_menu_update_single_row(
    HANDLE out_handle,
    int start_row,
    const char *const *items,
    size_t item_count,
    size_t idx,
    int is_focused
) {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    COORD pos;
    DWORD written = 0;

    if (!items || idx >= item_count) return;

    if (GetConsoleScreenBufferInfo(out_handle, &csbi)) {
        int width = (int)csbi.dwSize.X;
        pos.X = 0;
        pos.Y = (SHORT)(start_row + (int)idx);
        /* Clear the line. Prefer ANSI when VT is enabled. */
        DWORD out_mode = 0;
        int vt_out = 0;
        if (GetConsoleMode(out_handle, &out_mode) && (out_mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING)) vt_out = 1;
        if (vt_out) {
            printf("\x1b[%d;1H\x1b[2K", pos.Y + 1);
        } else {
            FillConsoleOutputCharacterA(out_handle, ' ', (DWORD)width, pos, &written);
            FillConsoleOutputAttribute(out_handle, (WORD)g_default_console_attr, (DWORD)width, pos, &written);
            SetConsoleCursorPosition(out_handle, pos);
        }
    } else {
        /* Fallback to ANSI EOL erase */
        printf("\r\x1b[2K");
    }

    if (is_focused) {
        printf("%s%s%s%u. %s%s", ANSI_WHITE, ANSI_BOLD, ANSI_UNDERLINE, (unsigned)(idx + 1), items[idx], ANSI_RESET);
    } else {
        DWORD out_mode2 = 0;
        int vt_out2 = 0;
        if (GetConsoleMode(out_handle, &out_mode2) && (out_mode2 & ENABLE_VIRTUAL_TERMINAL_PROCESSING)) vt_out2 = 1;
        if (vt_out2) {
            printf("  %s%s%u.%s %s%s%s", ANSI_BLUE, ANSI_BOLD, (unsigned)(idx + 1), ANSI_RESET, ANSI_WHITE, items[idx], ANSI_RESET);
        } else {
            printf("  %u. %s", (unsigned)(idx + 1), items[idx]);
        }
    }
    fflush(stdout);
}

/* Update a single multi-choice item (may span multiple wrapped lines). */
static void cptk_menu_update_multi_item(
    HANDLE out_handle,
    int start_row,
    int width,
    int avail,
    const char *const *items,
    size_t item_count,
    const unsigned char *sel_mask,
    int idx,
    int focused,
    const short *opt_row_start,
    const short *opt_row_end
) {
    if (!items || idx < 0 || (size_t)idx >= item_count) return;
    const char *lbl = items[idx] ? items[idx] : "";
    size_t len = strlen(lbl);
    size_t p = 0;
    int is_focused = (focused == idx);
    char sel_ch = sel_mask && sel_mask[idx] ? 'x' : ' ';
    COORD pos;
    DWORD written = 0;

    /* For each wrapped line for this item, clear the console line then print */
    int row = start_row + opt_row_start[idx];
    while (p < len) {
        size_t can = (size_t)avail;
        size_t take;
        if (len - p <= can) {
            take = len - p;
        } else {
            size_t j;
            for (j = can; j > 0; --j) {
                if (lbl[p + j - 1] == ' ') break;
            }
            if (j == 0) j = can;
            take = j;
        }

        pos.X = 0;
        pos.Y = (SHORT)row;
        /* Prefer ANSI clearing when VT is enabled to avoid attribute stomping. */
        {
            DWORD out_mode4 = 0;
            int vt_out4 = 0;
            if (GetConsoleMode(out_handle, &out_mode4) && (out_mode4 & ENABLE_VIRTUAL_TERMINAL_PROCESSING)) vt_out4 = 1;
            if (vt_out4) {
                printf("\x1b[%d;1H\x1b[2K", pos.Y + 1);
            } else {
                /* Try to clear line with Win32 API; fall back to ANSI if it fails. */
                if (!FillConsoleOutputCharacterA(out_handle, ' ', (DWORD)width, pos, &written)) {
                    printf("\r\x1b[2K");
                } else {
                    FillConsoleOutputAttribute(out_handle, (WORD)g_default_console_attr, (DWORD)width, pos, &written);
                    SetConsoleCursorPosition(out_handle, pos);
                }
            }
        }

        if (p + take >= len) {
            if (is_focused) {
                printf("[%c] %u. %.*s\n", sel_ch, (unsigned)(idx + 1), (int)take, lbl + p);
            } else {
                printf("  [%c] %u. %.*s\n", sel_ch, (unsigned)(idx + 1), (int)take, lbl + p);
            }
            p = len;
        } else {
            if (is_focused) {
                printf("[%c] %u. %.*s\n", sel_ch, (unsigned)(idx + 1), (int)take, lbl + p);
            } else {
                printf("  [%c] %u. %.*s\n", sel_ch, (unsigned)(idx + 1), (int)take, lbl + p);
            }
            p += take;
            while (p < len && lbl[p] == ' ') p++;
        }
        row++;
    }
    fflush(stdout);
}

cptk_status cptk_menu_multi_choice_interactive(
    cptk_context *ctx,
    const char *title,
    const char *const *items,
    size_t item_count,
    unsigned char *out_selected_mask,
    int animate_in,
    int max_height
) {
#ifdef _WIN32
    if (!ctx || !items || item_count == 0 || !out_selected_mask) {
        return CPTK_STATUS_EINVAL;
    }

    /* If not interactive, fall back to single-choice prompt to select one item. */
    if (!cptk_is_interactive()) {
        size_t sel = 0;
        cptk_status st = cptk_menu_choice(ctx, title, items, item_count, 0, &sel);
        memset(out_selected_mask, 0, item_count);
        if (st == CPTK_STATUS_OK) {
            out_selected_mask[sel] = 1;
        }
        return st;
    }

    HANDLE in_handle = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE out_handle = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD old_in = 0, old_out = 0;
    int vt_enabled = 0;
    cptk_vt100_parser parser;
    size_t i;

    if (!cptk_enable_vt_mode(in_handle, out_handle, &old_in, &old_out, &vt_enabled)) {
        /* fallback */
        size_t sel = 0;
        cptk_status st = cptk_menu_choice(ctx, title, items, item_count, 0, &sel);
        memset(out_selected_mask, 0, item_count);
        if (st == CPTK_STATUS_OK) out_selected_mask[sel] = 1;
        return st;
    }

    cptk_vt100_parser_init(&parser);

    if (title && title[0]) {
        printf("%s\n", title);
    }

    /* Determine console width */
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    int width = 80;
    int start_row = 0;
    if (GetConsoleScreenBufferInfo(out_handle, &csbi)) {
        width = (int)csbi.dwSize.X;
        start_row = (int)csbi.dwCursorPosition.Y;
    }

    if (width <= 10) width = 80;

    /* compute digit width for numbering */
    int digits = 1;
    for (i = 10; i <= item_count; i *= 10) digits++;
    int prefix_len = 6 + digits; /* e.g. " > [x] N. " approx */
    int avail = width - prefix_len;
    if (avail < 8) avail = width - 10 > 8 ? width - 10 : 8;

    /* allocate rows mapping */
    short *opt_row_start = (short *)malloc(sizeof(short) * item_count);
    short *opt_row_end = (short *)malloc(sizeof(short) * item_count);
    if (!opt_row_start || !opt_row_end) {
        if (opt_row_start) free(opt_row_start);
        if (opt_row_end) free(opt_row_end);
        cptk_restore_vt_mode(in_handle, out_handle, old_in, old_out);
        return CPTK_STATUS_ENOMEM;
    }

    /* Use helper to compute mapping and render initial menu. */
    size_t total_rows = cptk_menu_render_multi_helper(out_handle, start_row, width, avail, items, item_count, out_selected_mask, 0, opt_row_start, opt_row_end);
    if (max_height > 0 && (int)total_rows > max_height) {
        total_rows = (size_t)max_height;
    }

    /* initialize selection mask */
    for (i = 0; i < item_count; ++i) out_selected_mask[i] = 0;

    int focused = 0;
    menu_mouse_click_state click_state;
    unsigned int double_click_ms = menu_get_double_click_threshold_ms();
    unsigned long last_motion_tick = 0;

    menu_mouse_click_state_reset(&click_state);

    /* initial animation */
    if (animate_in) {
        /* simple animation pause after initial render */
        Sleep(120);
    } else {
        /* already rendered once above */
    }

    /* Event loop */
    while (1) {
        cptk_key_event key;
        cptk_mouse_event mouse_ev;
        int got = 0;

        memset(&mouse_ev, 0, sizeof(mouse_ev));
        got = cptk_read_vt_key(in_handle, &parser, &key, &mouse_ev);
        if (!got) {
            got = cptk_read_legacy_key(&key);
        }
        if (!got) continue;

        if (mouse_ev.present) {
            int b = (int)mouse_ev.button;
            int is_motion = (b & 0x20) != 0;
            int btn = b & 0x03; /* 0=left,1=middle,2=right */
            int click_row = mouse_ev.y;
            int target = -1;

            /* Map row to option index */
            for (size_t idx = 0; idx < item_count; ++idx) {
                int s = start_row + opt_row_start[idx];
                int e = start_row + opt_row_end[idx];
                if (click_row >= s && click_row <= e) {
                    target = (int)idx;
                    break;
                }
            }

            if (is_motion) {
                /* Throttle quick mouse moves to avoid excessive redraws */
                unsigned long now = GetTickCount();
                if (now - last_motion_tick < 30) {
                    continue;
                }
                last_motion_tick = now;
                /* Hover: update focused index for visual feedback. Update only two
                 * affected items to reduce redraw flicker. */
                if (target >= 0 && focused != target) {
                    int prev = focused;
                    focused = target;
                    cptk_menu_update_multi_item(out_handle, start_row, width, avail, items, item_count, out_selected_mask, prev, focused, opt_row_start, opt_row_end);
                    cptk_menu_update_multi_item(out_handle, start_row, width, avail, items, item_count, out_selected_mask, focused, focused, opt_row_start, opt_row_end);
                    /* restore cursor to after menu */
                    CONSOLE_SCREEN_BUFFER_INFO csbi2; COORD ppos2;
                    if (GetConsoleScreenBufferInfo(out_handle, &csbi2)) {
                        ppos2.X = 0; ppos2.Y = (SHORT)(start_row + (int)total_rows);
                        SetConsoleCursorPosition(out_handle, ppos2);
                    }
                }
                continue;
            }

            if (target >= 0) {
                unsigned long now = GetTickCount();
                if (btn == 2) { /* right click -> confirm */
                    break;
                }
                /* Toggle on left press (btn==0) or on explicit release */
                if (mouse_ev.is_release || btn == 0) {
                    out_selected_mask[target] = out_selected_mask[target] ? 0 : 1;
                    /* Update only the toggled item */
                    cptk_menu_update_multi_item(out_handle, start_row, width, avail, items, item_count, out_selected_mask, target, focused, opt_row_start, opt_row_end);
                    /* restore cursor to after menu */
                    CONSOLE_SCREEN_BUFFER_INFO csbi3; COORD ppos3;
                    if (GetConsoleScreenBufferInfo(out_handle, &csbi3)) {
                        ppos3.X = 0; ppos3.Y = (SHORT)(start_row + (int)total_rows);
                        SetConsoleCursorPosition(out_handle, ppos3);
                    }
                    if (menu_mouse_double_click_hit(&click_state, target, (unsigned long long)now, double_click_ms)) {
                        /* double click: finish */
                        break;
                    }
                    menu_mouse_click_state_update(&click_state, target, (unsigned long long)now);
                }
            }
            continue;
        }

        /* Keyboard navigation */
        if (key.key == CPTK_VT100_KEY_UP) {
            if (focused > 0) focused--;
            (void)cptk_menu_render_multi_helper(out_handle, start_row, width, avail, items, item_count, out_selected_mask, focused, opt_row_start, opt_row_end);
            continue;
        }
        if (key.key == CPTK_VT100_KEY_DOWN) {
            if (focused + 1 < (int)item_count) focused++;
            (void)cptk_menu_render_multi_helper(out_handle, start_row, width, avail, items, item_count, out_selected_mask, focused, opt_row_start, opt_row_end);
            continue;
        }
        if (key.key == CPTK_VT100_KEY_TEXT && key.codepoint == ' ') {
            out_selected_mask[focused] = out_selected_mask[focused] ? 0 : 1;
            (void)cptk_menu_render_multi_helper(out_handle, start_row, width, avail, items, item_count, out_selected_mask, focused, opt_row_start, opt_row_end);
            continue;
        }
        if (key.key == CPTK_VT100_KEY_ENTER) {
            break;
        }
        if (key.key == CPTK_VT100_KEY_ESCAPE) {
            /* cancel: clear mask and return EINVAL */
            memset(out_selected_mask, 0, item_count);
            cptk_restore_vt_mode(in_handle, out_handle, old_in, old_out);
            free(opt_row_start);
            free(opt_row_end);
            return CPTK_STATUS_EINVAL;
        }
    }

    cptk_restore_vt_mode(in_handle, out_handle, old_in, old_out);
    free(opt_row_start);
    free(opt_row_end);
    return CPTK_STATUS_OK;
#else
    (void)ctx; (void)title; (void)items; (void)item_count; (void)out_selected_mask; (void)animate_in; (void)max_height;
    return CPTK_STATUS_ENOTSUP;
#endif
}

cptk_status cptk_loop_get(const cptk_context *ctx, const cptk_loop **out_loop) {
    if (!ctx || !out_loop) {
        return CPTK_STATUS_EINVAL;
    }
    *out_loop = &ctx->loop;
    return CPTK_STATUS_OK;
}

const char *cptk_status_message(cptk_status status) {
    switch (status) {
        case CPTK_STATUS_OK: return k_status_msg_ok;
        case CPTK_STATUS_EINVAL: return k_status_msg_einval;
        case CPTK_STATUS_ENOMEM: return k_status_msg_enomem;
        case CPTK_STATUS_EIO: return k_status_msg_eio;
        case CPTK_STATUS_ENOTSUP: return k_status_msg_enotsup;
        case CPTK_STATUS_EINTERNAL: return k_status_msg_einternal;
        default: return "unknown status";
    }
}
