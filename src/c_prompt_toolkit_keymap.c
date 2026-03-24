#include "c_prompt_toolkit_keymap.h"

static cptk_editor_action cptk_keymap_resolve_emacs(const cptk_key_event *key_event) {
    if (!key_event) {
        return CPTK_EDITOR_ACTION_NONE;
    }

    switch (key_event->key) {
        case CPTK_VT100_KEY_ENTER:
            return CPTK_EDITOR_ACTION_ACCEPT;
        case CPTK_VT100_KEY_CTRL_C:
            return CPTK_EDITOR_ACTION_CANCEL;
        case CPTK_VT100_KEY_LEFT:
            return CPTK_EDITOR_ACTION_MOVE_LEFT;
        case CPTK_VT100_KEY_RIGHT:
            return CPTK_EDITOR_ACTION_MOVE_RIGHT;
        case CPTK_VT100_KEY_HOME:
            return CPTK_EDITOR_ACTION_MOVE_HOME;
        case CPTK_VT100_KEY_END:
            return CPTK_EDITOR_ACTION_MOVE_END;
        case CPTK_VT100_KEY_BACKSPACE:
            return CPTK_EDITOR_ACTION_BACKSPACE;
        case CPTK_VT100_KEY_DELETE:
            return CPTK_EDITOR_ACTION_DELETE;
        case CPTK_VT100_KEY_UP:
            return CPTK_EDITOR_ACTION_HISTORY_PREV;
        case CPTK_VT100_KEY_DOWN:
            return CPTK_EDITOR_ACTION_HISTORY_NEXT;
        case CPTK_VT100_KEY_TAB:
            return CPTK_EDITOR_ACTION_COMPLETE;
        case CPTK_VT100_KEY_TEXT:
            return CPTK_EDITOR_ACTION_INSERT_TEXT;
        default:
            return CPTK_EDITOR_ACTION_NONE;
    }
}

static cptk_editor_action cptk_vi_action_from_pending(int pending_op, char ch, int *vi_insert_mode, int *vi_pending_op) {
    if (!vi_pending_op || pending_op == CPTK_VI_OP_NONE) {
        return CPTK_EDITOR_ACTION_NONE;
    }

    *vi_pending_op = CPTK_VI_OP_NONE;

    switch (pending_op) {
        case CPTK_VI_OP_DELETE:
            switch (ch) {
                case 'w': case 'W': return CPTK_EDITOR_ACTION_DELETE_WORD_FORWARD;
                case 'b': case 'B': return CPTK_EDITOR_ACTION_DELETE_WORD_BACKWARD;
                case 'e': case 'E': return CPTK_EDITOR_ACTION_DELETE_WORD_END;
                case '$': return CPTK_EDITOR_ACTION_DELETE_TO_END;
                case 'd': return CPTK_EDITOR_ACTION_DELETE_LINE;
                default: return CPTK_EDITOR_ACTION_NONE;
            }
        case CPTK_VI_OP_CHANGE:
            switch (ch) {
                case 'w': case 'W': return CPTK_EDITOR_ACTION_CHANGE_WORD_FORWARD;
                case 'b': case 'B': return CPTK_EDITOR_ACTION_CHANGE_WORD_BACKWARD;
                case 'e': case 'E': return CPTK_EDITOR_ACTION_CHANGE_WORD_END;
                case '$': return CPTK_EDITOR_ACTION_CHANGE_TO_END;
                case 'c': return CPTK_EDITOR_ACTION_CHANGE_LINE;
                default: return CPTK_EDITOR_ACTION_NONE;
            }
        case CPTK_VI_OP_YANK:
            switch (ch) {
                case 'w': case 'W': return CPTK_EDITOR_ACTION_YANK_WORD_FORWARD;
                case 'b': case 'B': return CPTK_EDITOR_ACTION_YANK_WORD_BACKWARD;
                case 'e': case 'E': return CPTK_EDITOR_ACTION_YANK_WORD_END;
                case '$': return CPTK_EDITOR_ACTION_YANK_TO_END;
                case 'y': return CPTK_EDITOR_ACTION_YANK_LINE;
                default: return CPTK_EDITOR_ACTION_NONE;
            }
        default:
            break;
    }

    if (vi_insert_mode) {
        (void)vi_insert_mode;
    }
    return CPTK_EDITOR_ACTION_NONE;
}

cptk_editor_action cptk_keymap_resolve_ex2(cptk_edit_mode mode, int *vi_insert_mode, int *vi_pending_op, const cptk_key_event *key_event) {
    int insert_mode = 1;
    int pending_op = CPTK_VI_OP_NONE;

    if (!key_event) {
        return CPTK_EDITOR_ACTION_NONE;
    }

    if (mode != CPTK_EDIT_MODE_VI) {
        return cptk_keymap_resolve_emacs(key_event);
    }

    if (vi_insert_mode) {
        insert_mode = *vi_insert_mode ? 1 : 0;
    }
    if (vi_pending_op) {
        pending_op = *vi_pending_op;
    }

    if (key_event->key == CPTK_VT100_KEY_CTRL_C) {
        return CPTK_EDITOR_ACTION_CANCEL;
    }
    if (key_event->key == CPTK_VT100_KEY_ENTER) {
        return CPTK_EDITOR_ACTION_ACCEPT;
    }
    if (key_event->key == CPTK_VT100_KEY_ESCAPE) {
        if (vi_insert_mode) {
            *vi_insert_mode = 0;
        }
        return CPTK_EDITOR_ACTION_NONE;
    }

    if (insert_mode) {
        if (vi_pending_op) {
            *vi_pending_op = CPTK_VI_OP_NONE;
        }
        switch (key_event->key) {
            case CPTK_VT100_KEY_LEFT:
                return CPTK_EDITOR_ACTION_MOVE_LEFT;
            case CPTK_VT100_KEY_RIGHT:
                return CPTK_EDITOR_ACTION_MOVE_RIGHT;
            case CPTK_VT100_KEY_HOME:
                return CPTK_EDITOR_ACTION_MOVE_HOME;
            case CPTK_VT100_KEY_END:
                return CPTK_EDITOR_ACTION_MOVE_END;
            case CPTK_VT100_KEY_BACKSPACE:
                return CPTK_EDITOR_ACTION_BACKSPACE;
            case CPTK_VT100_KEY_DELETE:
                return CPTK_EDITOR_ACTION_DELETE;
            case CPTK_VT100_KEY_UP:
                return CPTK_EDITOR_ACTION_HISTORY_PREV;
            case CPTK_VT100_KEY_DOWN:
                return CPTK_EDITOR_ACTION_HISTORY_NEXT;
            case CPTK_VT100_KEY_TAB:
                return CPTK_EDITOR_ACTION_COMPLETE;
            case CPTK_VT100_KEY_TEXT:
                return CPTK_EDITOR_ACTION_INSERT_TEXT;
            default:
                return CPTK_EDITOR_ACTION_NONE;
        }
    }

    if (key_event->key == CPTK_VT100_KEY_LEFT) {
        return CPTK_EDITOR_ACTION_MOVE_LEFT;
    }
    if (key_event->key == CPTK_VT100_KEY_RIGHT) {
        return CPTK_EDITOR_ACTION_MOVE_RIGHT;
    }
    if (key_event->key == CPTK_VT100_KEY_HOME) {
        return CPTK_EDITOR_ACTION_MOVE_HOME;
    }
    if (key_event->key == CPTK_VT100_KEY_END) {
        return CPTK_EDITOR_ACTION_MOVE_END;
    }
    if (key_event->key == CPTK_VT100_KEY_BACKSPACE || key_event->key == CPTK_VT100_KEY_DELETE) {
        return CPTK_EDITOR_ACTION_DELETE;
    }
    if (key_event->key == CPTK_VT100_KEY_UP) {
        return CPTK_EDITOR_ACTION_HISTORY_PREV;
    }
    if (key_event->key == CPTK_VT100_KEY_DOWN) {
        return CPTK_EDITOR_ACTION_HISTORY_NEXT;
    }
    if (key_event->key == CPTK_VT100_KEY_TAB) {
        return CPTK_EDITOR_ACTION_COMPLETE;
    }

    if (key_event->key != CPTK_VT100_KEY_TEXT) {
        if (vi_pending_op) {
            *vi_pending_op = CPTK_VI_OP_NONE;
        }
        return CPTK_EDITOR_ACTION_NONE;
    }

    if (pending_op != CPTK_VI_OP_NONE) {
        return cptk_vi_action_from_pending(pending_op, (char)key_event->codepoint, vi_insert_mode, vi_pending_op);
    }

    switch ((char)key_event->codepoint) {
        case 'h':
            return CPTK_EDITOR_ACTION_MOVE_LEFT;
        case 'l':
            return CPTK_EDITOR_ACTION_MOVE_RIGHT;
        case 'w':
        case 'W':
            return CPTK_EDITOR_ACTION_MOVE_WORD_FORWARD;
        case 'b':
        case 'B':
            return CPTK_EDITOR_ACTION_MOVE_WORD_BACKWARD;
        case 'e':
        case 'E':
            return CPTK_EDITOR_ACTION_MOVE_WORD_END;
        case '0':
            return CPTK_EDITOR_ACTION_MOVE_HOME;
        case '$':
            return CPTK_EDITOR_ACTION_MOVE_END;
        case 'x':
            return CPTK_EDITOR_ACTION_DELETE;
        case 'd':
            if (vi_pending_op) {
                *vi_pending_op = CPTK_VI_OP_DELETE;
            }
            return CPTK_EDITOR_ACTION_NONE;
        case 'c':
            if (vi_pending_op) {
                *vi_pending_op = CPTK_VI_OP_CHANGE;
            }
            return CPTK_EDITOR_ACTION_NONE;
        case 'y':
            if (vi_pending_op) {
                *vi_pending_op = CPTK_VI_OP_YANK;
            }
            return CPTK_EDITOR_ACTION_NONE;
        case 'p':
            if (vi_pending_op) {
                *vi_pending_op = CPTK_VI_OP_NONE;
            }
            return CPTK_EDITOR_ACTION_PASTE_AFTER;
        case 'k':
            return CPTK_EDITOR_ACTION_HISTORY_PREV;
        case 'j':
            return CPTK_EDITOR_ACTION_HISTORY_NEXT;
        case 'i':
            if (vi_insert_mode) {
                *vi_insert_mode = 1;
            }
            return CPTK_EDITOR_ACTION_NONE;
        case 'a':
            if (vi_insert_mode) {
                *vi_insert_mode = 1;
            }
            return CPTK_EDITOR_ACTION_MOVE_RIGHT;
        case 'A':
            if (vi_insert_mode) {
                *vi_insert_mode = 1;
            }
            return CPTK_EDITOR_ACTION_MOVE_END;
        default:
            if (vi_pending_op) {
                *vi_pending_op = CPTK_VI_OP_NONE;
            }
            return CPTK_EDITOR_ACTION_NONE;
    }
}

cptk_editor_action cptk_keymap_resolve_ex(cptk_edit_mode mode, int *vi_insert_mode, const cptk_key_event *key_event) {
    int vi_pending_op = CPTK_VI_OP_NONE;
    return cptk_keymap_resolve_ex2(mode, vi_insert_mode, &vi_pending_op, key_event);
}

cptk_editor_action cptk_keymap_resolve(cptk_edit_mode mode, const cptk_key_event *key_event) {
    int vi_insert_mode = 1;
    int vi_pending_op = CPTK_VI_OP_NONE;
    return cptk_keymap_resolve_ex2(mode, &vi_insert_mode, &vi_pending_op, key_event);
}
