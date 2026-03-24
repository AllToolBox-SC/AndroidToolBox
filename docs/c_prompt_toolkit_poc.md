# c_prompt_toolkit PoC Guide

## What is included

- Native C API draft (`src/c_prompt_toolkit.h`)
- v0.2 editor buffer split (`src/c_prompt_toolkit_buffer.*`)
- v0.2 keymap split (`src/c_prompt_toolkit_keymap.*`)
- VT100 parser (`src/c_prompt_toolkit_vt100.*`)
- Event loop abstraction with optional libuv backend (`src/c_prompt_toolkit_loop.*`)
- Prompt/menu implementation PoC (`src/c_prompt_toolkit.c`)
- Runnable demo (`src/c_prompt_toolkit_demo.c`)

## Build (Windows / MSVC)

```bat
build_c_prompt_toolkit_msvc.bat
```

If `cl.exe` is not on PATH, run in a Developer Command Prompt or call `vcvars64.bat` first.

## One-click real libuv build

```bat
build_c_prompt_toolkit_libuv_msvc.bat
```

This script will:
- ensure MSVC env is active,
- clone/build libuv into `third_party/libuv` and `build/libuv`,
- force compile with `CPTK_ENABLE_LIBUV=1` and `CPTK_REQUIRE_LIBUV=1`,
- run smoke verification and require `backend: libuv` in output.

## Optional libuv build

Set these env vars before build:

```bat
set CPTK_ENABLE_LIBUV=1
set LIBUV_INCLUDE=<path-to-libuv-include>
set LIBUV_LIB=<path-to-libuv-lib-dir>
set LIBUV_LIBNAME=uv.lib
build_c_prompt_toolkit_msvc.bat
```

## Run demo

```bat
bin\c_prompt_toolkit_demo.exe
```

Quick non-interactive smoke run:

```bat
(echo hello&echo 10)|bin\c_prompt_toolkit_demo.exe
```

Expected key output snippets:
- `=== c_prompt_toolkit PoC ===`
- `You typed: hello`
- `Selected: 10. 退出工具`

## Current limitations

- This is a PoC, not full prompt_toolkit parity yet.
- Full widget/layout/highlight/IME parity is not implemented yet.
- libuv backend path exists but requires local libuv install and link paths.

## Replay tests

```bat
cmd /c test\c_prompt_toolkit_smoke.bat
cmd /c test\run_replay_case.bat test\replay\case_main_menu_001.trace test\replay\case_main_menu_001.expect
cmd /c test\run_replay_keys_case.bat test\replay\case_keys_vi_001.trace test\replay\case_keys_vi_001.expect
cmd /c test\run_replay_keys_case.bat test\replay\case_keys_history_001.trace test\replay\case_keys_history_001.expect
cmd /c test\run_replay_keys_case.bat test\replay\case_keys_completion_001.trace test\replay\case_keys_completion_001.expect
```
