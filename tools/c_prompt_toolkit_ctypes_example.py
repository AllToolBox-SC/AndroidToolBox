# -*- coding: utf-8 -*-
"""ctypes example for c_prompt_toolkit.dll (MSVC build)."""

import ctypes
import os
from ctypes import wintypes

DLL_PATH = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "bin", "c_prompt_toolkit.dll"))

if not os.path.exists(DLL_PATH):
    raise FileNotFoundError(f"DLL not found: {DLL_PATH}")

lib = ctypes.WinDLL(DLL_PATH)


class CptkPromptOptions(ctypes.Structure):
    _fields_ = [
        ("multiline", ctypes.c_int),
        ("mouse_support", ctypes.c_int),
        ("keymap", ctypes.c_int),
        ("max_buffer_chars", ctypes.c_size_t),
        ("history_capacity", ctypes.c_size_t),
        ("completion_cb", ctypes.c_void_p),
        ("highlight_cb", ctypes.c_void_p),
        ("user_data", ctypes.c_void_p),
    ]


lib.cptk_default_options.restype = CptkPromptOptions
lib.cptk_create.argtypes = [ctypes.POINTER(CptkPromptOptions)]
lib.cptk_create.restype = ctypes.c_void_p
lib.cptk_destroy.argtypes = [ctypes.c_void_p]
lib.cptk_destroy.restype = None

lib.cptk_prompt.argtypes = [
    ctypes.c_void_p,
    ctypes.c_wchar_p,
    ctypes.c_wchar_p,
    ctypes.c_size_t,
]
lib.cptk_prompt.restype = ctypes.c_int


def main() -> None:
    opts = lib.cptk_default_options()
    opts.multiline = 0
    opts.keymap = 0  # CPTK_KEYMAP_EMACS

    ctx = lib.cptk_create(ctypes.byref(opts))
    if not ctx:
        raise RuntimeError("cptk_create failed")

    try:
        buf = ctypes.create_unicode_buffer(1024)
        rc = lib.cptk_prompt(ctx, "PY> ", buf, 1024)
        print(f"rc={rc}, text={buf.value!r}")
    finally:
        lib.cptk_destroy(ctx)


if __name__ == "__main__":
    main()
