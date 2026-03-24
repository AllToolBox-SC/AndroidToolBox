#ifndef C_PROMPT_TOOLKIT_H
#define C_PROMPT_TOOLKIT_H

#include <stddef.h>
#include <stdint.h>

#include "c_prompt_toolkit_loop.h"
#include "c_prompt_toolkit_vt100.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef CPTK_API
#  ifdef CPTK_BUILD_DLL
#    define CPTK_API __declspec(dllexport)
#  else
#    define CPTK_API
#  endif
#endif

typedef enum cptk_status {
    CPTK_STATUS_OK = 0,
    CPTK_STATUS_EINVAL = -1,
    CPTK_STATUS_ENOMEM = -2,
    CPTK_STATUS_EIO = -3,
    CPTK_STATUS_ENOTSUP = -4,
    CPTK_STATUS_EINTERNAL = -5
} cptk_status;

typedef enum cptk_log_level {
    CPTK_LOG_ERROR = 0,
    CPTK_LOG_WARN = 1,
    CPTK_LOG_INFO = 2,
    CPTK_LOG_DEBUG = 3,
    CPTK_LOG_TRACE = 4
} cptk_log_level;

typedef enum cptk_edit_mode {
    CPTK_EDIT_MODE_EMACS = 0,
    CPTK_EDIT_MODE_VI = 1
} cptk_edit_mode;

typedef enum cptk_key_mod {
    CPTK_MOD_NONE = 0,
    CPTK_MOD_SHIFT = 1 << 0,
    CPTK_MOD_ALT = 1 << 1,
    CPTK_MOD_CTRL = 1 << 2
} cptk_key_mod;

typedef struct cptk_key_event {
    cptk_vt100_key_code key;
    uint32_t codepoint;
    uint32_t modifiers;
} cptk_key_event;

typedef struct cptk_mouse_event {
    int x;
    int y;
    int button;
    int is_release;
    int present;
} cptk_mouse_event;

typedef struct cptk_buffer_state {
    const char *text_utf8;
    size_t text_len;
    size_t cursor;
    size_t selection_start;
    size_t selection_end;
    int multiline;
} cptk_buffer_state;

typedef struct cptk_completion_item {
    const char *text;
    const char *display;
    const char *meta;
} cptk_completion_item;

typedef struct cptk_completion_list {
    cptk_completion_item *items;
    size_t count;
    size_t capacity;
} cptk_completion_list;

typedef struct cptk_render_region {
    int row;
    int col;
    int width;
    int height;
} cptk_render_region;

typedef struct cptk_prompt_options {
    const char *prompt_text;
    int multiline;
    int enable_mouse;
    int enable_history;
    int enable_completion;
    int render_bottom_toolbar;
    int history_limit;
    cptk_edit_mode edit_mode;
} cptk_prompt_options;

typedef void (*cptk_log_callback)(cptk_log_level level, const char *message, void *user_data);
typedef int (*cptk_completion_callback)(const cptk_buffer_state *state, cptk_completion_list *out_list, void *user_data);
typedef int (*cptk_highlight_callback)(const cptk_buffer_state *state, cptk_render_region *dirty_regions, size_t region_count, void *user_data);
typedef void (*cptk_event_callback)(const cptk_key_event *key_event, const cptk_mouse_event *mouse_event, void *user_data);

typedef struct cptk_callbacks {
    cptk_log_callback on_log;
    cptk_completion_callback on_completion;
    cptk_highlight_callback on_highlight;
    cptk_event_callback on_event;
} cptk_callbacks;

typedef struct cptk_context cptk_context;

CPTK_API const char *cptk_version_string(void);
CPTK_API uint32_t cptk_version_number(void);

CPTK_API void cptk_default_prompt_options(cptk_prompt_options *options);
CPTK_API cptk_context *cptk_context_create(void);
CPTK_API void cptk_context_destroy(cptk_context *ctx);
CPTK_API cptk_status cptk_context_set_user_data(cptk_context *ctx, void *user_data);
CPTK_API void *cptk_context_get_user_data(cptk_context *ctx);
CPTK_API cptk_status cptk_context_set_callbacks(cptk_context *ctx, const cptk_callbacks *callbacks);
CPTK_API cptk_status cptk_context_set_log_level(cptk_context *ctx, cptk_log_level level);
CPTK_API cptk_status cptk_context_set_edit_mode(cptk_context *ctx, cptk_edit_mode mode);

CPTK_API cptk_status cptk_history_add(cptk_context *ctx, const char *line_utf8);
CPTK_API size_t cptk_history_count(const cptk_context *ctx);
CPTK_API const char *cptk_history_get(const cptk_context *ctx, size_t index);

CPTK_API cptk_status cptk_prompt_run(
    cptk_context *ctx,
    const cptk_prompt_options *options,
    char *out_line_utf8,
    size_t out_capacity
);

CPTK_API cptk_status cptk_menu_choice(
    cptk_context *ctx,
    const char *title,
    const char *const *items,
    size_t item_count,
    size_t default_index,
    size_t *out_selected_index
);

/* Interactive menu that supports VT mouse reports and returns the
 * selected index via the interactive prompt loop. When running in a
 * VT-capable terminal this provides clickable / touchable selection.
 */
CPTK_API cptk_status cptk_menu_choice_interactive(
    cptk_context *ctx,
    const char *title,
    const char *const *items,
    size_t item_count,
    size_t default_index,
    size_t *out_selected_index
);

/* Multi-select interactive menu.
 * - `out_selected_mask` must point to a buffer of at least `item_count` bytes.
 *   On success each byte is 0/1 indicating whether the corresponding item
 *   was selected. Caller decides how to interpret the selection values.
 * - `animate_in` when non-zero will reveal items with a small animation.
 * - `max_height` when >0 can limit rendering height (0 == auto/full).
 */
CPTK_API cptk_status cptk_menu_multi_choice_interactive(
    cptk_context *ctx,
    const char *title,
    const char *const *items,
    size_t item_count,
    unsigned char *out_selected_mask,
    int animate_in,
    int max_height
);

CPTK_API cptk_status cptk_loop_get(const cptk_context *ctx, const cptk_loop **out_loop);

CPTK_API const char *cptk_status_message(cptk_status status);

#ifdef __cplusplus
}
#endif

#endif
