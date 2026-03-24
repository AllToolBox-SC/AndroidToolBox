# c_prompt_toolkit Design Draft (Phase 1)

## 1. Goal

This document defines the initial design for a native C implementation inspired by prompt_toolkit.
Target:
- Platform: Windows (MSVC) first.
- Runtime: native C runtime (no Python runtime required for core usage).
- Delivery: header + static/dynamic library + demo executable.
- Compatibility target: behavior-equivalent user experience for prompt/menu editing, not byte-for-byte internal parity.

## 2. Module Breakdown

### 2.1 `c_prompt_toolkit_buffer.{h,c}`
Responsibilities:
- Maintain prompt editor buffer and cursor state.
- Provide stable edit primitives (insert/delete/backspace/move/copy).

### 2.2 `c_prompt_toolkit_keymap.{h,c}`
Responsibilities:
- Resolve normalized key events to editor actions.
- Keep emacs/vi mode action mapping deterministic.

### 2.3 `c_prompt_toolkit_vt100.{h,c}`
Responsibilities:
- Parse VT100/ANSI input byte stream into normalized events.
- Decode arrows, home/end, delete, tab, enter, escape, ctrl-c.
- Decode SGR mouse (`CSI <b;x;yM/m`) into mouse events.

Public surface:
- `cptk_vt100_parser_init`
- `cptk_vt100_feed_byte`

### 2.4 `c_prompt_toolkit_loop.{h,c}`
Responsibilities:
- Abstract event loop backend.
- Provide default fallback backend.
- Optional `libuv` backend behind `CPTK_ENABLE_LIBUV`.

Public surface:
- `cptk_loop_init`, `cptk_loop_start`, `cptk_loop_poll`, `cptk_loop_stop`, `cptk_loop_close`
- `cptk_loop_backend_name`

### 2.5 `c_prompt_toolkit.{h,c}`
Responsibilities:
- Public API and context management.
- Prompt line editor with history/completion hooks.
- Menu choice helper.
- Logging and status mapping.

Public surface:
- Context lifecycle, callback registration, prompt run, menu run, history access.

### 2.6 `c_prompt_toolkit_demo.c`
Responsibilities:
- Show end-to-end PoC behavior.
- Demonstrate completion callback and menu rendering.

## 3. Key Data Structures

### 3.1 Context (`cptk_context`)
- Callback registry (`on_log`, `on_completion`, `on_highlight`, `on_event`).
- User data pointer.
- Edit mode (`emacs`/`vi`).
- Loop instance (`cptk_loop`).
- In-memory history ring/vector.

### 3.2 Buffer State (`cptk_buffer_state`)
- UTF-8 text view.
- Cursor index.
- Optional selection range.
- Multiline flag.

### 3.3 Input Event (`cptk_key_event`, `cptk_mouse_event`)
- Key code enum, codepoint and modifiers.
- Mouse x/y/button/release metadata.

### 3.4 Completion Containers
- `cptk_completion_item`
- `cptk_completion_list`

## 4. API Draft Snapshot

Primary API from `c_prompt_toolkit.h`:
- Version: `cptk_version_string`, `cptk_version_number`
- Context: `cptk_context_create`, `cptk_context_destroy`
- Callbacks: `cptk_context_set_callbacks`
- Prompt: `cptk_prompt_run`
- Menu: `cptk_menu_choice`
- History: `cptk_history_add`, `cptk_history_count`, `cptk_history_get`
- Loop: `cptk_loop_get`

## 5. PoC Scope (Implemented)

Included:
- Prompt rendering with cursor movement.
- Backspace/delete/home/end/left/right.
- History up/down.
- TAB completion hook (first suggestion).
- Basic menu selection.
- Optional libuv backend compile path.
- VT100 parser for key and mouse escape sequences.

Not yet included:
- Full multiline editing semantics.
- Full vi keymap and advanced keybinding resolver.
- Full layout/widgets system.
- Full style engine and syntax highlighting.
- IME pre-edit fidelity.

## 6. Build Notes

Current build script:
- `build_c_prompt_toolkit_msvc.bat`

Expected outputs:
- `bin/c_prompt_toolkit.dll`
- `bin/c_prompt_toolkit_demo.exe`

Optional libuv build (if installed):
- Define `CPTK_ENABLE_LIBUV`
- Provide include and library paths via environment variables in build script.

## 7. Next Design Deliverables (3-7 days)

1. API freeze proposal for v0.2.
2. State machine docs for editor and keybindings.
3. Render diff algorithm contract.
4. Test replay format for deterministic terminal behavior tests.
5. Compatibility matrix against prompt_toolkit use-cases in `start.py`/`menu.py`.
