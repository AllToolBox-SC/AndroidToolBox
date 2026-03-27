/* Standalone menu executable using menu.c helpers.
 * Usage: menu.exe [path-to-json]
 * JSON path is required.
 */

#include "menu.h"
#include "c_prompt_toolkit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <locale.h>
#ifdef _WIN32
# include <windows.h>
# include <commdlg.h>
# include <io.h>
# pragma comment(lib, "Comdlg32.lib")
#else
# include <unistd.h>
#endif

static void print_usage(void) {
    printf("Usage: menu.exe [path-to-json]\n");
    printf("Options:\n");
    printf("  -h, --h, --help, -help   Show help\n");
    printf("  -s, --s                  Multi-select mode\n");
    printf("  --logo, -logo            Try to run logo.bat before showing menu\n");
    printf("                           Can be used alone\n");
    printf("JSON path is required.\n");
}

static int menu_path_exists(const char *path) {
    if (!path || !path[0]) return 0;
#ifdef _WIN32
    return (_access(path, 0) == 0);
#else
    return (access(path, F_OK) == 0);
#endif
}

static int menu_is_help_option(const char *arg) {
    return arg && (
        strcmp(arg, "-h") == 0 ||
        strcmp(arg, "--h") == 0 ||
        strcmp(arg, "--help") == 0 ||
        strcmp(arg, "-help") == 0
    );
}

static int menu_is_multi_select_option(const char *arg) {
    return arg && (strcmp(arg, "-s") == 0 || strcmp(arg, "--s") == 0);
}

static int menu_is_logo_option(const char *arg) {
    return arg && (strcmp(arg, "--logo") == 0 || strcmp(arg, "-logo") == 0);
}

static void menu_print_error(const char *msg) {
    if (!msg || !msg[0]) return;
    fprintf(stderr, "[错误]%s\n", msg);
}

static void menu_extract_dir(const char *path, char *out_dir, size_t out_size) {
    size_t len;
    if (!out_dir || out_size == 0) return;
    out_dir[0] = '\0';
    if (!path || !path[0]) return;

    snprintf(out_dir, out_size, "%s", path);
    len = strlen(out_dir);
    while (len > 0) {
        char c = out_dir[len - 1];
        if (c == '\\' || c == '/') {
            out_dir[len - 1] = '\0';
            break;
        }
        len--;
    }
}

static int menu_guess_default_config_path(char *out_path, size_t out_size) {
    const char *candidates[] = {
        "src\\menu\\start\\menus.json",
        "menu\\start\\menus.json",
        "test\\test_options.yml",
        "test\\test_options.toml",
        "test\\test_options.xml",
        "test\\test_options.csv",
        "test\\test_options.txt"
    };
    size_t i;
    if (!out_path || out_size == 0) return 0;
    out_path[0] = '\0';
    for (i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i) {
        if (menu_path_exists(candidates[i])) {
            snprintf(out_path, out_size, "%s", candidates[i]);
            return 1;
        }
    }
    return 0;
}

static int menu_pick_config_file(char *out_path, size_t out_size) {
    if (!out_path || out_size == 0) return 0;
    out_path[0] = '\0';

#ifdef _WIN32
    OPENFILENAMEA ofn;
    char file_buf[MAX_PATH * 4];
    char default_path[MAX_PATH * 4];
    char initial_dir[MAX_PATH * 4];

    memset(&ofn, 0, sizeof(ofn));
    memset(file_buf, 0, sizeof(file_buf));
    memset(default_path, 0, sizeof(default_path));
    memset(initial_dir, 0, sizeof(initial_dir));

    if (menu_guess_default_config_path(default_path, sizeof(default_path))) {
        snprintf(file_buf, sizeof(file_buf), "%s", default_path);
        menu_extract_dir(default_path, initial_dir, sizeof(initial_dir));
    }

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = file_buf;
    ofn.nMaxFile = (DWORD)sizeof(file_buf);
    ofn.lpstrFilter =
        "菜单配置文件 (*.json;*.yaml;*.yml;*.toml;*.xml;*.csv;*.txt)\0"
        "*.json;*.yaml;*.yml;*.toml;*.xml;*.csv;*.txt\0"
        "所有文件 (*.*)\0*.*\0\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrDefExt = "json";
    ofn.lpstrInitialDir = initial_dir[0] ? initial_dir : NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    ofn.lpstrTitle = "请选择菜单配置文件";

    if (GetOpenFileNameA(&ofn)) {
        snprintf(out_path, out_size, "%s", file_buf);
        return 1;
    }

    return 0;
#else
    printf("请输入配置文件路径: ");
    fflush(stdout);
    if (!fgets(out_path, (int)out_size, stdin)) return 0;
    {
        size_t len = strlen(out_path);
        while (len > 0 && (out_path[len - 1] == '\n' || out_path[len - 1] == '\r')) {
            out_path[--len] = '\0';
        }
    }
    return out_path[0] ? 1 : 0;
#endif
}

static void menu_try_run_logo(void) {
    if (menu_path_exists("logo.bat")) {
        char cmd[1024];
        snprintf(cmd, sizeof(cmd), "cmd /c call \"%s\"", "logo.bat");
        (void)system(cmd);
    }
}

static int menu_is_interactive_console(void) {
#ifdef _WIN32
    if (!_isatty(_fileno(stdin)) || !_isatty(_fileno(stdout))) {
        return 0;
    }
    {
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD mode = 0;
        if (!hOut || hOut == INVALID_HANDLE_VALUE) return 0;
        if (!GetConsoleMode(hOut, &mode)) return 0;
    }
    return 1;
#else
    return isatty(fileno(stdin)) && isatty(fileno(stdout));
#endif
}

static void menu_clear_console(void) {
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    COORD home = {0, 0};
    DWORD written = 0;
    DWORD cells = 0;

    if (!hOut || hOut == INVALID_HANDLE_VALUE) return;
    if (!GetConsoleScreenBufferInfo(hOut, &csbi)) return;

    cells = (DWORD)csbi.dwSize.X * (DWORD)csbi.dwSize.Y;
    FillConsoleOutputCharacterA(hOut, ' ', cells, home, &written);
    FillConsoleOutputAttribute(hOut, csbi.wAttributes, cells, home, &written);
    SetConsoleCursorPosition(hOut, home);
#else
    printf("\x1b[2J\x1b[H");
    fflush(stdout);
#endif
}

static void menu_clear_console_after_select(void) {
    menu_clear_console();
}

static void menu_setup_console_utf8(void) {
#ifdef _WIN32
    /* Keep runtime multibyte handling and console code page on UTF-8.
     * Without this, /utf-8 string literals can be interpreted as ANSI bytes
     * and produce mojibake in Windows terminals.
     */
    SetConsoleCP(65001);
    SetConsoleOutputCP(65001);
    (void)setlocale(LC_ALL, ".UTF-8");
#endif
}

static char *menu_read_text_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long len = ftell(f);
    if (len < 0) { fclose(f); return NULL; }
    rewind(f);
    char *buf = (char *)malloc((size_t)len + 1);
    if (!buf) { fclose(f); return NULL; }
    (void)fread(buf, 1, (size_t)len, f);
    buf[len] = '\0';
    fclose(f);
    return buf;
}

static char *menu_read_line_dynamic(FILE *stream) {
    if (!stream) return NULL;
    size_t cap = 1024;
    size_t len = 0;
    char *buf = (char *)malloc(cap);
    int ch;

    if (!buf) return NULL;
    while ((ch = fgetc(stream)) != EOF) {
        if (len + 1 >= cap) {
            size_t ncap = cap * 2;
            char *nbuf = (char *)realloc(buf, ncap);
            if (!nbuf) {
                free(buf);
                return NULL;
            }
            buf = nbuf;
            cap = ncap;
        }
        buf[len++] = (char)ch;
        if (ch == '\n') break;
    }

    if (len == 0 && ch == EOF) {
        free(buf);
        return NULL;
    }
    buf[len] = '\0';
    return buf;
}

static const char *menu_skip_ws_and_bom(const char *p) {
    if (!p) return NULL;
    if ((unsigned char)p[0] == 0xEF && (unsigned char)p[1] == 0xBB && (unsigned char)p[2] == 0xBF) {
        p += 3;
    }
    while (*p && isspace((unsigned char)*p)) p++;
    return p;
}

static char *menu_extract_json_field_local(const char *start, const char *end, const char *key) {
    const char *p = start;
    size_t keylen = strlen(key);
    while (p < end) {
        const char *k = strstr(p, key);
        if (!k || k >= end) break;
        const char *q = k + keylen;
        const char *colon = strchr(q, ':');
        if (!colon || colon >= end) { p = q; continue; }
        const char *open = strchr(colon, '"');
        if (!open || open >= end) { p = q; continue; }
        open++;
        const char *close = open;
        while (close < end && *close != '"') {
            if (*close == '\\' && (close + 1) < end) close += 2;
            else close++;
        }
        if (close >= end) { p = q; continue; }
        size_t len = (size_t)(close - open);
        char *out = (char *)malloc(len + 1);
        if (!out) return NULL;
        memcpy(out, open, len);
        out[len] = '\0';
        return out;
    }
    return NULL;
}

static const char *menu_find_matching_array_end(const char *arr_start) {
    const char *p;
    int depth = 0;
    int in_string = 0;
    int escaped = 0;

    if (!arr_start || *arr_start != '[') return NULL;

    for (p = arr_start; *p; ++p) {
        char c = *p;
        if (in_string) {
            if (escaped) {
                escaped = 0;
            } else if (c == '\\') {
                escaped = 1;
            } else if (c == '"') {
                in_string = 0;
            }
            continue;
        }
        if (c == '"') {
            in_string = 1;
            continue;
        }
        if (c == '[') {
            depth++;
            continue;
        }
        if (c == ']') {
            depth--;
            if (depth == 0) return p;
        }
    }
    return NULL;
}

/* menu_main-level fallback parser:
 * - supports {"options": [...]} (preferred)
 * - supports top-level array [...]
 * - each item supports value+label (or key+label / value+text / key+text)
 */
static int menu_load_options_from_json_main(const char *path, char ***out_values, char ***out_labels, size_t *out_count) {
    if (!path || !out_values || !out_labels || !out_count) return 0;
    *out_values = NULL;
    *out_labels = NULL;
    *out_count = 0;

    char *buf = menu_read_text_file(path);
    if (!buf) return 0;

    const char *root = menu_skip_ws_and_bom(buf);
    if (!root) { free(buf); return 0; }

    const char *arr = NULL;
    if (*root == '{') {
        const char *opt = strstr(root, "\"options\"");
        if (opt) arr = strchr(opt, '[');
    } else if (*root == '[') {
        arr = root;
    }
    if (!arr) {
        free(buf);
        return 0;
    }

    const char *arr_end = menu_find_matching_array_end(arr);
    if (!arr_end) {
        free(buf);
        return 0;
    }

    const char *p = arr + 1;
    size_t cap = 0, cnt = 0;
    char **vals = NULL;
    char **labs = NULL;

    while (p < arr_end) {
        const char *obj = strchr(p, '{');
        if (!obj || obj >= arr_end) break;
        const char *obj_end = obj + 1;
        int depth = 1;
        while (obj_end < arr_end && depth > 0) {
            if (*obj_end == '{') depth++;
            else if (*obj_end == '}') depth--;
            obj_end++;
        }
        if (depth != 0 || obj_end > arr_end) break;

        char *label = menu_extract_json_field_local(obj, obj_end, "\"label\"");
        char *value = menu_extract_json_field_local(obj, obj_end, "\"value\"");
        if (!value) value = menu_extract_json_field_local(obj, obj_end, "\"key\"");
        if (!label) label = menu_extract_json_field_local(obj, obj_end, "\"text\"");

        if (label && value) {
            if (cnt + 1 > cap) {
                size_t ncap = cap ? cap * 2 : 8;
                char **nv = (char **)realloc(vals, ncap * sizeof(char *));
                char **nl = (char **)realloc(labs, ncap * sizeof(char *));
                if (!nv || !nl) {
                    free(label);
                    free(value);
                    break;
                }
                vals = nv;
                labs = nl;
                cap = ncap;
            }
            vals[cnt] = value;
            labs[cnt] = label;
            cnt++;
        } else {
            if (label) free(label);
            if (value) free(value);
        }

        p = obj_end;
    }

    free(buf);

    if (cnt == 0) {
        free(vals);
        free(labs);
        return 0;
    }

    *out_values = vals;
    *out_labels = labs;
    *out_count = cnt;
    return 1;
}

int main(int argc, char **argv) {
    const char *config_path = NULL;
    char selected_config_path[MAX_PATH * 4];
    int multi_select = 0;
    int run_logo = 0;
    int i;

    memset(selected_config_path, 0, sizeof(selected_config_path));

    menu_setup_console_utf8();
    menu_mouse_click_state_reset(menu_get_global_mouse_click_state());

    for (i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        if (menu_is_help_option(arg)) {
            print_usage();
            return 0;
        }
        if (menu_is_multi_select_option(arg)) {
            multi_select = 1;
            continue;
        }
        if (menu_is_logo_option(arg)) {
            run_logo = 1;
            continue;
        }
        if (arg[0] == '-') {
            fprintf(stderr, "[错误]无效参数: %s\n", arg);
            return 1;
        }
        if (!config_path) {
            config_path = arg;
        } else {
            menu_print_error("参数过多");
            return 1;
        }
    }

    if (run_logo) {
        menu_try_run_logo();
    }

    if (!config_path) {
        if (run_logo) {
            return 0;
        }
        fprintf(stderr, "[错误]未指定配置文件\n");
        return 1;
    }

    if (!menu_path_exists(config_path)) {
        fprintf(stderr, "[错误]配置文件不存在: %s\n", config_path);
        return 1;
    }

    {
        char **values = NULL;
        char **labels = NULL;
        size_t count = 0;

        /* Prefer shared loader, but fallback to menu_main-level parser so
         * top-level {"options": [...]} is guaranteed to work.
         */
        if (!menu_load_options_from_file(config_path, &values, &labels, &count)
            && !menu_load_options_from_json_main(config_path, &values, &labels, &count)) {
            menu_print_error("请选择正确的配置文件");
            return 1;
        }

        /* One-shot selection and write menutmp.txt */
        if (multi_select) {
            unsigned char *mask = (unsigned char *)malloc(count);
            if (!mask) {
                menu_free_options(values, labels, count);
                return 1;
            }
            memset(mask, 0, count);
            cptk_context *ctx = cptk_context_create();
            int ok = 0;
            if (ctx) {
                menu_mouse_click_state_reset(menu_get_global_mouse_click_state());
                cptk_status st = cptk_menu_multi_choice_interactive(ctx, "请选择操作", (const char *const *)labels, count, mask, 0, 0);
                cptk_context_destroy(ctx);
                if (st == CPTK_STATUS_OK) ok = 1;
            }
            if (!ok) {
                /* Console fallback */
                for (size_t i = 0; i < count; ++i) printf("%3zu. %s\n", i + 1, labels[i] ? labels[i] : "");
                printf("选择（以逗号分隔索引）: "); fflush(stdout);
                char *line = menu_read_line_dynamic(stdin);
                if (!line) { free(mask); menu_free_options(values, labels, count); return 1; }
                char *tok = NULL;
#ifdef _WIN32
                char *ctx_tok = NULL;
                tok = strtok_s(line, ", \t\n", &ctx_tok);
#else
                tok = strtok(line, ", \t\n");
#endif
                while (tok) {
                    int idx = atoi(tok);
                    if (idx >= 1 && (size_t)idx <= count) mask[idx - 1] = 1;
#ifdef _WIN32
                    tok = strtok_s(NULL, ", \t\n", &ctx_tok);
#else
                    tok = strtok(NULL, ", \t\n");
#endif
                }
                free(line);
            }

            menu_clear_console_after_select();
            /* Write menutmp.txt */
            FILE *f = fopen("menutmp.txt", "w");
            if (f) {
                int first = 1;
                for (size_t i = 0; i < count; ++i) {
                    if (!mask[i]) continue;
                    const char *v = values[i] ? values[i] : "";
                    if (!first) fputc(',', f);
                    if (v[0]) fwrite(v, 1, strlen(v), f);
                    first = 0;
                }
                fclose(f);
            }
            free(mask);
            menu_free_options(values, labels, count);
            return 0;
        } else {
            /* single choice */
            size_t sel = 0; int ok = 0;
            cptk_context *ctx = cptk_context_create();
            if (ctx) {
                menu_mouse_click_state_reset(menu_get_global_mouse_click_state());
                cptk_status st = cptk_menu_choice_interactive(ctx, "请选择操作", (const char *const *)labels, count, 0, &sel);
                cptk_context_destroy(ctx);
                if (st == CPTK_STATUS_OK) ok = 1;
            }
            if (!ok) {
                /* Console fallback */
                for (size_t i = 0; i < count; ++i) printf("%3zu. %s\n", i + 1, labels[i] ? labels[i] : "");
                printf("%s", "选择: "); fflush(stdout);
                int idx = 0;
                char *line = menu_read_line_dynamic(stdin);
                if (!line) { menu_free_options(values, labels, count); return 1; }
                idx = atoi(line);
                free(line);
                if (idx < 1) idx = 1;
                if ((size_t)idx > count) idx = (int)count;
                sel = (size_t)(idx - 1);
            }

            menu_clear_console_after_select();
            const char *v = values[sel] ? values[sel] : "";
            FILE *f = fopen("menutmp.txt", "w");
            if (f) {
                if (v[0]) fwrite(v, 1, strlen(v), f);
                else fwrite("", 1, 0, f);
                fclose(f);
            }
            menu_free_options(values, labels, count);
            return 0;
        }
    }
}

