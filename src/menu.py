# -*- coding: utf-8 -*-
"""
Menu utilities extracted from start.py for reuse by Python and batch callers.
Provides interactive menu rendering plus config-driven action resolution via JSON.
"""
from __future__ import annotations
import asyncio
import time
import threading
import os
import json
import sys
import urllib.request
import urllib.error
import subprocess
import tempfile
import ctypes
from ctypes import wintypes
from typing import Iterable, List, Tuple

from prompt_toolkit import HTML, prompt
from prompt_toolkit.application import Application
from prompt_toolkit.application.current import get_app
from prompt_toolkit.key_binding import KeyBindings, merge_key_bindings
from prompt_toolkit.layout import Layout
from prompt_toolkit.layout.containers import Window
from prompt_toolkit.layout.controls import FormattedTextControl
from prompt_toolkit.mouse_events import MouseButton, MouseEventType
from prompt_toolkit.styles import Style, merge_styles

DEFAULT_STYLE: Style | None = None





# Try to enable Windows ANSI support if available. This is optional so we
# don't hard-fail when `colorama` isn't installed.
try:
    import colorama

    try:
        colorama.init()
    except Exception:
        pass
except Exception:
    colorama = None


def _enable_windows_vt() -> bool:
    """Enable Windows virtual terminal processing (ANSI) as a fallback when
    colorama is not available or didn't succeed. Returns True on success.
    """
    if os.name != "nt":
        return False
    try:

        kernel32 = ctypes.windll.kernel32
        STD_OUTPUT_HANDLE = -11
        hOut = kernel32.GetStdHandle(STD_OUTPUT_HANDLE)
        if not hOut:
            return False
        mode = ctypes.c_uint()
        if kernel32.GetConsoleMode(hOut, ctypes.byref(mode)) == 0:
            return False
        ENABLE_VIRTUAL_TERMINAL_PROCESSING = 0x0004
        new_mode = mode.value | ENABLE_VIRTUAL_TERMINAL_PROCESSING
        if kernel32.SetConsoleMode(hOut, new_mode) == 0:
            return False
        return True
    except Exception:
        return False


# Try to enable VT processing regardless of colorama presence (best-effort).
try:
    _enable_windows_vt()
except Exception:
    pass


def _style_spec_to_ansi(spec: str | None) -> tuple[str, str]:
    """Convert a simple style spec into ANSI prefix/suffix strings.

    Supported inputs (examples):
    - "red" -> foreground red
    - "fg:red bold" -> bold red foreground
    - "fg:#RRGGBB" -> 24-bit truecolor foreground
    - "bg:#RRGGBB" -> 24-bit truecolor background
    - "fg:#ff0000 bold" -> combined
    Returns (prefix, suffix). If spec is None or unrecognized returns ("", "").
    """
    if not spec:
        return "", ""

    spec = spec.strip()
    if not spec:
        return "", ""

    parts = spec.split()
    main = parts[0]
    mods = set(p.lower() for p in parts[1:])

    kind = "fg"
    val = main
    if ":" in main:
        k, v = main.split(":", 1)
        kind = k or "fg"
        val = v

    prefix_codes: list[str] = []

    # bold modifier
    if "bold" in mods:
        prefix_codes.append("1")

    # simple color names
    simple_colors = {
        "black": 0,
        "red": 1,
        "green": 2,
        "yellow": 3,
        "blue": 4,
        "magenta": 5,
        "purple": 5,
        "cyan": 6,
        "white": 7,
        "grey": 7,
        "gray": 7,
    }

    # Hex color
    if val.startswith("#") and len(val) in (4, 7):
        # expand #RGB to #RRGGBB
        if len(val) == 4:
            r = int(val[1] * 2, 16)
            g = int(val[2] * 2, 16)
            b = int(val[3] * 2, 16)
        else:
            r = int(val[1:3], 16)
            g = int(val[3:5], 16)
            b = int(val[5:7], 16)
        if kind == "fg":
            prefix = f"\x1b[38;2;{r};{g};{b}m"
        else:
            prefix = f"\x1b[48;2;{r};{g};{b}m"
        # If we also have numeric codes (e.g. bold) prepend them
        if prefix_codes:
            # join numeric codes before the truecolor code
            nums = ";".join(prefix_codes)
            prefix = f"\x1b[{nums}m" + prefix
        return prefix, "\x1b[0m"

    # Named color (map to 30-37 / 40-47)
    lname = val.lower()
    if lname in simple_colors:
        base = 30 if kind == "fg" else 40
        code = base + simple_colors[lname]
        if prefix_codes:
            prefix = f"\x1b[{';'.join(prefix_codes)};{code}m"
        else:
            prefix = f"\x1b[{code}m"
        return prefix, "\x1b[0m"

    # Fallback: unrecognized
    return "", ""


def _clear_menu_lines(count: int) -> None:
    """Clear the last `count` lines from the terminal (ANSI)."""
    try:
        for _ in range(count):
            sys.stdout.write("\033[F\033[K")  # move cursor up 1 line and clear it
        sys.stdout.flush()
    except Exception:
        pass


def _fetch_cloud_flag(url: str, timeout: float = 3.0) -> str | None:
    try:
        with urllib.request.urlopen(url, timeout=timeout) as resp:
            return resp.read().decode("utf-8", errors="ignore").strip()
    except Exception:
        return None


# Cloud control runtime state
CLOUD_CONTROL_URL = "https://atb.xgj.qzz.io/check.info"
CLOUD_CONTROL_TIMEOUT = 3.0
CLOUD_CONTROL_REACHABLE: bool | None = None
CLOUD_CONTROL_INITIALIZED = False


def init_cloud_control(url: str | None = None, timeout: float | None = None) -> bool:
    """Attempt to contact cloud-control endpoint once and set module-level state.

    Returns True if the endpoint is reachable (regardless of its returned value),
    False otherwise. The state is stored in `CLOUD_CONTROL_REACHABLE` and
    `CLOUD_CONTROL_INITIALIZED`.
    """
    global CLOUD_CONTROL_REACHABLE, CLOUD_CONTROL_INITIALIZED
    if url is None:
        url = CLOUD_CONTROL_URL
    if timeout is None:
        timeout = CLOUD_CONTROL_TIMEOUT
    try:
        res = _fetch_cloud_flag(url, timeout=timeout)
        CLOUD_CONTROL_REACHABLE = res is not None
    except Exception:
        CLOUD_CONTROL_REACHABLE = False
    CLOUD_CONTROL_INITIALIZED = True
    return bool(CLOUD_CONTROL_REACHABLE)


def _is_main_running() -> bool:
    """Return True if a process named 'main.exe' is running (best-effort)."""
    try:
        import psutil

        for p in psutil.process_iter(["name"]):
            try:
                if (p.info.get("name") or "").lower() == "main.exe":
                    return True
            except Exception:
                continue
        return False
    except Exception:
        # Fallback to tasklist on Windows
        try:
            out = subprocess.check_output(["tasklist", "/FI", "IMAGENAME eq main.exe"], stderr=subprocess.DEVNULL, text=True)
            return "main.exe" in out.lower()
        except Exception:
            return False


def _is_main_present() -> bool:
    """Check common locations for a main.exe file."""
    cwd = os.getcwd()
    candidates = [os.path.join(cwd, "main.exe"), os.path.join(cwd, "bin", "main.exe")]
    for p in candidates:
        if os.path.isfile(p):
            return True
    return False


def _increment_menufailed_count() -> int:
    td = tempfile.gettempdir()
    f = os.path.join(td, "menufailed.txt")
    try:
        cur = 0
        if os.path.isfile(f):
            with open(f, "r", encoding="utf-8") as fh:
                cur = int(fh.read().strip() or "0")
    except Exception:
        cur = 0
    cur += 1
    try:
        with open(f, "w", encoding="utf-8") as fh:
            fh.write(str(cur))
    except Exception:
        pass
    return cur


class Option:
    def __init__(self, value, label):
        self.value = value
        self.label = label


def set_default_style(style: Style) -> None:
    global DEFAULT_STYLE
    DEFAULT_STYLE = style


def menu_choice(
    message: str,
    options: Iterable[Tuple[str, str]],
    default: str | None = None,
    style_override: Style | None = None,
    extra_bindings: KeyBindings | None = None,
):
    option_list: List[tuple] = list(options)
    if not option_list:
        raise ValueError("menu_choice requires at least one option")

    selected_index = 0
    display_index = 0
    animate_in = True
    visible_count = 0 if animate_in else len(option_list)
    move_task = None

    if default is not None:
        for idx, (value, _) in enumerate(option_list):
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
        if mouse_event.event_type != MouseEventType.MOUSE_UP:
            return NotImplemented

        y = mouse_event.position.y
        current_len = visible_count if animate_in else len(option_list)
        if current_len == 0:
            return None
        if 0 <= y < current_len:
            _set_selection(y, get_app())
        # Right-click acts as Enter/confirm: exit with current selection.
        btn = getattr(mouse_event, "button", None)
        if btn == MouseButton.RIGHT:
            try:
                value = option_list[selected_index][0]
            except Exception:
                value = option_list[0][0]
            get_app().exit(result=value)
            return None

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

    chosen_style = style_override or DEFAULT_STYLE
    app_style = merge_styles([chosen_style, radio_style]) if chosen_style else radio_style

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
        # prompt_toolkit expects callables with no return; wrap the coroutine
        def _start_anim():
            app.create_background_task(_animate_in())

        app.pre_run_callables.append(_start_anim)

    result = app.run()
    if result is None:
        sel = selected_index or 0
        result = option_list[sel][0]
    # Clear only the menu lines from the terminal, preserving earlier output.
    _clear_menu_lines(len(option_list))
    return result


def menu_multi_choice(
    message: str,
    options: Iterable[Tuple[str, str]],
    style_override: Style | None = None,
    extra_bindings: KeyBindings | None = None,
) -> List[str]:
    """Interactive multi-select menu. Returns list of selected values."""
    option_list: List[tuple] = list(options)
    if not option_list:
        raise ValueError("menu_multi_choice requires at least one option")

    selected_index = 0
    selected_set: set[int] = set()
    animate_in = True
    visible_count = 0 if animate_in else len(option_list)
    move_task = None

    kb = KeyBindings()

    @kb.add("enter", eager=True)
    def _enter(event):
        # Exit and return current selections
        event.app.exit()

    @kb.add(" ", eager=True)
    def _space(event):
        idx = selected_index or 0
        if idx in selected_set:
            selected_set.remove(idx)
        else:
            selected_set.add(idx)
        event.app.invalidate()

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
        nonlocal visible_count, move_task
        if move_task and not move_task.done():
            move_task.cancel()

        async def _run():
            nonlocal visible_count
            while visible_count != len(option_list):
                visible_count = len(option_list)
                app.invalidate()
                await asyncio.sleep(0.02)
            app.invalidate()

        move_task = app.create_background_task(_run())

    def _set_selection(idx: int, app):
        nonlocal selected_index
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
        # Only handle MOUSE_UP for mouse clicks to avoid double-trigger
        # (MOUSE_DOWN then MOUSE_UP). Treat button==None (touch) as left.
        if mouse_event.event_type != MouseEventType.MOUSE_UP:
            return NotImplemented

        y = mouse_event.position.y
        current_len = visible_count if animate_in else len(option_list)
        if current_len == 0:
            return None
        if 0 <= y < current_len:
            _set_selection(y, get_app())
        btn = getattr(mouse_event, "button", None)
        # Right-click confirms (acts as Enter)
        if btn == MouseButton.RIGHT:
            try:
                get_app().exit()
            except Exception:
                pass
            return None

        is_left = (btn == MouseButton.LEFT) or (btn is None)
        if is_left:
            now = time.monotonic()
            idx = selected_index
            # toggle selection on mouse-up/touch-up
            if idx in selected_set:
                selected_set.remove(idx)
            else:
                selected_set.add(idx)
            # double click / double tap exits
            if idx == last_click["idx"] and (now - last_click["t"]) <= 0.5:
                try:
                    get_app().exit()
                except Exception:
                    pass
            last_click["idx"] = idx
            last_click["t"] = now
        return None

    def render_lines():
        fragments = []
        current_len = visible_count if animate_in else len(option_list)
        if current_len == 0:
            current_len = 1
        current_len = min(current_len, len(option_list))
        sel = min(selected_index, current_len - 1)

        for idx, (_, label) in enumerate(option_list[:current_len]):
            pointer = ">" if idx == sel else " "
            prefix = f" {pointer} " if idx == sel else "   "
            checked = "[x]" if idx in selected_set else "[ ]"
            text = f"{prefix}{checked} {idx + 1}. {label}"
            style_class = "class:radio-selected" if idx in selected_set else "class:radio"
            fragments.append((style_class, text))
            if idx != current_len - 1:
                fragments.append(("", "\n"))
        return fragments

    class MultiControl(FormattedTextControl):
        def __init__(self, render_fn, handler):
            super().__init__(render_fn, focusable=True, show_cursor=False)
            self._handler = handler

        def mouse_handler(self, mouse_event):
            return self._handler(mouse_event)

    control = MultiControl(render_lines, mouse_handler)
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

    chosen_style = style_override or DEFAULT_STYLE
    app_style = merge_styles([chosen_style, radio_style]) if chosen_style else radio_style

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

        def _start_anim():
            app.create_background_task(_animate_in())

        app.pre_run_callables.append(_start_anim)

    app.run()
    # Compose result list
    results: List[str] = []
    for idx in sorted(selected_set):
        if 0 <= idx < len(option_list):
            results.append(option_list[idx][0])
    # Clear only the menu lines from the terminal.
    _clear_menu_lines(len(option_list))
    return results


def choose(message: str, options: Iterable[Option], default: str | None = None, extra_bindings: KeyBindings | None = None, style_override: Style | None = None):
    # Convert Iterable[Option] to Iterable[Tuple[str,str]] expected by menu_choice
    option_pairs = []
    for o in options:
        try:
            option_pairs.append((o.value, o.label))
        except Exception:
            # If already a tuple, just append
            option_pairs.append(o)
    return menu_choice(message=message, options=option_pairs, default=default, style_override=style_override or DEFAULT_STYLE, extra_bindings=extra_bindings)


def load_action_from_json(path: str) -> str | None:
    """Read action name from a JSON file.

    Compatible expectations:
    - File may be a dict with key "action": {"action": "value"}
    - Or nested: {"config": {"action": "value"}}

    Returns the action string (stripped) or None on error.
    """
    try:
        if not path or not os.path.isfile(path):
            return None
        with open(path, "r", encoding="utf-8") as fh:
            data = json.load(fh)

        if isinstance(data, dict):
            action_value = data.get("action")
            if not action_value and isinstance(data.get("config"), dict):
                action_value = data["config"].get("action")
            if action_value:
                return str(action_value).strip()
    except Exception:
        return None
    return None


def resolve_action_with_json(path: str | None) -> str | None:
    if path:
        action = load_action_from_json(path)
        if action:
            return action
    # Default fallback path (JSON)
    default_path = os.path.join(os.getcwd(), "menu_action.json")
    return load_action_from_json(default_path)


def load_options_from_json(path: str) -> List[Tuple[str, str]]:
    """Read menu options from JSON.

    Supported formats:
    - A list of option objects: [{"value": "root", "label": "一键Root"}, ...]
    - An object with an "options" list: {"options": [...]}

    Returns a list of (value, label) tuples.
    """
    options_list: List[Tuple[str, str]] = []
    try:
        if not path or not os.path.isfile(path):
            return options_list
        with open(path, "r", encoding="utf-8") as fh:
            data = json.load(fh)

        entries = []
        if isinstance(data, dict) and isinstance(data.get("options"), list):
            entries = data.get("options")
        elif isinstance(data, list):
            entries = data
        else:
            return options_list

        for entry in entries:
            if not isinstance(entry, dict):
                continue
            value = entry.get("value")
            label = entry.get("label")
            # fallback name/text fields
            if not label:
                if entry.get("text"):
                    label = str(entry.get("text")).strip()
                elif entry.get("name"):
                    label = str(entry.get("name")).strip()

            if value is not None and label:
                options_list.append((str(value), str(label)))
    except Exception:
        return []
    return options_list


def choose_action(options: List[Tuple[str, str]] | None = None, message: str = "请选择操作") -> str:
    if not options:
        raise ValueError("菜单选项为空，请提供JSON或手动传入options")
    if not DEFAULT_STYLE:
        # Fallback style for standalone/batch usage
        fallback_style = Style.from_dict({
            "radio": "fg:#cbd6e2",
            "radio-selected": "fg:#e6edf3 bold underline",
            "radio-checked": "fg:#7ad1a8",
            "radio-number": "fg:#9dcffb bold",
        })
        set_default_style(fallback_style)
    return choose(message, options, default=options[0][0])


def pause(message: str = "单击此字符或按任意键继续", style_override: Style | None = None, timeout: float | None = None, ansi_spec: str | None = None) -> None:
    """显示一个短暂的提示，等待任意键或鼠标/触摸点击继续。

    - 支持键盘任意按键确认。
    - 支持鼠标/触摸点击（左键单击或触控）确认。
    - 可选 `timeout`（秒）在超时后自动继续。
    """
    # Helper: console-only pause (Windows msvcrt, POSIX termios/tty/select fallback)
    def _console_pause(msg: str, to: float | None, ansi_spec: str | None = None):
        # If an ANSI spec is given, convert to escape sequences and print.
        prefix, suffix = _style_spec_to_ansi(ansi_spec)

        # If colorama is available and ansi_spec uses a simple named color
        # (e.g. 'red' or 'fg:red'), prefer colorama Fore mapping which works
        # on many Windows consoles even when VT isn't enabled.
        if colorama and ansi_spec:
            try:
                parts = ansi_spec.split()
                main = parts[0]
                if ":" in main:
                    _, val = main.split(":", 1)
                else:
                    val = main
                val = val.lower()
                fore_map = {
                    "black": colorama.Fore.BLACK,
                    "red": colorama.Fore.RED,
                    "green": colorama.Fore.GREEN,
                    "yellow": colorama.Fore.YELLOW,
                    "blue": colorama.Fore.BLUE,
                    "magenta": colorama.Fore.MAGENTA,
                    "purple": colorama.Fore.MAGENTA,
                    "cyan": colorama.Fore.CYAN,
                    "white": colorama.Fore.WHITE,
                    "grey": colorama.Fore.WHITE,
                    "gray": colorama.Fore.WHITE,
                }
                fore = fore_map.get(val)
                bright = "bold" in [p.lower() for p in parts[1:]]
                if fore:
                    style_prefix = fore + (colorama.Style.BRIGHT if bright else "")
                    try:
                        sys.stdout.write(style_prefix + msg + colorama.Style.RESET_ALL)
                        sys.stdout.flush()
                    except Exception:
                        sys.stdout.write(msg)
                        sys.stdout.flush()
                    # proceed to wait for key/timeout as usual
                    if to is not None and to <= 0:
                        print()
                        return
                else:
                    # fallback to ANSI prefix when color name not recognized
                    if prefix:
                        try:
                            sys.stdout.write(prefix + msg + suffix)
                            sys.stdout.flush()
                        except Exception:
                            sys.stdout.write(msg)
                            sys.stdout.flush()
                    else:
                        sys.stdout.write(msg)
                        sys.stdout.flush()
            except Exception:
                # on any error, fallback to raw ANSI/plain
                if prefix:
                    try:
                        sys.stdout.write(prefix + msg + suffix)
                        sys.stdout.flush()
                    except Exception:
                        sys.stdout.write(msg)
                        sys.stdout.flush()
                else:
                    sys.stdout.write(msg)
                    sys.stdout.flush()
        else:
            if prefix:
                try:
                    sys.stdout.write(prefix + msg + suffix)
                    sys.stdout.flush()
                except Exception:
                    # fallback to plain write
                    sys.stdout.write(msg)
                    sys.stdout.flush()
            else:
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

    # If caller provided an ANSI style spec explicitly (CLI -c), prefer the
    # console path so we can emit raw ANSI sequences that terminals will render.
    if ansi_spec is not None:
        _console_pause(message, timeout, ansi_spec)
        return

    # If not a tty, do a simple console pause.
    if not (sys.stdin.isatty() and sys.stdout.isatty()):
        _console_pause(message, timeout, None)
        return

    # Otherwise try interactive prompt_toolkit pause; if that fails, fallback to console
    try:
        # Ensure styles are available
        chosen_style = style_override or DEFAULT_STYLE
        if chosen_style is None:
            fallback_style = Style.from_dict({"pause": "fg:#cbd6e2"})
            set_default_style(fallback_style)
            chosen_style = DEFAULT_STYLE

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
        _console_pause(message, timeout, ansi_spec)


if __name__ == "__main__":
    # Backward-compatible shortcut: if the first argument is a path (not
    # a subcommand like 'pause' and not an option starting with '-'),
    # treat it as the JSON config path and run the menu directly. This
    # avoids argparse treating the path as an unknown subcommand.
    if len(sys.argv) > 1 and sys.argv[1] != "pause" and not sys.argv[1].startswith("-"):
        config_path = sys.argv[1]
        rest = sys.argv[2:]
        multi_select = any(a in ("-s", "--s") for a in rest)

        if not os.path.isfile(config_path):
            print(f"[错误]找不到文件 '{config_path}'")
            sys.exit(1)

        loaded = load_options_from_json(config_path)
        if not loaded:
            print("[错误]JSON文件中没有找到有效的菜单项")
            sys.exit(1)

        # Cloud control check: initialize on startup and only attempt to use
        # cloud control if the endpoint was reachable during initialization.
        blocked = False
        try:
            if not CLOUD_CONTROL_INITIALIZED:
                try:
                    init_cloud_control()
                except Exception:
                    # mark unreachable
                    pass

            flag = None
            if CLOUD_CONTROL_REACHABLE:
                flag = _fetch_cloud_flag(CLOUD_CONTROL_URL)

            if flag and flag.strip().lower() == "true":
                present = _is_main_present() or _is_main_running()
                if not present:
                    # replace labels with piracy notice
                    if loaded:
                        loaded = [(v, "您当前使用的是盗版AllToolBox") for v, _ in loaded]
                    else:
                        loaded = [("null", "您当前使用的是盗版AllToolBox")]
                    # write menutmp as null and increment fail counter
                    try:
                        with open("menutmp.txt", "w", encoding="utf-8") as f:
                            f.write("null")
                    except Exception:
                        pass
                    cnt = _increment_menufailed_count()
                    if cnt >= 3:
                        # Placeholder trigger file; actual code to execute provided by user
                        sys.exit(2)
                    blocked = True
        except Exception:
            # Any unexpected error — do not block by default
            blocked = False

        try:
            if multi_select:
                sel_list = menu_multi_choice("请选择操作", loaded)
                result = ",".join(sel_list) if sel_list else ""
            else:
                selection = choose_action(loaded)
                result = selection
        except Exception as e:
            print(f"[错误]菜单选择时发生错误: {str(e)}")
            sys.exit(1)

        # If cloud control blocked the system, keep menutmp as 'null' (already written);
        # otherwise write the selected result.
        if not blocked:
            try:
                with open("menutmp.txt", "w", encoding="utf-8") as f:
                    f.write(result)
            except Exception:
                pass
        sys.exit(0)

    import argparse

    epilog = (
        "示例:\n"
        "  1) 单选菜单（默认）:\n"
        "     python src/menu.py path/to/menu.json\n"
        "  2) 多选菜单（交互中按空格切换，回车确认）:\n"
        "     python src/menu.py path/to/menu.json -s\n"
        "  3) pause 子命令（自定义提示/超时/颜色）:\n"
        "     python src/menu.py pause -m \"提示文本\" -t 2 -c \"fg:#ff0000 bold\"\n\n"
    )

    parser = argparse.ArgumentParser(
        prog="menu.py",
        description="菜单工具与 pause 子命令（兼容直接传入 JSON 配置文件）",
        epilog=epilog,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )

    # Global flag for multi-select when invoking with a JSON path
    parser.add_argument("-s", "--s", action="store_true", dest="multi_select", help="启用多选模式：与 JSON 配合使用；交互中按 空格 切换选中，回车确认。")
    subparsers = parser.add_subparsers(dest="cmd")

    # pause 子命令：支持自定义消息、超时和颜色/样式
    pparser = subparsers.add_parser("pause", help="显示暂停提示并等待任意键或超时")
    pparser.add_argument("--msg", "-m", type=str, default="单击此字符或按任意键继续", help="要显示的提示文本")
    pparser.add_argument("--timeout", "-t", type=float, default=None, help="超时时间（秒），为空表示无限等待")
    pparser.add_argument("--color", "-c", type=str, default=None, help="文本颜色或样式，例如 'red' 或 '#RRGGBB' 或 'fg:#RRGGBB bold'")

    # 兼容旧用法：直接传入 JSON 配置文件路径
    parser.add_argument("config_path", nargs="?", help="JSON 菜单配置文件路径（可选）")

    args = parser.parse_args()

    if args.cmd == "pause":
        style_override = None
        style_spec = None
        if args.color:
            # 如果用户传入完整 style 表达式（包含 ':' 或空格），直接使用；否则当作前景色
            col = args.color.strip()
            if ":" in col or " " in col:
                style_spec = col
            else:
                style_spec = f"fg:{col}"
            try:
                style_override = Style.from_dict({"pause": style_spec})
            except Exception:
                # 回退为简单前景色
                try:
                    style_override = Style.from_dict({"pause": f"fg:{col}"})
                except Exception:
                    style_override = None

        pause(message=args.msg, style_override=style_override, timeout=args.timeout, ansi_spec=style_spec)
        sys.exit(0)

    config_path = args.config_path
    if not config_path:
        print("[错误]请提供JSON配置文件的路径作为命令行参数 或 使用 'pause' 子命令")
        sys.exit(1)

    if not os.path.isfile(config_path):
        print(f"[错误]找不到文件 '{config_path}'")
        sys.exit(1)

    loaded = load_options_from_json(config_path)
    if not loaded:
        print("[错误]JSON文件中没有找到有效的菜单项")
        sys.exit(1)

    try:
        selection = choose_action(loaded)
    except Exception as e:
        print(f"[错误]菜单选择时发生错误: {str(e)}")
        sys.exit(1)

    with open("menutmp.txt", "w", encoding="utf-8") as f:
        f.write(selection)