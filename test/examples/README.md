示例配置文件 (test/examples)

包含多种格式的菜单选项示例，供 `menu.exe` 或 `menu_main` 加载测试。

示例文件：

- menu_options.json     - JSON 格式（options 数组）
- menu_options.yml      - YAML 格式
- menu_options.toml     - TOML 格式
- menu_options.xml      - XML 格式
- menu_options.csv      - CSV 格式（每行 value,label）
- menu_options.txt      - 文本格式（用竖线分隔）
- backup_9008.json      - 示例：src/bats/menu/backup_9008.json 的副本

运行示例：

在 Windows 命令行中（假设已编译出 menu.exe）：

```bat
menu.exe test\examples\menu_options.json
menu.exe test\examples\menu_options.yml
menu.exe -s test\examples\menu_options.csv
menu.exe    # 不带参数时会弹出文件选择对话框
```

错误信息现在会输出到标准输出（stdout），方便重定向到日志文件，例如：

```bat
menu.exe test\examples\menu_options.json > out.log
```
