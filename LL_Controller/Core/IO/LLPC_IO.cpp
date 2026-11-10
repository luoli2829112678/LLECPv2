
#include "LLPC_IO.h"

LLPC_IO::LLPC_IO(uint16_t slaveID, uint16_t pdoIndex, uint16_t pdoSubIndex, uint16_t bit)
    : m_IO_id(-1), m_nSlaveID(slaveID), m_nPDOIndex(pdoIndex), m_nPDOSubIndex(pdoSubIndex), m_nBit(bit), m_pData(nullptr)
{
}

bool LLPC_IO::IO_IsValid() const
{
    return m_pData != nullptr && m_nBit < 8; // 检查PDO数据指针及位偏移是否合法
}

bool LLPC_IO::IO_Read(bool& value) const
{
    if (!IO_IsValid()) return false; // 检查IO有效性

    const uint8_t* data = static_cast<const uint8_t*>(m_pData); // 获取PDO数据地址
    value = (((*data >> m_nBit) & 0x01u) != 0) != m_bInverted; // 读取指定IO位

    return true;
}

bool LLPC_IO::IO_Write(bool value)
{
    if (!IO_IsValid()) return false; // 检查IO有效性

    uint8_t* data = static_cast<uint8_t*>(m_pData); // 获取PDO数据地址
    uint8_t mask = static_cast<uint8_t>(1u << m_nBit); // 生成目标位掩码

    if (value != m_bInverted) *data |= mask; // 设置目标位为1
    else *data &= static_cast<uint8_t>(~mask); // 设置目标位为0

    return true;
}
