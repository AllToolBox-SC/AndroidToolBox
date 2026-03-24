# start.py 关键交互兼容对照（PoC）

目标：内部实现可不同，呈现与关键交互保持一致。

## 对照范围

- Prompt 输入呈现
- 光标左右移动
- Backspace/Delete
- History 上下导航
- TAB completion（首候选）
- 编号菜单选择

## 对照矩阵

| 场景 | Python(prompt_toolkit) 现状 | Native C PoC 现状 | 状态 |
|---|---|---|---|
| 单行提示输入 (`ATB>`) | 支持 | 支持 | 已对齐 |
| 左右移动 | 支持 | 支持 | 已对齐 |
| Home/End | 支持 | 支持 | 已对齐 |
| Backspace/Delete | 支持 | 支持 | 已对齐 |
| 历史上下 | 支持 | 支持 | 已对齐 |
| TAB completion | 支持候选系统 | 支持回调首候选 | 部分对齐 |
| 菜单编号选择 | 支持 | 支持 | 已对齐 |
| VI 基础模式切换（ESC/i/a/A） | 支持 | 支持基础子集 | 部分对齐 |
| 多行复杂布局 | 支持 | 未完整实现 | 未对齐 |
| Widgets/Toolbars | 支持 | 未完整实现 | 未对齐 |
| IME 高级行为 | 支持 | 未完整实现 | 未对齐 |

## 验收基线

- 通过 `test/c_prompt_toolkit_smoke.bat`
- 通过 `test/run_replay_case.bat test/replay/case_main_menu_001.trace test/replay/case_main_menu_001.expect`
- 通过 `test/run_replay_keys_case.bat test/replay/case_keys_vi_001.trace test/replay/case_keys_vi_001.expect`
- 通过 `test/run_replay_keys_case.bat test/replay/case_keys_history_001.trace test/replay/case_keys_history_001.expect`
- 通过 `test/run_replay_keys_case.bat test/replay/case_keys_completion_001.trace test/replay/case_keys_completion_001.expect`

## 判定标准

- 输出中必须出现核心语句：
  - `=== c_prompt_toolkit PoC ===`
  - `You typed: <input>`
  - `Selected: <n>. <label>`
