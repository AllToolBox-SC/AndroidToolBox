#include "c_prompt_toolkit_loop.h"

#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <windows.h>
#else
#include <time.h>
#endif

#ifdef CPTK_ENABLE_LIBUV
#include <uv.h>

typedef struct cptk_uv_state {
    uv_loop_t *loop;
    uv_timer_t *tick_timer;
    cptk_loop *owner;
} cptk_uv_state;

static void cptk_uv_tick_cb(uv_timer_t *timer) {
    cptk_uv_state *state = (cptk_uv_state *)timer->data;
    if (state && state->owner) {
        state->owner->ticks++;
    }
}

static void cptk_uv_noop_close_cb(uv_handle_t *handle) {
    (void)handle;
}
#endif

int cptk_loop_init(cptk_loop *loop) {
    if (!loop) {
        return -1;
    }
    memset(loop, 0, sizeof(*loop));
    loop->backend = CPTK_LOOP_BACKEND_FALLBACK;

#ifdef CPTK_ENABLE_LIBUV
    {
        cptk_uv_state *state = (cptk_uv_state *)calloc(1, sizeof(cptk_uv_state));
        if (!state) {
            goto libuv_init_failed;
        }

        state->loop = (uv_loop_t *)calloc(1, sizeof(uv_loop_t));
        state->tick_timer = (uv_timer_t *)calloc(1, sizeof(uv_timer_t));
        if (!state->loop || !state->tick_timer) {
            free(state->tick_timer);
            free(state->loop);
            free(state);
            goto libuv_init_failed;
        }

        if (uv_loop_init(state->loop) != 0) {
            free(state->tick_timer);
            free(state->loop);
            free(state);
            goto libuv_init_failed;
        }

        if (uv_timer_init(state->loop, state->tick_timer) != 0) {
            uv_loop_close(state->loop);
            free(state->tick_timer);
            free(state->loop);
            free(state);
            goto libuv_init_failed;
        }

        state->owner = loop;
        state->tick_timer->data = state;

        if (uv_timer_start(state->tick_timer, cptk_uv_tick_cb, 0, 16) != 0) {
            uv_loop_close(state->loop);
            free(state->tick_timer);
            free(state->loop);
            free(state);
            goto libuv_init_failed;
        }

        loop->uv_loop = state->loop;
        loop->uv_timer = state;
        loop->backend = CPTK_LOOP_BACKEND_LIBUV;
    }
#endif

    return 0;

#ifdef CPTK_ENABLE_LIBUV
libuv_init_failed:
#ifdef CPTK_REQUIRE_LIBUV
    return -2;
#else
    return 0;
#endif
#endif
}

void cptk_loop_close(cptk_loop *loop) {
    if (!loop) {
        return;
    }

#ifdef CPTK_ENABLE_LIBUV
    if (loop->backend == CPTK_LOOP_BACKEND_LIBUV && loop->uv_timer) {
        cptk_uv_state *state = (cptk_uv_state *)loop->uv_timer;
        if (state->tick_timer) {
            uv_timer_stop(state->tick_timer);
            uv_close((uv_handle_t *)state->tick_timer, cptk_uv_noop_close_cb);
        }
        if (state->loop) {
            uv_run(state->loop, UV_RUN_DEFAULT);
            uv_loop_close(state->loop);
        }
        free(state->tick_timer);
        free(state->loop);
        free(state);
    }
#endif

    memset(loop, 0, sizeof(*loop));
    loop->backend = CPTK_LOOP_BACKEND_FALLBACK;
}

int cptk_loop_start(cptk_loop *loop) {
    if (!loop) {
        return -1;
    }
    loop->running = 1;
    return 0;
}

void cptk_loop_stop(cptk_loop *loop) {
    if (!loop) {
        return;
    }
    loop->running = 0;
#ifdef CPTK_ENABLE_LIBUV
    if (loop->backend == CPTK_LOOP_BACKEND_LIBUV && loop->uv_loop) {
        uv_stop((uv_loop_t *)loop->uv_loop);
    }
#endif
}

int cptk_loop_poll(cptk_loop *loop) {
    if (!loop) {
        return -1;
    }

#ifdef CPTK_ENABLE_LIBUV
    if (loop->backend == CPTK_LOOP_BACKEND_LIBUV && loop->uv_loop) {
        uv_run((uv_loop_t *)loop->uv_loop, UV_RUN_NOWAIT);
        return 0;
    }
#endif

#ifdef _WIN32
    Sleep(0);
#else
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 1000000;
    nanosleep(&ts, NULL);
#endif
    loop->ticks++;
    return 0;
}

const char *cptk_loop_backend_name(const cptk_loop *loop) {
    if (!loop) {
        return "invalid";
    }
    if (loop->backend == CPTK_LOOP_BACKEND_LIBUV) {
        return "libuv";
    }
    return "fallback";
}
