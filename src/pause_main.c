#include "pause.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static int pause_arg_eq(const char *a, const char *b) {
    if (!a || !b) return 0;
    return strcmp(a, b) == 0;
}

static int pause_has_style_meta(const char *s) {
    size_t i;
    if (!s) return 0;
    for (i = 0; s[i] != '\0'; ++i) {
        if (s[i] == ':' || isspace((unsigned char)s[i])) {
            return 1;
        }
    }
    return 0;
}

static int pause_parse_timeout_ms(const char *s, int *out_ms) {
    char *endp = NULL;
    double sec;
    double ms;

    if (!s || !out_ms) return 0;
    sec = strtod(s, &endp);
    if (!endp || *endp != '\0') return 0;

    ms = sec * 1000.0;
    if (ms <= 0.0) {
        *out_ms = 0;
        return 1;
    }
    if (ms > 2147483647.0) {
        *out_ms = 2147483647;
        return 1;
    }
    *out_ms = (int)(ms + 0.5);
    return 1;
}

static void pause_print_usage(const char *prog) {
    printf("Usage: %s [options]\n", prog ? prog : "pause.exe");
    printf("Options:\n");
    printf("  -h, --help             Show this help\n");
    printf("  -m, --msg <text>       Pause prompt text\n");
    printf("  -t, --timeout <sec>    Timeout in seconds (empty = wait forever)\n");
    printf("  -c, --color <spec>     ANSI style like red, #RRGGBB, fg:#RRGGBB bold\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s -m \"单击此字符或按任意键继续\"\n", prog ? prog : "pause.exe");
    printf("  %s -m \"2秒后自动继续\" -t 2\n", prog ? prog : "pause.exe");
    printf("  %s -m \"红色提示\" -c \"fg:#ff0000 bold\"\n", prog ? prog : "pause.exe");
}

int main(int argc, char **argv) {
    const char *message = "单击此字符或按任意键继续";
    const char *color_arg = NULL;
    char normalized_color[256];
    int timeout_ms = -1;
    int i;

    normalized_color[0] = '\0';

    for (i = 1; i < argc; ++i) {
        const char *arg = argv[i];

        if (pause_arg_eq(arg, "-h") || pause_arg_eq(arg, "--help")) {
            pause_print_usage(argv[0]);
            return 0;
        }

        if (pause_arg_eq(arg, "-m") || pause_arg_eq(arg, "--msg")) {
            if (i + 1 >= argc) {
                fprintf(stderr, "[错误]参数 %s 需要一个值\n", arg);
                return 1;
            }
            message = argv[++i];
            continue;
        }

        if (pause_arg_eq(arg, "-t") || pause_arg_eq(arg, "--timeout")) {
            if (i + 1 >= argc) {
                fprintf(stderr, "[错误]参数 %s 需要一个值\n", arg);
                return 1;
            }
            if (!pause_parse_timeout_ms(argv[++i], &timeout_ms)) {
                fprintf(stderr, "[错误]timeout 格式无效: %s\n", argv[i]);
                return 1;
            }
            continue;
        }

        if (pause_arg_eq(arg, "-c") || pause_arg_eq(arg, "--color")) {
            if (i + 1 >= argc) {
                fprintf(stderr, "[错误]参数 %s 需要一个值\n", arg);
                return 1;
            }
            color_arg = argv[++i];
            continue;
        }

        if (strncmp(arg, "--msg=", 6) == 0) {
            message = arg + 6;
            continue;
        }
        if (strncmp(arg, "--timeout=", 10) == 0) {
            if (!pause_parse_timeout_ms(arg + 10, &timeout_ms)) {
                fprintf(stderr, "[错误]timeout 格式无效: %s\n", arg + 10);
                return 1;
            }
            continue;
        }
        if (strncmp(arg, "--color=", 8) == 0) {
            color_arg = arg + 8;
            continue;
        }

        fprintf(stderr, "[错误]未知参数: %s\n", arg);
        pause_print_usage(argv[0]);
        return 1;
    }

    if (color_arg && color_arg[0]) {
        if (pause_has_style_meta(color_arg)) {
            snprintf(normalized_color, sizeof(normalized_color), "%s", color_arg);
        } else {
            snprintf(normalized_color, sizeof(normalized_color), "fg:%s", color_arg);
        }
    }

    (void)pause_wait(message, timeout_ms, normalized_color[0] ? normalized_color : NULL);
    return 0;
}
