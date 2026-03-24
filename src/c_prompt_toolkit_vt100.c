#include "c_prompt_toolkit_vt100.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

enum {
    CPTK_VT_STATE_GROUND = 0,
    CPTK_VT_STATE_ESC = 1,
    CPTK_VT_STATE_CSI = 2
};

static void cptk_clear_event(cptk_vt100_event *ev) {
    if (!ev) {
        return;
    }
    memset(ev, 0, sizeof(*ev));
    ev->key = CPTK_VT100_KEY_NONE;
}

void cptk_vt100_parser_init(cptk_vt100_parser *parser) {
    if (!parser) {
        return;
    }
    memset(parser, 0, sizeof(*parser));
    parser->state = CPTK_VT_STATE_GROUND;
}

static int cptk_parse_csi_number(const char *buf, size_t len, size_t *cursor) {
    int value = 0;
    size_t i = *cursor;
    while (i < len && isdigit((unsigned char)buf[i])) {
        value = value * 10 + (buf[i] - '0');
        i++;
    }
    *cursor = i;
    return value;
}

static int cptk_decode_csi(cptk_vt100_parser *parser, cptk_vt100_event *out_event) {
    size_t len = parser->csi_len;
    char final_char;
    const char *buf = parser->csi_buf;

    if (len == 0) {
        return 0;
    }
    final_char = buf[len - 1];

    cptk_clear_event(out_event);

    if (final_char == 'A') {
        out_event->key = CPTK_VT100_KEY_UP;
        return 1;
    }
    if (final_char == 'B') {
        out_event->key = CPTK_VT100_KEY_DOWN;
        return 1;
    }
    if (final_char == 'C') {
        out_event->key = CPTK_VT100_KEY_RIGHT;
        return 1;
    }
    if (final_char == 'D') {
        out_event->key = CPTK_VT100_KEY_LEFT;
        return 1;
    }
    if (final_char == 'H') {
        out_event->key = CPTK_VT100_KEY_HOME;
        return 1;
    }
    if (final_char == 'F') {
        out_event->key = CPTK_VT100_KEY_END;
        return 1;
    }

    if (final_char == '~') {
        size_t cursor = 0;
        int code = cptk_parse_csi_number(buf, len - 1, &cursor);
        if (code == 1 || code == 7) {
            out_event->key = CPTK_VT100_KEY_HOME;
            return 1;
        }
        if (code == 4 || code == 8) {
            out_event->key = CPTK_VT100_KEY_END;
            return 1;
        }
        if (code == 3) {
            out_event->key = CPTK_VT100_KEY_DELETE;
            return 1;
        }
        return 0;
    }

    if ((final_char == 'M' || final_char == 'm') && buf[0] == '<') {
        size_t cursor = 1;
        int b = cptk_parse_csi_number(buf, len - 1, &cursor);
        int x = 0;
        int y = 0;

        if (cursor < len && buf[cursor] == ';') {
            cursor++;
            x = cptk_parse_csi_number(buf, len - 1, &cursor);
        }
        if (cursor < len && buf[cursor] == ';') {
            cursor++;
            y = cptk_parse_csi_number(buf, len - 1, &cursor);
        }

        out_event->is_mouse = 1;
        out_event->mouse_button = b;
        out_event->mouse_x = x;
        out_event->mouse_y = y;
        out_event->mouse_is_release = (final_char == 'm') ? 1 : 0;
        return 1;
    }

    return 0;
}

int cptk_vt100_feed_byte(cptk_vt100_parser *parser, unsigned char b, cptk_vt100_event *out_event) {
    if (!parser || !out_event) {
        return 0;
    }

    cptk_clear_event(out_event);

    if (parser->state == CPTK_VT_STATE_GROUND) {
        if (b == 0x1B) {
            parser->state = CPTK_VT_STATE_ESC;
            return 0;
        }
        if (b == '\r' || b == '\n') {
            out_event->key = CPTK_VT100_KEY_ENTER;
            return 1;
        }
        if (b == 0x08 || b == 0x7F) {
            out_event->key = CPTK_VT100_KEY_BACKSPACE;
            return 1;
        }
        if (b == 0x09) {
            out_event->key = CPTK_VT100_KEY_TAB;
            return 1;
        }
        if (b == 0x03) {
            out_event->key = CPTK_VT100_KEY_CTRL_C;
            return 1;
        }
        if (b >= 0x20) {
            out_event->key = CPTK_VT100_KEY_TEXT;
            out_event->codepoint = (uint32_t)b;
            return 1;
        }
        return 0;
    }

    if (parser->state == CPTK_VT_STATE_ESC) {
        if (b == '[') {
            parser->state = CPTK_VT_STATE_CSI;
            parser->csi_len = 0;
            memset(parser->csi_buf, 0, sizeof(parser->csi_buf));
            return 0;
        }
        parser->state = CPTK_VT_STATE_GROUND;
        out_event->key = CPTK_VT100_KEY_ESCAPE;
        return 1;
    }

    if (parser->state == CPTK_VT_STATE_CSI) {
        if (parser->csi_len + 1 < sizeof(parser->csi_buf)) {
            parser->csi_buf[parser->csi_len++] = (char)b;
            parser->csi_buf[parser->csi_len] = '\0';
        } else {
            parser->state = CPTK_VT_STATE_GROUND;
            parser->csi_len = 0;
            return 0;
        }

        if ((b >= '@' && b <= '~') || b == '~') {
            int emitted = cptk_decode_csi(parser, out_event);
            parser->state = CPTK_VT_STATE_GROUND;
            parser->csi_len = 0;
            memset(parser->csi_buf, 0, sizeof(parser->csi_buf));
            return emitted;
        }
        return 0;
    }

    parser->state = CPTK_VT_STATE_GROUND;
    return 0;
}
