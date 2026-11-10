# EtherCAT_Master

一个对象对应一张独立网卡和一条 EtherCAT 总线。每个对象有独立的 SOEM 上下文、PDO 内存、周期线程、监测计数器和 FoE 状态。使用不同网卡创建多个对象，即可同时工作；不要让两个对象或两个进程控制同一条总线。

## 构建与测试

工程使用 Linux/POSIX SOEM 后端。需要 CMake、C/C++17 编译器和 pthread。网卡操作需要 root 或相应的 RAW 网络权限；模拟测试无需网卡和特权。

```sh
cmake -S . -B build-linux -DBUILD_TESTING=ON
cmake --build build-linux --target LL_Controller LL_CPS master_tests --parallel
ctest --test-dir build-linux --output-on-failure
```

本目录是底层主站库；工程入口已改为 `LL_Controller` 和 `LL_CPS`，启动与主站注册方式见工程根目录 README。原多网卡直接调用示例保留在 `LL_Controller/examples/multi_master.cpp`，不作为生产可执行文件构建。

周期线程采用单调时钟和绝对截止时间，不提供操作系统硬实时保证。控制器选择 `CycleOptions::background=false` 统一调度当前主站；独立主站默认使用自己的后台线程。

## 单个主站的使用顺序

```cpp
EtherCAT_Master master;
if (!master.Open("enp2s0") || master.ScanSlaves() <= 0) {
    // master.GetLastError()
    return;
}
// 在 PRE_OP 使用 ReadSDO / WriteSDO 配置设备。
if (master.ConfigureProcessData(true) <= 0) return;
// 如设备需要：master.ConfigureSync0(1, true, 1000000, 0);
// 设备要求的周期应与下方 period 匹配。
EtherCAT_Master::CycleOptions cycle;
cycle.period = std::chrono::microseconds(1000);
cycle.autoRecover = false;
if (!master.Start(cycle)) return;
// WriteOutputs / ReadInputs 按从站复制数据，不暴露内部指针。
auto status = master.GetMonitorStatus();
auto slaves = master.GetSlaves();
master.Stop();
master.Close();
```

`Start` 先确认 SAFE_OP，再发送有效过程数据并请求 OP，成功后创建周期线程。`Stop` 回收该线程并请求 SAFE_OP；`Close` 完成停止后释放网卡。再次扫描会关闭并重新打开当前网卡，避免重复初始化 SOEM 邮箱队列。运行时拒绝扫描、重映射、手动周期收发和状态切换。

PDO 使用分离输入/输出、按从站字节对齐的映射。读写范围按从站位数向上取整，`SlaveInfo` 同时提供起始位；数据的位、字节序及设备含义由应用处理。输入和状态 getter 返回快照。

`ReadSDO` / `WriteSDO` 返回原始 SOEM 工作计数器（大于零表示成功），读取长度通过引用返回，不会把通讯失败伪装成数值零。邮箱操作和周期线程共享该实例的互斥锁；周期运行中的同步 SDO 可能推迟周期，严格时序应用应在停止周期时配置 SDO。

## 监测与恢复

`GetMonitorStatus()` 包括实际/预期 WKC、收发周期数、发送错误、WKC 错误、连续错误、错过的周期截止时间、恢复尝试数、DC 时间和总线 AL 状态。计数表示本封装的周期操作，不是 SOEM 驱动层精确的物理帧统计。

`GetSlaves()` 包括厂商/产品/版本/序列号、名称、PDO 大小、DC 能力、AL 状态/错误码及掉站标记。周期线程默认每 100 ms 刷新状态，连续 WKC 错误达到阈值时提前检测；`PollMonitor()` 可手动刷新。

`autoRecover` 默认关闭。显式开启后，会确认 SAFE_OP 错误、请求 OP、重配置异常状态从站并尝试找回掉站从站。恢复按后续监测结果确认，恢复 OP 后将继续使用已保存的输出。停止或执行 FoE 时不会自动恢复 OP。

所有网络调用均使用当前实例的 `ecx_*` 上下文。SOEM 自带全局 PDO 映射工作区，封装只在扫描/映射/恢复配置时使用全局配置锁；正常周期收发不使用这个全局锁。配置和恢复属于非实时操作，可能导致对应主站跳周期。

## FoE

FoE 为同步读写接口，文件上限为 256 MiB；读取必须指定容量上限。返回值大于零表示传输成功，其余保留 SOEM 错误值，本地参数/文件/状态错误返回 -1；详情通过 `GetLastError()` 获取。

```cpp
master.Stop();
EtherCAT_Master::FoEOptions options;
options.bootstrap = true;
options.password = 0;
const int result = master.FoEWriteFile(1, "/tmp/firmware.bin", "firmware.bin", options);
if (result <= 0) {
    // master.GetLastError()
}
// BOOT 传输成功或失败后均请求 INIT，并使旧 PDO 映射失效。
// 检查设备升级结果/重启要求后，重新扫描、映射、启动。
if (master.ScanSlaves() > 0 && master.ConfigureProcessData() > 0) {
    master.Start();
}
```

`bootstrap=true` 执行 INIT → 从 SII 读取 BOOT 邮箱 → 配置 SM0/SM1 → BOOT → FoE → INIT。失败路径也请求 INIT 并恢复本地普通邮箱参数，强制重新扫描后才能映射。固件可能要求设备重启，库不会自行重启设备或自动返回 OP。

`bootstrap=false` 在 PRE_OP 或已有 BOOT 邮箱状态传输，适用于普通设备文件。可在 `Stop()` 后用 `RequestState(0, State::PreOp)` 请求 PRE_OP。FoE 读失败不会覆盖调用方已有数据；文件读取成功后才写入目标本地文件。目标文件写入失败通过返回值和 `GetLastError` 报告，FoE 状态仍表示远端传输的结果。

可在另一个线程调用 `GetFoEStatus()` 查看活动状态、已传输字节、总量和结果。读取时总量在成功后才确定；写入进度按当前 SOEM 的“剩余字节”hook 转换。FoE 状态由每个实例独立保存，C 回调通过线程局部指针路由到当前传输，支持两个主站并行 FoE。其他状态 getter 与 FoE 共用实例锁，传输期间会等待。

参考代码中名为“写 ESI”的操作实际上直接写 EEPROM 二进制，不能用 XML 替代；本次没有将其作为 FoE 功能接入。

## 验证范围

`tests/master_tests.cpp` 使用真实 SOEM 头文件和模拟网络函数，覆盖多实例 PDO/SDO 隔离、配置工作区串行化、并行周期、WKC 掉站恢复、FoE 进度、BOOT 失败清理、重新扫描约束，以及关闭一个实例后另一个继续运行。它不验证真实网卡、设备邮箱或实时周期精度，仍需在 Linux 和目标从站上进行实机验收。