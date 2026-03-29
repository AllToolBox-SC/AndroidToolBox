# -*- coding: utf-8 -*-

"""AllToolBox startup script and interactive menu entrypoint."""

import logging
import os
import sys
import shutil
import platform
import time
import datetime
import asyncio
from typing import Optional, Tuple, Iterable, List, Dict, Any
from prompt_toolkit import (
    print_formatted_text,
    ANSI,
    HTML,
    prompt,
    PromptSession
)
from prompt_toolkit.styles import Style, merge_styles
from prompt_toolkit.shortcuts import clear, set_title
from prompt_toolkit.key_binding import KeyBindings, merge_key_bindings
from prompt_toolkit.application import Application
from prompt_toolkit.application.current import get_app
from prompt_toolkit.layout import Layout
from prompt_toolkit.layout.containers import Window
from prompt_toolkit.layout.controls import FormattedTextControl
from prompt_toolkit.mouse_events import MouseEventType, MouseButton
from functools import wraps
from flash_ak3 import AnyKernel3
from musicplayer import AudioPlayer
import colorama
import subprocess
import socket
import requests
import filehash
import json
import traceback
import locale
import threading
import uuid
import atexit
import pluginutils, pluginutils.load, pluginutils.manage

try:
    from build_info import BUILD_TYPE
except (ImportError, ModuleNotFoundError):
    BUILD_TYPE = "release"

ALLOW_XTC = True


style = Style.from_dict({
    "yellow": "fg:#f1c40f",
    "red": "fg:#ff7b7b",
    "orange": "fg:#f4a261",
    "info": "fg:#3B78FF",
    "black": "fg:#0c1118",
    "cyan": "fg:#67e0c2",
    "green": "fg:#7ad1a8",
    "blue": "fg:#8ab4f8",
    "magenta": "fg:#c7a0ff",
    "white": "fg:#e6edf3",
    "reset": "",
    "number": "bold",
    "selected-option": "fg:#e6edf3 bold underline",
})

INFO = "<info>[信息]</info>"
ERROR = "<red>[错误]</red>"
WARN = "<orange>[警告]</orange>"

flag = False
key = False
started = False
_RUN_ENV_READY = False
_RUN_ENV_CACHE: Optional[Dict[str, str]] = None
_PATHEXT_EXTRA = [".COM", ".EXE", ".BAT", ".CMD", ".VBS", ".VBE", ".JS", ".JSE", ".WSF", ".WSH", ".MSC"]
_RUN_SHELL: Optional["PersistentCmdShell"] = None


def _normalize_build_type(value: str) -> str:
    return "debug" if str(value).strip().lower() == "debug" else "release"


BUILD_MODE = _normalize_build_type(BUILD_TYPE)
CURRENT_BUILD_META: Dict[str, str] = {
    "ro.build.type": BUILD_MODE,
    "persist.atb.xtc.allow": "True",
}

LINE = "-" * 68
DEBUG = BUILD_MODE == "debug"
allow_xtc = True


class MultilineFormatter(logging.Formatter):
    def format(self, record):
        original = super().format(record)
        lines = original.splitlines()
        if len(lines) <= 1:
            return original
        prefix = lines[0][:len(lines[0]) - len(record.getMessage())]
        return '\n'.join([lines[0]] + [prefix + line for line in lines[1:]])


class BreakOut(Exception):
    pass


""" UTILITY FUNCTIONS """

def page_transition(text: str = "", duration: float = 0.35) -> None:
    """Simple console transition animation when switching pages."""
    msg = text.strip() or "正在切换..."
    frames = ["", ".", "..", "...", " ..", "  ."]
    end_time = time.monotonic() + duration
    width = len(msg) + 4
    while time.monotonic() < end_time:
        for frame in frames:
            if time.monotonic() >= end_time:
                break
            sys.stdout.write(f"\r{msg}{frame:<3}")
            sys.stdout.flush()
            time.sleep(max(duration / (len(frames) * 2), 0.02))
    sys.stdout.write("\r" + " " * width + "\r")
    sys.stdout.flush()


def onerror(fn):
    @wraps(fn)
    def wrapper(*args, **kwargs):
        try:
            return fn(*args, **kwargs)
        except Exception as e:
            logger.error(f"Error in {fn.__name__}: {e}")
            logger.exception("msg")
            print_formatted_text(HTML(ERROR + "抱歉，脚本遇到了未经捕获的异常，即将退出..."), style=style)
            print_formatted_text(HTML(INFO + "错误详情已记录到 bin/logs 文件夹中，您可以将该文件发送给技术支持以获取帮助。"), style=style)
            time.sleep(3)
            cleanup(-1)
    return wrapper


def auto_clear(fn=None, *, logo=False, end=False):
    def decorator(func):
        @wraps(func)
        def wrapper(*args, **kwargs):
            clear()
            if logo:
                run("call .\\logo.bat")
            result = func(*args, **kwargs)
            if end: clear()
            return result
        return wrapper

    return decorator(fn) if fn is not None else decorator


def _build_run_env() -> Dict[str, str]:
    env = os.environ.copy()
    path = env.get("PATH", "")
    if "." not in path.split(";"):
        env["PATH"] = f".;{path}" if path else "."
    current_ext = str(env.get("PATHEXT") or "")
    ext_items = [item.strip() for item in current_ext.split(";") if item.strip()]
    ext_upper = {item.upper() for item in ext_items}
    for item in _PATHEXT_EXTRA:
        if item.upper() not in ext_upper:
            ext_items.append(item)
            ext_upper.add(item.upper())
    env["PATHEXT"] = ";".join(ext_items)
    return env


class PersistentCmdShell:
    def __init__(self, env: Dict[str, str]):
        self._encoding = "mbcs"
        self._lock = threading.Lock()
        self._proc = subprocess.Popen(
            ["cmd.exe", "/d", "/q", "/v:on"],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            encoding=self._encoding,
            errors="replace",
            env=env,
        )
        self._initialize_shell()

    def _initialize_shell(self) -> None:
        # One-time shell setup.
        self._write_line("@echo off")
        if os.path.isfile(".\\color.bat"):
            self._write_line("call .\\color.bat 1>nul 2>nul")
        self._flush_stdin()

    def _write_line(self, line: str) -> None:
        if not self._proc.stdin:
            raise RuntimeError("Persistent cmd stdin is unavailable")
        self._proc.stdin.write(line + "\n")

    def _flush_stdin(self) -> None:
        if not self._proc.stdin:
            raise RuntimeError("Persistent cmd stdin is unavailable")
        self._proc.stdin.flush()

    def is_alive(self) -> bool:
        return self._proc.poll() is None

    def close(self) -> None:
        with self._lock:
            try:
                if self.is_alive():
                    self._write_line("exit")
                    self._flush_stdin()
                    self._proc.wait(timeout=1)
            except Exception:
                pass
            try:
                if self._proc.stdin:
                    self._proc.stdin.close()
            except Exception:
                pass
            try:
                if self._proc.stdout:
                    self._proc.stdout.close()
            except Exception:
                pass

    def run_command(
        self,
        command: str,
        extra_env: Optional[Dict[str, str]] = None,
        capture_output: bool = False,
    ) -> Tuple[subprocess.CompletedProcess, Dict[str, str]]:
        if not self.is_alive():
            raise RuntimeError("Persistent cmd process has exited")

        token = uuid.uuid4().hex.upper()
        begin_marker = f"__ATB_BEGIN_{token}__"
        end_marker = f"__ATB_END_{token}__"
        env_begin_marker = f"__ATB_ENV_BEGIN_{token}__"
        env_end_marker = f"__ATB_ENV_END_{token}__"
        rc_prefix = f"__ATB_RC_{token}="

        output_lines: List[str] = []
        env_snapshot: Dict[str, str] = {}
        rc = 0
        in_payload = False
        in_env = False

        with self._lock:
            self._write_line("@echo off")
            if extra_env:
                for k, v in extra_env.items():
                    key = str(k)
                    value = str(v)
                    self._write_line(f'set "{key}={value}"')

            self._write_line(f"echo {begin_marker}")
            self._write_line(command)
            self._write_line("echo(")
            self._write_line(f"echo {rc_prefix}!ERRORLEVEL!")
            self._write_line(f"echo {end_marker}")
            self._write_line(f"echo {env_begin_marker}")
            self._write_line("set")
            self._write_line(f"echo {env_end_marker}")
            self._flush_stdin()

            if not self._proc.stdout:
                raise RuntimeError("Persistent cmd stdout is unavailable")

            # Timeout (seconds) used to detect a stuck "pause" prompt with no trailing newline.
            READLINE_TIMEOUT = 20.0

            while True:
                timer = None
                # Only enable the timeout-based auto-enter once we've entered the payload section.
                if in_payload:
                    def _send_enter():
                        try:
                            if self._proc.stdin:
                                self._proc.stdin.write("\n")
                                self._proc.stdin.flush()
                        except Exception:
                            pass

                    timer = threading.Timer(READLINE_TIMEOUT, _send_enter)
                    timer.daemon = True
                    timer.start()

                try:
                    line = self._proc.stdout.readline()
                finally:
                    if timer:
                        try:
                            timer.cancel()
                        except Exception:
                            pass

                if line == "":
                    raise RuntimeError("Persistent cmd stdout closed unexpectedly")

                clean = line.rstrip("\r\n")
                if clean == begin_marker:
                    in_payload = True
                    continue
                if clean == end_marker:
                    in_payload = False
                    continue
                if clean == env_begin_marker:
                    in_env = True
                    continue
                if clean == env_end_marker:
                    break
                if clean.startswith(rc_prefix):
                    try:
                        rc = int(clean.split("=", 1)[1].strip())
                    except Exception:
                        rc = 1
                    continue

                if in_env:
                    if "=" in clean:
                        key, value = clean.split("=", 1)
                        env_snapshot[key] = value
                    continue

                if in_payload:
                    # Detect pause prompts from batch scripts (e.g. "按任意键..." / "Press any key...")
                    is_pause_prompt = False
                    try:
                        if ("按任意键" in clean) or ("Press any key" in clean) or ("press any key" in clean.lower()):
                            is_pause_prompt = True
                    except Exception:
                        is_pause_prompt = False

                    output_lines.append(clean)
                    if not capture_output:
                        try:
                            # Ensure all output is adapted for ANSI color codes
                            print_formatted_text(ANSI(clean))
                        except Exception as e:
                            logging.error(f"Failed to render ANSI color: {e}")
                            print(clean)

                    if is_pause_prompt:
                        # Let the user acknowledge, then send an Enter to the shell so the batch can continue.
                        try:
                            pause()
                        except Exception:
                            try:
                                input()
                            except Exception:
                                pass
                        try:
                            if self._proc.stdin:
                                self._proc.stdin.write("\n")
                                self._proc.stdin.flush()
                        except Exception:
                            pass

        stdout_text = "\n".join(output_lines) if capture_output else None
        return subprocess.CompletedProcess(args=command, returncode=rc, stdout=stdout_text, stderr=None), env_snapshot


def _ensure_run_initialized() -> PersistentCmdShell:
    global _RUN_ENV_READY
    global _RUN_ENV_CACHE
    global _RUN_SHELL

    if _RUN_ENV_CACHE is None:
        _RUN_ENV_CACHE = _build_run_env()

    if not _RUN_ENV_READY:
        _RUN_SHELL = PersistentCmdShell(_RUN_ENV_CACHE)
        _RUN_ENV_READY = True

        def _close_shell_on_exit() -> None:
            global _RUN_SHELL
            try:
                if _RUN_SHELL is not None:
                    _RUN_SHELL.close()
            finally:
                _RUN_SHELL = None

        atexit.register(_close_shell_on_exit)

    if _RUN_SHELL is None or not _RUN_SHELL.is_alive():
        _RUN_SHELL = PersistentCmdShell(_RUN_ENV_CACHE)

    return _RUN_SHELL


def run(
    cmd: str,
    *,
    extra_env: Optional[Dict[str, str]] = None,
    capture_output: bool = False,
    check: bool = False,
):
    """
    TODO: Persistent Shell
    """
    global _RUN_SHELL
    global _RUN_ENV_CACHE

    merged_env = {str(k): str(v) for k, v in (extra_env or {}).items()}
    last_error: Optional[Exception] = None

    # 快速检测：若是简单的 "call <script>" 且脚本文件包含交互性 PAUSE/按任意键 提示，
    # 则改为在新进程中直接运行并继承父终端（stdin/stdout），以便用户能按键继续。
    if not capture_output:
        try:
            s = cmd.strip()
            if s.lower().startswith("call "):
                target = s[5:].strip()
                if target.startswith('"') and target.endswith('"'):
                    target = target[1:-1]
                token = target.split()[0]
                candidates = [token, token + ".bat", os.path.join(os.getcwd(), token), os.path.join(os.getcwd(), token + ".bat")]
                found = None
                for cand in candidates:
                    if os.path.exists(cand) and os.path.isfile(cand):
                        try:
                            with open(cand, 'r', encoding='utf-8', errors='ignore') as f:
                                text = f.read().lower()
                                if 'pause' in text or '按任意键' in text or 'press any key' in text:
                                    found = cand
                                    break
                        except Exception:
                            continue
                if found:
                    env_for_run = (_RUN_ENV_CACHE.copy() if _RUN_ENV_CACHE else os.environ.copy())
                    env_for_run.update(merged_env)
                    try:
                        cmd_to_run = cmd
                        # Ensure @echo off is present so the batch doesn't echo commands
                        if not cmd_to_run.strip().lower().startswith("@echo off"):
                            cmd_to_run = f"@echo off & {cmd_to_run}"
                        ret = subprocess.run(["cmd.exe", "/c", cmd_to_run], env=env_for_run, shell=False)
                        return subprocess.CompletedProcess(args=cmd, returncode=ret.returncode, stdout=None, stderr=None)
                    except Exception:
                        # 如果以交互方式运行失败，则继续使用持久 shell
                        pass
        except Exception:
            pass

    for _attempt in range(2):
        shell = _ensure_run_initialized()
        try:
            result, env_snapshot = shell.run_command(
                cmd,
                extra_env=merged_env if merged_env else None,
                capture_output=capture_output,
            )
            if env_snapshot:
                _RUN_ENV_CACHE = env_snapshot
            break
        except RuntimeError as e:
            last_error = e
            # Shell may be killed by an external script; recreate once and retry.
            _RUN_SHELL = None
            continue
    else:
        raise RuntimeError(f"run() failed after retry: {last_error}")

    # 强制所有输出用 ansi (mbcs) 解码，彻底规避终端影响
    if capture_output and result.stdout is not None:
        try:
            # 若已是 str，encode 再 decode，防止终端编码影响
            result = subprocess.CompletedProcess(
                args=result.args,
                returncode=result.returncode,
                stdout=result.stdout.encode("mbcs", errors="replace").decode("mbcs", errors="replace"),
                stderr=result.stderr,
            )
        except Exception:
            pass

    if check and result.returncode != 0:
        raise subprocess.CalledProcessError(
            returncode=result.returncode,
            cmd=cmd,
            output=result.stdout,
            stderr=result.stderr,
        )
    return result
    """

    # 这里已经舍去了很多初始化的东西，所以不需要再说每次都要初始化了 By 星旬
    subprocess.run(["cmd.exe", "/v:on", "/c", f'''
                    @echo off &
                    setlocal enabledelayedexpansion 1>nul 2>nul &
                    @{cmd} &
                    endlocal 1>nul 2>nul &
                    '''.replace("\n", "").replace(20*" ", "")])
    """


def checkwin() -> Tuple[str, str, str, Tuple[str, str]]:
    return (
        platform.system(),
        platform.release(),
        platform.version(),
        platform.architecture()
    )


def pause(message: str = "单击此字符或按任意键继续", style_override: Optional[Style] = None, timeout: Optional[float] = None) -> None:
    """显示一个短暂的提示，等待任意键或鼠标/触摸点击继续。

    向 `src/menu.py` 的 `pause` 保持一致：
    - 支持键盘任意按键确认。
    - 支持鼠标/触摸点击确认。
    - 可选 `timeout`（秒）在超时后自动继续。
    """
    def _console_pause(msg: str, to: Optional[float]):
        sys.stdout.write(msg)
        sys.stdout.flush()
        if to is not None and to <= 0:
            print()
            return
        # Windows
        try:
            import msvcrt

            start = time.time()
            if to is None:
                msvcrt.getwch()
            else:
                while True:
                    if msvcrt.kbhit():
                        msvcrt.getwch()
                        break
                    if time.time() - start >= to:
                        break
                    time.sleep(0.05)
            print()
            return
        except Exception:
            pass

        # POSIX
        try:
            import termios
            import tty
            import select

            fd = sys.stdin.fileno()
            old_attrs = termios.tcgetattr(fd)
            try:
                tty.setraw(fd)
                if to is None:
                    os.read(fd, 1)
                else:
                    rlist, _, _ = select.select([fd], [], [], to)
                    if rlist:
                        os.read(fd, 1)
            finally:
                termios.tcsetattr(fd, termios.TCSADRAIN, old_attrs)
            print()
            return
        except Exception:
            pass

        # Fallback to input()
        try:
            input()
        except Exception:
            pass

    # If not a tty, do a simple console pause
    if not (sys.stdin.isatty() and sys.stdout.isatty()):
        _console_pause(message, timeout)
        return

    # Otherwise try interactive prompt_toolkit pause; if that fails, fallback to console
    try:
        chosen_style = style_override or style
        if chosen_style is None:
            fallback_style = Style.from_dict({"pause": "fg:#cbd6e2"})
            chosen_style = fallback_style

        pause_style = Style.from_dict({"pause": "fg:#9dcffb bold"})
        app_style = merge_styles([chosen_style, pause_style]) if chosen_style else pause_style

        kb = KeyBindings()

        @kb.add("enter", eager=True)
        @kb.add(" ", eager=True)
        def _enter(event):
            event.app.exit()

        @kb.add("<any>")
        def _any(event):
            event.app.exit()

        def _mouse_handler(mouse_event):
            if mouse_event.event_type == MouseEventType.MOUSE_UP:
                try:
                    get_app().exit()
                except Exception:
                    pass
            return None

        class PauseControl(FormattedTextControl):
            def __init__(self, render_fn, handler):
                super().__init__(render_fn, focusable=True, show_cursor=False)
                self._handler = handler

            def mouse_handler(self, mouse_event):
                return self._handler(mouse_event)

        control = PauseControl(lambda: [("class:pause", message)], _mouse_handler)
        window = Window(content=control, always_hide_cursor=True, dont_extend_width=False, dont_extend_height=False)

        app = Application(
            layout=Layout(window, focused_element=control),
            key_bindings=kb,
            mouse_support=True,
            full_screen=False,
            style=app_style,
        )

        if timeout is not None and timeout > 0:
            def _timer():
                time.sleep(timeout)
                try:
                    app.exit()
                except Exception:
                    pass

            t = threading.Thread(target=_timer, daemon=True)
            t.start()

        app.run()
    except Exception:
        _console_pause(message, timeout)


def check_adb_server() -> Tuple[bool, Optional[Exception]]:
    adb_server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    # ADB Server request timeout: fast 0.2, usually 0.25, default 0.3, slowly 0.6
    adb_server.settimeout(0.3)
    try:
        adb_server.connect(("127.0.0.1", 5037))
    except socket.timeout:
        return False, None
    except Exception as e:
        return False, e
    adb_server.close()
    return True, None


""" CONFIGURATION/BUILD FUNCTIONS """

def debug_features_allowed() -> bool:
    return DEBUG


""" UI/MENU FUNCTIONS """

def menu_choice(
    message: str,
    options: Iterable[Tuple[Any, str]],
    default: Optional[str] = None,
    style_override: Optional[Style] = None,
    extra_bindings: Optional[KeyBindings] = None,
    header_text: Optional[str] = None,
    header_click_callback=None,
):
    option_list: List[Tuple[Any, str]] = list(options)
    if not option_list:
        raise ValueError("menu_choice requires at least one option")

    selected_index = 0
    display_index = 0
    animate_in = True
    visible_count = 0 if animate_in else len(option_list)
    move_task = None
    header_rows = 1 if header_text else 0

    if default is not None:
        for idx, (value, _label) in enumerate(option_list):
            if value == default:
                selected_index = idx
                display_index = idx
                break

    kb = KeyBindings()

    @kb.add("enter", eager=True)
    @kb.add(" ", eager=True)
    def _(event):
        idx = selected_index or 0
        event.app.exit(result=option_list[idx][0])

    digit_buf = {"text": "", "t": 0.0}

    def _select_by_number(event, digit: str) -> None:
        now = time.monotonic()
        if now - digit_buf["t"] > 0.2:
            digit_buf["text"] = ""
        digit_buf["text"] += digit
        digit_buf["t"] = now
        try:
            idx = int(digit_buf["text"]) - 1
        except ValueError:
            return
        if 0 <= idx < len(option_list):
            _set_selection(idx, event.app)

    for d in "0123456789":
        @kb.add(d, eager=True)
        def _(event, _d=d):
            _select_by_number(event, _d)

    def _clamp(idx: int) -> int:
        count = len(option_list)
        return max(0, min(count - 1, idx))

    def _animate_move(target: int, app):
        nonlocal display_index, move_task
        if move_task and not move_task.done():
            move_task.cancel()

        async def _run():
            nonlocal display_index
            while display_index != target:
                step = 1 if display_index < target else -1
                display_index += step
                app.invalidate()
                await asyncio.sleep(0.02)
            app.invalidate()

        move_task = app.create_background_task(_run())

    def _set_selection(idx: int, app):
        nonlocal selected_index, display_index
        selected_index = _clamp(idx)
        _animate_move(selected_index, app)

    @kb.add("up")
    @kb.add("k")
    def _(event):
        _set_selection(selected_index - 1, event.app)

    @kb.add("down")
    @kb.add("j")
    def _(event):
        _set_selection(selected_index + 1, event.app)

    last_click = {"idx": None, "t": 0.0}

    def mouse_handler(mouse_event):
        nonlocal selected_index
        # You can add MouseEventType.MOUSE_MOVE event type to allow mouse-move-select.
        if mouse_event.event_type != MouseEventType.MOUSE_UP:
            return NotImplemented

        y = mouse_event.position.y
        if header_rows and y == 0:
            if header_click_callback:
                header_click_callback()
            return None

        if header_rows:
            y -= header_rows
        current_len = visible_count if animate_in else len(option_list)
        if current_len == 0:
            return None
        if 0 <= y < current_len:
            _set_selection(y, get_app())

        if mouse_event.event_type == MouseEventType.MOUSE_UP and mouse_event.button == MouseButton.LEFT:
            now = time.monotonic()
            idx = selected_index
            if idx == last_click["idx"] and (now - last_click["t"]) <= 0.5:
                value = option_list[idx][0]
                get_app().exit(result=value)
            last_click["idx"] = idx
            last_click["t"] = now
        return None

    def render_lines():
        fragments = []
        if header_text:
            fragments.append(("class:menu-header", header_text))
            fragments.append(("", "\n"))

        current_len = visible_count if animate_in else len(option_list)
        if current_len == 0:
            current_len = 1
        current_len = min(current_len, len(option_list))
        sel = min(display_index, current_len - 1)

        for idx, (_, label) in enumerate(option_list[:current_len]):
            pointer = ">" if idx == sel else " "
            prefix = f" {pointer} " if idx == sel else "   "
            text = f"{prefix}{idx + 1}. {label}"
            style_class = "class:radio-selected" if idx == sel else "class:radio"
            fragments.append((style_class, text))
            if idx != current_len - 1:
                fragments.append(("", "\n"))
        return fragments

    class MenuControl(FormattedTextControl):
        def __init__(self, render_fn, handler):
            super().__init__(render_fn, focusable=True, show_cursor=False)
            self._handler = handler

        def mouse_handler(self, mouse_event):
            return self._handler(mouse_event)

    control = MenuControl(render_lines, mouse_handler)
    window = Window(content=control, always_hide_cursor=True, dont_extend_width=True, dont_extend_height=True)

    radio_style = Style.from_dict(
        {
            "radio-list": "",
            "radio": "fg:#cbd6e2",
            "radio-selected": "fg:#e6edf3 bold underline",
            "radio-checked": "fg:#7ad1a8",
            "radio-number": "fg:#9dcffb bold",
        }
    )

    app_style = merge_styles([style_override, radio_style]) if style_override else radio_style

    kb_final = merge_key_bindings([kb, extra_bindings]) if extra_bindings else kb

    app = Application(
        layout=Layout(window, focused_element=control),
        key_bindings=kb_final,
        mouse_support=True,
        full_screen=False,
        style=app_style,
    )

    if animate_in:
        async def _animate_in():
            nonlocal visible_count, selected_index
            for i in range(1, len(option_list) + 1):
                visible_count = i
                if selected_index >= visible_count:
                    selected_index = visible_count - 1
                get_app().invalidate()
                await asyncio.sleep(0.03)

        def _schedule_animate_in() -> None:
            app.create_background_task(_animate_in())

        app.pre_run_callables.append(_schedule_animate_in)

    result = app.run()
    if result is None:
        sel = selected_index or 0
        result = option_list[sel][0]
    return result


class Option:
    def __init__(self, value, label):
        self.value = value
        self.label = label


def choose(message: str, options: Iterable[Option], default: Optional[str] = None,
           extra_bindings: Optional[KeyBindings] = None, header_text: Optional[str] = None,
           header_click_callback=None):
    page_transition(message or "正在切换...")
    return menu_choice(
        message=message,
        options=[(opt.value, opt.label) for opt in options],
        default=default,
        style_override=style,
        extra_bindings=extra_bindings,
        header_text=header_text,
        header_click_callback=header_click_callback,
    )


@onerror
def menu() -> str:
    global CURRENT_BUILD_META
    if os.path.exists("mod") and os.path.isdir("mod"):
        print_formatted_text(HTML("已加载扩展列表："), style=style) if len(os.listdir("mod")) != 0 else print_formatted_text(HTML("已加载扩展列表：未加载任何扩展"), style=style)
        if len(os.listdir("mod")) != 0:
            i: int = 1
            for item in os.listdir("mod"):
                print_formatted_text(f"{i}. {item}", style=style)
                i += 1
    else:
        # print_formatted_text(HTML(WARN + "扩展文件夹没有创建，正在创建..."), style=style)
        os.remove("mod") if os.path.isfile("mod") else ...
        os.makedirs("mod", exist_ok=True)
    kb = KeyBindings()

    @kb.add('D')
    def _(event):
        event.app.exit(result="SHIFT_D")

    print_formatted_text(ANSI("鼠标双击或按回车键确定，方向键，数字键，鼠标单击来选择功能"))

    # Compose header with embedded softversion if available.
    version_str = CURRENT_BUILD_META.get("ro.product.current.softversion") or ""
    header = f"AllToolBox {version_str} 控制台&主菜单 by xgj_236" if version_str else "AllToolBox 控制台&主菜单 by xgj_236"

    options = [
        Option("onekeyroot", "一键Root"),
        Option("gbl", "粗粮高通机型临时Root&强解bl"),
        Option("openshell", "在此处打开cmd[含adb环境]"),
        Option("about", "关于脚本"),
        Option("mods", "扩展管理"),
        Option("commonly", "常用合集[子菜单]"),
        Option("help-links", "链接合集[子菜单]"),
        Option("man-apps", "应用管理[子菜单]"),
        Option("magisk-mod", "magisk模块管理[子菜单]"),
    ]
    options.append(Option("user-debug", "高级菜单[子菜单]"))
    options.append(Option("exit", "退出工具"))

    result = choose(
        message="",
        options=options,
        default="onekeyroot",
        extra_bindings=kb,
        header_text=header,
        header_click_callback=None,
    )

    clear()
    return result


""" SPECIAL FUNCTIONS """

@onerror
def sel():
    clear()
    run("call sel file s .")
    run("pause")
    run("call sel file m .")
    run("pause")


@onerror
def color():
    clear()
    print_formatted_text(HTML(INFO + "<black>BLACK</black>"), style=style)
    print_formatted_text(HTML(INFO + "<red>RED</red>"), style=style)
    print_formatted_text(HTML(INFO + "<green>GREEN</green>"), style=style)
    print_formatted_text(HTML(INFO + "<orange>ORANGE</orange>"), style=style)
    print_formatted_text(HTML(INFO + "<blue>BLUE</blue>"), style=style)
    print_formatted_text(HTML(INFO + "<magenta>MAGENTA</magenta>"), style=style)
    print_formatted_text(HTML(INFO + "<cyan>CYAN</cyan>"), style=style)
    print_formatted_text(HTML(INFO + "<white>WHITE</white>"), style=style)
    run("pause")


""" FLASHING FUNCTIONS """

@onerror
def flash_anykernel3(ak3_path, boot_path):
    global logger
    try:
        ak3 = AnyKernel3(ak3_path)
        ak3.extract_zip()
        ak3.unpack_bootimg(boot_path, "./tmp/boot_unpacked")
        ak3.rename_mainfile()
        ak3.patch_bootimg()
        ak3.repack_bootimg(repack_to="./tmp/boot_patched.img")
        ak3.clean_up()
        os.remove("./tmp/kernel")
    except Exception as e:
        logger.error(traceback.format_exc())
        return 1


def flash_partation(imgpath: str, part: Optional[str] = "boot"):
    run("device_check.exe fastboot")
    print("\n")
    run(f"fastboot.exe flash {part} {imgpath}")


def _extract_boot_to_tmp(paths: List[str], partition_name: str) -> bool:
    run("adb start-server 1>nul")
    run("device_check.exe adb")
    print_formatted_text("\n", style=style)
    for prefix in paths:
        process = subprocess.run(["adb.exe", "shell", f"su -c \"dd if={prefix}{partition_name} of=/sdcard/boot.img bs=2048\""])
        if process.returncode == 0:
            break
    process = subprocess.run(["adb.exe", "pull", "/sdcard/boot.img", "./tmp/boot.img"])
    return process.returncode == 0


@auto_clear
@onerror
def anykernel3():
    paths = ["/dev/block/by-name/", "/dev/block/bootdevice/by-name"]
    mapping = {
        "1": ("boot", "boot"),
        "2": ("boot_a", "boot_a"),
        "3": ("boot_b", "boot_b"),
    }

    while True:
        print_formatted_text(HTML(WARN + "目前仅支持boot.img修补，并可能存在未知问题！"), style=style)
        result = choose(message="", options=[
            Option("A", "返回上级菜单"),
            Option("1", "A-only槽位"),
            Option("2", "AB分区-A槽位"),
            Option("3", "AB分区-B槽位")
        ], default="A")
        if result == "A":
            clear()
            return
        if result not in mapping:
            print_formatted_text(HTML(ERROR + "输入错误，请重新输入"), style=style)
            continue

        src_partition, dst_partition = mapping[result]
        if not _extract_boot_to_tmp(paths, src_partition):
            print_formatted_text(HTML(ERROR + "提取Boot失败"), style=style)
            continue
        print_formatted_text(HTML(INFO + "请加载AnyKernel3 Zip"), style=style)
        run("call sel file s .")
        with open("./tmp/output.txt", "r", encoding="utf-8") as f:
            filepath = f.read().rstrip("\r\n").rstrip("\n")
        flash_anykernel3(filepath, "./tmp/boot.img")
        flash_partation("./tmp/boot_patched.img", dst_partition)
        print_formatted_text(HTML(INFO + "刷入成功"), style=style)


""" FEATURE MENU HANDLERS """

@onerror
@auto_clear(logo=True, end=True)
def root():
    while True:
        result = choose(
            message="一键Root菜单",
            options=[
                Option("A", "返回上级菜单"),
                Option("1", "Wear一键Root"),
                Option("2", "手机通用Root"),
            ],
            default="A"
        )
        match result:
            case "A":
                return
            case "1":
                ato = AudioPlayer(os.path.join(".", "music"))
                ato.play_audio(auto_next=True)
                run("call root.bat"); clear()
                ato.stop_audio()
            case "2":
                run("call otherroot.bat 3"); clear()


@onerror
@auto_clear(logo=True, end=True)
def qcom_gbl():
    run("call qcom_gbl_selinux.bat"); clear()


@onerror
@auto_clear(logo=True, end=True)
def appset():
    while True:
        result = choose(
            message="应用管理菜单",
            options=[
                Option("A", "返回上级菜单"),
                Option("1", "安装应用"),
                Option("2", "卸载应用"),
                Option("3", "安装状态栏悬浮窗"),
                Option("4", "设置微信QQ为开机自启应用"),
                Option("5", "解除z10 V2版本安装限制-V3不适用"),
            ],
            default="A"
        )
        if result == "A":
            return
        if result == "1":
            run("call userinstapp")
        if result == "2":
            run("call unapp")
        if result == "3":
            run("call xtcztl")
        if result == "4":
            run("call qqwxautestart")
        if result == "5":
            run("call z10openinst")


@onerror
@auto_clear(logo=True, end=True)
def userdebug():
    while True:
        result = choose(
            message="Advanced Options",
            options=[
                Option("A", "返回上级菜单"),
                Option("1", "设备信息"),
                Option("2", "打开充电可用"),
                Option("3", "型号与innermodel对照表"),
                Option("4", "导入本地root文件"),
                Option("5", "一键root[不刷userdata]"),
                Option("6", "恢复出厂设置[不是超级恢复][Root后禁用]"),
                Option("7", "开机自刷Recovery"),
                Option("8", "强制加好友[已失效]"),
            ],
            default="A"
        )

        if result == "A":
            return
        if result == "1":
            run("call listbuild"); clear();run("call logo")
        if result == "2":
            run("call opencharge"); clear();run("call logo")
        if result == "3":
            run("call innermodel")
            print_formatted_text(HTML(INFO + "单击此字符或按任意键继续返回上级菜单"), style=style)
            pause(); clear();run("call logo")
        if result == "4":
            run("call pashroot"); clear();run("call logo")
        if result == "5":
            run("call root nouserdata"); clear();run("call logo")
        if result == "6":
            run("call miscre"); clear();run("call logo")
        if result == "7":
            run("call pashtwrppro"); clear();run("call logo")
        if result == "8":
            run("call friend"); clear();run("call logo")


@onerror
@auto_clear(logo=True, end=True)
def commonly():
    while True:
        commonly_list: List[Option] = [
            Option("A", "返回上级菜单"),
            Option("1", "ADB/自检校验码计算"),
            Option("2", "离线OTA升级"),
            Option("3", "刷入TWRP"),
            Option("4", "刷入XTC Patch"),
            Option("5", "备份与恢复"),
            Option("6", "安卓8.1root后优化"),
            Option("7", "进入qmmi[9008]"),
            Option("8", "scrcpy投屏"),
            Option("9", "高级重启"),
            Option("11", "开启无线调试")
        ]
        if DEBUG:
            commonly_list.append(Option("10", "刷入AnyKernel3[实验性]"))
        result = choose(
            message="常用合集",
            options=commonly_list,
            default="A"
        )
        match result:
            case "A":
                return
            case "1":
                run("powershell -ExecutionPolicy Bypass -File zj.ps1")
            case "2":
                run("call ota")
            case "3":
                run("call pashtwrp")
            case "4":
                run("call xtcpatch")
            case "5":
                run("call backup")
            case "6":
                run("call rootpro")
            case "7":
                run("call qmmi")
                print_formatted_text(HTML(INFO + "单击此字符或按任意键继续返回上级菜单"), style=style)
                pause()
            case "8":
                run("call scrcpy-ui.bat")
            case "9":
                run("call rebootpro")
            case "10":
                clear()
                anykernel3()
            case "11":
                run("call wifiadb")
            case _:
                print_formatted_text(HTML(ERROR + "输入错误，请重新输入"), style=style)


@onerror
@auto_clear(logo=True, end=True)
def magisk():
    while True:
        result = choose(
            message="magisk模块管理",
            options=[
                Option("A", "返回上级菜单"),
                Option("1", "刷入Magisk模块"),
                # Option("2", "刷入LSPosed-Android8.1机型"),
                Option("3", "刷入Xposed框架-适用于安卓4.4.4和7.1.1"),
            ],
            default="A"
        )
        if result == "A":
            return
        if result == "1":
            run("call userinstmodule"); clear();run("call logo")
        if result == "2":
            run("call InstLSPosed810"); clear();run("call logo")
        if result == "3":
            run("call Xposed"); clear();run("call logo")


@onerror
@auto_clear(logo=True, end=True)
def debug():
    global allow_xtc
    if not DEBUG:
        print_formatted_text(HTML(ERROR + "当前为 Release 构建，DEBUG 菜单不可用"), style=style)
        time.sleep(1)
        return
    while True:
        result = choose(
            message="DEBUG菜单",
            options=[
                Option("A", "返回上级菜单"),
                Option("1", "色卡"),
                Option("2", "调整为未使用状态"),
                Option("3", "调整为使用状态"),
                Option("4", "调整为更新状态"),
                Option("5", "debug sel"),
                Option("6", "允许使用部分一键root功能"),
            ],
            default="A"
        )
        match result:
            case "A":
                return
            case "1":
                color()
            case "2":
                open("whoyou.txt", "w").write("1")
            case "3":
                open("whoyou.txt", "w").write("2")
            case "4":
                open("whoyou.txt", "w").write("3")
            case "5":
                sel()
            case "6":
                allow_xtc = True
                print_formatted_text(HTML(INFO + "已允许使用部分一键root功能"), style=style)
                time.sleep(1)


@onerror
@auto_clear(logo=True, end=True)
def help_menu():
    while True:
        result = choose(
            message="帮助与链接",
            options=[
                Option("A", "返回上级菜单"),
                Option("1", "超级恢复文件下载"),
                Option("2", "离线OTA下载"),
                Option("3", "面具模块下载"),
                Option("4", "APK下载"),
                Option("5", "工具箱官网"),
                Option("6", "开发文档"),
                Option("7", "123云盘解除下载限制"),
            ],
            default="A"
        )
        match result:
            case "A":
                return
            case "1":
                run("start https://www.123865.com/s/Q5JfTd-hEbWH")
            case "2":
                run("start https://www.123865.com/s/Q5JfTd-HEbWH")
            case "3":
                run("start https://www.123684.com/s/Q5JfTd-cEbWH")
            case "4":
                run("start https://www.123684.com/s/Q5JfTd-ZEbWH")
            case "5":
                run("start https://atb.xgj.qzz.io")
            case "6":
                with open("开发文档.txt", "r", encoding="utf-8") as f:
                    doc = f.read()
                    print_formatted_text(HTML(doc), style=style)
                kb = KeyBindings()
                prompt(
                    HTML(INFO + "单击此字符或按任意键继续返回上级菜单"),
                    key_bindings=kb,
                    style=style
                )
            case "7":
                run("call patch123")


""" MOD MANAGEMENT FUNCTIONS """ 

def load_mod_menu():
    mod_dir = ".\\mod"
    if not os.path.isdir(mod_dir):
        print_formatted_text(HTML(ERROR + "未找到 mod 目录"), style=style)
        return None

    dirs = [d for d in os.listdir(mod_dir)
            if os.path.isdir(os.path.join(mod_dir, d))]

    if not dirs:
        print_formatted_text(HTML(WARN + "未发现任何扩展"), style=style)
        time.sleep(2)
        return None

    base = 10
    mapping = {}
    options = [("A", "返回上级菜单")]

    for i, name in enumerate(dirs, start=base + 1):
        key = str(i)
        mapping[key] = name
        options.append((key, name))

    result = choose(
        message="已加载扩展",
        options=[Option(value, label) for value, label in options],
        default="A"
    )

    if result == "A":
        return None

    return mapping.get(result)


@onerror
def run_mod_main(modname):
    run(f'cd /d mod\\{modname} && call main.bat')


@onerror
@auto_clear(logo=True, end=True)
def mod():
    while True:
        result = choose(
            message="扩展管理",
            options=[
                Option("A", "返回上级菜单"),
                Option("1", "运行已安装扩展"),
                Option("2", "安装扩展"),
                Option("3", "卸载扩展"),
            ],
            default="A"
        )
        if result == "A":
            return
        if result == "1":
            modname = load_mod_menu()
            if modname:
                run_mod_main(modname)
        if result == "2":
            run("call mod")
        if result == "3":
            run("call unmod")


""" MAIN FLOW FUNCTIONS """

@onerror
@auto_clear(end=True)
def about():
    print_formatted_text(
        HTML(f"<yellow>{LINE}</yellow>"),
        style=style
    )

    print_formatted_text(
        HTML(INFO + "本脚本由快乐小公爵236等开发者制作"),
        style=style
    )

    run("call thank.bat")

    print_formatted_text(
        HTML(INFO + "工具官网：https://atb.xgj.qzz.io"),
        style=style
    )
    print_formatted_text(
        HTML(INFO + "作者QQ：3247039462"),
        style=style
    )
    print_formatted_text(
        HTML(INFO + "工具箱交流与反馈QQ群：907491503"),
        style=style
    )
    print_formatted_text(
        HTML(INFO + "作者哔哩哔哩账号：https://b23.tv/L54R5ZV"),
        style=style
    )
    print_formatted_text(
        HTML(INFO + "bug与建议反馈邮箱：ATBbug@xgj.qzz.io"),
        style=style
    )

    run("call uplog.bat")

    print_formatted_text(
        HTML(f"<yellow>{LINE}</yellow>"),
        style=style
    )

    kb = KeyBindings()

    prompt(
        HTML(INFO + "单击此字符或按任意键继续返回上级菜单"),
        key_bindings=kb,
        style=style
    )


@onerror
def pre_main() -> bool:
    global allow_xtc
    global flag
    global logger
    global DEBUG
    global CURRENT_BUILD_META
    # 这是我在这里最后的痕迹，不要删除，希望以后你们能记得我还是贡献过的 --Starry2233
    colorama.init(autoreset=True, convert=True)
    run("call .\\color.bat")
    _PATHEXT: str = os.environ["PATHEXT"]
    if not _PATHEXT.endswith(';'): _PATHEXT += ';'
    _PATHEXT += ';'.join(_PATHEXT_EXTRA)
    os.environ["PATHEXT"] = _PATHEXT
    print(_PATHEXT)
    if DEBUG:
        print_formatted_text(HTML(INFO + "已启用调试模式"), style=style)
        logger.debug("Debug mode is enabled")
    env_path_lower = (os.environ.get("PATH") or "").lower()
    keywords = ["windows", "system32", "powershell"]
    if not all(k in env_path_lower for k in keywords):
        print_formatted_text(HTML(ERROR + "你的系统环境变量异常，这可能导致异常问题，输入no跳过"), style=style)
        answer = input().strip().lower()
        if answer != "no":
            return False

    softver = CURRENT_BUILD_META.get("ro.product.current.softversion") or ""
    title = f"AllToolBox {softver} by xgj_236" if softver else "AllToolBox by xgj_236"
    set_title(title)
    os.makedirs("mod", exist_ok=True)
    for item in os.listdir("mod"):
        item_path = os.path.join("mod", item)
        if os.path.isdir(item_path):
            if os.path.exists(os.path.join(item_path, "start.bat")):
                run(f'cd /d mod\\{item} && call start.bat')

    try:
        this_path = os.path.dirname(os.path.abspath(__file__))
        candidate_bin = os.path.normpath(os.path.join(this_path, "..", "bin"))
        if os.path.isdir(candidate_bin):
            os.chdir(candidate_bin)
            logger.debug("Changed working directory to %s", candidate_bin)
        else:
            logger.debug("Bin directory not found: %s; continuing in current working directory", candidate_bin)
    except Exception:
        logger.exception("Failed to change working directory to bin; continuing")

    def _run_if_present(base_name: str):
        try:
            if os.path.exists(base_name) or os.path.exists(base_name + ".bat"):
                run(f"call {base_name}")
            else:
                logger.debug("%s not found; skipping", base_name)
        except Exception:
            logger.exception("Error running %s", base_name)

    _run_if_present("withone")
    _run_if_present("afterup")
    if os.path.exists("..\\bugjump.7z"):
        os.remove("..\\bugjump.7z")
    if os.path.exists("..\\repair.exe"):
        os.remove("..\\repair.exe")
    if os.getenv("ATB_SKIP_UPDATE", "0") != "1":
        print_formatted_text(HTML(INFO + "正在检查更新..."), style=style)
        try:
            meta_update = CURRENT_BUILD_META
            is_debug_build = DEBUG
            base = "https://raw.githubusercontent.com/xgj236/AllToolBox/main"
            version_tmp_url = f"{base}/versiontmp.txt"
            if is_debug_build:
                version_tmp_url = f"{base}/betaversiontmp.txt"

            local_path = "bugversion.txt"
            if not os.path.exists(local_path):
                with open(local_path, "w") as bv:
                    bv.write("0")

            with open(local_path, "r") as fv:
                try:
                    filev = int(fv.read().strip())
                except ValueError:
                    filev = 0

            resp = requests.get(
                version_tmp_url,
                headers={'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/58.0.3029.110 Safari/537.36)'},
                timeout=8
            )
            resp.raise_for_status()
            try:
                webv = int(resp.text.strip())
            except ValueError:
                webv = filev

            if webv > filev:
                new_version = meta_update.get("ro.product.current.softversion", "") if meta_update else ""
                if not new_version:
                    try:
                        vt = requests.get(
                            version_tmp_url,
                            headers={'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/58.0.3029.110 Safari/537.36)'},
                            timeout=8
                        )
                        vt.raise_for_status()
                        new_version = vt.text.strip()
                    except Exception:
                        new_version = ""
                print_formatted_text(HTML(WARN + "当前补丁版本过时，必须更新"), style=style)
                if new_version:
                    print_formatted_text(HTML(INFO + f"最新补丁版本：{new_version}"), style=style)
                print_formatted_text(HTML(INFO + "单击此字符或按任意键继续开始更新..."), style=style)
                pause()
                shutil.copy2("repair.exe", "..\\repair.exe")
                os.chdir("..\\")
                subprocess.run(["cmd", "/c", "start", "repair.exe"])
                cleanup(2)
        except Exception:
            pass
        run("call upall.bat run")
        """fucking xtc"""
    """allow_xtc = True"""

    if os.getenv("ATB_SKIP_PLATFORM_CHECK", "0") != "1":
        print_formatted_text(HTML(INFO + "正在检查Windows属性..."), style=style)
        os_name, os_release, os_version, arch = checkwin()
        match arch[0]:
            case "64bit":
                arch = "x64"
            case "32bit":
                arch = "x86"
            case _:
                arch = "arm64-v8a"
        print_formatted_text(HTML(INFO + f"当前运行环境:{os_name}{os_release}_{arch}_{os_version}"), style=style)
        os_vercode = 0
        try:
            os_vercode = float(os_release)
        except ValueError:
            pass
        if os_vercode <= 7:
            print_formatted_text(HTML(ERROR + "此脚本需要 Windows 8 或更高版本"), style=style)
            pause()
            return False
        print_formatted_text(HTML(INFO + f"当前系统: {os_name} {os_release}"), style=style)
        adb_process = subprocess.Popen(["adb.exe", "version"], stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, shell=True)
        adb_process.wait()
        if adb_process.returncode != 0:
            print_formatted_text(HTML(ERROR + f"ADB检查失败，返回值：{adb_process.returncode}"), style=style)
            return False
        print_formatted_text(HTML(INFO + "检查ADB命令成功"), style=style)
    whoyou = open("whoyou.txt", "w", encoding="gbk")
    whoyou.write("2")
    whoyou.close()
    print_formatted_text(HTML(f"""{WARN}关于解绑：该工具不提供手表强制解绑服务，如您拾取他人的手表，请联系当地110公安机关归还失主。手表解绑属于非法行为，请归还失主。而不要尝试通过任何手段解除挂失锁
        {WARN}关于收费：这个工具是完全免费的，如果你付费购买了那么请退款
        {WARN}本脚本部分功能可能造成侵权问题，并可能受到法律追究，所以仅供个人使用，请勿用于商业用途
        {INFO}---请永远相信我们能给你带来免费又好用的工具---
        {INFO}关于官网：https://atb.xgj.qzz.io
        {INFO}关于作者：本脚本由快乐小公爵236等作者制作
        {INFO}作者QQ：3247039462
        {INFO}工具箱交流与反馈QQ群：907491503
        {INFO}作者哔哩哔哩账号：https://b23.tv/L54R5ZV
        {INFO}bug与建议反馈邮箱：ATBbug@xgj.qzz.io""".replace(" " * 8, "")), style=style)
    print_formatted_text(HTML(INFO + "单击此字符或按任意键继续进入主界面"), style=style)

    pause()
    flag = True
    clear()
    return True


@onerror
def cleanup(code: int = 0):
    print_formatted_text(HTML(INFO + "正在结束ADB服务..."), style=style)
    if check_adb_server()[0]:
        subprocess.Popen(["adb.exe", "kill-server"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, shell=True)
    sys.exit(code)


class AllToolBox(object):
    """High-level controller for preflight, menu loop, and action dispatch."""

    def __init__(self) -> None:
        self._debug_allowed = debug_features_allowed()

    def _run_pre_main(self) -> bool:
        return pre_main() if not flag else True

    def _handle_action(self, action: str) -> Optional[int]:
        if action == "SHIFT_D":
            if self._debug_allowed:
                debug()
            else:
                print_formatted_text(HTML(ERROR + "当前为 Release 构建，DEBUG 功能已禁用"), style=style)
                time.sleep(1)
            return None
        if action == "onekeyroot":
            root()
            return None
        if action == "gbl":
            qcom_gbl()
            return None
        if action == "openshell":
            clear()
            subprocess.run(["cmd.exe", "/k"], shell=True)
            clear()
            return None
        if action == "about":
            about()
            return None
        if action == "mods":
            mod()
            return None
        if action == "commonly":
            commonly()
            return None
        if action == "user-debug":
            userdebug()
            return None
        if action == "man-apps":
            appset()
            return None
        if action == "magisk-mod":
            magisk()
            return None
        if action == "help-links":
            help_menu()
            return None
        if action == "exit":
            return 0
        return None

    def run(self) -> int:
        try:
            pre_ok = self._run_pre_main()
            if not pre_ok:
                return 1
            while True:
                clear()
                run("call logo")
                selection = menu()
                maybe_code = self._handle_action(selection)
                if maybe_code is not None:
                    return maybe_code
        except KeyboardInterrupt:
            print_formatted_text(HTML("\n" + WARN + "检测到用户中断，正在退出..."), style=style)
            return 0


@onerror
def main() -> int:
    return AllToolBox().run()


if __name__ == "__main__":
    logger = logging.getLogger()
    logger.setLevel(logging.DEBUG)
    logger.handlers.clear()
    os.makedirs("logs", exist_ok=True)
    filename = f"logs/atb_{datetime.datetime.now().strftime('%Y%m%d_%H%M%S')}.log"
    file_handler = logging.FileHandler(filename, encoding="utf-8")
    file_handler.setLevel(logging.DEBUG)
    formatter = MultilineFormatter('%(asctime)s - %(levelname)s - %(message)s')
    file_handler.setFormatter(formatter)
    logger.addHandler(file_handler)

    # Initialize cloud-control detection at startup: if the endpoint is
    # unreachable, cloud control features will be disabled for this run.
    try:
        import menu as _menu
        try:
            _menu.init_cloud_control()
        except Exception:
            # If init fails, ensure cloud control is treated as disabled for this run
            pass
    except Exception:
        # If importing menu fails, ignore and continue startup
        pass

    exit_code = main()
    logger.debug("ATBExitEvent, main returned: %s", exit_code)
    cleanup(exit_code)
