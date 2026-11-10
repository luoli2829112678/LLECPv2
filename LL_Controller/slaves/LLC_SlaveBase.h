#pragma once
#include "LLC_main.h"
#include "LLC_Robot_def.h"
#include "LLSMC/CIA402Axis/CIA402Axis.h"
#define AXISSLOTSIZE 4


class LLC_SlaveBase
{
protected:
    //是否为虚拟
    bool m_bVirtual = false;
    //从站id
    uint16 m_nSlaveID = 0;
    // 总线周期时间，单位: ms
    float m_fAxisEtherCatCycleTime;
    //设置总线周期
    int SetEtherCatCycle(float fEtherCatCycle);
    //轴配置
    std::vector<CIA402Axis*>m_vAxes;
    unsigned int m_nAxisCNT = 0;
    // 从站PDO信息
    ec_slavet* m_pSlaves = nullptr;
    // 从站操作
    ST_EtherCATInterface m_stEtherCATInterface;

    ST_LLPCSlaveConfigParam m_stSlaveConfigParam;
    const ESI_Parse::Device* m_pDeviceConfig = nullptr;
    //轴插槽
    vector<ST_LLPC_AxisPDO> m_vAxisPDOs;
    //配置好的过程数据
    std::vector<ESI_Parse::Pdo> m_vTxPDOs;
    std::vector<ESI_Parse::Pdo> m_vRxPDOs;
    // SOEM FoE回调函数必须是普通函数或者static函数
    static int FoEProgressHook(uint16 slave, int packetnumber, int datasize);
    // 当前正在执行FoE升级的Slave对象
    static thread_local LLC_SlaveBase* s_pFoEUpdatingSlave;
    // 当前固件总大小
    int m_nFoETotalSize = 0;
    // 上层设置的进度回调
    std::function<void(double)> m_fnFoEProgressCallback;
public:
    LLC_SlaveBase(/* args */);
    ~LLC_SlaveBase();
    // 映射从站PDO信息
    int InitSlave(int nSalveID, bool bVirtual,double dEtherCatTimeCycle,ST_LLPCSlaveConfigParam stSlaveConfigParam,ec_slavet* pSlaves, bool configureAssignment = true);

    //输出轴
    vector<CIA402Axis*> GetCIA402Axis();
    void MC_SlaveAxisRT();

    bool IsSlaveOP();
    //
    int InsertAxis(vector<ST_LLPC_AxisPDO> v_AxisPDOs);
    //读取PDO
    std::vector<ESI_Parse::Pdo>* GetTXPDOList();
    std::vector<ESI_Parse::Pdo>* GetRXPDOList();

    //设置设备配置
    int SetDeviceConfig(const ESI_Parse::Device* DeviceConfig);
    //设置EtherCAT控制块
    void SetEtherCATFuncCB(ST_EtherCATInterface stEtherCATInterface);
    // 读取SDO
    int ReadSDO(int nSlave, uint16_t index, uint8_t subidx, int nLen, int &Value);
    // 写入SDO
    int WriteSDO(int nSlave, uint16_t index, uint8_t subidx, int Value, int nLen);

    // uint8_t writeSDO_CHAR(int slave_no, uint16_t index, uint8_t subidx, char value);
    uint8_t writeSDO_SHORT(int slave_no, uint16_t index, uint8_t subidx, short value);
    uint8_t writeSDO_INT(int slave_no, uint16_t index, uint8_t subidx, int value);
    uint8_t writeSDO_LONG(int slave_no, uint16_t index, uint8_t subidx, long value);
    uint8_t writeSDO_UCHAR(int slave_no, uint16_t index, uint8_t subidx,unsigned char value);
    uint8_t writeSDO_USHORT(int slave_no, uint16_t index, uint8_t subidx,unsigned short value);
    uint8_t writeSDO_UINT(int slave_no, uint16_t index, uint8_t subidx,unsigned int value);
    uint8_t writeSDO_ULONG(int slave_no, uint16_t index, uint8_t subidx,unsigned long value);

    // char readSDO_CHAR(int slave_no, uint16_t index, uint8_t subidx);
    short readSDO_SHORT(int slave_no, uint16_t index, uint8_t subidx);
    int readSDO_INT(int slave_no, uint16_t index, uint8_t subidx, int *Value);
    long readSDO_LONG(int slave_no, uint16_t index, uint8_t subidx);
    unsigned char readSDO_UCHAR(int slave_no, uint16_t index, uint8_t subidx);
    unsigned short readSDO_USHORT(int slave_no, uint16_t index, uint8_t subidx);
    unsigned int readSDO_UINT(int slave_no, uint16_t index, uint8_t subidx);
    unsigned long readSDO_ULONG(int slave_no, uint16_t index, uint8_t subidx);

    int WritePDO(uint16_t index, uint8_t subidx, int Value);
protected:
    int InitPDOOffset();
};