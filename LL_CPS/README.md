# LL_CPS 工程站

界面使用随附的 EUI-NEO 源码，C++17，GLFW / OpenGL，默认使用白色主题。Windows 默认构建工程站；Linux 服务器默认仅构建 `LL_Controller`。需要 Linux 图形界面时显式开启 `LLECP_BUILD_CPS`。测试程序只在 `BUILD_TESTING=ON` 时构建。

## Windows / Visual Studio

安装 Visual Studio 的“使用 C++ 的桌面开发”和 Windows SDK。仓库不需要联网下载 UI 依赖。

```powershell
powershell -ExecutionPolicy Bypass -File .\LL_CPS\GenerateVisualStudio.ps1 -Build
```

脚本自动查找 Visual Studio 2022 / 2026 和其随附的 CMake，生成 `build/windows-vs/LLECP.sln` 或 `LLECP.slnx`。也可以在 Visual Studio 中打开该解决方案，将 `LL_CPS` 设为启动项目，选择 x64 / Release。输出位于 `build/windows-vs/bin/LL_CPS.exe`。运行时保留同目录的 `ESI_Parse.dll` 与 `assets` 文件夹。

Release 输出在 `bin`，Debug 在 `bin/DEBUG`，其他配置各有子目录，避免调试库与发布库互相覆盖。

## Linux / CMake

以 Debian / Ubuntu 为例，准备 C/C++ 编译器、CMake、Ninja、OpenGL 与 X11 开发包：

```sh
sudo apt install build-essential cmake ninja-build libgl1-mesa-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev zenity
sudo mkdir -p /usr/local/bin/LLECT /usr/local/ect/LLECP
# 将这两个专用目录的写权限交给实际构建/运行用户，或用具备权限的账户构建。
cmake -S . -B /usr/local/bin/LLECT/build/linux-desktop -G Ninja -DCMAKE_BUILD_TYPE=Release -DLLECP_BUILD_CPS=ON
cmake --build /usr/local/bin/LLECT/build/linux-desktop --parallel
/usr/local/bin/LLECT/LL_CPS
```

Linux 的可执行文件、动态库和静态库统一输出到 `/usr/local/bin/LLECT`，沿用控制器工程设置。默认使用 X11，Wayland 桌面可通过 XWayland 运行。控制器所需的原始网口访问权限沿用原工程。

只构建其中一端：`-DLLECP_BUILD_CONTROLLER=OFF` 或 `-DLLECP_BUILD_CPS=OFF`。输出目录可通过 `-DLLECP_OUTPUT_DIRECTORY=...` 指定。

VS Code 的“配置 LLECP / 编译全部 / 配置测试”在 Linux 使用 `/usr/local/bin/LLECT/build/linux`，明确设置 `LLECP_BUILD_CPS=OFF`，不依赖 X11。“编译 LL_CPS”单独使用 `/usr/local/bin/LLECT/build/linux-cps`，需要上面的图形开发包。CMake 预设 `linux` 为服务器配置，`linux-cps` 为 Linux 图形端配置。旧缓存中的开关不会被默认值自动覆盖，请重新执行配置任务。

## 数据与项目

Linux 默认根目录严格使用 `/usr/local/ect/LLECP`；Windows 默认使用 `%LOCALAPPDATA%\LLECP`。设置 UTF-8 环境变量 `LLECP_DATA_ROOT` 可覆盖默认目录。两个进程均在启动时扫描 `ESI`。

```text
LLECP/
  manager.json                 默认项目与项目索引
  ESI/*.xml                    导入的 EtherCAT 设备描述
  PRJ/<项目标识>/project.json    完整项目配置与源码
  PRJ/<项目标识>/HSTR/*          可直接查看的源码副本
  transfers/                   控制器临时分块传输文件
```

“打开项目”选择 `project.json`；外部项目导入到托管目录并分配新的内部标识。“另存为项目”复制所有源码与配置。项目中心可进入和删除项目，最近保存/进入的项目设为默认项目。运行中的控制项目需要先停止主站再改动拓扑。

## 使用流程

1. 新建项目，添加 EtherCAT 主站，填写**控制器上的**网口名、总线周期和模拟模式。模拟模式不会访问物理网口。
2. 在“工具 / ESI 设备库”导入 XML。连接控制器后导入会同步给控制器；下载总线配置前也会同步设备库。通过扫描按实际拓扑顺序导入从站，或从设备库手动添加。厂商 ID、产品代码与版本必须匹配，不支持的设备不能作为已支持设备导入。
3. 勾选 PDO 通道，设置 DC 和偏移。添加轴后检测 CiA 402 对象，未匹配字段为空；点击映射字段从已选 PDO 中选择。控制字和状态字不完整时不能注册该轴。改变 PDO 后需要重新检测/检查轴映射。
4. 在 HSTR 页面编辑程序、函数、功能块、类型、GVL 与 `.task`。任务以设计文档中的 JSON 格式编辑，包含 Cyclic / Event / FreeRunning、周期、优先级、看门狗、CPU 与程序列表。添加任务不会自动分配程序，请填写 `programs`。
5. 连接控制器，生成解决方案，查看底部“生成与输出”。编译发生在 Linux 控制器上；Windows 工程站不使用本机编译器生成控制器模块。编译异步进行，保留总线通信。运动中禁止重新编译。
6. 启动主站，再运行或调试 HSTR。多个主站可同时运行并独立停止；修改拓扑需要全部停止。首次启动会预先验证并准备项目内全部总线，因此所有配置的物理网口与设备应当可访问。轴在运动核心内部采用连续编号，工程保存的轴标识与内部编号由控制器映射。
7. 底部变量调试框可搜索/添加变量，写入、强制与取消强制。普通标量用十进制，BOOL 用 TRUE/FALSE；聚合值可用 `HEX:` 后接精确长度的字节。强制值在 HSTR 扫描前后施加，停止、复位、下载和重新编译会清除强制。
8. 示波器搜索变量后开始采样。CPS通过8888下发采样列表，随后使用已有8893实时数据端口上的独立TCP连接收取数据；变量监视使用另一独立连接。最多32通道，采样周期1–1000ms，显示最近20000个样本。通道可隐藏/移除，支持CSV导入导出；未连接时不生成模拟实时数据。1000ms等于1Hz，不能用于观察快速运动波形。
9. 主站启动后才允许 FoE、EEPROM 和从站状态操作。FoE 会停止目标主站并进入所需传输状态，完成后手动重新启动。读取固件后点击“传输进度 / 保存”保存到 CPS 本机。`烧录 XML / SII` 接受厂家生成的 `.bin/.eep` EEPROM 镜像；通用 ESI XML 不能直接写入 EEPROM，未进行虚假的 XML 到 SII 转换。

顶部急停为控制器软件停机操作：停止应用、清除运动输出并停止主站。解除后需要显式重新启动，不会自动恢复运动。它不代替驱动器的硬件安全回路。状态栏显示实际连接、控制器状态与 HSTR 状态。

主站页显示成功发送的过程数据帧数量、实测每秒帧数、TX发送失败、RX无帧/超时以及独立的WKC异常计数；计数从网口打开/扫描开始，模拟主站没有物理帧统计。主站页、从站轴列表和轴页面显示轴状态；轴页面还显示实际位置、速度、力矩、状态字与错误码。

右上角LED显示控制器离线、待机、运行、处理中、状态更新超时、故障或急停。启动主站时显示同步配置与请求OP的进度，失败后自动展开输出，并可点击“操作失败”查看完整原因。控制器准备总线时初始化0x1C12/0x1C13，顺序为清零计数、写入PDO索引、写入计数；SOEM建立过程映像后才初始化PDO偏移并请求OP。无法进入OP时返回主站错误和各从站AL状态。

`Fixed="1"`不表示禁止PDO分配，`PdoUpload="0"`也不再用于跳过分配初始化。写入优先使用标准UINT8计数、UINT16索引；清零被拒绝时切换为参考控制器`writeSDO_INT`的INT32格式，索引和最终计数也使用4字节。支持上传的设备读回校验；不支持上传的设备检查每次写入结果并核对实际映射的输入/输出总位数。仅当读回已证明分配一致、且两种写入宽度均被拒绝时保留只读分配。启动日志打印分配格式和PDO偏移位数。

扫描完成后，主站会逐站等待并读取PRE_OP状态，刷新SOEM的状态缓存后才允许PDO分配初始化。`ecx_config_init`只发出状态切换请求，不能直接视为邮箱已可用；当前SOEM同步邮箱实现会拒绝缓存状态低于PRE_OP的发送。SDO错误会附带对象地址、从站状态、AL状态、传输长度、邮箱长度及SOEM错误码。

从站页启动后每250ms读取一次实际过程映像，每页128项，显示PDO类型、位宽和当前值。RxPDO可单次写入，TxPDO保持只读；BOOL和不按字节对齐的位通道也支持，写入不会改变相邻位。已被注册轴接管的RxPDO显示“轴占用”，应使用轴调试操作，或停止主站、取消轴注册并重新下载配置后再直接写。HSTR程序可以在后续扫描中覆盖单次PDO写入。

PDO读取前直接刷新主站输入镜像，不触发输出发送；手动交换在接收帧后再同步输入。从站页同时显示实际/期望WKC、周期计数和前64字节的原始输入/输出。WKC不足时TxPDO标为“无有效输入帧（缓存）”；收到有效输入但内容全零时单独提示，不将缓存零值当作有效反馈。

从站顶部“从PDO生成轴”根据已选PDO中的CiA402控制字/状态字识别轴通道，生成相应轴对象。进入轴页面可选择通道、点击“自动映射PDO”，或逐项选择对象；自动映射仅接受唯一且字节对齐的标准对象，缺失或重复的对象保持为空。手动补齐控制字和状态字后会更新轴注册与PDO组信息。轴配置修改后需停止主站并重新写入配置。

## 通信与验证

控制器连接使用右上角独立按钮，文件菜单只管理项目。HSTR文件树和编辑页签使用文档图标与类型徽标：ST程序为蓝色、FUN函数为青色、FB功能块为紫色、GVL为橙色、TYPE为玫红色、TASK为灰蓝色。页签支持滚轮横向浏览，从文件树选择文件时自动定位对应页签。

未打开项目时连接按钮置灰。轴页以表格显示运行状态、OP、使能、运动、故障、控制字/状态字、实际位置/速度/力矩及模式，表格上方“添加到示波器”可直接添加该轴三个反馈通道。示波器选择器支持按轴名称、中文字段名或`Axis.0.ActualPosition`等标准变量名搜索；订阅失败会显示错误原因。控制器按解析后的标准变量名读取采样值，兼容大小写及唯一后缀。

原有 8888 / 8889 / 8890 / 8891 / 8893 / 8894 接口保留。新增 `CPS_Request` 仍走 8888，所有操作名和消息编码在 `LL_Public/include/LL_CpsProtocol.h`。请求和响应使用 Base64 JSON，文件分块传输；并无改变原有指令分隔符。

批量数据协议在`LL_Public/include/LL_DataStream.h`：每100个采样点组包，停止时发送不足100点的尾包和结束标记；单包达到编码大小上限时提前组包。采样点携带控制器实际时间戳和序号，前端不按网络到达时间重建波形，也不跨缺口连线。采样分辨率受控制器扫描周期约束，迟到时记录序号缺口，不补造历史样本。

新CPS通过`scope.subscribe`的`transport=tcp-batch`获得绑定客户端IP的随机会话，再在8893发送`DataAttach`。`DataBatch`按批序号确认，未确认包250ms后重发，断线重连继续发送；CPS去重后确认。控制器每流保留最多64个待确认数据包，超限明确上报丢样，默认30秒无确认清理，低速流按积累100点所需时间延长超时；TCP传输与采集循环分离。原8893二进制协议及旧UDP订阅保留兼容，不新增监听端口。停止按钮会等待尾包确认，超时会显示提示。

轴使能复用原CiA402状态机：无急停、无轴故障且处于允许状态时设置目标模式，位置对齐后按0x06、0x07、0x0F推进；功能块/HSTR更新后在周期末同步命令镜像至输出PDO并提交主站。软件静止状态不再直接视为驱动已使能；故障和无效状态会明确拒绝，接口“已受理”不代表驱动已完成使能，应以反馈状态字为准。

```sh
cmake -S . -B build/test -DBUILD_TESTING=ON
cmake --build build/test --parallel
ctest --test-dir build/test --output-on-failure
```

Windows 用 `ctest --test-dir build/windows-vs -C Release --output-on-failure`。覆盖项目、PDO、CSV、100帧队列、ACK保留、缓冲上限、尾包、TCP分片/去重/断线重发，以及BOOL/字符串监视。Linux另有`axis_enable_pdo_sequence`和控制器集成测试，覆盖映射缓冲上的模式及控制字序列、8893会话、ACK和未确认包重连。Linux测试占用服务端口，请先停掉正在运行的控制器。

`BUILD_TESTING=ON` 的 UI 可通过环境变量 `LLECP_UI_CHECK=<截图目录>` 运行应用内渲染检查，同时必须将 `LLECP_DATA_ROOT` 指向独立测试目录。该检查仅创建离线项目与 CSV 夹具，自动截图后退出，不连接硬件。实际 EtherCAT 时序、驱动运动、DC 同步与 FoE 升级必须在目标 Linux 控制器和设备上联调。


实时采样默认1ms，旧项目保留原先周期，点击“实时1ms”后再开始采样。1000ms是1Hz，积累100点需100秒；1ms下每批约100ms。实际频率受控制器扫描周期和系统调度约束。数据包使用精确载荷字节的CRC32校验，失败不确认、等待重传；界面分开显示已收点数、实测Hz、控制器漏采、传输缓冲溢出、重复重传与校验失败。采样过程中周期锁定，改周期需先停止。

绘图按控制器时间戳从一批数据的缓冲逐帧推进，运行时滚动显示最近5秒，停止后显示保存的历史；保留最近20000点用于CSV导出。像素列内保留首点、最大值、最小值和末点，不再固定间隔跳点，缺样处断线。接收、采集和绘图分别运行，不用插值补造丢失数据。

主站运行或启动处理中，主站配置、从站PDO选择、DC模式/偏移、轴映射与轴参数不可编辑；运动调试及过程数据写入继续按其运行条件控制。控制器仍会拒绝运行中修改总线配置。

HSTR使用独立编程工作区，带专用文件列表、类型颜色图标和横向文件页签。进入时收起底部调试框，最小窗口下文件栏可同时展示十多个文件；可用“监视/输出”展开调试框，通过“返回工程”回到设备配置。
