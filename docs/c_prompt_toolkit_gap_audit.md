# c_prompt_toolkit 差距审计（当前基线）

## 已完成

- 交互核心：单行编辑、左右/Home/End、Backspace/Delete
- 历史：上下导航 + 快照恢复
- 补全：回调首候选插入
- 编辑模式：Emacs + Vi 基础双态（ESC/i/a/A/h/j/k/l/x/0/$）
- 菜单：编号选择
- 数据源：demo 可读取 src/menu/start/menus.json 的 main/options/label
- 回归：stdin 回放 + 键级状态机回放

## 未完成

- 多行布局、软换行、滚动窗口
- 可视化补全菜单与多候选导航
- 高亮与样式渲染管线
- 鼠标事件映射到组件行为
- IME 与 Unicode 组合输入（当前键级回放为 ASCII）
- widgets/toolbars/高级布局容器
- 更完整 vi 命令集（w/b/e/d/c/y 等）

## 回归门禁建议

- 构建：build_c_prompt_toolkit_msvc.bat
- 基础烟测：test/c_prompt_toolkit_smoke.bat
- stdin 回放：test/run_replay_case.bat test/replay/case_main_menu_001.trace test/replay/case_main_menu_001.expect
- 键级回放：
  - test/run_replay_keys_case.bat test/replay/case_keys_vi_001.trace test/replay/case_keys_vi_001.expect
  - test/run_replay_keys_case.bat test/replay/case_keys_history_001.trace test/replay/case_keys_history_001.expect
  - test/run_replay_keys_case.bat test/replay/case_keys_completion_001.trace test/replay/case_keys_completion_001.expect
