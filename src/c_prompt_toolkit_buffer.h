#ifndef CPTK_BUFFER_H
#define CPTK_BUFFER_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct cptk_buffer {
    char *data;
    size_t len;
    size_t cap;
    size_t cursor;
} cptk_buffer;

int cptk_buffer_init(cptk_buffer *buf, size_t initial_capacity);
void cptk_buffer_free(cptk_buffer *buf);
int cptk_buffer_set_text(cptk_buffer *buf, const char *text);
const char *cptk_buffer_text(const cptk_buffer *buf);
size_t cptk_buffer_len(const cptk_buffer *buf);
size_t cptk_buffer_cursor(const cptk_buffer *buf);

int cptk_buffer_insert_ascii(cptk_buffer *buf, char ch);
int cptk_buffer_insert_text(cptk_buffer *buf, const char *text);
int cptk_buffer_backspace(cptk_buffer *buf);
int cptk_buffer_delete(cptk_buffer *buf);
int cptk_buffer_delete_span(cptk_buffer *buf, size_t start, size_t end);
void cptk_buffer_move_left(cptk_buffer *buf);
void cptk_buffer_move_right(cptk_buffer *buf);
void cptk_buffer_move_home(cptk_buffer *buf);
void cptk_buffer_move_end(cptk_buffer *buf);
size_t cptk_buffer_word_forward_target(const cptk_buffer *buf);
size_t cptk_buffer_word_backward_target(const cptk_buffer *buf);
size_t cptk_buffer_word_end_target(const cptk_buffer *buf);
void cptk_buffer_move_word_forward(cptk_buffer *buf);
void cptk_buffer_move_word_backward(cptk_buffer *buf);
void cptk_buffer_move_word_end(cptk_buffer *buf);

int cptk_buffer_copy_to(const cptk_buffer *buf, char *out, size_t out_capacity);

#ifdef __cplusplus
}
#endif

#endif
