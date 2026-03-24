# Replay Trace Format (v1)

本格式用于对 `c_prompt_toolkit_demo.exe` 做可重复回放测试。

## trace 文件

- 文本文件，UTF-8。
- 每行代表一次 `stdin` 输入（按回车提交）。
- 第一行用于 prompt 输入，第二行用于菜单选择。
- 注释行以 `#` 开头，空行忽略。

示例：

hello
5

## expect 文件

- 文本文件，每行是一个必须出现在 stdout 的子串。
- 注释行以 `#` 开头，空行忽略。

示例：

=== c_prompt_toolkit PoC ===
You typed: hello
Selected: 5. Exit

## 执行器

`test/run_replay_case.bat <trace> <expect>`

判定规则：
- demo 进程退出码必须为 0。
- expect 中每行都必须被 `findstr` 命中。

## 键级回放（v2）

用于验证编辑状态机（vi/emacs、历史、补全）而不是终端交互。

执行器：

`test/run_replay_keys_case.bat <trace> <expect>`

trace 指令：

- `MODE EMACS|VI`
- `HISTORY <text>`
- `SET <text>`
- `TEXT <text>`
- `KEY ENTER|LEFT|RIGHT|HOME|END|UP|DOWN|BACKSPACE|DELETE|TAB|ESC|CTRL_C`

vi 普通模式补充说明：

- 通过 `TEXT` 输入普通模式命令字符（如 `w/b/e/d/c/y/p/i/a/A/0/$/x`）。
- 组合命令使用连续字符表示，例如：`TEXT dw`、`TEXT ce`、`TEXT yy`。

输出基线：

- `FINAL text=<...>`
- `FINAL cursor=<...>`
- `FINAL mode=<emacs|insert|normal>`
