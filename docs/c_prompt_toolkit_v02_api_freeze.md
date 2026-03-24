# c_prompt_toolkit v0.2 API Freeze Draft

This file freezes the initial native-C API shape for the editor core split.

## Frozen module boundaries

- `c_prompt_toolkit.h`: public context/prompt/menu/history APIs.
- `c_prompt_toolkit_buffer.{h,c}`: internal editor buffer model.
- `c_prompt_toolkit_keymap.{h,c}`: key-event to action resolver.
- `c_prompt_toolkit_vt100.{h,c}`: vt100 event parser.
- `c_prompt_toolkit_loop.{h,c}`: event-loop abstraction (libuv/fallback).

## Frozen internal contracts

### Buffer contract

- Buffer stores UTF-8 bytes and tracks cursor in byte offsets.
- Must support insertion/deletion/home/end/left/right in O(n) worst-case for PoC.
- Public snapshot uses `cptk_buffer_state` from `c_prompt_toolkit.h`.

### Keymap contract

- Input: `cptk_key_event` + `cptk_edit_mode`.
- Output: one deterministic `cptk_editor_action`.
- v0.2 behavior target:
  - Emacs mode: standard arrow/home/end/backspace/delete/tab/enter.
  - Vi mode: temporary compatibility alias to emacs action map in PoC.

### Loop contract

- `CPTK_ENABLE_LIBUV` path must report backend `libuv` when linked successfully.
- Fallback path remains available for constrained environments.

## Compatibility objective for this freeze

- Rendering and behavior are implementation-different but user-visible prompt/menu flow remains equivalent to current ATB usage.
- Priority order:
  1. Cursor correctness.
  2. Key navigation/history.
  3. Completion callback.
  4. Menu selection stability.

## Deferred from v0.2

- Full multiline widgets/layout parity.
- Full IME preedit parity.
- Pygments-compatible syntax highlighting.

## Acceptance checks

- Build succeeds with MSVC and optional libuv.
- Smoke test passes.
- Replay case format is defined and at least one replay case passes.
