#ifndef _LLPC_IO_H
#define _LLPC_IO_H
#include"stdint.h"


#include "LL_IO.h"




class LLPC_IO
{
protected:
    int m_IO_id;
    uint16_t m_nSlaveID = 0;          // EtherCAT 从站ID
    uint16_t m_nPDOIndex = 0;         // PDO对象索引
    uint16_t m_nPDOSubIndex = 0;      // PDO对象子索引
    uint16_t m_nBit = 0;              // IO位偏移（0~7）
    bool m_bInverted = false;
    void* m_pData = nullptr;            // PDO映射数据指针

public:
    LLPC_IO(uint16_t slaveID, uint16_t pdoIndex, uint16_t pdoSubIndex, uint16_t bit);
    ~LLPC_IO() = default;

    bool IO_Read(bool& value) const;     // 读取IO状态
    bool IO_Write(bool value);           // 写入IO状态
    bool IO_IsValid() const;             // 检查IO是否有效

    uint16_t IO_GetSlaveID() const { return m_nSlaveID; }
    uint16_t IO_GetPDOIndex() const { return m_nPDOIndex; }
    uint16_t IO_GetPDOSubIndex() const { return m_nPDOSubIndex; }
    uint16_t IO_GetBit() const { return m_nBit; }
    void IO_SetPDOPtr(void* ptr) {m_pData = ptr;}
    void* IO_GetDataPtr() const { return m_pData; }
    void IO_SetID(int id) { m_IO_id = id; } // 设置IO编号
    int IO_GetID() const { return m_IO_id; } // 获取IO编号
    void IO_SetInverted(bool inverted) { m_bInverted = inverted; } // 设置取反属性
};


























#endif