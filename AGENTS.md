# 项目规则

- 如果需要用到代理，使用以下地址：`http://127.0.0.1:7890`。

## C++ 标准与可移植性

- 使用 C++23，主要工具链为 MSVC，Linux 上使用 GCC、Clang。
- 尽量使用标准 C++ 和标准库，避免编译器扩展和操作系统专有 API。
- 确实必须使用平台或编译器专有实现时，将其集中在兼容层，通过宏进行条件编译，并提供可用的兼容实现或清晰的编译期诊断。
- 不将本机 Visual Studio 安装路径写死到可共享的工程配置中。
- CMake 中的项目文件和目录路径必须基于 `PROJECT_SOURCE_DIR`、`PROJECT_BINARY_DIR` 或相应的 `CMAKE_CURRENT_SOURCE_DIR`／`CMAKE_CURRENT_BINARY_DIR` 等变量；源码列表、include 目录、生成脚本、依赖和测试输入均不得依赖启动进程的工作目录。
- 业务规则不直接依赖 HTTP、WebSocket、操作系统时钟、线程调度或集群设施。

## 核心代码设计

- 大多数已有核心逻辑优先使用可内联、直接调用／引用的实现方式。短小、稳定、频繁调用的函数优先在类内或头文件中定义；不为了形式上的分层增加转发或动态分派。
- 优先具体类型、值语义和引用，尽量减少继承、派生类与指针；不为尚未出现的需求建立虚基类、泛化工厂或智能指针所有权网络。
- 优先低开销实现。可直接原位构造时使用 `emplace`／`emplace_back`，减少不必要的临时对象、拷贝、重复查找和热路径内存分配；可复用的工作缓冲区预留容量并重复使用。对于已经构造好的对象，根据实际复制／移动语义选择操作，不把机械替换 `push` 当作性能优化。
- 浮点大小比较使用标准库浮点比较函数（`std::isless`、`std::isgreater`、`std::islessequal`、`std::isgreaterequal` 等）；需要近似相等时显式规定误差阈值并用标准库函数比较，不能用随意增大的容差掩盖逻辑差异。整数比较不套用浮点函数。
- 在确实转移所有权且原值不再使用的位置采用移动语义；避免移动 `const` 对象造成实际复制，避免返回局部值时多余的 `std::move` 阻碍复制消除。容器尽量按已知规模预分配，热路径临时缓冲区优先复用／缓存。
- 在不影响核心功能和正确性的前提下，能静态就静态，能编译期就编译期：固定规则、映射和容量优先采用 `constexpr`、固定数组和静态分派，避免不必要的动态分配、动态分派及重复运行时计算。不以任意固定上限截断游戏内容，不把每局可变状态放进共享静态对象。
- 只读共享数据优先由调用方持有，核心通过 `const` 引用使用。引用的生命周期必须清晰，不能把减少指针变成悬空引用；只有确实需要共享所有权时才用 `shared_ptr`。
- 内联不使用编译器专有的强制内联指令。复杂实现可保留在 `.cpp` 中，由编译器优化，避免为内联而把整个后端堆进头文件。
- 在必要的边界保留扩展接口，例如数据载入、命令输入、状态／事件输出、外部时间推进和未来的战斗执行；优先用参数、值类型结果、组合或静态多态表达，确有运行时替换需求时再引入动态多态。
- 所有自有代码应有较为详细且有用的注释，说明模块职责、公开接口约束、所有权／生命周期、游戏规则来源、状态转换、关键计算顺序及易错边界；注释解释原因和契约，避免逐行复述代码。生成代码由生成器维护来源和结构说明，不手工改生成结果。
- 完成全部功能任务后，再统一补充并复核必要注释；以公开接口契约、生命周期、关键规则、状态转换及边界条件为重点，不以注释整理替代未完成的功能实现。

## C++ 命名规范

| 类别 | 格式 | 示例 |
| --- | --- | --- |
| 类和结构体 | `UpperCamelCase` | `BattleState` |
| Concepts | `vUpperCamelCase` | `vBattleExecutor` |
| 枚举类型 | `UpperCamelCase` | `DamageType` |
| 联合类型 | `UpperCamelCase` | `EventPayload` |
| 模板参数 | `_UpperCamelCase` | `_ValueType` |
| 函数、方法、构造函数及 Lambda 的参数 | `_lowerCamelCase` | `_playerId` |
| 局部变量 | `lowerCamelCase` | `remainingCopies` |
| 全局变量 | `UpperCamelCase` | `DefaultCatalog` |
| Lambda 对象 | `lowerCamelCase` | `canPurchase` |
| 全局函数 | `UpperCamelCase` | `DeriveSeed` |
| 类和结构体方法 | `UpperCamelCase` | `StartRound` |
| 类和结构体的非 public 字段 | `_MyUpperCamelCase` | `_MyPlayerId` |
| 类和结构体的 public 字段 | `MyUpperCamelCase` | `MyPlayerId` |
| 联合成员 | `UpperCamelCase` | `IntegerValue` |
| 枚举值 | `ALL_UPPER` | `WRONG_PHASE` |
| 其他常量 | `UpperCamelCase` | `TickSeconds` |
| 全局常量 | `UpperCamelCase` | `MaxPlayerCapacity` |
| 命名空间 | `UpperCamelCase` | `Stronghold` |
| 类型别名（typedef / using） | `UpperCamelCase` | `PlayerId` |
| 宏 | `ALL_UPPER` | `STRONGHOLD_HAS_PLATFORM_CLOCK` |
| Properties | `aUpperCamelCase` | `aHealth` |
| Events | `eUpperCamelCase` | `eBattleEnded` |

- Properties / Events 规则仅在确实存在相应语言或框架实体时适用，不为使用命名规则而引入非标准 C++ 属性／事件扩展。
- 外部接口强制的名字（如 `main`、标准库定制点）保留规范要求的拼写。
- 原项目的 JSON 字段、协议字段、数据 ID 和测试对照输入保持原名；在适配器中映射为符合上述规范的 C++ 名称。
- 结构体采用非空花括号聚合初始化时，必须使用指定成员初始化，例如 `CombatStats{.MyMaxHealth = 100, .MyAttack = 20}`；指定成员按声明顺序排列，允许省略采用默认值的成员。空 `{}` 可保留作为默认初始化。该规则不要求把普通构造函数调用或 `emplace` 参数改为聚合初始化。
- 文件名和目录名不在截图的命名规则中，沿用工程内已确立的形式。