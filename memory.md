# Mouse Double-Click Behavior Memory

Date: 2026-03-23

## Scope
- src/menu_main.c
- src/start.c
- src/menu.h
- src/menu.c
- src/c_prompt_toolkit.c

## Rules
- Left single-click: only focus/select highlight, do not activate.
- Left double-click on the same menu item: confirm and activate.
- Double-click on a different item: only move focus to that item, do not activate.

## Threshold
- Environment variable: ATB_DOUBLE_CLICK_MS
- Default: 300 ms
- Clamp range: 100..1000 ms

## Notes
- start.c native console path now applies the same rule for DOUBLE_CLICK and release events.
- c_prompt_toolkit single-choice path now confirms only on same-item double-click.
- Shared click-state helper API added to menu.h/menu.c for menu-side reuse.
