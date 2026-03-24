#ifndef CPTK_VT100_H
#define CPTK_VT100_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum cptk_vt100_key_code {
    CPTK_VT100_KEY_NONE = 0,
    CPTK_VT100_KEY_TEXT,
    CPTK_VT100_KEY_ENTER,
    CPTK_VT100_KEY_BACKSPACE,
    CPTK_VT100_KEY_DELETE,
    CPTK_VT100_KEY_LEFT,
    CPTK_VT100_KEY_RIGHT,
    CPTK_VT100_KEY_UP,
    CPTK_VT100_KEY_DOWN,
    CPTK_VT100_KEY_HOME,
    CPTK_VT100_KEY_END,
    CPTK_VT100_KEY_TAB,
    CPTK_VT100_KEY_ESCAPE,
    CPTK_VT100_KEY_CTRL_C
} cptk_vt100_key_code;

typedef struct cptk_vt100_event {
    cptk_vt100_key_code key;
    uint32_t codepoint;
    int is_mouse;
    int mouse_x;
    int mouse_y;
    int mouse_button;
    int mouse_is_release;
} cptk_vt100_event;

typedef struct cptk_vt100_parser {
    int state;
    char csi_buf[64];
    size_t csi_len;
} cptk_vt100_parser;

void cptk_vt100_parser_init(cptk_vt100_parser *parser);
int cptk_vt100_feed_byte(cptk_vt100_parser *parser, unsigned char b, cptk_vt100_event *out_event);

#ifdef __cplusplus
}
#endif

#endif
