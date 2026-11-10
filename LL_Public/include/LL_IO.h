#pragma once
#include <cstdint>
struct ST_LLPC_IO_Config
{
    int m_IO_id;
    uint16_t m_nSlaveID = 0;          // EtherCAT 从站ID
    uint16_t m_nPDOIndex = 0;         // PDO对象索引
    uint16_t m_nPDOSubIndex = 0;      // PDO对象子索引
    uint16_t m_nBit = 0;              // IO位偏移（0~7）
    bool m_bInverted = false;
	ST_LLPC_IO_Config():m_IO_id(-1),m_nSlaveID(0),m_nPDOIndex(0),m_nPDOSubIndex(0),m_nBit(0)
	{
	}
};
