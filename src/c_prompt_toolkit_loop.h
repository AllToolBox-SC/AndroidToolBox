#ifndef CPTK_LOOP_H
#define CPTK_LOOP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum cptk_loop_backend {
    CPTK_LOOP_BACKEND_FALLBACK = 0,
    CPTK_LOOP_BACKEND_LIBUV = 1
} cptk_loop_backend;

typedef struct cptk_loop {
    cptk_loop_backend backend;
    int running;
    uint64_t ticks;
#ifdef CPTK_ENABLE_LIBUV
    void *uv_loop;
    void *uv_timer;
#endif
} cptk_loop;

int cptk_loop_init(cptk_loop *loop);
void cptk_loop_close(cptk_loop *loop);
int cptk_loop_start(cptk_loop *loop);
void cptk_loop_stop(cptk_loop *loop);
int cptk_loop_poll(cptk_loop *loop);
const char *cptk_loop_backend_name(const cptk_loop *loop);

#ifdef __cplusplus
}
#endif

#endif
