#include <iostream> // 提供终端输出
#include "EtherCAT_Master.h" // 引入主站类声明，头文件路径由库目标自动传递

int main(int argc, char* argv[]) // argc 是命令行参数数量，argv 保存参数字符串
{
    const char* interfaceName = argc > 1 ? argv[1] : "enp2s0"; // 优先使用命令行指定的网卡，否则使用 enp2s0
    EtherCAT_Master master; // 创建主站对象，内部持有独立的 SOEM 上下文

    std::cout << "打开 EtherCAT 网卡：" << interfaceName << std::endl; // 输出准备打开的网卡名称

    if (!master.Open(interfaceName)) // 调用主站库打开网卡，并检查是否失败
    {
        std::cerr << "打开网卡失败，请检查网卡名称和运行权限。" << std::endl; // 输出错误信息
        return 1; // 返回非零值，表示程序执行失败
    }

    const int slaveCount = master.ScanSlaves(); // 调用 SOEM 扫描并初始化从站

    if (slaveCount <= 0) // 检查是否没有发现从站或初始化失败
    {
        std::cerr << "扫描失败或未发现 EtherCAT 从站，返回值：" << slaveCount << std::endl; // 输出扫描结果
        return 2; // 退出时 master 的析构函数会自动关闭网卡
    }

    std::cout << "发现从站数量：" << master.GetSlaveCount() << std::endl; // 读取并输出已发现的从站数量

    master.Close(); // 主动关闭主站连接，析构时再次调用 Close 也不会重复关闭
    return 0; // 返回零，表示程序执行成功
}
