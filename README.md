# LLECP

## 目录和构建

工程源代码分为三层：

| 目录 | 内容 |
| --- | --- |
| `LL_Public` | 指令名称、端口、公共结构体、状态码、主站协议契约、文本协议编解码、cJSON 和 `ESI_Parse` |
| `LL_Controller` | `LLC_Rbt` 核心、原状态机及动作、轴/IO/点位接口、HSTR、LLSMC、从站、EtherCAT 主站及注册表 |
| `LL_CPS` | 简单的命令行上位机入口，与控制器使用同一套公共指令定义 |

默认只生成两个应用：`/usr/local/bin/LLECT/LL_Controller`、`/usr/local/bin/LLECT/LL_CPS`。动态库和静态库也统一生成到 `/usr/local/bin/LLECT`，程序通过 `$ORIGIN` 查找同目录动态库，避开 `/mnt` 的挂载执行限制。中间文件仍放在 `build`。

在 Linux 上，需要 CMake 3.20+、C/C++17 编译器、Python 3 和 pthread；SOEM 使用 Linux RAW 网卡后端。HSTR 已内嵌 ST 编译器和已生成的解析器文件，通常无需另装 Bison/Flex。

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
/usr/local/bin/LLECT/LL_Controller /usr/local/bin/LLECT/runtime
```

默认输出目录需要当前编译用户有写权限；可使用 `-DLLECP_OUTPUT_DIRECTORY=其他目录` 覆盖。修改输出位置后，需要重新执行 CMake 配置和编译。

控制器启动通信和 HSTR，不自动打开或启动任何总线。`runtime/ESI` 存放设备 ESI XML，`runtime/Application` 存放 HSTR 应用，`runtime/logs` 存放日志。访问 EtherCAT 网卡需要相应 RAW 网络权限。

VS Code 提供两个独立的编译/调试入口、编译全部和测试任务；按 F5 时选择 `调试 LL_Controller` 或 `调试 LL_CPS`。这些配置用于 Linux 或远程 Linux 工作区。

## 注册主站

启动主站之前必须注册。第一个注册主站自动成为旧控制接口所使用的当前主站。

```sh
/usr/local/bin/LLECT/LL_CPS 127.0.0.1 8888 RegistrationMaster m1 EtherCAT enp2s0 1000
/usr/local/bin/LLECT/LL_CPS 127.0.0.1 8888 ListMasters
```

`RegistrationMaster` 参数顺序：主站 ID、协议类型、网卡/端点、周期微秒。周期范围为 100..1000000 us，同一 EtherCAT 网卡不能重复注册。注册只创建后端对象，不访问硬件。未注册或未提供工厂的协议会明确返回失败。

公共 `LL::IMaster`、`LL::MasterConfig` 和协议类型位于 `LL_Public/include/LL_Master.h`；工厂注册表位于 `LL_Controller/Bus`。本次提供 EtherCAT 后端，Profinet、Modbus 保留类型和工厂扩展入口，尚未实现这两个协议的驱动。

### 多主站

```sh
/usr/local/bin/LLECT/LL_CPS 127.0.0.1 8888 RegistrationMaster m2 EtherCAT enp3s0 1000
/usr/local/bin/LLECT/LL_CPS 127.0.0.1 8888 StartRegisteredMaster m1
/usr/local/bin/LLECT/LL_CPS 127.0.0.1 8888 StartRegisteredMaster m2
/usr/local/bin/LLECT/LL_CPS 127.0.0.1 8888 ReadMasterStatus m2
/usr/local/bin/LLECT/LL_CPS 127.0.0.1 8888 StopRegisteredMaster m2
```

每个主站有独立上下文、PDO 缓冲区、状态和文件传输进度。当前主站由控制器周期统一调度；其他主站使用自己的周期线程。停止、升级或关闭一个主站不会关闭其他主站。

参考控制器原有的轴/IO/点位接口仍作用于当前主站。`SelectMaster,id` 要求旧主站已停止且旧硬件配置已清理，不能在运动中把原轴指针切换到另一条总线。多个主站可同时收发、监测和切换从站状态；本次没有把旧单机器人轴 ID 协议改成跨总线的轴地址协议。

## 旧控制接口与状态机

核心类统一命名为 `LLC_Rbt`，保留原有 12 个控制器状态、状态守卫、进入/退出动作和转换表。原 `RegistrationSlave`、`ConfigureSlavePDO`、`RegistrationAxis`、`RegistrationIO`、轴运动/跟随、点位、SDO/PDO、FOE、复位、停止、共享内存和数据上报等接口保留。

原命名中的品牌前缀统一改为 LL，包括文件/目录、类、结构体、宏、回调和 HSTR 硬件适配器。第三方 SOEM、matiec、pugixml、nlohmann 的上游实现和标准宏保持不变。

旧运动配置流程继续使用：

1. `RegistrationMaster` 注册并选中主站。
2. `SetSimulationType`、`InitMaster` 设置模式和周期；`InitMaster` 的端点/周期必须与注册信息一致。
3. `RegistrationSlave`、`ConfigureSlavePDO`、`RegistrationAxis`、`RegistrationIO` 配置硬件。
4. `StartMaster` 通过原状态机启动，`ControllerStop` / `ControllerReset` / `CloseMaster` 继续使用原入口。

从站 ID 按 1 开始连续注册，轴 ID 按 0 开始连续注册；IO ID 不重复。物理启动顺序为：扫描至 PRE_OP、写 PDO 分配、SOEM 映射、配置 DC、绑定逻辑轴和 IO、确认 OP。ESI 与实际 PDO 位数不符时拒绝绑定。模拟模式保留虚拟轴及虚拟 IO，不打开物理网卡。

`InitMaster` 的周期单位为微秒；`RegistrationSlave` 的 DC 偏移沿用参考接口的微秒单位，底层转换成纳秒。

## 通信端口与格式

| 端口 | 用途 |
| --- | --- |
| 8888 | 常规状态机指令、硬件配置、主站注册及管理 |
| 8889 | 跟随运动 |
| 8890 | HSTR 应用、编译、运行、变量和调试接口 |
| 8891 | 原异步/查询指令，包括 SDO 和文件传输进度 |
| 8893 | 原二进制实时状态流 |
| 8894 | 原回复/轴信息接口 |
| 10003 | 控制器向上位机发指令的客户端通道，需上位机提供服务 |

8892 已移除，不再监听。所有指令定义集中于 `LL_Public/include/LLC_CmdMsg.h`，端口及回调定义位于 `LLC_Def.h`。`LL_Public/interfaces.json` 列出保留的指令名称。

文本请求使用逗号分隔、分号结束，保留末尾逗号：

```text
RegistrationMaster,m1,EtherCAT,enp2s0,1000,;
ListMasters,;
SetSlaveState,m1,1,4,;
```

响应格式为 `命令,OK,结果...,;` 或 `命令,Fail,错误码...,;`。支持 TCP 分包和连续多条指令，单个缓冲上限 1 MiB。同一端口的执行/异步回复串行，防止结果串到其他客户端。

8893 保留原 `LTBR` 魔数、12 字节头和二进制 `RobotRTData` 数据布局，定义见公共头文件。原轴信息查询存在丢失输出参数的问题，已修复；8894 的 `ReadAxisInformation` 返回状态码和公共轴信息区的十六进制编码，避免二进制分隔符破坏文本帧。

`ReadMasterStatus,id` 返回：打开、运行、通信连接、实际 WKC、预期 WKC、周期数、WKC 错误数、错过截止时间次数。

`ReadSlaveStates,id` 返回：从站数，以及各站的 ID、AL 状态、连接标志和 AL 错误码。

## 从站状态和断联

`SetSlaveState,主站ID,从站ID,状态值,;` 支持 INIT=1、PRE_OP=2、SAFE_OP=4、OP=8。BOOT=3 由文件传输流程管理。

主动切换 PRE_OP/SAFE_OP 不会被定义为掉线，自动恢复也不会覆盖这种主动状态设置。断联依据实际 AL 状态读取无响应/丢站，不能仅依据退出 OP 或 WKC 不匹配。WKC 异常、AL 状态异常和断联分别可查询。

后台主站可配置自动恢复；默认关闭。原控制器状态机中的掉线守卫查询连接状态，进入 OP 的就绪守卫仍检查 OP。状态切换可能导致设备停输出，控制器只保证通信状态和请求结果，不替代设备安全逻辑。

## HSTR 与文件传输

HSTR 是唯一编程执行对象，保留应用/文件/任务管理、编译、运行/停止/复位、变量读写、断点、单步及任务统计接口。硬件接口清单位于 `LL_Controller/HSTR/interfaces/llpc.json`，适配器自动生成。旧执行引擎的类、回调、指令、状态字段和端口均已移除。

```sh
/usr/local/bin/LLECT/LL_CPS 127.0.0.1 8890 HSTR_CreateApplication sample
```

FOE 读写在独立任务中执行，停止当前主站周期和当前 HSTR 应用后传输，可继续查询进度。其他主站继续运行。BOOT 传输结束后返回 INIT，并要求重新扫描/映射；不自动重启设备或重新运行应用。

`UpdateESI` 保留参考实现的 EEPROM 更新用途，但输入必须是有效 SII 二进制映像，不能直接写入 XML。写入前校验长度、头部 CRC 和设备容量，逐字写后校验，并归还 EEPROM 所有权。XML 更新应替换 `runtime/ESI` 文件并调用 `ReloadESI`。

## 依赖与验证

没有引入 MQTT、XMLRPC、数据库、libevent 或平台 GUI。仅保留控制器使用的公共定义、cJSON、网络/日志/时间与字符串工具、ESI、HSTR、运动库和 SOEM。

```sh
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

测试包含模拟 SOEM 的多主站/状态/FoE/EEPROM回归、协议/并发注册测试，以及 Linux 的真实 TCP 控制器入口测试。默认关闭测试，不增加生产可执行文件。

本次已在 Windows 主机执行模拟主站、EEPROM、协议/注册测试和 HSTR 接口生成器测试，并用 Linux 交叉工具链完成全部库、两个应用和测试目标的编译/链接。Linux TCP 测试已编译，尚未在该主机执行；真实网卡、驱动器、运动和时序仍需在目标 Linux 设备上验收。
