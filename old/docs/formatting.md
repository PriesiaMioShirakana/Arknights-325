# ReSharper 格式设置

以本机最新保存的 ReSharper C/C++ 设置为准。`resharper-formatting.json` 保存读取快照、来源和默认值；2026-10-08 本次刷新读取到 24 项明确覆盖，插件版本仍为 777.0.0.0。未保存的 IDE 界面值不包含在快照中。

相较上一份快照，`KEEP_USER_LINEBREAKS` 改为 `false`，`WRAP_BEFORE_COLON` 改为 `true`，`LABELED_STATEMENT_STYLE` 改为 `DO_NOT_CHANGE`。clang-format 与 EditorConfig 的 IDE 支持均为关闭状态。本仓库不恢复自动生效的 `.clang-format`。

`tools/resharper.clang-format` 是显式调用的辅助配置，面向 clang-format 22+。主要对应 Tab/4、120 列、Allman 大括号、命名空间缩进、长参数逐行及右括号单独成行、初始化列表冒号前换行。示例（在工作区根目录执行）：

```powershell
clang-format --style=file:backend/tools/resharper.clang-format -i backend/src/simulation/battle/battle.cpp
clang-format --style=file:backend/tools/resharper.clang-format --dry-run --Werror backend/src/simulation/battle/battle.cpp
```

两种格式器并非完全等价，以下以 ReSharper 为准并人工复核：

- ReSharper 区分类、多行函数、函数声明与单行函数的空行；clang-format 的统一定义块选项无法准确表达，辅助配置保留空行。
- `DO_NOT_CHANGE` 的简单嵌入语句／case 排列、`KEEP_EXISTING_ENUM_ARRANGEMENT`、case 块的 `NEXT_LINE_SHIFTED_2`、参数包省略号前后空格、空模板参数中的空格不能全部一一映射。
- `LINE_FEED_AT_FILE_END=false` 表示不强制补文件尾换行，并不要求删除已有换行。
- 头文件排序、模板声明换行等未在本次提取范围内明确对应的行为沿用已有工程约定；不要由辅助配置推断额外的 IDE 要求。

本机设置再次更新时，先刷新快照和映射，再格式化新改动；不要修改 IDE 全局配置来迎合命令行工具。
