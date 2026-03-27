#include "pause.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#endif

#ifndef _WIN32
#include <unistd.h>
#include <sys/select.h>
#include <termios.h>
#include <time.h>
#endif

static int pause_eq_icase(const char *a, const char *b) {
    unsigned char ca;
    unsigned char cb;
    if (!a || !b) return 0;
    while (*a && *b) {
        ca = (unsigned char)*a;
        cb = (unsigned char)*b;
        if (tolower(ca) != tolower(cb)) return 0;
        ++a;
        ++b;
    }
    return (*a == '\0' && *b == '\0');
}

static int pause_hex_digit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

static int pause_parse_hex_color(const char *s, int *r, int *g, int *b) {
    int h0;
    int h1;
    int h2;
    int h3;
    int h4;
    int h5;

    if (!s || s[0] != '#') return 0;
    if (s[1] == '\0') return 0;

    if (s[4] == '\0') {
        h0 = pause_hex_digit(s[1]);
        h1 = pause_hex_digit(s[2]);
        h2 = pause_hex_digit(s[3]);
        if (h0 < 0 || h1 < 0 || h2 < 0) return 0;
        *r = h0 * 17;
        *g = h1 * 17;
        *b = h2 * 17;
        return 1;
    }

    if (s[7] == '\0') {
        h0 = pause_hex_digit(s[1]);
        h1 = pause_hex_digit(s[2]);
        h2 = pause_hex_digit(s[3]);
        h3 = pause_hex_digit(s[4]);
        h4 = pause_hex_digit(s[5]);
        h5 = pause_hex_digit(s[6]);
        if (h0 < 0 || h1 < 0 || h2 < 0 || h3 < 0 || h4 < 0 || h5 < 0) return 0;
        *r = h0 * 16 + h1;
        *g = h2 * 16 + h3;
        *b = h4 * 16 + h5;
        return 1;
    }

    return 0;
}

static void pause_style_to_ansi(const char *spec, char *prefix, size_t prefix_cap, char *suffix, size_t suffix_cap) {
    char buf[256];
    char first[64];
    char kind[16];
    char value[64];
    int bold = 0;
    int i;
    int r;
    int g;
    int b;
    int code = -1;

#ifdef _WIN32
    char *ctx = NULL;
    char *tok = NULL;
#else
    char *tok = NULL;
#endif

    if (prefix_cap > 0) prefix[0] = '\0';
    if (suffix_cap > 0) suffix[0] = '\0';
    if (!spec || !spec[0] || prefix_cap == 0 || suffix_cap == 0) return;

    snprintf(buf, sizeof(buf), "%s", spec);

#ifdef _WIN32
    tok = strtok_s(buf, " \t\r\n", &ctx);
#else
    tok = strtok(buf, " \t\r\n");
#endif
    if (!tok) return;

    snprintf(first, sizeof(first), "%s", tok);
    for (;;) {
#ifdef _WIN32
        tok = strtok_s(NULL, " \t\r\n", &ctx);
#else
        tok = strtok(NULL, " \t\r\n");
#endif
        if (!tok) break;
        if (pause_eq_icase(tok, "bold")) bold = 1;
    }

    snprintf(kind, sizeof(kind), "fg");
    snprintf(value, sizeof(value), "%s", first);

    for (i = 0; first[i] != '\0'; ++i) {
        if (first[i] == ':') {
            size_t klen = (size_t)i;
            if (klen >= sizeof(kind)) klen = sizeof(kind) - 1;
            memcpy(kind, first, klen);
            kind[klen] = '\0';
            snprintf(value, sizeof(value), "%s", first + i + 1);
            break;
        }
    }

    if (pause_parse_hex_color(value, &r, &g, &b)) {
        if (pause_eq_icase(kind, "bg")) {
            if (bold) {
                snprintf(prefix, prefix_cap, "\x1b[1m\x1b[48;2;%d;%d;%dm", r, g, b);
            } else {
                snprintf(prefix, prefix_cap, "\x1b[48;2;%d;%d;%dm", r, g, b);
            }
        } else {
            if (bold) {
                snprintf(prefix, prefix_cap, "\x1b[1m\x1b[38;2;%d;%d;%dm", r, g, b);
            } else {
                snprintf(prefix, prefix_cap, "\x1b[38;2;%d;%d;%dm", r, g, b);
            }
        }
        snprintf(suffix, suffix_cap, "\x1b[0m");
        return;
    }

    if (pause_eq_icase(value, "black")) code = 0;
    else if (pause_eq_icase(value, "red")) code = 1;
    else if (pause_eq_icase(value, "green")) code = 2;
    else if (pause_eq_icase(value, "yellow")) code = 3;
    else if (pause_eq_icase(value, "blue")) code = 4;
    else if (pause_eq_icase(value, "magenta") || pause_eq_icase(value, "purple")) code = 5;
    else if (pause_eq_icase(value, "cyan")) code = 6;
    else if (pause_eq_icase(value, "white") || pause_eq_icase(value, "grey") || pause_eq_icase(value, "gray")) code = 7;

    if (code >= 0) {
        if (pause_eq_icase(kind, "bg")) {
            code += 40;
        } else {
            code += 30;
        }

        if (bold) {
            snprintf(prefix, prefix_cap, "\x1b[1;%dm", code);
        } else {
            snprintf(prefix, prefix_cap, "\x1b[%dm", code);
        }
        snprintf(suffix, suffix_cap, "\x1b[0m");
    }
}

#ifdef _WIN32
static int pause_wait_windows(int timeout_ms) {
    HANDLE in = GetStdHandle(STD_INPUT_HANDLE);
    DWORD old_mode = 0;
    int has_mode = 0;
    ULONGLONG start_ms;

    if (!in || in == INVALID_HANDLE_VALUE) {
        if (timeout_ms < 0) {
            (void)_getch();
            return 0;
        }
        if (timeout_ms == 0) return 1;

        start_ms = GetTickCount64();
        while ((int)(GetTickCount64() - start_ms) < timeout_ms) {
            if (_kbhit()) {
                (void)_getch();
                return 0;
            }
            Sleep(20);
        }
        return 1;
    }

    if (GetConsoleMode(in, &old_mode)) {
        DWORD new_mode = old_mode;
        has_mode = 1;
        new_mode |= ENABLE_EXTENDED_FLAGS;
        new_mode &= ~ENABLE_QUICK_EDIT_MODE;
        new_mode |= ENABLE_MOUSE_INPUT;
        new_mode |= ENABLE_WINDOW_INPUT;
        SetConsoleMode(in, new_mode);
    }

    start_ms = GetTickCount64();
    while (1) {
        DWORD wait_ms = INFINITE;
        DWORD wr;
        INPUT_RECORD rec[16];
        DWORD got = 0;
        DWORD i;

        if (timeout_ms >= 0) {
            ULONGLONG elapsed = GetTickCount64() - start_ms;
            if ((int)elapsed >= timeout_ms) {
                if (has_mode) SetConsoleMode(in, old_mode);
                return 1;
            }
            wait_ms = (DWORD)(timeout_ms - (int)elapsed);
            if (wait_ms > 50) wait_ms = 50;
        }

        wr = WaitForSingleObject(in, wait_ms);
        if (wr == WAIT_TIMEOUT) {
            continue;
        }
        if (wr != WAIT_OBJECT_0) {
            if (_kbhit()) {
                (void)_getch();
                if (has_mode) SetConsoleMode(in, old_mode);
                return 0;
            }
            continue;
        }

        if (!ReadConsoleInputW(in, rec, (DWORD)(sizeof(rec) / sizeof(rec[0])), &got) || got == 0) {
            continue;
        }

        for (i = 0; i < got; ++i) {
            if (rec[i].EventType == KEY_EVENT) {
                if (rec[i].Event.KeyEvent.bKeyDown) {
                    if (has_mode) SetConsoleMode(in, old_mode);
                    return 0;
                }
            } else if (rec[i].EventType == MOUSE_EVENT) {
                MOUSE_EVENT_RECORD m = rec[i].Event.MouseEvent;
                if (m.dwEventFlags == 0 || m.dwEventFlags == DOUBLE_CLICK) {
                    if (has_mode) SetConsoleMode(in, old_mode);
                    return 0;
                }
            }
        }
    }
}
#endif

#ifndef _WIN32
static int pause_wait_posix(int timeout_ms) {
    int fd = fileno(stdin);
    struct termios old_attr;
    struct termios raw_attr;
    int has_tty;

    has_tty = isatty(fd);
    if (!has_tty) {
        if (timeout_ms < 0) {
            (void)getchar();
            return 0;
        }
        if (timeout_ms == 0) return 1;
        {
            fd_set rfds;
            struct timeval tv;
            FD_ZERO(&rfds);
            FD_SET(fd, &rfds);
            tv.tv_sec = timeout_ms / 1000;
            tv.tv_usec = (timeout_ms % 1000) * 1000;
            if (select(fd + 1, &rfds, NULL, NULL, &tv) > 0) {
                char ch;
                (void)read(fd, &ch, 1);
                return 0;
            }
            return 1;
        }
    }

    if (tcgetattr(fd, &old_attr) != 0) {
        if (timeout_ms < 0) {
            (void)getchar();
            return 0;
        }
        if (timeout_ms == 0) return 1;
        {
            fd_set rfds;
            struct timeval tv;
            FD_ZERO(&rfds);
            FD_SET(fd, &rfds);
            tv.tv_sec = timeout_ms / 1000;
            tv.tv_usec = (timeout_ms % 1000) * 1000;
            if (select(fd + 1, &rfds, NULL, NULL, &tv) > 0) {
                char ch;
                (void)read(fd, &ch, 1);
                return 0;
            }
            return 1;
        }
    }

    raw_attr = old_attr;
    raw_attr.c_lflag &= (unsigned int)(~(ICANON | ECHO));
    raw_attr.c_cc[VMIN] = 1;
    raw_attr.c_cc[VTIME] = 0;
    (void)tcsetattr(fd, TCSANOW, &raw_attr);

    if (timeout_ms < 0) {
        char ch;
        (void)read(fd, &ch, 1);
        (void)tcsetattr(fd, TCSANOW, &old_attr);
        return 0;
    }

    if (timeout_ms == 0) {
        (void)tcsetattr(fd, TCSANOW, &old_attr);
        return 1;
    }

    {
        fd_set rfds;
        struct timeval tv;
        int ret;
        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        ret = select(fd + 1, &rfds, NULL, NULL, &tv);
        if (ret > 0) {
            char ch;
            (void)read(fd, &ch, 1);
            (void)tcsetattr(fd, TCSANOW, &old_attr);
            return 0;
        }
    }

    (void)tcsetattr(fd, TCSANOW, &old_attr);
    return 1;
}
#endif

int pause_wait(const char *message, int timeout_ms, const char *ansi_spec) {
    const char *msg = message ? message : "";
    char prefix[96];
    char suffix[16];
    int timed_out = 0;

    pause_style_to_ansi(ansi_spec, prefix, sizeof(prefix), suffix, sizeof(suffix));
    if (prefix[0]) {
        printf("%s%s%s", prefix, msg, suffix);
    } else {
        printf("%s", msg);
    }
    fflush(stdout);

    if (timeout_ms == 0) {
        printf("\n");
        fflush(stdout);
        return 1;
    }

#ifdef _WIN32
    timed_out = pause_wait_windows(timeout_ms);
#else
    timed_out = pause_wait_posix(timeout_ms);
#endif

    printf("\n");
    fflush(stdout);
    return timed_out;
}
