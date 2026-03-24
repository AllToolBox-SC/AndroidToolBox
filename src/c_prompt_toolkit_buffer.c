#include "c_prompt_toolkit_buffer.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define CPTK_BUFFER_MIN_CAP 64u

static int cptk_buffer_reserve(cptk_buffer *buf, size_t need) {
    char *next;
    size_t next_cap;

    if (!buf) {
        return -1;
    }
    if (need + 1 <= buf->cap) {
        return 0;
    }

    next_cap = (buf->cap > 0) ? buf->cap : CPTK_BUFFER_MIN_CAP;
    while (need + 1 > next_cap) {
        next_cap *= 2u;
    }

    next = (char *)realloc(buf->data, next_cap);
    if (!next) {
        return -1;
    }

    buf->data = next;
    buf->cap = next_cap;
    return 0;
}

static int cptk_buffer_is_word_char(char ch) {
    unsigned char u = (unsigned char)ch;
    return isalnum(u) || ch == '_';
}

static size_t cptk_buffer_find_word_forward(const cptk_buffer *buf, size_t from) {
    size_t i;
    const char *s;

    if (!buf || !buf->data) {
        return 0;
    }
    s = buf->data;
    i = (from <= buf->len) ? from : buf->len;
    if (i >= buf->len) {
        return buf->len;
    }

    if (cptk_buffer_is_word_char(s[i])) {
        while (i < buf->len && cptk_buffer_is_word_char(s[i])) {
            i++;
        }
    }
    while (i < buf->len && !cptk_buffer_is_word_char(s[i])) {
        i++;
    }
    return i;
}

static size_t cptk_buffer_find_word_backward(const cptk_buffer *buf, size_t from) {
    size_t i;
    const char *s;

    if (!buf || !buf->data || from == 0) {
        return 0;
    }
    s = buf->data;
    i = (from <= buf->len) ? from : buf->len;
    if (i > 0) {
        i--;
    }

    while (i > 0 && !cptk_buffer_is_word_char(s[i])) {
        i--;
    }
    while (i > 0 && cptk_buffer_is_word_char(s[i - 1])) {
        i--;
    }
    return i;
}

static size_t cptk_buffer_find_word_end(const cptk_buffer *buf, size_t from) {
    size_t i;
    const char *s;

    if (!buf || !buf->data) {
        return 0;
    }
    s = buf->data;
    i = (from <= buf->len) ? from : buf->len;
    if (i >= buf->len) {
        return buf->len;
    }

    while (i < buf->len && !cptk_buffer_is_word_char(s[i])) {
        i++;
    }
    if (i >= buf->len) {
        return buf->len;
    }
    while (i + 1 < buf->len && cptk_buffer_is_word_char(s[i + 1])) {
        i++;
    }
    return i;
}

int cptk_buffer_init(cptk_buffer *buf, size_t initial_capacity) {
    if (!buf) {
        return -1;
    }

    memset(buf, 0, sizeof(*buf));
    if (initial_capacity < CPTK_BUFFER_MIN_CAP) {
        initial_capacity = CPTK_BUFFER_MIN_CAP;
    }

    buf->data = (char *)calloc(1, initial_capacity);
    if (!buf->data) {
        return -1;
    }

    buf->cap = initial_capacity;
    buf->len = 0;
    buf->cursor = 0;
    return 0;
}

void cptk_buffer_free(cptk_buffer *buf) {
    if (!buf) {
        return;
    }

    free(buf->data);
    buf->data = NULL;
    buf->len = 0;
    buf->cap = 0;
    buf->cursor = 0;
}

int cptk_buffer_set_text(cptk_buffer *buf, const char *text) {
    size_t n = 0;

    if (!buf) {
        return -1;
    }
    if (text) {
        n = strlen(text);
    }

    if (cptk_buffer_reserve(buf, n) != 0) {
        return -1;
    }

    if (n > 0) {
        memcpy(buf->data, text, n);
    }
    buf->data[n] = '\0';
    buf->len = n;
    buf->cursor = n;
    return 0;
}

const char *cptk_buffer_text(const cptk_buffer *buf) {
    if (!buf || !buf->data) {
        return "";
    }
    return buf->data;
}

size_t cptk_buffer_len(const cptk_buffer *buf) {
    return buf ? buf->len : 0;
}

size_t cptk_buffer_cursor(const cptk_buffer *buf) {
    return buf ? buf->cursor : 0;
}

int cptk_buffer_insert_ascii(cptk_buffer *buf, char ch) {
    if (!buf || !buf->data) {
        return -1;
    }

    if (cptk_buffer_reserve(buf, buf->len + 1) != 0) {
        return -1;
    }

    memmove(buf->data + buf->cursor + 1, buf->data + buf->cursor, buf->len - buf->cursor + 1);
    buf->data[buf->cursor] = ch;
    buf->cursor++;
    buf->len++;
    return 0;
}

int cptk_buffer_insert_text(cptk_buffer *buf, const char *text) {
    size_t n;

    if (!buf || !buf->data || !text) {
        return -1;
    }

    n = strlen(text);
    if (n == 0) {
        return 0;
    }
    if (cptk_buffer_reserve(buf, buf->len + n) != 0) {
        return -1;
    }

    memmove(buf->data + buf->cursor + n, buf->data + buf->cursor, buf->len - buf->cursor + 1);
    memcpy(buf->data + buf->cursor, text, n);
    buf->cursor += n;
    buf->len += n;
    return 0;
}

int cptk_buffer_backspace(cptk_buffer *buf) {
    if (!buf || !buf->data || buf->cursor == 0) {
        return 0;
    }

    memmove(buf->data + buf->cursor - 1, buf->data + buf->cursor, buf->len - buf->cursor + 1);
    buf->cursor--;
    buf->len--;
    return 0;
}

int cptk_buffer_delete(cptk_buffer *buf) {
    if (!buf || !buf->data || buf->cursor >= buf->len) {
        return 0;
    }

    memmove(buf->data + buf->cursor, buf->data + buf->cursor + 1, buf->len - buf->cursor);
    buf->len--;
    return 0;
}

int cptk_buffer_delete_span(cptk_buffer *buf, size_t start, size_t end) {
    size_t a;
    size_t b;
    size_t removed;

    if (!buf || !buf->data) {
        return -1;
    }

    a = (start <= buf->len) ? start : buf->len;
    b = (end <= buf->len) ? end : buf->len;
    if (a > b) {
        size_t t = a;
        a = b;
        b = t;
    }
    if (a == b) {
        buf->cursor = a;
        return 0;
    }

    removed = b - a;
    memmove(buf->data + a, buf->data + b, buf->len - b + 1);
    buf->len -= removed;
    buf->cursor = a;
    return 0;
}

void cptk_buffer_move_left(cptk_buffer *buf) {
    if (!buf) {
        return;
    }
    if (buf->cursor > 0) {
        buf->cursor--;
    }
}

void cptk_buffer_move_right(cptk_buffer *buf) {
    if (!buf) {
        return;
    }
    if (buf->cursor < buf->len) {
        buf->cursor++;
    }
}

void cptk_buffer_move_home(cptk_buffer *buf) {
    if (!buf) {
        return;
    }
    buf->cursor = 0;
}

void cptk_buffer_move_end(cptk_buffer *buf) {
    if (!buf) {
        return;
    }
    buf->cursor = buf->len;
}

size_t cptk_buffer_word_forward_target(const cptk_buffer *buf) {
    if (!buf) {
        return 0;
    }
    return cptk_buffer_find_word_forward(buf, buf->cursor);
}

size_t cptk_buffer_word_backward_target(const cptk_buffer *buf) {
    if (!buf) {
        return 0;
    }
    return cptk_buffer_find_word_backward(buf, buf->cursor);
}

size_t cptk_buffer_word_end_target(const cptk_buffer *buf) {
    if (!buf) {
        return 0;
    }
    return cptk_buffer_find_word_end(buf, buf->cursor);
}

void cptk_buffer_move_word_forward(cptk_buffer *buf) {
    if (!buf) {
        return;
    }
    buf->cursor = cptk_buffer_find_word_forward(buf, buf->cursor);
}

void cptk_buffer_move_word_backward(cptk_buffer *buf) {
    if (!buf) {
        return;
    }
    buf->cursor = cptk_buffer_find_word_backward(buf, buf->cursor);
}

void cptk_buffer_move_word_end(cptk_buffer *buf) {
    if (!buf) {
        return;
    }
    buf->cursor = cptk_buffer_find_word_end(buf, buf->cursor);
}

int cptk_buffer_copy_to(const cptk_buffer *buf, char *out, size_t out_capacity) {
    if (!buf || !out || out_capacity == 0) {
        return -1;
    }
    if (buf->len + 1 > out_capacity) {
        return -1;
    }

    memcpy(out, cptk_buffer_text(buf), buf->len + 1);
    return 0;
}
