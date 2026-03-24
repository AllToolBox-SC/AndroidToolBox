# start.c 移植说明 (AllToolBox)

概述:
- 这是 `src/start.py` 的 C 语言移植版本，位于 `src/start.c`。
- 已实现的主要功能：
  - 持久化 cmd 桥（保持 cmd.exe 进程，使用管道读写）
  - 命令完成标记（marker）同步，支持等待命令完成及返回码
  - 自动检测并继续常见的“Press any key to continue”提示
  - 通过 WinINet 做一个简短的云端检查（`cloud_check()`）
  - 简单的多选菜单（控制台实现）
  - menufailed 触发文件写入（`menufailed.trigger`）

构建 (MSVC):
1. 打开 "x64 Native Tools Command Prompt for VS"（或相应的 Developer Command Prompt）。
2. 在仓库根目录运行：

```
build_msvc.bat
```

生成产物：`bin/start.exe`

运行:
- 直接运行 `bin\start.exe`。

注意事项与限制:
- 该实现旨在功能等价，但不是逐字逐句的特性镜像。
- 高级 TUI（如 prompt_toolkit 的复杂行为）、触摸/鼠标精细交互、音频播放、插件系统等仍需单独实现。
- 若需进一步完善（例如 WinHTTP、TLS 强化、完整环境表管理、线程清理更精细化），请告知我将继续迭代。
