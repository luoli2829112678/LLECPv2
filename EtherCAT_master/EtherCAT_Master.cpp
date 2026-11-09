#include "EtherCAT_Master.h" // 引入类声明
extern "C" { // SOEM 使用 C 链接规则
#include "soem/soem.h" // 引入当前版本的 SOEM 接口
}

struct EtherCAT_Master::Impl
{
    ecx_contextt context{}; // 每个主站拥有独立上下文，初始清零
    bool opened = false; // 记录网卡打开状态
};

EtherCAT_Master::EtherCAT_Master() : m_impl(std::make_unique<Impl>()) {} // 在堆上分配上下文
EtherCAT_Master::~EtherCAT_Master() { Close(); } // 销毁前释放网卡资源

bool EtherCAT_Master::Open(const char* interfaceName)
{
    if (interfaceName == nullptr || interfaceName[0] == '\0') return false; // 拒绝无效网卡名称
    Close(); // 先释放旧连接
    m_impl = std::make_unique<Impl>(); // 重新创建干净上下文
    m_impl->opened = ecx_init(&m_impl->context, interfaceName) > 0; // 调用 SOEM 打开网卡
    return m_impl->opened; // 返回打开结果
}

void EtherCAT_Master::Close()
{
    if (!m_impl->opened) return; // 未打开时直接返回
    ecx_close(&m_impl->context); // 关闭 SOEM 网卡连接
    m_impl->opened = false; // 清除连接状态
    m_impl->context.slavecount = 0; // 清除从站数量
}

int EtherCAT_Master::ScanSlaves()
{
    if (!m_impl->opened) return -1; // 必须先打开网卡
    return ecx_config_init(&m_impl->context); // 当前 SOEM 版本仅接收上下文参数
}

int EtherCAT_Master::GetSlaveCount() const { return m_impl->context.slavecount; } // 读取从站数量
bool EtherCAT_Master::IsOpen() const { return m_impl->opened; } // 读取连接状态
