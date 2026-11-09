#pragma once // 防止头文件重复包含
#include <memory> // 提供智能指针

class EtherCAT_Master
{
public:
    EtherCAT_Master(); // 创建独立的 SOEM 上下文
    ~EtherCAT_Master(); // 自动关闭网卡
    EtherCAT_Master(const EtherCAT_Master&) = delete; // 禁止复制网卡资源
    EtherCAT_Master& operator=(const EtherCAT_Master&) = delete; // 禁止复制赋值
    bool Open(const char* interfaceName); // 打开网卡，例如 enp2s0
    void Close(); // 关闭网卡，可重复调用
    int ScanSlaves(); // 扫描并初始化从站；未打开返回 -1
    int GetSlaveCount() const; // 返回已发现的从站数量
    bool IsOpen() const; // 返回网卡是否已打开

private:
    struct Impl; // 隐藏 SOEM 的具体类型
    std::unique_ptr<Impl> m_impl; // 独占内部实现，同一实例需串行调用
};
