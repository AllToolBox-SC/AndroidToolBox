#ifndef CPTK_KEYMAP_H
#define CPTK_KEYMAP_H

#include "c_prompt_toolkit.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum cptk_editor_action {
    CPTK_EDITOR_ACTION_NONE = 0,
    CPTK_EDITOR_ACTION_ACCEPT,
    CPTK_EDITOR_ACTION_CANCEL,
    CPTK_EDITOR_ACTION_MOVE_LEFT,
    CPTK_EDITOR_ACTION_MOVE_RIGHT,
    CPTK_EDITOR_ACTION_MOVE_HOME,
    CPTK_EDITOR_ACTION_MOVE_END,
    CPTK_EDITOR_ACTION_BACKSPACE,
    CPTK_EDITOR_ACTION_DELETE,
    CPTK_EDITOR_ACTION_HISTORY_PREV,
    CPTK_EDITOR_ACTION_HISTORY_NEXT,
    CPTK_EDITOR_ACTION_COMPLETE,
    CPTK_EDITOR_ACTION_INSERT_TEXT,
    CPTK_EDITOR_ACTION_MOVE_WORD_FORWARD,
    CPTK_EDITOR_ACTION_MOVE_WORD_BACKWARD,
    CPTK_EDITOR_ACTION_MOVE_WORD_END,
    CPTK_EDITOR_ACTION_DELETE_TO_END,
    CPTK_EDITOR_ACTION_DELETE_WORD_FORWARD,
    CPTK_EDITOR_ACTION_DELETE_WORD_BACKWARD,
    CPTK_EDITOR_ACTION_DELETE_WORD_END,
    CPTK_EDITOR_ACTION_DELETE_LINE,
    CPTK_EDITOR_ACTION_CHANGE_TO_END,
    CPTK_EDITOR_ACTION_CHANGE_WORD_FORWARD,
    CPTK_EDITOR_ACTION_CHANGE_WORD_BACKWARD,
    CPTK_EDITOR_ACTION_CHANGE_WORD_END,
    CPTK_EDITOR_ACTION_CHANGE_LINE,
    CPTK_EDITOR_ACTION_YANK_TO_END,
    CPTK_EDITOR_ACTION_YANK_WORD_FORWARD,
    CPTK_EDITOR_ACTION_YANK_WORD_BACKWARD,
    CPTK_EDITOR_ACTION_YANK_WORD_END,
    CPTK_EDITOR_ACTION_YANK_LINE,
    CPTK_EDITOR_ACTION_PASTE_AFTER
} cptk_editor_action;

typedef enum cptk_vi_input_state {
    CPTK_VI_STATE_INSERT = 1,
    CPTK_VI_STATE_NORMAL = 0
} cptk_vi_input_state;

typedef enum cptk_vi_pending_op {
    CPTK_VI_OP_NONE = 0,
    CPTK_VI_OP_DELETE = 1,
    CPTK_VI_OP_CHANGE = 2,
    CPTK_VI_OP_YANK = 3
} cptk_vi_pending_op;

cptk_editor_action cptk_keymap_resolve(cptk_edit_mode mode, const cptk_key_event *key_event);
cptk_editor_action cptk_keymap_resolve_ex(cptk_edit_mode mode, int *vi_insert_mode, const cptk_key_event *key_event);
cptk_editor_action cptk_keymap_resolve_ex2(cptk_edit_mode mode, int *vi_insert_mode, int *vi_pending_op, const cptk_key_event *key_event);

#ifdef __cplusplus
}
#endif

#endif
