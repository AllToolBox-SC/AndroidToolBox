// start.c - native C entrypoint for AllToolBox
// Goals:
// 1) Keep user-facing menu text/flow aligned with src/start.py.
// 2) Remove runtime dependency on Python for startup/menu dispatch.
// 3) Load menu structure from JSON exported to src/menu/start/menus.json.

#define _CRT_SECURE_NO_WARNINGS

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <wininet.h>
#include <conio.h>
#include <direct.h>
#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <ctype.h>
#include <stdarg.h>
#include "c_prompt_toolkit.h"

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "User32.lib")

#define SAFE_CLOSE(h) do { if ((h) && (h) != INVALID_HANDLE_VALUE) { CloseHandle(h); (h) = INVALID_HANDLE_VALUE; } } while (0)

#define MAX_MENUS 32
#define MAX_OPTIONS_PER_MENU 64
#define MAX_KEY_LEN 32
#define MAX_LABEL_LEN 256
#define MAX_ACTION_LEN 48
#define MAX_TARGET_LEN 128
#define MAX_COMMAND_LEN 1024

#define MENU_RESULT_CONTINUE 0
#define MENU_RESULT_BACK 1
#define MENU_RESULT_EXIT 2

typedef struct PersistentShell {
    HANDLE hChildProcess;
    HANDLE hChildStdInWr;
    HANDLE hChildStdOutRd;
    HANDLE hOutputThread;
    HANDLE hMarkerEvent;
    CRITICAL_SECTION cs;
    char marker[128];
    int last_exit_code;
    unsigned int seq;
    volatile bool running;
    volatile int output_allowed;
} PersistentShell;

typedef struct {
    char key[MAX_KEY_LEN];
    char label[MAX_LABEL_LEN];
    char value[MAX_KEY_LEN];
    char action[MAX_ACTION_LEN];
    char target[MAX_TARGET_LEN];
    char command[MAX_COMMAND_LEN];
    int timeout_ms;
    int requires_debug;
    int pause_after;
} MenuOption;

typedef struct {
    char id[64];
    char title[MAX_LABEL_LEN];
    char prompt[64];
    int show_logo;
    int allow_hotkey_debug;
    int multi_select;
    int animate_in;
    int max_height;
    MenuOption options[MAX_OPTIONS_PER_MENU];
    int option_count;
} MenuDef;

typedef struct {
    MenuDef menus[MAX_MENUS];
    int menu_count;
} MenuConfig;

static const char *PATHEXT_EXTRA = ".COM;.EXE;.BAT;.CMD;.VBS;.VBE;.JS;.JSE;.WSF;.WSH;.MSC";
static int g_debug_mode = 0;
static int g_allow_xtc = 1;
static int g_vt_enabled = 0;
static WORD g_default_console_attr = (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
static int g_disable_transition_animation = 1;

/* Debug logging helper: when ATB_MENU_DEBUG is set (non-zero), write
 * formatted messages to stderr and append to file ATB_MENU_DEBUG_FILE
 * (defaults to atb_menu_debug.log). Use this for MENU_DBG lines. */
static void menu_debug_log(const char *fmt, ...) {
    const char *dbg = getenv("ATB_MENU_DEBUG");
    if (!dbg || dbg[0] == '\0' || strcmp(dbg, "0") == 0) return;
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

#define ANSI_RESET "\x1b[0m"
#define ANSI_YELLOW "\x1b[38;2;241;196;15m"
#define ANSI_RED "\x1b[38;2;255;123;123m"
#define ANSI_ORANGE "\x1b[38;2;244;162;97m"
#define ANSI_INFO "\x1b[38;2;59;120;255m"
#define ANSI_BLACK "\x1b[38;2;12;17;24m"
#define ANSI_CYAN "\x1b[38;2;103;224;194m"
#define ANSI_GREEN "\x1b[38;2;122;209;168m"
#define ANSI_BLUE "\x1b[38;2;138;180;248m"
#define ANSI_MAGENTA "\x1b[38;2;199;160;255m"
#define ANSI_WHITE "\x1b[38;2;230;237;243m"
#define ANSI_BOLD "\x1b[1m"
#define ANSI_UNDERLINE "\x1b[4m"

static void clear_console(void);

static int console_has_pause_prompt(void) {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (!hOut || hOut == INVALID_HANDLE_VALUE) return 0;
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (!GetConsoleScreenBufferInfo(hOut, &csbi)) return 0;
    int width = (int)csbi.dwSize.X;
    int row = (int)csbi.dwCursorPosition.Y;
    int start_row = row - 1;
    if (start_row < 0) start_row = 0;
    int lines = row - start_row + 1;
    int count = width * lines;
    char *buf = (char *)malloc((size_t)count + 1);
    if (!buf) return 0;
    COORD coord;
    coord.X = 0;
    coord.Y = (SHORT)start_row;
    DWORD read = 0;
    if (!ReadConsoleOutputCharacterA(hOut, buf, (DWORD)count, coord, &read)) {
        free(buf);
        return 0;
    }
    buf[read] = '\0';
    int found = 0;
    if (strstr(buf, "按任意键") || strstr(buf, "Press any key") || strstr(buf, "press any key")) {
        found = 1;
    }
    free(buf);
    return found;
}

static void pause_any(const char *message, double timeout_seconds) {
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    ULONGLONG start = GetTickCount64();

    int has_prompt = console_has_pause_prompt();
    int printed_prompt = 0;
    if (!has_prompt) {
        putchar('\n');
        if (message && message[0]) {
            printf("%s", message);
        } else {
            printf("按任意键继续...");
        }
        fflush(stdout);
        printed_prompt = 1;
    } else {
        fflush(stdout);
    }

    /* Wait for a key or mouse event. If timeout_seconds > 0, return after timeout. */
    while (1) {
        if (timeout_seconds > 0.0) {
            ULONGLONG now = GetTickCount64();
            if (((double)(now - start) / 1000.0) >= timeout_seconds) break;
        }

        if (!hIn || hIn == INVALID_HANDLE_VALUE) {
            /* Fallback to blocking _getch if no console handle */
            (void)_getch();
            break;
        }

        INPUT_RECORD ir;
        DWORD read_count = 0;
        if (!ReadConsoleInputA(hIn, &ir, 1, &read_count) || read_count == 0) {
            Sleep(10);
            continue;
        }

        if (ir.EventType == KEY_EVENT) {
            KEY_EVENT_RECORD key = ir.Event.KeyEvent;
            if (key.bKeyDown) break;
            continue;
        }

        if (ir.EventType == MOUSE_EVENT) {
            MOUSE_EVENT_RECORD me = ir.Event.MouseEvent;
            /* Accept simple button press/release or double-click as continuation */
            if (me.dwEventFlags == 0) {
                if (me.dwButtonState != 0) {
                    break;
                }
                if (me.dwButtonState == 0) {
                    break;
                }
            }
            if (me.dwEventFlags == DOUBLE_CLICK) break;
            continue;
        }
    }

    if (printed_prompt) {
        /* Clear the console if we printed the pause prompt to restore UI. */
        clear_console();
    } else {
        putchar('\n');
        fflush(stdout);
    }
}
static void setup_console_encoding_and_vt(void);
static void console_set_color(WORD attr);
static void page_transition(const char *text, double duration_seconds);
static void log_info(const char *fmt, ...);
static void log_warn(const char *fmt, ...);
static void log_error(const char *fmt, ...);
static void set_pathext_extra(void);
static bool check_adb_server(void);
static void cleanup_and_exit(PersistentShell *ps, int code);

static PersistentShell *ps_create(void);
static void ps_destroy(PersistentShell *ps);
static int ps_run_command(PersistentShell *ps, const char *cmd, int timeout_ms);
static bool ps_is_alive(PersistentShell *ps);
static void run_if_present(PersistentShell *ps, const char *base_name);

static bool http_get_w(const wchar_t *url, char *out_buf, size_t out_size, int timeout_ms);

static char *read_text_file(const char *path, size_t *out_size);
static int load_menu_config(const char *json_path, MenuConfig *config);
static MenuDef *find_menu(MenuConfig *config, const char *id);
static int resolve_menu_json_path(char *out_path, size_t out_size);

static int run_menu_by_id(PersistentShell *ps, MenuConfig *config, const char *menu_id);
static int execute_menu_action(PersistentShell *ps, MenuConfig *config, const MenuOption *opt);
static int find_visible_option_index_by_key(const MenuOption *const *visible_opts, int visible_count, const char *key);
static int resolve_click_index_from_row(const short option_rows[MAX_OPTIONS_PER_MENU], int visible_count, short prompt_row, short mouse_y);
static const char *get_menu_prompt_for_display(const MenuDef *menu);
static int has_visible_option_with_prefix(const MenuOption *const *visible_opts, int visible_count, const char *prefix);
static void print_menu_option_line(const MenuOption *opt, int is_selected, int append_newline);
static void redraw_menu_interaction_block(
    const MenuDef *menu,
    const MenuOption *const *visible_opts,
    int visible_count,
    int selected_index,
    const char *typed,
    const short option_rows[MAX_OPTIONS_PER_MENU]
);
static int wait_menu_selection(
    PersistentShell *ps,
    const MenuDef *menu,
    const MenuOption *const *visible_opts,
    int visible_count,
    int *selected_index,
    const MenuOption **selected_opt,
    short option_rows[MAX_OPTIONS_PER_MENU]
);
static void draw_menu_frame(
    PersistentShell *ps,
    const MenuDef *menu,
    const MenuOption *const *visible_opts,
    int visible_count,
    int shown_count,
    int selected_index,
    const char *typed,
    short option_rows[MAX_OPTIONS_PER_MENU]
);
static void print_main_mods_summary(void);
static int run_mod_installed_menu(PersistentShell *ps);
static void show_about_screen(PersistentShell *ps);
static void show_doc_screen(void);
static void show_color_card(void);
static void run_debug_sel(PersistentShell *ps);
static void write_whoyou(const char *value);

static int pre_main(void);
static int detect_debug_build_mode(void);
static int resolve_build_info_path(char *out_path, size_t out_size);
static void run_mod_startup_scripts(void);
static void get_windows_version(int *major, int *minor, int *build);
static void get_exe_dir(char *out_dir, size_t out_size);
static int build_path_join(char *out, size_t out_size, const char *a, const char *b);
static char *trim_inplace(char *s);
static int str_eq_icase(const char *a, const char *b);

static void clear_console(void) {
    /* When VT/ANSI is enabled prefer ANSI full-screen clear so any ANSI
     * drawn content (including alternate buffer writes) is cleared.
     * Fall back to Win32 buffer clearing when VT is not available. */
    if (g_vt_enabled) {
        printf("\x1b[0m\x1b[2J\x1b[H");
        fflush(stdout);
        return;
    }

    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (!hOut || hOut == INVALID_HANDLE_VALUE) {
        /* fallback */
        system("cls");
        return;
    }

    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (!GetConsoleScreenBufferInfo(hOut, &csbi)) {
        system("cls");
        return;
    }

    COORD home = {0, 0};
    DWORD cellCount = (DWORD)csbi.dwSize.X * (DWORD)csbi.dwSize.Y;
    DWORD written = 0;

    FillConsoleOutputCharacterA(hOut, ' ', cellCount, home, &written);
    FillConsoleOutputAttribute(hOut, csbi.wAttributes, cellCount, home, &written);
    SetConsoleCursorPosition(hOut, home);
}

static void page_transition(const char *text, double duration_seconds) {
    const char *frames[] = {"", ".", "..", "...", " ..", "  ."};
    const int frame_count = (int)(sizeof(frames) / sizeof(frames[0]));
    const char *msg = (text && text[0]) ? text : "正在切换...";
    clock_t start = clock();
    int width = (int)strlen(msg) + 6;
    int i;

    if (g_disable_transition_animation || duration_seconds <= 0.0) {
        return;
    }

    while (((double)(clock() - start) / CLOCKS_PER_SEC) < duration_seconds) {
        for (i = 0; i < frame_count; ++i) {
            if (((double)(clock() - start) / CLOCKS_PER_SEC) >= duration_seconds) {
                break;
            }
            printf("\r%s%s   ", msg, frames[i]);
            fflush(stdout);
            Sleep(26);
        }
    }

    printf("\r");
    for (i = 0; i < width; ++i) {
        putchar(' ');
    }
    printf("\r");
    fflush(stdout);
}

static void draw_menu_frame(
    PersistentShell *ps,
    const MenuDef *menu,
    const MenuOption *const *visible_opts,
    int visible_count,
    int shown_count,
    int selected_index,
    const char *typed,
    short option_rows[MAX_OPTIONS_PER_MENU]
) {
    int i;
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    const char *prompt_text;

    if (!menu || !visible_opts || !option_rows) {
        return;
    }

    if (shown_count < 0) {
        shown_count = 0;
    }
    if (shown_count > visible_count) {
        shown_count = visible_count;
    }

    clear_console();

    /* Re-run console setup before drawing. Some submenu paths may change
     * console modes or SGR state; reinitializing here helps keep colors
     * consistent when returning from submenus. */
    setup_console_encoding_and_vt();
    if (getenv("ATB_MENU_DEBUG") && getenv("ATB_MENU_DEBUG")[0] && strcmp(getenv("ATB_MENU_DEBUG"), "0") != 0) {
        menu_debug_log("[MENU_DBG] draw_menu_frame: g_vt_enabled=%d\n", g_vt_enabled);
    }

    if (menu->show_logo) {
        run_if_present(ps, "logo");
    }

    /* Ensure ANSI SGR state is reset when using VT output. This helps when
     * returning from submenus that may leave the terminal in a different
     * SGR/attribute state. */
    if (g_vt_enabled) {
        printf("\x1b[0m");
        fflush(stdout);
    }

    if (str_eq_icase(menu->id, "main")) {
        print_main_mods_summary();
        log_info("鼠标双击或按回车键确定，方向键，数字键，鼠标单击定位功能");
    }

    if (menu->title[0]) {
        if (g_vt_enabled) {
            printf("%s%s%s%s\n", ANSI_INFO, ANSI_BOLD, menu->title, ANSI_RESET);
        } else {
            console_set_color(FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
            printf("%s\n", menu->title);
            console_set_color(g_default_console_attr);
        }
    }

    for (i = 0; i < MAX_OPTIONS_PER_MENU; ++i) {
        option_rows[i] = -1;
    }

    /* Clear whole block covering options + prompt to remove stale previous output */
    {
        CONSOLE_SCREEN_BUFFER_INFO csbi2;
        if (GetConsoleScreenBufferInfo(hOut, &csbi2)) {
            int top = option_rows[0] >= 0 ? option_rows[0] : (int)csbi2.dwCursorPosition.Y;
            int bottom = option_rows[visible_count - 1] >= 0 ? option_rows[visible_count - 1] : top + visible_count - 1;
            if (top < 0) top = 0;
            if (bottom < top) bottom = top + visible_count - 1;
            /* When in VT mode, prefer ANSI sequences to clear lines so we do
             * not interfere with terminal SGR handling. For non-VT consoles
             * use the Win32 API to blank the buffer and set attributes. */
            if (g_vt_enabled) {
                int y;
                /* Clear each line in the block using ESC[2K (erase entire line)
                 * and position the cursor at column 1 for each cleared line. */
                for (y = top; y <= bottom + 1; ++y) {
                    /* ANSI cursor position is 1-based */
                    printf("\x1b[%d;1H\x1b[2K", y + 1);
                }
                /* Place cursor at start of first cleared line */
                printf("\x1b[%d;1H", top + 1);
            } else {
                COORD p = {0, (SHORT)top};
                DWORD total = (DWORD)csbi2.dwSize.X * (DWORD)(bottom - top + 2);
                DWORD w = 0;
                FillConsoleOutputCharacterA(hOut, ' ', total, p, &w);
                FillConsoleOutputAttribute(hOut, g_default_console_attr, total, p, &w);
            }
        }
    }

    for (i = 0; i < visible_count; ++i) {
        const MenuOption *opt = visible_opts[i];
        const int is_selected = (i == selected_index);
        CONSOLE_SCREEN_BUFFER_INFO csbi;

        if (hOut && hOut != INVALID_HANDLE_VALUE && GetConsoleScreenBufferInfo(hOut, &csbi)) {
            option_rows[i] = csbi.dwCursorPosition.Y;
        }

        print_menu_option_line(opt, is_selected, 1);
    }

    prompt_text = get_menu_prompt_for_display(menu);
    if (g_vt_enabled) {
        if (prompt_text[0]) {
            printf("%s%s%s%s", ANSI_GREEN, ANSI_BOLD, prompt_text, ANSI_RESET);
        }
    } else {
        if (prompt_text[0]) {
            console_set_color(FOREGROUND_GREEN | FOREGROUND_INTENSITY);
            printf("%s", prompt_text);
            console_set_color(g_default_console_attr);
        }
    }
    if (typed && typed[0]) {
        printf("%s", typed);
    }
    fflush(stdout);
}

static const char *get_menu_prompt_for_display(const MenuDef *menu) {
    const char *prompt_text = "";

    if (menu && menu->prompt[0]) {
        prompt_text = menu->prompt;
    }

    if (strstr(prompt_text, "选择") != NULL) {
        return "> ";
    }
    return prompt_text;
}

static int has_visible_option_with_prefix(const MenuOption *const *visible_opts, int visible_count, const char *prefix) {
    int i;
    size_t n;

    if (!visible_opts || visible_count <= 0 || !prefix || !prefix[0]) {
        return 0;
    }
    n = strlen(prefix);

    for (i = 0; i < visible_count; ++i) {
        const char *key = visible_opts[i] ? visible_opts[i]->key : NULL;
        if (!key || !key[0]) {
            continue;
        }
        if (_strnicmp(key, prefix, n) == 0) {
            return 1;
        }
    }
    return 0;
}

static void print_menu_option_line(const MenuOption *opt, int is_selected, int append_newline) {
    if (!opt) {
        return;
    }

    if (g_vt_enabled) {
        if (is_selected) {
            printf("> %s%s%s%s. %s%s", ANSI_WHITE, ANSI_BOLD, ANSI_UNDERLINE, opt->key, opt->label, ANSI_RESET);
        } else {
            /* Key in blue bold, label in white, reset at end */
            printf("  %s%s%s.%s %s%s%s", ANSI_BLUE, ANSI_BOLD, opt->key, ANSI_RESET, ANSI_WHITE, opt->label, ANSI_RESET);
        }
    } else {
        if (is_selected) {
            console_set_color(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
            printf("> %s. %s", opt->key, opt->label);
            console_set_color(g_default_console_attr);
        } else {
            console_set_color(FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
            printf("  %s.", opt->key);
            console_set_color(g_default_console_attr);
            printf(" %s", opt->label);
        }
    }

    if (append_newline) {
        printf("\n");
    }
}

static void redraw_menu_interaction_block(
    const MenuDef *menu,
    const MenuOption *const *visible_opts,
    int visible_count,
    int selected_index,
    const char *typed,
    const short option_rows[MAX_OPTIONS_PER_MENU]
) {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    const char *prompt_text = get_menu_prompt_for_display(menu);
    int i;

    if (!menu || !visible_opts || !option_rows || visible_count <= 0 || !hOut || hOut == INVALID_HANDLE_VALUE) {
        return;
    }
    if (!GetConsoleScreenBufferInfo(hOut, &csbi)) {
        return;
    }

    for (i = 0; i < visible_count; ++i) {
        COORD pos;
        DWORD written;
        int width;

        if (option_rows[i] < 0) {
            continue;
        }

        pos.X = 0;
        pos.Y = option_rows[i];
        SetConsoleCursorPosition(hOut, pos);

        width = (int)csbi.dwSize.X;
        if (width <= 0) {
            width = 120;
        }

        if (g_vt_enabled) {
            printf("\x1b[2K");
        } else {
            FillConsoleOutputCharacterA(hOut, ' ', (DWORD)width, pos, &written);
            FillConsoleOutputAttribute(hOut, g_default_console_attr, (DWORD)width, pos, &written);
            SetConsoleCursorPosition(hOut, pos);
        }

        print_menu_option_line(visible_opts[i], i == selected_index, 0);
    }

    {
        short prompt_row = (short)(option_rows[visible_count - 1] + 1);
        COORD pos;
        DWORD written;
        int width = (int)csbi.dwSize.X;

        if (width <= 0) {
            width = 120;
        }
        if (prompt_row < 0) {
            prompt_row = 0;
        }

        pos.X = 0;
        pos.Y = prompt_row;
        SetConsoleCursorPosition(hOut, pos);

        if (g_vt_enabled) {
            printf("\x1b[2K");
        } else {
            FillConsoleOutputCharacterA(hOut, ' ', (DWORD)width, pos, &written);
            FillConsoleOutputAttribute(hOut, g_default_console_attr, (DWORD)width, pos, &written);
            SetConsoleCursorPosition(hOut, pos);
        }

        if (g_vt_enabled) {
            if (prompt_text[0]) {
                printf("%s%s%s%s", ANSI_GREEN, ANSI_BOLD, prompt_text, ANSI_RESET);
            }
        } else {
            if (prompt_text[0]) {
                console_set_color(FOREGROUND_GREEN | FOREGROUND_INTENSITY);
                printf("%s", prompt_text);
                console_set_color(g_default_console_attr);
            }
        }

        if (typed && typed[0]) {
            printf("%s", typed);
        }
    }

    fflush(stdout);
}

static void log_with_tag(const char *tag, WORD attr, const char *ansi_color, const char *fmt, va_list ap) {
    if (g_vt_enabled) {
        printf("%s[%s]%s ", ansi_color, tag, ANSI_RESET);
        vprintf(fmt, ap);
        printf("\n");
    } else {
        console_set_color(attr);
        printf("[%s] ", tag);
        console_set_color(g_default_console_attr);
        vprintf(fmt, ap);
        printf("\n");
    }
}

static void log_info(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    log_with_tag("信息", FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY, ANSI_INFO, fmt, ap);
    va_end(ap);
}

static void log_warn(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    log_with_tag("警告", FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY, ANSI_ORANGE, fmt, ap);
    va_end(ap);
}

static void log_error(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    log_with_tag("错误", FOREGROUND_RED | FOREGROUND_INTENSITY, ANSI_RED, fmt, ap);
    va_end(ap);
}

static void console_set_color(WORD attr) {
    HANDLE h;
    h = GetStdHandle(STD_OUTPUT_HANDLE);
    if (!h || h == INVALID_HANDLE_VALUE) {
        return;
    }
    SetConsoleTextAttribute(h, attr);
}

static void setup_console_encoding_and_vt(void) {
    HANDLE hOut;
    HANDLE hIn;
    DWORD mode;

    SetConsoleCP(65001);
    SetConsoleOutputCP(65001);

    hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut && hOut != INVALID_HANDLE_VALUE) {
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        if (GetConsoleScreenBufferInfo(hOut, &csbi)) {
            g_default_console_attr = csbi.wAttributes;
        }
        if (GetConsoleMode(hOut, &mode)) {
            DWORD new_mode = mode | ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
#ifdef DISABLE_NEWLINE_AUTO_RETURN
            new_mode |= DISABLE_NEWLINE_AUTO_RETURN;
#endif
            if (SetConsoleMode(hOut, new_mode)) {
                g_vt_enabled = 1;
                printf("\x1b[0m");
            }
            else {
                /* Allow forcing VT/ANSI output even if SetConsoleMode failed.
                 * Useful when running under terminals that already understand
                 * ANSI escapes but SetConsoleMode cannot be set (container,
                 * remote, or non-standard hosts). */
                const char *force_vt = getenv("ATB_FORCE_VT_BYTES");
                const char *force_vt_out = getenv("ATB_FORCE_VT_OUTPUT");
                if ((force_vt && force_vt[0] && strcmp(force_vt, "0") != 0) ||
                    (force_vt_out && force_vt_out[0] && strcmp(force_vt_out, "0") != 0)) {
                    g_vt_enabled = 1;
                    printf("\x1b[0m");
                }
            }
        }
    }

    hIn = GetStdHandle(STD_INPUT_HANDLE);
    if (hIn && hIn != INVALID_HANDLE_VALUE && GetConsoleMode(hIn, &mode)) {
        DWORD new_mode = mode;
        new_mode |= ENABLE_EXTENDED_FLAGS;
        new_mode &= ~ENABLE_QUICK_EDIT_MODE;
        new_mode |= ENABLE_MOUSE_INPUT;
        new_mode |= ENABLE_WINDOW_INPUT;
    #ifdef ENABLE_VIRTUAL_TERMINAL_INPUT
        new_mode &= ~ENABLE_VIRTUAL_TERMINAL_INPUT;
    #endif
        SetConsoleMode(hIn, new_mode);
    }
}

static int file_exists(const char *path) {
    return path && (_access(path, 0) == 0);
}

static int dir_exists(const char *path) {
    DWORD attr;
    if (!path) {
        return 0;
    }
    attr = GetFileAttributesA(path);
    if (attr == INVALID_FILE_ATTRIBUTES) {
        return 0;
    }
    return (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

static int str_eq_icase(const char *a, const char *b) {
    if (!a || !b) {
        return 0;
    }
    return _stricmp(a, b) == 0;
}

static char *trim_inplace(char *s) {
    char *end;
    if (!s) {
        return s;
    }
    while (*s && isspace((unsigned char)*s)) {
        ++s;
    }
    if (*s == '\0') {
        return s;
    }
    end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) {
        *end = '\0';
        --end;
    }
    return s;
}

static int run_command(const char *cmd) {
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    DWORD exit_code = 0;
    BOOL created;
    char command_line[8192];

    if (!cmd) {
        return -1;
    }

    snprintf(command_line, sizeof(command_line), "cmd.exe /d /v:on /c %s", cmd);

    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));
    si.cb = sizeof(si);

    created = CreateProcessA(NULL, command_line, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi);
    if (!created) {
        return -1;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return (int)exit_code;
}

static bool ps_is_alive(PersistentShell *ps) {
    DWORD wait;
    if (!ps || !ps->hChildProcess) {
        return false;
    }
    wait = WaitForSingleObject(ps->hChildProcess, 0);
    return wait == WAIT_TIMEOUT;
}

static int run_via_shell(PersistentShell *ps, const char *cmd, int timeout_ms) {
    if (ps && ps_is_alive(ps)) {
        return ps_run_command(ps, cmd, timeout_ms);
    }
    return run_command(cmd);
}

static void run_if_present(PersistentShell *ps, const char *base_name) {
    char path[MAX_PATH];
    char cmd[MAX_COMMAND_LEN];
    const char *dot;

    if (!base_name || !base_name[0]) {
        return;
    }

    if (file_exists(base_name)) {
        dot = strrchr(base_name, '.');
        if (dot && (str_eq_icase(dot, ".bat") || str_eq_icase(dot, ".cmd"))) {
            snprintf(cmd, sizeof(cmd), "call %s", base_name);
        } else {
            snprintf(cmd, sizeof(cmd), "%s", base_name);
        }
        run_via_shell(ps, cmd, 120000);
        return;
    }

    snprintf(path, sizeof(path), "%s.bat", base_name);
    if (file_exists(path)) {
        snprintf(cmd, sizeof(cmd), "call %s", path);
        run_via_shell(ps, cmd, 120000);
        return;
    }

    snprintf(path, sizeof(path), "%s.cmd", base_name);
    if (file_exists(path)) {
        snprintf(cmd, sizeof(cmd), "call %s", path);
        run_via_shell(ps, cmd, 120000);
        return;
    }

    snprintf(path, sizeof(path), "%s.exe", base_name);
    if (file_exists(path)) {
        run_via_shell(ps, path, 120000);
    }
}

static void set_console_title(const char *title) {
    if (title) {
        SetConsoleTitleA(title);
    }
}

static void set_pathext_extra(void) {
    char *orig = getenv("PATHEXT");
    char merged[4096];
    char upper[4096];

    if (orig && orig[0]) {
        snprintf(merged, sizeof(merged), "%s", orig);
    } else {
        merged[0] = '\0';
    }

    snprintf(upper, sizeof(upper), "%s", merged);
    CharUpperA(upper);

    {
        const char *cursor = PATHEXT_EXTRA;
        while (*cursor) {
            char token[32];
            size_t tlen = 0;
            if (*cursor == ';') {
                ++cursor;
                continue;
            }
            while (*cursor && *cursor != ';' && tlen + 1 < sizeof(token)) {
                token[tlen++] = *cursor;
                ++cursor;
            }
            token[tlen] = '\0';
            if (tlen > 0) {
                char token_upper[32];
                snprintf(token_upper, sizeof(token_upper), "%s", token);
                CharUpperA(token_upper);
                if (!strstr(upper, token_upper)) {
                    if (merged[0] && merged[strlen(merged) - 1] != ';') {
                        strncat(merged, ";", sizeof(merged) - strlen(merged) - 1);
                    }
                    strncat(merged, token, sizeof(merged) - strlen(merged) - 1);
                    if (upper[0] && upper[strlen(upper) - 1] != ';') {
                        strncat(upper, ";", sizeof(upper) - strlen(upper) - 1);
                    }
                    strncat(upper, token_upper, sizeof(upper) - strlen(upper) - 1);
                }
            }
        }
    }

    SetEnvironmentVariableA("PATHEXT", merged);
}

static bool check_adb_server(void) {
    WSADATA wsa;
    SOCKET s;
    struct sockaddr_in addr;
    int res;

    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        return false;
    }

    s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) {
        WSACleanup();
        return false;
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(5037);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    {
        u_long mode = 1;
        ioctlsocket(s, FIONBIO, &mode);
    }

    res = connect(s, (struct sockaddr *)&addr, sizeof(addr));
    if (res == SOCKET_ERROR) {
        int err = WSAGetLastError();
        if (err != WSAEWOULDBLOCK && err != WSAEINPROGRESS) {
            closesocket(s);
            WSACleanup();
            return false;
        }
        {
            fd_set wf;
            struct timeval tv;
            FD_ZERO(&wf);
            FD_SET(s, &wf);
            tv.tv_sec = 0;
            tv.tv_usec = 300000;
            res = select(0, NULL, &wf, NULL, &tv);
            if (res <= 0) {
                closesocket(s);
                WSACleanup();
                return false;
            }
        }
    }

    closesocket(s);
    WSACleanup();
    return true;
}

static DWORD WINAPI ps_output_thread(LPVOID param) {
    PersistentShell *ps = (PersistentShell *)param;
    char buf[4096];
    DWORD bytes_read = 0;
    char accum[32768];
    size_t accum_len = 0;
    HANDLE hstdout = GetStdHandle(STD_OUTPUT_HANDLE);

    accum[0] = '\0';

    while (ps->running) {
        BOOL ok = ReadFile(ps->hChildStdOutRd, buf, (DWORD)sizeof(buf) - 1, &bytes_read, NULL);
        if (!ok || bytes_read == 0) {
            DWORD err = GetLastError();
            if (err == ERROR_BROKEN_PIPE || err == ERROR_INVALID_HANDLE) {
                break;
            }
            Sleep(8);
            continue;
        }

        buf[bytes_read] = '\0';

        {
            DWORD written = 0;
            /* Only write child output to the visible console when allowed.
             * We still accumulate the data to detect command markers even
             * while suppression is active. */
            if (ps->output_allowed) {
                if (hstdout && hstdout != INVALID_HANDLE_VALUE) {
                    WriteFile(hstdout, buf, bytes_read, &written, NULL);
                }
            }
        }

        if (accum_len + bytes_read >= sizeof(accum) - 1) {
            size_t keep = 16000;
            if (keep > accum_len) {
                keep = accum_len;
            }
            memmove(accum, accum + (accum_len - keep), keep);
            accum_len = keep;
            accum[accum_len] = '\0';
        }

        memcpy(accum + accum_len, buf, bytes_read);
        accum_len += bytes_read;
        accum[accum_len] = '\0';

        if (strstr(accum, "按任意键") || strstr(accum, "Press any key") || strstr(accum, "press any key")) {
            const char nl = '\n';
            DWORD w = 0;
            WriteFile(ps->hChildStdInWr, &nl, 1, &w, NULL);
            accum_len = 0;
            accum[0] = '\0';
        }

        EnterCriticalSection(&ps->cs);
        if (ps->marker[0]) {
            char *m = strstr(accum, ps->marker);
            if (m) {
                int code = 0;
                char *s = m + strlen(ps->marker);
                while (*s == ' ' || *s == '\t') {
                    ++s;
                }
                if (*s) {
                    code = atoi(s);
                }
                ps->last_exit_code = code;
                ps->marker[0] = '\0';
                SetEvent(ps->hMarkerEvent);
            }
        }
        LeaveCriticalSection(&ps->cs);
    }
    return 0;
}

static PersistentShell *ps_create(void) {
    SECURITY_ATTRIBUTES sa;
    HANDLE child_stdout_rd = NULL, child_stdout_wr = NULL;
    HANDLE child_stdin_rd = NULL, child_stdin_wr = NULL;
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    BOOL created;
    PersistentShell *ps;
    char cmdline[] = "cmd.exe /d /q /v:on";

    sa.nLength = sizeof(sa);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = TRUE;

    if (!CreatePipe(&child_stdout_rd, &child_stdout_wr, &sa, 0)) {
        return NULL;
    }
    if (!SetHandleInformation(child_stdout_rd, HANDLE_FLAG_INHERIT, 0)) {
        SAFE_CLOSE(child_stdout_rd);
        SAFE_CLOSE(child_stdout_wr);
        return NULL;
    }

    if (!CreatePipe(&child_stdin_rd, &child_stdin_wr, &sa, 0)) {
        SAFE_CLOSE(child_stdout_rd);
        SAFE_CLOSE(child_stdout_wr);
        return NULL;
    }
    if (!SetHandleInformation(child_stdin_wr, HANDLE_FLAG_INHERIT, 0)) {
        SAFE_CLOSE(child_stdout_rd);
        SAFE_CLOSE(child_stdout_wr);
        SAFE_CLOSE(child_stdin_rd);
        SAFE_CLOSE(child_stdin_wr);
        return NULL;
    }

    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));
    si.cb = sizeof(si);
    si.dwFlags |= STARTF_USESTDHANDLES;
    si.hStdInput = child_stdin_rd;
    si.hStdOutput = child_stdout_wr;
    si.hStdError = child_stdout_wr;

    created = CreateProcessA(NULL, cmdline, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    if (!created) {
        SAFE_CLOSE(child_stdout_rd);
        SAFE_CLOSE(child_stdout_wr);
        SAFE_CLOSE(child_stdin_rd);
        SAFE_CLOSE(child_stdin_wr);
        return NULL;
    }

    SAFE_CLOSE(child_stdout_wr);
    SAFE_CLOSE(child_stdin_rd);
    CloseHandle(pi.hThread);

    ps = (PersistentShell *)calloc(1, sizeof(PersistentShell));
    if (!ps) {
        TerminateProcess(pi.hProcess, 0);
        CloseHandle(pi.hProcess);
        SAFE_CLOSE(child_stdout_rd);
        SAFE_CLOSE(child_stdin_wr);
        return NULL;
    }

    ps->hChildProcess = pi.hProcess;
    ps->hChildStdInWr = child_stdin_wr;
    ps->hChildStdOutRd = child_stdout_rd;
    ps->hMarkerEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
    InitializeCriticalSection(&ps->cs);
    ps->last_exit_code = 0;
    ps->seq = 0;
    ps->running = true;
    /* Keep child output suppressed at startup so cmd banner/prompt lines
     * are not printed unexpectedly. Output will be enabled when a real
     * command is executed via ps_run_command(). */
    ps->output_allowed = 0;
    ps->marker[0] = '\0';
    ps->hOutputThread = CreateThread(NULL, 0, ps_output_thread, ps, 0, NULL);

    return ps;
}

static void ps_destroy(PersistentShell *ps) {
    if (!ps) {
        return;
    }

    ps->running = false;

    SAFE_CLOSE(ps->hChildStdOutRd);
    SAFE_CLOSE(ps->hChildStdInWr);

    if (ps->hOutputThread) {
        WaitForSingleObject(ps->hOutputThread, 500);
        SAFE_CLOSE(ps->hOutputThread);
    }
    if (ps->hChildProcess) {
        TerminateProcess(ps->hChildProcess, 0);
        SAFE_CLOSE(ps->hChildProcess);
    }
    if (ps->hMarkerEvent) {
        SAFE_CLOSE(ps->hMarkerEvent);
    }
    DeleteCriticalSection(&ps->cs);
    free(ps);
}

static void ps_allow_output(PersistentShell *ps) {
    if (!ps) return;
    EnterCriticalSection(&ps->cs);
    ps->output_allowed = 1;
    LeaveCriticalSection(&ps->cs);
}

static int ps_run_command(PersistentShell *ps, const char *cmd, int timeout_ms) {
    char line[8192];
    DWORD written = 0;
    DWORD wait_rc;

    if (!ps || !cmd) {
        return -1;
    }
    if (!ps_is_alive(ps)) {
        return -1;
    }

    EnterCriticalSection(&ps->cs);
    /* Enable visible output only when running an actual command. This avoids
     * showing startup cmd banner/prompt during idle menu rendering. */
    ps->output_allowed = 1;
    ps->seq++;
    snprintf(ps->marker, sizeof(ps->marker), "__ATB_DONE__%u__", ps->seq);
    ResetEvent(ps->hMarkerEvent);
    ps->last_exit_code = -1;

    snprintf(line, sizeof(line), "@echo off\r\n%s\r\necho %s !ERRORLEVEL!\r\n", cmd, ps->marker);
    WriteFile(ps->hChildStdInWr, line, (DWORD)strlen(line), &written, NULL);
    LeaveCriticalSection(&ps->cs);

    wait_rc = WaitForSingleObject(ps->hMarkerEvent, timeout_ms > 0 ? (DWORD)timeout_ms : INFINITE);
    if (wait_rc == WAIT_OBJECT_0) {
        return ps->last_exit_code;
    }
    return -2;
}

static bool http_get_w(const wchar_t *url, char *out_buf, size_t out_size, int timeout_ms) {
    HINTERNET h = NULL;
    HINTERNET h_file = NULL;
    DWORD read = 0;
    size_t total = 0;
    char chunk[4096];

    if (!url || !out_buf || out_size == 0) {
        return false;
    }

    h = InternetOpenW(L"AllToolBox/1.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!h) {
        return false;
    }

    InternetSetOptionW(h, INTERNET_OPTION_CONNECT_TIMEOUT, &timeout_ms, sizeof(timeout_ms));
    InternetSetOptionW(h, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeout_ms, sizeof(timeout_ms));

    h_file = InternetOpenUrlW(h, url, NULL, 0, INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0);
    if (!h_file) {
        InternetCloseHandle(h);
        return false;
    }

    while (InternetReadFile(h_file, chunk, sizeof(chunk), &read) && read > 0) {
        if (total + read >= out_size - 1) {
            size_t room = out_size - 1 - total;
            memcpy(out_buf + total, chunk, room);
            total += room;
            break;
        }
        memcpy(out_buf + total, chunk, read);
        total += read;
    }
    out_buf[total] = '\0';

    InternetCloseHandle(h_file);
    InternetCloseHandle(h);
    return true;
}

static void get_exe_dir(char *out_dir, size_t out_size) {
    char path[MAX_PATH];
    char *slash;
    if (!out_dir || out_size == 0) {
        return;
    }
    out_dir[0] = '\0';
    if (!GetModuleFileNameA(NULL, path, sizeof(path))) {
        return;
    }
    slash = strrchr(path, '\\');
    if (slash) {
        *slash = '\0';
    }
    snprintf(out_dir, out_size, "%s", path);
}

static int build_path_join(char *out, size_t out_size, const char *a, const char *b) {
    size_t len;
    if (!out || !a || !b || out_size == 0) {
        return 0;
    }
    len = strlen(a);
    if (len > 0 && (a[len - 1] == '\\' || a[len - 1] == '/')) {
        snprintf(out, out_size, "%s%s", a, b);
    } else {
        snprintf(out, out_size, "%s\\%s", a, b);
    }
    return 1;
}

static char *read_text_file(const char *path, size_t *out_size) {
    FILE *f;
    long len;
    size_t read_len;
    char *buf;

    if (!path) {
        return NULL;
    }

    f = fopen(path, "rb");
    if (!f) {
        return NULL;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    len = ftell(f);
    if (len < 0) {
        fclose(f);
        return NULL;
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return NULL;
    }

    buf = (char *)malloc((size_t)len + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    read_len = fread(buf, 1, (size_t)len, f);
    fclose(f);
    buf[read_len] = '\0';
    if (out_size) {
        *out_size = read_len;
    }
    return buf;
}

static const char *json_skip_ws(const char *p, const char *end) {
    while (p < end && isspace((unsigned char)*p)) {
        ++p;
    }
    return p;
}

static const char *json_find_matching(const char *open, char open_ch, char close_ch, const char *end) {
    int depth = 0;
    int in_string = 0;
    int escape = 0;
    const char *p;

    for (p = open; p < end && *p; ++p) {
        char c = *p;
        if (in_string) {
            if (escape) {
                escape = 0;
            } else if (c == '\\') {
                escape = 1;
            } else if (c == '"') {
                in_string = 0;
            }
            continue;
        }

        if (c == '"') {
            in_string = 1;
            continue;
        }
        if (c == open_ch) {
            depth++;
            continue;
        }
        if (c == close_ch) {
            depth--;
            if (depth == 0) {
                return p;
            }
        }
    }
    return NULL;
}

static int json_parse_string(const char *value, const char *end, char *out, size_t out_size, const char **out_next) {
    const char *p;
    size_t used = 0;
    if (!value || !out || out_size == 0) {
        return 0;
    }
    p = json_skip_ws(value, end);
    if (p >= end || *p != '"') {
        return 0;
    }
    ++p;

    while (p < end && *p) {
        char c = *p++;
        if (c == '"') {
            break;
        }
        if (c == '\\' && p < end) {
            char esc = *p++;
            switch (esc) {
                case 'n': c = '\n'; break;
                case 'r': c = '\r'; break;
                case 't': c = '\t'; break;
                case '\\': c = '\\'; break;
                case '"': c = '"'; break;
                case '/': c = '/'; break;
                default: c = esc; break;
            }
        }
        if (used + 1 < out_size) {
            out[used++] = c;
        }
    }
    out[used] = '\0';
    if (out_next) {
        *out_next = p;
    }
    return 1;
}

static const char *json_find_key(const char *obj_start, const char *obj_end, const char *key) {
    char pattern[128];
    const char *p;
    size_t key_len;
    size_t pattern_len;

    if (!obj_start || !obj_end || !key) {
        return NULL;
    }

    key_len = strlen(key);
    if (key_len + 3 >= sizeof(pattern)) {
        return NULL;
    }
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    pattern_len = strlen(pattern);

    p = obj_start;
    while (p < obj_end) {
        const char *hit = strstr(p, pattern);
        if (!hit || hit >= obj_end) {
            return NULL;
        }
        {
            const char *q = hit + pattern_len;
            q = json_skip_ws(q, obj_end);
            if (q < obj_end && *q == ':') {
                return q + 1;
            }
        }
        p = hit + 1;
    }
    return NULL;
}

static int json_get_string_field(const char *obj_start, const char *obj_end, const char *key, char *out, size_t out_size) {
    const char *value = json_find_key(obj_start, obj_end, key);
    if (!value) {
        return 0;
    }
    return json_parse_string(value, obj_end, out, out_size, NULL);
}

static int json_get_int_field(const char *obj_start, const char *obj_end, const char *key, int *out) {
    const char *value = json_find_key(obj_start, obj_end, key);
    char buf[64];
    const char *p;
    size_t used = 0;

    if (!value || !out) {
        return 0;
    }

    p = json_skip_ws(value, obj_end);
    while (p < obj_end && used + 1 < sizeof(buf) && (*p == '-' || isdigit((unsigned char)*p))) {
        buf[used++] = *p;
        ++p;
    }
    if (used == 0) {
        return 0;
    }
    buf[used] = '\0';
    *out = atoi(buf);
    return 1;
}

static int json_get_bool_field(const char *obj_start, const char *obj_end, const char *key, int *out) {
    const char *value = json_find_key(obj_start, obj_end, key);
    const char *p;
    if (!value || !out) {
        return 0;
    }
    p = json_skip_ws(value, obj_end);
    if ((obj_end - p) >= 4 && strncmp(p, "true", 4) == 0) {
        *out = 1;
        return 1;
    }
    if ((obj_end - p) >= 5 && strncmp(p, "false", 5) == 0) {
        *out = 0;
        return 1;
    }
    return 0;
}

static int parse_option_object(const char *obj_start, const char *obj_end, MenuOption *opt) {
    if (!obj_start || !obj_end || !opt) {
        return 0;
    }

    memset(opt, 0, sizeof(*opt));
    opt->timeout_ms = 120000;
    opt->requires_debug = 0;
    opt->pause_after = 0;

    if (!json_get_string_field(obj_start, obj_end, "key", opt->key, sizeof(opt->key))) {
        return 0;
    }
    /* optional 'value' (fallback to key if absent) */
    json_get_string_field(obj_start, obj_end, "value", opt->value, sizeof(opt->value));
    if (!json_get_string_field(obj_start, obj_end, "label", opt->label, sizeof(opt->label))) {
        return 0;
    }
    if (!json_get_string_field(obj_start, obj_end, "action", opt->action, sizeof(opt->action))) {
        return 0;
    }

    json_get_string_field(obj_start, obj_end, "target", opt->target, sizeof(opt->target));
    json_get_string_field(obj_start, obj_end, "command", opt->command, sizeof(opt->command));
    json_get_int_field(obj_start, obj_end, "timeout_ms", &opt->timeout_ms);
    json_get_bool_field(obj_start, obj_end, "requires_debug", &opt->requires_debug);
    json_get_bool_field(obj_start, obj_end, "pause_after", &opt->pause_after);

    return 1;
}

static int parse_menu_object(const char *obj_start, const char *obj_end, MenuDef *menu) {
    const char *options_value;
    const char *arr_start;
    const char *arr_end;
    const char *p;

    if (!obj_start || !obj_end || !menu) {
        return 0;
    }

    memset(menu, 0, sizeof(*menu));
    snprintf(menu->prompt, sizeof(menu->prompt), "输入: ");

    if (!json_get_string_field(obj_start, obj_end, "id", menu->id, sizeof(menu->id))) {
        return 0;
    }
    json_get_string_field(obj_start, obj_end, "title", menu->title, sizeof(menu->title));
    json_get_string_field(obj_start, obj_end, "prompt", menu->prompt, sizeof(menu->prompt));
    json_get_bool_field(obj_start, obj_end, "show_logo", &menu->show_logo);
    json_get_bool_field(obj_start, obj_end, "allow_hotkey_debug", &menu->allow_hotkey_debug);
    json_get_bool_field(obj_start, obj_end, "multi_select", &menu->multi_select);
    json_get_bool_field(obj_start, obj_end, "animate_in", &menu->animate_in);
    json_get_int_field(obj_start, obj_end, "max_height", &menu->max_height);

    options_value = json_find_key(obj_start, obj_end, "options");
    if (!options_value) {
        return 0;
    }
    arr_start = json_skip_ws(options_value, obj_end);
    if (arr_start >= obj_end || *arr_start != '[') {
        return 0;
    }
    arr_end = json_find_matching(arr_start, '[', ']', obj_end);
    if (!arr_end) {
        return 0;
    }

    p = arr_start + 1;
    while (p < arr_end) {
        const char *item_start;
        const char *item_end;

        p = json_skip_ws(p, arr_end);
        if (p >= arr_end) {
            break;
        }
        if (*p != '{') {
            ++p;
            continue;
        }

        item_start = p;
        item_end = json_find_matching(item_start, '{', '}', arr_end + 1);
        if (!item_end) {
            return 0;
        }

        if (menu->option_count < MAX_OPTIONS_PER_MENU) {
            if (parse_option_object(item_start, item_end + 1, &menu->options[menu->option_count])) {
                menu->option_count++;
            }
        }
        p = item_end + 1;
    }

    return 1;
}

static int load_menu_config(const char *json_path, MenuConfig *config) {
    char *json;
    size_t size = 0;
    const char *root_start;
    const char *root_end;
    const char *menus_value;
    const char *arr_start;
    const char *arr_end;
    const char *p;

    if (!json_path || !config) {
        return 0;
    }

    memset(config, 0, sizeof(*config));

    json = read_text_file(json_path, &size);
    if (!json || size == 0) {
        if (json) {
            free(json);
        }
        return 0;
    }

    root_start = json_skip_ws(json, json + size);
    if (*root_start != '{') {
        free(json);
        return 0;
    }
    root_end = json_find_matching(root_start, '{', '}', json + size);
    if (!root_end) {
        free(json);
        return 0;
    }

    menus_value = json_find_key(root_start, root_end + 1, "menus");
    if (!menus_value) {
        free(json);
        return 0;
    }

    arr_start = json_skip_ws(menus_value, root_end + 1);
    if (*arr_start != '[') {
        free(json);
        return 0;
    }
    arr_end = json_find_matching(arr_start, '[', ']', root_end + 1);
    if (!arr_end) {
        free(json);
        return 0;
    }

    p = arr_start + 1;
    while (p < arr_end) {
        const char *item_start;
        const char *item_end;
        p = json_skip_ws(p, arr_end);
        if (p >= arr_end) {
            break;
        }
        if (*p != '{') {
            ++p;
            continue;
        }
        item_start = p;
        item_end = json_find_matching(item_start, '{', '}', arr_end + 1);
        if (!item_end) {
            free(json);
            return 0;
        }

        if (config->menu_count < MAX_MENUS) {
            if (parse_menu_object(item_start, item_end + 1, &config->menus[config->menu_count])) {
                config->menu_count++;
            }
        }
        p = item_end + 1;
    }

    free(json);
    return config->menu_count > 0;
}

static MenuDef *find_menu(MenuConfig *config, const char *id) {
    int i;
    if (!config || !id) {
        return NULL;
    }
    for (i = 0; i < config->menu_count; ++i) {
        if (str_eq_icase(config->menus[i].id, id)) {
            return &config->menus[i];
        }
    }
    return NULL;
}

static int resolve_menu_json_path(char *out_path, size_t out_size) {
    char exe_dir[MAX_PATH];
    const char *candidates[] = {
        ".\\src\\menu\\start\\menus.json",
        ".\\menu\\start\\menus.json",
        "..\\src\\menu\\start\\menus.json",
        "..\\..\\src\\menu\\start\\menus.json",
        "..\\..\\..\\src\\menu\\start\\menus.json"
    };
    char joined[MAX_PATH];
    int i;

    if (!out_path || out_size == 0) {
        return 0;
    }
    out_path[0] = '\0';

    for (i = 0; i < (int)(sizeof(candidates) / sizeof(candidates[0])); ++i) {
        if (file_exists(candidates[i])) {
            snprintf(out_path, out_size, "%s", candidates[i]);
            return 1;
        }
    }

    get_exe_dir(exe_dir, sizeof(exe_dir));
    if (!exe_dir[0]) {
        return 0;
    }

    for (i = 0; i < (int)(sizeof(candidates) / sizeof(candidates[0])); ++i) {
        if (build_path_join(joined, sizeof(joined), exe_dir, candidates[i])) {
            if (file_exists(joined)) {
                snprintf(out_path, out_size, "%s", joined);
                return 1;
            }
        }
    }

    return 0;
}

static int collect_mod_names(char names[][MAX_LABEL_LEN], int max_count) {
    WIN32_FIND_DATAA fdat;
    HANDLE h;
    int count = 0;

    h = FindFirstFileA("mod\\*", &fdat);
    if (h == INVALID_HANDLE_VALUE) {
        return 0;
    }

    do {
        if (!(fdat.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            continue;
        }
        if (strcmp(fdat.cFileName, ".") == 0 || strcmp(fdat.cFileName, "..") == 0) {
            continue;
        }
        if (count < max_count) {
            snprintf(names[count], MAX_LABEL_LEN, "%s", fdat.cFileName);
            count++;
        }
    } while (FindNextFileA(h, &fdat));

    FindClose(h);
    return count;
}

static void print_main_mods_summary(void) {
    char names[128][MAX_LABEL_LEN];
    int count;
    int i;

    if (!dir_exists("mod")) {
        if (file_exists("mod")) {
            remove("mod");
        }
        _mkdir("mod");
    }

    count = collect_mod_names(names, 128);
    if (count <= 0) {
        log_info("已加载扩展列表：未加载任何扩展");
        return;
    }

    log_info("已加载扩展列表：");
    for (i = 0; i < count; ++i) {
        if (g_vt_enabled) {
            printf("%s%s%d.%s %s%s\n", ANSI_BOLD, ANSI_BLUE, i + 1, ANSI_RESET, names[i], ANSI_RESET);
        } else {
            console_set_color(FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
            printf("%d.", i + 1);
            console_set_color(g_default_console_attr);
            printf(" %s\n", names[i]);
        }
    }
}

static void show_about_screen(PersistentShell *ps) {
    if (g_vt_enabled) {
        printf("%s--------------------------------------------------------------------%s\n", ANSI_YELLOW, ANSI_RESET);
    } else {
        printf("--------------------------------------------------------------------\n");
    }
    log_info("本脚本由快乐小公爵236等开发者制作");
    run_if_present(ps, "thank.bat");
    log_info("工具官网：https://atb.xgj.qzz.io");
    log_info("作者QQ：3247039462");
    log_info("工具箱交流与反馈QQ群：907491503");
    log_info("作者哔哩哔哩账号：https://b23.tv/L54R5ZV");
    log_info("bug与建议反馈邮箱：ATBbug@xgj.qzz.io");
    run_if_present(ps, "uplog.bat");
    if (g_vt_enabled) {
        printf("%s--------------------------------------------------------------------%s\n", ANSI_YELLOW, ANSI_RESET);
    } else {
        printf("--------------------------------------------------------------------\n");
    }
    pause_any("单击此字符或按任意键继续返回上级菜单", 0);
}

static void show_doc_screen(void) {
    FILE *f = fopen("开发文档.txt", "r");
    if (!f) {
        log_error("未找到 开发文档.txt");
        return;
    }
    while (!feof(f)) {
        char line[1024];
        if (fgets(line, sizeof(line), f)) {
            fputs(line, stdout);
        }
    }
    fclose(f);
    pause_any("单击此字符或按任意键继续返回上级菜单", 0);
}

static void show_color_card(void) {
    log_info("BLACK");
    log_info("RED");
    log_info("GREEN");
    log_info("ORANGE");
    log_info("BLUE");
    log_info("MAGENTA");
    log_info("CYAN");
    log_info("WHITE");
    pause_any("单击此字符或按任意键继续", 0);
}

static void run_debug_sel(PersistentShell *ps) {
    run_via_shell(ps, "call sel file s .", 120000);
    pause_any("单击此字符或按任意键继续", 0);
    run_via_shell(ps, "call sel file m .", 120000);
    pause_any("单击此字符或按任意键继续", 0);
}

static void write_whoyou(const char *value) {
    FILE *f = fopen("whoyou.txt", "w");
    if (!f) {
        return;
    }
    fputs(value ? value : "2", f);
    fclose(f);
}

static int run_mod_installed_menu(PersistentShell *ps) {
    char names[128][MAX_LABEL_LEN];
    int count;
    int i;
    char input[64];

    count = collect_mod_names(names, 128);
    if (count <= 0) {
        log_warn("未发现任何扩展");
        Sleep(1500);
        return MENU_RESULT_CONTINUE;
    }

    while (1) {
        clear_console();
        printf("已加载扩展\n");
        printf("A. 返回上级菜单\n");
        for (i = 0; i < count; ++i) {
            printf("%d. %s\n", 11 + i, names[i]);
        }
        printf("输入: ");

        if (!fgets(input, sizeof(input), stdin)) {
            return MENU_RESULT_CONTINUE;
        }
        {
            char *s = trim_inplace(input);
            if (str_eq_icase(s, "A")) {
                return MENU_RESULT_CONTINUE;
            }
            {
                int idx = atoi(s) - 11;
                if (idx >= 0 && idx < count) {
                    char cmd[MAX_COMMAND_LEN];
                    snprintf(cmd, sizeof(cmd), "cd /d mod\\%s && call main.bat", names[idx]);
                    run_via_shell(ps, cmd, 120000);
                    return MENU_RESULT_CONTINUE;
                }
            }
            log_error("输入错误，请重新输入");
            Sleep(800);
        }
    }
}

static int execute_menu_action(PersistentShell *ps, MenuConfig *config, const MenuOption *opt) {
    if (!opt) {
        return MENU_RESULT_CONTINUE;
    }

    if (opt->requires_debug && !g_debug_mode) {
        log_error("当前为 Release 构建，DEBUG 功能已禁用");
        Sleep(1000);
        return MENU_RESULT_CONTINUE;
    }

    if (str_eq_icase(opt->action, "back")) {
        return MENU_RESULT_BACK;
    }
    if (str_eq_icase(opt->action, "exit")) {
        return MENU_RESULT_EXIT;
    }

    page_transition("正在切换", 0.25);

    if (str_eq_icase(opt->action, "submenu")) {
        if (opt->target[0]) {
            /* Show a clean transition into the submenu: clear screen, show
             * logo and the submenu title for visual consistency. */
            clear_console();
            /* Try to find the submenu definition to display its title. */
            {
                MenuDef *sub = find_menu(config, opt->target);
                /* Always attempt to show logo if present on disk. */
                run_if_present(ps, "logo");
                if (sub && sub->title[0]) {
                    if (g_vt_enabled) {
                        printf("%s%s%s%s\n", ANSI_INFO, ANSI_BOLD, sub->title, ANSI_RESET);
                    } else {
                        console_set_color(FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
                        printf("%s\n", sub->title);
                        console_set_color(g_default_console_attr);
                    }
                }
            }

            int r = run_menu_by_id(ps, config, opt->target);

            /* After returning from submenu, clear and show logo to avoid
             * residual output before repainting the parent menu. */
            clear_console();
            run_if_present(ps, "logo");

            if (r == MENU_RESULT_EXIT) {
                return MENU_RESULT_EXIT;
            }
        }
        return MENU_RESULT_CONTINUE;
    }
    if (str_eq_icase(opt->action, "command")) {
        if (opt->command[0]) {
            run_via_shell(ps, opt->command, opt->timeout_ms > 0 ? opt->timeout_ms : 120000);
        }
        if (opt->pause_after) {
            pause_any("单击此字符或按任意键继续返回上级菜单", 0);
        }
        return MENU_RESULT_CONTINUE;
    }
    if (str_eq_icase(opt->action, "openshell")) {
        clear_console();
        run_command("cmd.exe /k");
        return MENU_RESULT_CONTINUE;
    }
    if (str_eq_icase(opt->action, "about")) {
        clear_console();
        show_about_screen(ps);
        return MENU_RESULT_CONTINUE;
    }
    if (str_eq_icase(opt->action, "run-mod")) {
        return run_mod_installed_menu(ps);
    }
    if (str_eq_icase(opt->action, "show-doc")) {
        clear_console();
        show_doc_screen();
        return MENU_RESULT_CONTINUE;
    }
    if (str_eq_icase(opt->action, "color-card")) {
        clear_console();
        show_color_card();
        return MENU_RESULT_CONTINUE;
    }
    if (str_eq_icase(opt->action, "debug-sel")) {
        clear_console();
        run_debug_sel(ps);
        return MENU_RESULT_CONTINUE;
    }
    if (str_eq_icase(opt->action, "write-whoyou")) {
        if (opt->target[0]) {
            write_whoyou(opt->target);
        }
        return MENU_RESULT_CONTINUE;
    }
    if (str_eq_icase(opt->action, "allow-xtc")) {
        g_allow_xtc = 1;
        log_info("已允许使用部分一键root功能");
        Sleep(1000);
        return MENU_RESULT_CONTINUE;
    }
    if (str_eq_icase(opt->action, "not-implemented")) {
        log_warn("该功能正在迁移中，后续将补齐与 Python 版一致实现");
        pause_any("单击此字符或按任意键继续", 0);
        return MENU_RESULT_CONTINUE;
    }

    return MENU_RESULT_CONTINUE;
}

static int find_visible_option_index_by_key(const MenuOption *const *visible_opts, int visible_count, const char *key) {
    int i;
    if (!visible_opts || visible_count <= 0 || !key || !key[0]) {
        return -1;
    }
    for (i = 0; i < visible_count; ++i) {
        if (str_eq_icase(key, visible_opts[i]->key)) {
            return i;
        }
    }
    return -1;
}

static int resolve_click_index_from_row(const short option_rows[MAX_OPTIONS_PER_MENU], int visible_count, short prompt_row, short mouse_y) {
    int i;
    if (!option_rows || visible_count <= 0) {
        return -1;
    }
    for (i = 0; i < visible_count; ++i) {
        short start_row;
        short end_row;

        if (option_rows[i] < 0) {
            continue;
        }

        start_row = option_rows[i];
        if (i + 1 < visible_count && option_rows[i + 1] >= 0) {
            end_row = (short)(option_rows[i + 1] - 1);
        } else if (prompt_row >= 0) {
            end_row = (short)(prompt_row - 1);
        } else {
            end_row = start_row;
        }

        if (end_row < start_row) {
            end_row = start_row;
        }

        if (mouse_y >= start_row && mouse_y <= end_row) {
            return i;
        }
    }
    return -1;
}

static unsigned int menu_double_click_threshold_ms(void) {
    static int cached = -1;
    const char *raw;
    char *end = NULL;
    unsigned long v;

    if (cached >= 0) {
        return (unsigned int)cached;
    }

    raw = getenv("ATB_DOUBLE_CLICK_MS");
    v = 300;
    if (raw && raw[0]) {
        unsigned long parsed = strtoul(raw, &end, 10);
        if (end && *end == '\0' && parsed > 0) {
            v = parsed;
        }
    }
    if (v < 100) v = 100;
    if (v > 1000) v = 1000;
    cached = (int)v;
    return (unsigned int)cached;
}

static int menu_is_double_click_local(int last_index, ULONGLONG last_tick, int index, ULONGLONG now_tick, unsigned int threshold_ms) {
    if (index < 0 || last_index != index) {
        return 0;
    }
    if (now_tick < last_tick) {
        return 0;
    }
    return (now_tick - last_tick) <= (ULONGLONG)threshold_ms;
}

static int wait_menu_selection(
    PersistentShell *ps,
    const MenuDef *menu,
    const MenuOption *const *visible_opts,
    int visible_count,
    int *selected_index,
    const MenuOption **selected_opt,
    short option_rows[MAX_OPTIONS_PER_MENU]
) {
    enum {
        WAIT_SELECTED = 1,
        WAIT_OPEN_DEBUG = 2,
        WAIT_INVALID = 3
    };
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    char typed[32];
    ULONGLONG last_click_tick = 0;
    int last_click_index = -1;
    unsigned int double_click_ms = menu_double_click_threshold_ms();
    int need_redraw = 1;
    int mouse_pressed = 0;
    int mouse_pressed_index = -1;
    int mouse_diag = -1;

    /* Debugging for menu input: set ATB_MENU_DEBUG=1 to enable verbose prints */
    /* Enable menu debug by default during investigation. */
    static int menu_dbg_cached = 1;
    int menu_dbg = menu_dbg_cached;
    if (menu_dbg) {
        DWORD in_mode_v = 0, out_mode_v = 0;
        GetConsoleMode(hIn, &in_mode_v);
        GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), &out_mode_v);
        menu_debug_log("[MENU_DBG] enter wait_menu_selection g_vt_enabled=%d in_mode=0x%08x out_mode=0x%08x selected=%d dbl_ms=%u\n",
            g_vt_enabled, (unsigned)in_mode_v, (unsigned)out_mode_v, *selected_index, (unsigned)double_click_ms);
    }

    if (!menu || !visible_opts || visible_count <= 0 || !selected_index || !selected_opt || !option_rows) {
        return WAIT_INVALID;
    }
    *selected_opt = NULL;
    typed[0] = '\0';

    while (1) {
        INPUT_RECORD ir;
        DWORD read_count = 0;

        if (need_redraw) {
            /* Do a full frame redraw instead of incremental redraw. Incremental
             * redraw relies on stable `option_rows` which can be invalidated
             * when subcommands or child processes write to the console. A
             * full redraw clears the console buffer and repaints reliably. */
            draw_menu_frame(ps, menu, visible_opts, visible_count, visible_count, *selected_index, typed, option_rows);
            need_redraw = 0;
        }

        if (!hIn || hIn == INVALID_HANDLE_VALUE || !ReadConsoleInputA(hIn, &ir, 1, &read_count) || read_count == 0) {
            int ch = _getch();
            if (ch == 0 || ch == 224) {
                int ext = _getch();
                if (ext == 72) {
                    int prev = *selected_index;
                    int next_index = (*selected_index + visible_count - 1) % visible_count;
                    if (next_index != *selected_index) {
                        *selected_index = next_index;
                        need_redraw = 1;
                        if (menu_dbg) menu_debug_log("[MENU_DBG] fallback EXT UP prev=%d next=%d\n", prev, *selected_index);
                    }
                } else if (ext == 80) {
                    int prev = *selected_index;
                    int next_index = (*selected_index + 1) % visible_count;
                    if (next_index != *selected_index) {
                        *selected_index = next_index;
                        need_redraw = 1;
                        if (menu_dbg) menu_debug_log("[MENU_DBG] fallback EXT DOWN prev=%d next=%d\n", prev, *selected_index);
                    }
                }
                continue;
            }
            if (ch == 13 || ch == ' ') {
                if (menu_dbg) menu_debug_log("[MENU_DBG] fallback ENTER/SPACE selected=%d typed='%s'\n", *selected_index, typed);
                if (typed[0]) {
                    int idx = find_visible_option_index_by_key(visible_opts, visible_count, typed);
                    if (idx < 0) {
                        typed[0] = '\0';
                        return WAIT_INVALID;
                    }
                    *selected_index = idx;
                    typed[0] = '\0';
                    need_redraw = 1;
                }
                *selected_opt = visible_opts[*selected_index];
                return WAIT_SELECTED;
            }
            if (ch == 8) {
                size_t len = strlen(typed);
                if (len > 0) {
                    typed[len - 1] = '\0';
                    need_redraw = 1;
                }
                continue;
            }
            if (menu->allow_hotkey_debug && ch == 'D') {
                return WAIT_OPEN_DEBUG;
            }
            if (isprint((unsigned char)ch)) {
                size_t len = strlen(typed);
                char candidate[32];
                char single[2];
                int idx;

                single[0] = (char)ch;
                single[1] = '\0';
                candidate[0] = '\0';

                if (len + 1 < sizeof(candidate)) {
                    memcpy(candidate, typed, len);
                    candidate[len] = (char)ch;
                    candidate[len + 1] = '\0';
                }

                if (candidate[0] && has_visible_option_with_prefix(visible_opts, visible_count, candidate)) {
                    if (strcmp(typed, candidate) != 0) {
                        strcpy(typed, candidate);
                        need_redraw = 1;
                    }
                } else if (has_visible_option_with_prefix(visible_opts, visible_count, single)) {
                    if (strcmp(typed, single) != 0) {
                        strcpy(typed, single);
                        need_redraw = 1;
                    }
                } else if (typed[0]) {
                    typed[0] = '\0';
                    need_redraw = 1;
                }

                idx = find_visible_option_index_by_key(visible_opts, visible_count, typed);
                if (idx >= 0 && *selected_index != idx) {
                    *selected_index = idx;
                    need_redraw = 1;
                }
            }
            continue;
        }

        if (ir.EventType == KEY_EVENT && ir.Event.KeyEvent.bKeyDown) {
            KEY_EVENT_RECORD key = ir.Event.KeyEvent;
            DWORD mods = key.dwControlKeyState;
            int ch = (unsigned char)key.uChar.AsciiChar;

            if (menu_dbg) menu_debug_log("[MENU_DBG] KEY_EVENT vk=%u ch=%d mods=0x%08x selected=%d\n", key.wVirtualKeyCode, ch, mods, *selected_index);

            if (key.wVirtualKeyCode == VK_UP) {
                int prev = *selected_index;
                int next_index = (*selected_index + visible_count - 1) % visible_count;
                if (next_index != *selected_index) {
                    *selected_index = next_index;
                    need_redraw = 1;
                    if (menu_dbg) menu_debug_log("[MENU_DBG] KEY_EVENT UP prev=%d next=%d\n", prev, *selected_index);
                }
                continue;
            }
            if (key.wVirtualKeyCode == VK_DOWN) {
                int prev = *selected_index;
                int next_index = (*selected_index + 1) % visible_count;
                if (next_index != *selected_index) {
                    *selected_index = next_index;
                    need_redraw = 1;
                    if (menu_dbg) menu_debug_log("[MENU_DBG] KEY_EVENT DOWN prev=%d next=%d\n", prev, *selected_index);
                }
                continue;
            }
            if (key.wVirtualKeyCode == VK_RETURN || ch == ' ') {
                if (menu_dbg) menu_debug_log("[MENU_DBG] KEY_EVENT ENTER/SPACE final_selected=%d typed='%s'\n", *selected_index, typed);
                if (typed[0]) {
                    int idx = find_visible_option_index_by_key(visible_opts, visible_count, typed);
                    if (idx < 0) {
                        typed[0] = '\0';
                        return WAIT_INVALID;
                    }
                    *selected_index = idx;
                    typed[0] = '\0';
                    need_redraw = 1;
                }
                *selected_opt = visible_opts[*selected_index];
                return WAIT_SELECTED;
            }
            if (key.wVirtualKeyCode == VK_BACK) {
                size_t len = strlen(typed);
                if (len > 0) {
                    typed[len - 1] = '\0';
                    need_redraw = 1;
                }
                continue;
            }
            if (menu->allow_hotkey_debug && key.wVirtualKeyCode == 'D' && (mods & SHIFT_PRESSED)) {
                return WAIT_OPEN_DEBUG;
            }
            if ((mods & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED | LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED)) == 0 &&
                ch && isprint((unsigned char)ch)) {
                size_t len = strlen(typed);
                char candidate[32];
                char single[2];
                int idx;

                single[0] = (char)ch;
                single[1] = '\0';
                candidate[0] = '\0';

                if (len + 1 < sizeof(candidate)) {
                    memcpy(candidate, typed, len);
                    candidate[len] = (char)ch;
                    candidate[len + 1] = '\0';
                }

                if (candidate[0] && has_visible_option_with_prefix(visible_opts, visible_count, candidate)) {
                    if (strcmp(typed, candidate) != 0) {
                        strcpy(typed, candidate);
                        need_redraw = 1;
                    }
                } else if (has_visible_option_with_prefix(visible_opts, visible_count, single)) {
                    if (strcmp(typed, single) != 0) {
                        strcpy(typed, single);
                        need_redraw = 1;
                    }
                } else if (typed[0]) {
                    typed[0] = '\0';
                    need_redraw = 1;
                }

                idx = find_visible_option_index_by_key(visible_opts, visible_count, typed);
                if (idx >= 0 && *selected_index != idx) {
                    *selected_index = idx;
                    need_redraw = 1;
                }
            }
            continue;
        }

        if (ir.EventType == MOUSE_EVENT) {
            MOUSE_EVENT_RECORD mouse = ir.Event.MouseEvent;
            short prompt_row = -1;

            if (menu_dbg) menu_debug_log("[MENU_DBG] MOUSE_EVENT flags=%lu btn=%lu x=%d y=%d selected=%d\n",
                (unsigned long)mouse.dwEventFlags, (unsigned long)mouse.dwButtonState,
                (int)mouse.dwMousePosition.X, (int)mouse.dwMousePosition.Y, *selected_index);

            if (visible_count > 0 && option_rows[visible_count - 1] >= 0) {
                prompt_row = (short)(option_rows[visible_count - 1] + 1);
            }

            if (mouse_diag < 0) {
                const char *diag_env = getenv("ATB_MOUSE_DIAG");
                mouse_diag = (diag_env && diag_env[0] && strcmp(diag_env, "0") != 0) ? 1 : 0;
            }

            if (mouse_diag) {
                fprintf(stderr,
                    "\r\n[MOUSE] t=%llu flags=%lu btn=%lu x=%d y=%d sel=%d\r\n",
                    (unsigned long long)GetTickCount64(),
                    (unsigned long)mouse.dwEventFlags,
                    (unsigned long)mouse.dwButtonState,
                    (int)mouse.dwMousePosition.X,
                    (int)mouse.dwMousePosition.Y,
                    *selected_index);
                need_redraw = 1;
            }

            if (mouse.dwEventFlags == MOUSE_MOVED) {
                int hover_index = resolve_click_index_from_row(option_rows, visible_count, prompt_row, mouse.dwMousePosition.Y);
                if (hover_index >= 0 && hover_index != *selected_index) {
                    int prev = *selected_index;
                    *selected_index = hover_index;
                    need_redraw = 1;
                    if (menu_dbg) menu_debug_log("[MENU_DBG] MOUSE_MOVED hover prev=%d next=%d row=%d\n", prev, *selected_index, mouse.dwMousePosition.Y);
                }
                continue;
            }

            if (mouse.dwEventFlags == DOUBLE_CLICK) {
                int click_index = resolve_click_index_from_row(option_rows, visible_count, prompt_row, mouse.dwMousePosition.Y);
                if (click_index >= 0) {
                    ULONGLONG now = GetTickCount64();
                    if (*selected_index != click_index) {
                        int prev = *selected_index;
                        *selected_index = click_index;
                        need_redraw = 1;
                        if (menu_dbg) menu_debug_log("[MENU_DBG] DOUBLE_CLICK select prev=%d next=%d\n", prev, *selected_index);
                    }
                    if (typed[0]) {
                        typed[0] = '\0';
                        need_redraw = 1;
                    }

                    if (menu_is_double_click_local(last_click_index, last_click_tick, click_index, now, double_click_ms)) {
                        if (menu_dbg) menu_debug_log("[MENU_DBG] DOUBLE_CLICK confirm selected=%d\n", click_index);
                        /* Consume any pending follow-up mouse events to avoid
                         * accidental extra activations after confirming. */
                        FlushConsoleInputBuffer(hIn);
                        *selected_opt = visible_opts[click_index];
                        return WAIT_SELECTED;
                    }

                    if (menu_dbg) menu_debug_log("[MENU_DBG] DOUBLE_CLICK focus-only selected=%d\n", click_index);
                    last_click_index = click_index;
                    last_click_tick = now;
                }
                continue;
            }

            if (mouse.dwEventFlags == 0) {
                int click_index = resolve_click_index_from_row(option_rows, visible_count, prompt_row, mouse.dwMousePosition.Y);
                ULONGLONG now = GetTickCount64();
                int is_left_pressed = (mouse.dwButtonState & FROM_LEFT_1ST_BUTTON_PRESSED) != 0;
                int is_release = (mouse.dwButtonState == 0);

                if (is_left_pressed) {
                    /* On left-button press: update selection and record pressed
                     * state but do NOT accept immediately. Acceptance happens on
                     * release (or double-click) according to configuration. */
                    if (click_index >= 0) {
                        if (*selected_index != click_index) {
                            *selected_index = click_index;
                            need_redraw = 1;
                        }
                        if (typed[0]) {
                            typed[0] = '\0';
                            need_redraw = 1;
                        }
                        mouse_pressed = 1;
                        mouse_pressed_index = click_index;
                    }
                    continue;
                }

                if (is_release) {
                    int release_index = click_index;

                    if (release_index < 0 && mouse_pressed) {
                        release_index = mouse_pressed_index;
                    }

                    if (release_index >= 0) {
                        int same_press_release = mouse_pressed && (mouse_pressed_index == release_index);

                        if (*selected_index != release_index) {
                            *selected_index = release_index;
                            need_redraw = 1;
                        }
                        if (typed[0]) {
                            typed[0] = '\0';
                            need_redraw = 1;
                        }

                        if (same_press_release || !mouse_pressed) {
                            if (menu_is_double_click_local(last_click_index, last_click_tick, release_index, now, double_click_ms)) {
                                if (menu_dbg) menu_debug_log("[MENU_DBG] RELEASE double-click confirm selected=%d\n", release_index);
                                FlushConsoleInputBuffer(hIn);
                                *selected_opt = visible_opts[release_index];
                                return WAIT_SELECTED;
                            }
                            if (menu_dbg) menu_debug_log("[MENU_DBG] RELEASE single-click focus selected=%d\n", release_index);
                            last_click_index = release_index;
                            last_click_tick = now;
                        }
                    }

                    mouse_pressed = 0;
                    mouse_pressed_index = -1;
                }
                continue;
            }
            continue;
        }
    }
}

static int run_menu_by_id(PersistentShell *ps, MenuConfig *config, const char *menu_id) {
    MenuDef *menu;
    short option_rows[MAX_OPTIONS_PER_MENU];
    int first_entry = 1; /* force a one-time refresh when entering main */

    menu = find_menu(config, menu_id);
    if (!menu) {
        log_error("菜单不存在: %s", menu_id ? menu_id : "(null)");
        Sleep(1000);
        return MENU_RESULT_CONTINUE;
    }

    while (1) {
        const MenuOption *visible_opts[MAX_OPTIONS_PER_MENU];
        int visible_count = 0;
        int selected_index = 0;
        const MenuOption *selected = NULL;
        int wait_result;
        int i;

        for (i = 0; i < menu->option_count; ++i) {
            const MenuOption *opt = &menu->options[i];
            if (opt->requires_debug && !g_debug_mode) {
                continue;
            }
            if (visible_count < MAX_OPTIONS_PER_MENU) {
                visible_opts[visible_count++] = opt;
            }
        }

        if (visible_count <= 0) {
            log_error("菜单中无可用选项");
            Sleep(800);
            return MENU_RESULT_CONTINUE;
        }

        page_transition(menu->title[0] ? menu->title : "正在切换", 0.20);
        /* Ensure console modes and attributes are restored after any
         * interactive cptk usage (submenus may change VT state). This
         * refreshes g_default_console_attr and re-enables VT processing
         * if supported. */
        setup_console_encoding_and_vt();
        /* Ensure a logo is printed for menus that didn't opt-in via
         * the menu definition. This makes sure every menu shows a logo
         * before being drawn. If the menu file already has show_logo
         * set, draw_menu_frame will handle printing and we skip here. */
        if (!menu->show_logo) {
            run_if_present(ps, "logo");
        }

        /* Previously we forced an initial redraw here to avoid stray output
         * appearing between startup draws. Per request, skip that forced
         * redraw now and allow persistent shell output immediately. */
        if (first_entry) {
            first_entry = 0;
        }

        /* If VT mode is enabled, prefer the c_prompt_toolkit interactive
         * menu (supports VT mouse/touch reports). Otherwise fall back to the
         * existing console-driven menu. */
        if (g_vt_enabled) {
            const char *items[MAX_OPTIONS_PER_MENU];
            size_t sel = 0;
            size_t i2;
            /* Debug: indicate we're taking VT/cptk branch when ATB_MENU_DEBUG=1 */
            {
                const char *dbg_env = getenv("ATB_MENU_DEBUG");
                if (dbg_env && dbg_env[0] && strcmp(dbg_env, "0") != 0) {
                    menu_debug_log("[MENU_DBG] run_menu_by_id: g_vt_enabled=1 multi_select=%d visible_count=%d title=%s\n",
                        menu->multi_select, visible_count, menu->title);
                }
            }
            for (i2 = 0; i2 < (size_t)visible_count; ++i2) {
                items[i2] = visible_opts[i2]->label;
            }
            cptk_context *ctx = cptk_context_create();
            if (ctx) {
                if (menu->multi_select) {
                    unsigned char *mask = (unsigned char *)malloc((size_t)visible_count);
                    if (!mask) {
                        cptk_context_destroy(ctx);
                        draw_menu_frame(ps, menu, visible_opts, visible_count, visible_count, selected_index, "", option_rows);
                        wait_result = wait_menu_selection(ps, menu, visible_opts, visible_count, &selected_index, &selected, option_rows);
                    } else {
                        memset(mask, 0, (size_t)visible_count);
                        cptk_status st = cptk_menu_multi_choice_interactive(ctx, menu->title, items, (size_t)visible_count, mask, menu->animate_in, menu->max_height);
                        if (getenv("ATB_MENU_DEBUG") && getenv("ATB_MENU_DEBUG")[0] && strcmp(getenv("ATB_MENU_DEBUG"), "0") != 0) {
                            menu_debug_log("[MENU_DBG] cptk_menu_multi_choice_interactive returned st=%d\n", (int)st);
                        }
                        cptk_context_destroy(ctx);
                        if (st == CPTK_STATUS_OK) {
                            /* Compose comma-separated result from selected values (use value, fallback to key or label) */
                            char buf[4096];
                            size_t p = 0;
                            int first = 1;
                            for (size_t k = 0; k < (size_t)visible_count; ++k) {
                                if (mask[k]) {
                                    const char *v = visible_opts[k]->value[0] ? visible_opts[k]->value : (visible_opts[k]->key[0] ? visible_opts[k]->key : visible_opts[k]->label);
                                    size_t need = strlen(v) + (first ? 0 : 1);
                                    if (p + need + 1 < sizeof(buf)) {
                                        if (!first) buf[p++] = ',';
                                        memcpy(buf + p, v, strlen(v));
                                        p += strlen(v);
                                        buf[p] = '\0';
                                    }
                                    first = 0;
                                }
                            }
                            /* Write menutmp.txt like Python menu does */
                            if (p > 0) {
                                FILE *f = fopen("menutmp.txt", "w");
                                if (f) {
                                    fwrite(buf, 1, p, f);
                                    fclose(f);
                                }
                            } else {
                                FILE *f = fopen("menutmp.txt", "w");
                                if (f) { fwrite("", 1, 0, f); fclose(f); }
                            }
                            free(mask);
                            /* After multi-select we simply continue (menu written), return to caller */
                            return MENU_RESULT_CONTINUE;
                        } else {
                            free(mask);
                            /* fallback to console */
                            draw_menu_frame(ps, menu, visible_opts, visible_count, visible_count, selected_index, "", option_rows);
                            wait_result = wait_menu_selection(ps, menu, visible_opts, visible_count, &selected_index, &selected, option_rows);
                        }
                    }
                } else {
                    cptk_status st = cptk_menu_choice_interactive(ctx, menu->title, items, (size_t)visible_count, (size_t)selected_index, &sel);
                    if (getenv("ATB_MENU_DEBUG") && getenv("ATB_MENU_DEBUG")[0] && strcmp(getenv("ATB_MENU_DEBUG"), "0") != 0) {
                        menu_debug_log("[MENU_DBG] cptk_menu_choice_interactive returned st=%d sel=%u\n", (int)st, (unsigned)sel);
                    }
                    cptk_context_destroy(ctx);
                    if (st == CPTK_STATUS_OK) {
                        selected_index = (int)sel;
                        selected = visible_opts[selected_index];
                        wait_result = 1; /* WAIT_SELECTED */
                    } else {
                        /* fallback to console mode */
                        draw_menu_frame(ps, menu, visible_opts, visible_count, visible_count, selected_index, "", option_rows);
                        wait_result = wait_menu_selection(ps, menu, visible_opts, visible_count, &selected_index, &selected, option_rows);
                    }
                }
            } else {
                draw_menu_frame(ps, menu, visible_opts, visible_count, visible_count, selected_index, "", option_rows);
                wait_result = wait_menu_selection(ps, menu, visible_opts, visible_count, &selected_index, &selected, option_rows);
            }
        } else {
            draw_menu_frame(ps, menu, visible_opts, visible_count, visible_count, selected_index, "", option_rows);
            wait_result = wait_menu_selection(ps, menu, visible_opts, visible_count, &selected_index, &selected, option_rows);
        }
        if (wait_result == 2) {
            if (!g_debug_mode) {
                log_error("当前为 Release 构建，DEBUG 菜单不可用");
                Sleep(1000);
            } else {
                int r = run_menu_by_id(ps, config, "debug");
                if (r == MENU_RESULT_EXIT) {
                    return MENU_RESULT_EXIT;
                }
            }
            continue;
        }
        if (wait_result == 3 || !selected) {
            log_error("输入错误，请重新输入");
            Sleep(700);
            continue;
        }

        {
            int action_result = execute_menu_action(ps, config, selected);
            if (action_result == MENU_RESULT_EXIT) {
                return MENU_RESULT_EXIT;
            }
            if (action_result == MENU_RESULT_BACK) {
                return MENU_RESULT_CONTINUE;
            }
        }
    }
}

static void get_windows_version(int *major, int *minor, int *build) {
    OSVERSIONINFOEXA osv;
    ZeroMemory(&osv, sizeof(osv));
    osv.dwOSVersionInfoSize = sizeof(osv);
    if (GetVersionExA((OSVERSIONINFOA *)&osv)) {
        if (major) {
            *major = (int)osv.dwMajorVersion;
        }
        if (minor) {
            *minor = (int)osv.dwMinorVersion;
        }
        if (build) {
            *build = (int)osv.dwBuildNumber;
        }
    } else {
        if (major) {
            *major = 0;
        }
        if (minor) {
            *minor = 0;
        }
        if (build) {
            *build = 0;
        }
    }
}

static int resolve_build_info_path(char *out_path, size_t out_size) {
    const char *candidates[] = {
        ".\\src\\build_info.py",
        ".\\build_info.py",
        "..\\src\\build_info.py",
        "..\\..\\src\\build_info.py"
    };
    int i;

    if (!out_path || out_size == 0) {
        return 0;
    }
    out_path[0] = '\0';

    for (i = 0; i < (int)(sizeof(candidates) / sizeof(candidates[0])); ++i) {
        if (file_exists(candidates[i])) {
            snprintf(out_path, out_size, "%s", candidates[i]);
            return 1;
        }
    }
    return 0;
}

static int detect_debug_build_mode(void) {
    char path[MAX_PATH];
    char *text;
    size_t size = 0;
    const char *env_debug = getenv("ATB_DEBUG");
    const char *env_build = getenv("ATB_BUILD_TYPE");

    if (env_debug && strcmp(env_debug, "1") == 0) {
        return 1;
    }
    if (env_build && str_eq_icase(env_build, "debug")) {
        return 1;
    }

    if (!resolve_build_info_path(path, sizeof(path))) {
        return 0;
    }
    text = read_text_file(path, &size);
    if (!text) {
        return 0;
    }

    {
        int is_debug = 0;
        char *p = strstr(text, "BUILD_TYPE");
        if (p && strstr(p, "debug")) {
            is_debug = 1;
        }
        free(text);
        return is_debug;
    }
}

static void run_mod_startup_scripts(void) {
    WIN32_FIND_DATAA fdat;
    HANDLE h;

    if (!dir_exists("mod")) {
        _mkdir("mod");
    }

    h = FindFirstFileA("mod\\*", &fdat);
    if (h == INVALID_HANDLE_VALUE) {
        return;
    }

    do {
        if (!(fdat.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            continue;
        }
        if (strcmp(fdat.cFileName, ".") == 0 || strcmp(fdat.cFileName, "..") == 0) {
            continue;
        }
        {
            char start_bat[MAX_PATH];
            char cmd[MAX_COMMAND_LEN];
            snprintf(start_bat, sizeof(start_bat), "mod\\%s\\start.bat", fdat.cFileName);
            if (file_exists(start_bat)) {
                snprintf(cmd, sizeof(cmd), "cd /d mod\\%s && call start.bat", fdat.cFileName);
                run_command(cmd);
            }
        }
    } while (FindNextFileA(h, &fdat));

    FindClose(h);
}

static int pre_main(void) {
    const char *skip_update;
    const char *skip_platform;

    set_console_title("AllToolBox by xgj_236");
    set_pathext_extra();

    /* 如果没有控制台则分配一个控制台并绑定 stdin/stdout/stderr，避免创建任何文件 */
    if (!GetConsoleWindow()) {
        if (AllocConsole()) {
            FILE *in_fp = NULL, *out_fp = NULL, *err_fp = NULL;
            freopen_s(&in_fp, "CONIN$", "r", stdin);
            freopen_s(&out_fp, "CONOUT$", "w", stdout);
            freopen_s(&err_fp, "CONOUT$", "w", stderr);
            setvbuf(stdout, NULL, _IONBF, 0);
            setvbuf(stderr, NULL, _IONBF, 0);
        }
    }

    setup_console_encoding_and_vt();

    /* 先执行必要文件检查（不依赖 build_info.py），并给出可见提示 */
    log_info("正在检查必要文件...");

    {
        char menu_json_path[MAX_PATH];
        if (!resolve_menu_json_path(menu_json_path, sizeof(menu_json_path))) {
            log_error("未找到菜单配置文件 menus.json");
            log_info("请确保存在 src\\menu\\start\\menus.json 或 menu\\start\\menus.json");
            pause_any("按任意键退出", 0);
            return 0;
        }
    }

    log_info("检查 ADB 可用性...");
    if (run_command("adb.exe version 1>nul 2>nul") != 0) {
        log_error("adb.exe 未在 PATH 中或不可执行，请安装 Android Platform Tools 并将 adb 加入 PATH");
        pause_any("按任意键退出", 0);
        return 0;
    }

    /* 在完成基础文件与工具检查后再决定调试模式 */
    g_debug_mode = detect_debug_build_mode();
    g_allow_xtc = 1;

    if (file_exists(".\\color.bat")) {
        run_command("call .\\color.bat 1>nul 2>nul");
    }

    if (g_debug_mode) {
        log_info("已启用调试模式");
    }

    {
        const char *path_env = getenv("PATH");
        int suspicious = 0;
        if (!path_env) {
            suspicious = 1;
        } else {
            char *dup = _strdup(path_env);
            if (dup) {
                _strlwr(dup);
                if (!strstr(dup, "windows") || !strstr(dup, "system32") || !strstr(dup, "powershell")) {
                    suspicious = 1;
                }
                free(dup);
            }
        }
        if (suspicious) {
            log_error("你的系统环境变量异常，这可能导致异常问题，输入no跳过");
            {
                char line[32];
                if (!fgets(line, sizeof(line), stdin)) {
                    return 0;
                }
                if (!str_eq_icase(trim_inplace(line), "no")) {
                    return 0;
                }
            }
        }
    }

    if (!dir_exists("mod")) {
        _mkdir("mod");
    }
    run_mod_startup_scripts();

    {
        char exe_dir[MAX_PATH];
        char candidate[MAX_PATH];
        get_exe_dir(exe_dir, sizeof(exe_dir));
        if (exe_dir[0]) {
            build_path_join(candidate, sizeof(candidate), exe_dir, "..\\bin");
            if (dir_exists(candidate)) {
                _chdir(candidate);
            }
        }
    }

    run_if_present(NULL, "withone");
    run_if_present(NULL, "afterup");

    if (file_exists("..\\bugjump.7z")) {
        remove("..\\bugjump.7z");
    }
    if (file_exists("..\\repair.exe")) {
        remove("..\\repair.exe");
    }

    skip_update = getenv("ATB_SKIP_UPDATE");
    if (!skip_update || strcmp(skip_update, "1") != 0) {
        log_info("正在检查更新...");
        {
            const wchar_t *version_url = g_debug_mode
                ? L"https://raw.githubusercontent.com/xgj236/AllToolBox/main/betaversiontmp.txt"
                : L"https://raw.githubusercontent.com/xgj236/AllToolBox/main/versiontmp.txt";
            int local_v = 0;
            char remote_buf[4096];

            if (!file_exists("bugversion.txt")) {
                FILE *f = fopen("bugversion.txt", "w");
                if (f) {
                    fputs("0", f);
                    fclose(f);
                }
            }

            {
                FILE *f = fopen("bugversion.txt", "r");
                if (f) {
                    char tmp[64];
                    if (fgets(tmp, sizeof(tmp), f)) {
                        local_v = atoi(tmp);
                    }
                    fclose(f);
                }
            }

            if (http_get_w(version_url, remote_buf, sizeof(remote_buf), 8000)) {
                int remote_v = atoi(remote_buf);
                if (remote_v > local_v) {
                    log_warn("当前补丁版本过时，必须更新");
                    pause_any("单击此字符或按任意键继续开始更新...", 0);
                    if (file_exists("repair.exe")) {
                        CopyFileA("repair.exe", "..\\repair.exe", FALSE);
                        _chdir("..\\");
                        run_command("start repair.exe");
                        cleanup_and_exit(NULL, 2);
                    }
                }
            }
        }
        run_command("call upall.bat run");
    }

    skip_platform = getenv("ATB_SKIP_PLATFORM_CHECK");
    if (!skip_platform || strcmp(skip_platform, "1") != 0) {
        int major = 0;
        int minor = 0;
        int build = 0;
        const char *arch = "x64";
        SYSTEM_INFO si;

        log_info("正在检查Windows属性...");

        get_windows_version(&major, &minor, &build);
        GetNativeSystemInfo(&si);
        switch (si.wProcessorArchitecture) {
            case PROCESSOR_ARCHITECTURE_AMD64: arch = "x64"; break;
            case PROCESSOR_ARCHITECTURE_INTEL: arch = "x86"; break;
            case PROCESSOR_ARCHITECTURE_ARM64: arch = "arm64-v8a"; break;
            default: arch = "unknown"; break;
        }

        log_info("当前运行环境:Windows%d.%d_%s_build%d", major, minor, arch, build);

        if (major < 6 || (major == 6 && minor < 2)) {
            log_error("此脚本需要 Windows 8 或更高版本");
            pause_any(NULL, 0);
            return 0;
        }

        /* 早先已经在基础检查阶段验证过 adb，这里可选地再次确认或跳过 */
        log_info("检查ADB命令成功");
    }

    write_whoyou("2");

    log_warn("关于解绑：该工具不提供手表强制解绑服务，如您拾取他人的手表，请联系当地110公安机关归还失主。手表解绑属于非法行为，请归还失主。而不要尝试通过任何手段解除挂失锁");
    log_warn("关于收费：这个工具是完全免费的，如果你付费购买了那么请退款");
    log_warn("本脚本部分功能可能造成侵权问题，并可能受到法律追究，所以仅供个人使用，请勿用于商业用途");
    log_info("---请永远相信我们能给你带来免费又好用的工具---");
    log_info("关于官网：https://atb.xgj.qzz.io");
    log_info("关于作者：本脚本由快乐小公爵236等作者制作");
    log_info("作者QQ：3247039462");
    log_info("工具箱交流与反馈QQ群：907491503");
    log_info("作者哔哩哔哩账号：https://b23.tv/L54R5ZV");
    log_info("bug与建议反馈邮箱：ATBbug@xgj.qzz.io");

    log_info("单击此字符或按任意键继续进入主界面");
    pause_any(NULL, 0);
    clear_console();
    return 1;
}

static void cleanup_and_exit(PersistentShell *ps, int code) {
    log_info("正在结束ADB服务...");
    if (check_adb_server()) {
        run_command("adb.exe kill-server 1>nul 2>nul");
    }
    if (ps) {
        ps_destroy(ps);
    }
    exit(code);
}

int main(void) {
    MenuConfig *config = NULL;
    char menu_json_path[MAX_PATH];
    PersistentShell *ps;
    int menu_result;

    /* 不创建任何额外文件；直接进入预初始化 */

    if (!pre_main()) {
        return 1;
    }

    config = (MenuConfig *)calloc(1, sizeof(MenuConfig));
    if (!config) {
        fprintf(stderr, "内存不足，无法初始化菜单配置\n");
        return 1;
    }

    if (!resolve_menu_json_path(menu_json_path, sizeof(menu_json_path))) {
        fprintf(stderr, "menu json not found\n");
        free(config);
        return 1;
    }
    if (!load_menu_config(menu_json_path, config)) {
        fprintf(stderr, "failed to load menu config: %s\n", menu_json_path);
        free(config);
        return 1;
    }

    ps = ps_create();
    if (!ps) {
        fprintf(stderr, "无法启动持久 cmd，已回退到直接命令模式\n");
    }

    menu_result = run_menu_by_id(ps, config, "main");
    if (menu_result == MENU_RESULT_EXIT) {
        free(config);
        cleanup_and_exit(ps, 0);
    }

    free(config);
    cleanup_and_exit(ps, 0);
    return 0;
}
