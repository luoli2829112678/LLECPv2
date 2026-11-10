#include"LLC_SlaveBase.h"
#include <limits>
#include <cstdio>

#define FALSE               0
#define LLC_TIMEOUTSAFE     700000//20000
#define LLC_TIMEOUTRXM      700000
// 从站操作

thread_local LLC_SlaveBase* LLC_SlaveBase::s_pFoEUpdatingSlave = nullptr;

LLC_SlaveBase::LLC_SlaveBase(/* args */)
{
}

LLC_SlaveBase::~LLC_SlaveBase()
{
    //清空轴
    for (size_t i = 0; i < m_vAxes.size(); i++)
    {
        delete m_vAxes[i];
        m_vAxes[i] = nullptr;
    }
    m_vAxes.clear();
}


int LLC_SlaveBase::SetDeviceConfig(const ESI_Parse::Device* pDeviceConfig)
{
    m_pDeviceConfig = pDeviceConfig;
    return REC_Successed;
}

// 初始化从站映射PDO信息
int LLC_SlaveBase::InitSlave(int nSalveID, bool bVirtual,double dEtherCatTimeCycle,ST_LLPCSlaveConfigParam stSlaveConfigParam,ec_slavet* pSlaves, bool configureAssignment)
{
    if (!m_pDeviceConfig || (!bVirtual && !pSlaves)) return REC_ParametersError;
    m_nSlaveID = nSalveID;
    m_bVirtual = bVirtual;
    m_pSlaves = pSlaves;
    m_fAxisEtherCatCycleTime = dEtherCatTimeCycle * 1000;
    m_stSlaveConfigParam = stSlaveConfigParam;
    m_vTxPDOs.clear();
    m_vRxPDOs.clear();
    // 获取从站PDO信息
    for (size_t i = 0; i < stSlaveConfigParam.nTxProcessData.size(); i++)
    {
        for (size_t j = 0; j < m_pDeviceConfig->txPdos.size(); j++)
        {
            if(stSlaveConfigParam.nTxProcessData[i] == m_pDeviceConfig->txPdos[j].index)
            {
                m_vTxPDOs.push_back(m_pDeviceConfig->txPdos[j]);
                break;
            }
        }
    }
    for (size_t i = 0; i < stSlaveConfigParam.nRxProcessData.size(); i++)
    {
        for (size_t j = 0; j < m_pDeviceConfig->rxPdos.size(); j++)
        {
            if(stSlaveConfigParam.nRxProcessData[i] == m_pDeviceConfig->rxPdos[j].index)
            {
                m_vRxPDOs.push_back(m_pDeviceConfig->rxPdos[j]);
                break;
            }
        }
    }
    //虚拟模式不做操作
    if(m_bVirtual)
    {
        return REC_Successed;
    }
    if(configureAssignment) {
        auto assign=[this](std::uint16_t index,const std::vector<ESI_Parse::Pdo>& pdos){
            if(!m_stEtherCATInterface.pSDOWrite||pdos.size()>255)return false;
            std::uint8_t count=0;
            if(m_stEtherCATInterface.pSDOWrite(m_nSlaveID,index,0,FALSE,sizeof(count),&count,LLC_TIMEOUTRXM)<=0)return false;
            for(std::size_t i=0;i<pdos.size();++i){
                const auto pdo=static_cast<std::uint16_t>(pdos[i].index);
                if(m_stEtherCATInterface.pSDOWrite(m_nSlaveID,index,static_cast<std::uint8_t>(i+1),FALSE,sizeof(pdo),&pdo,LLC_TIMEOUTRXM)<=0)return false;
            }
            count=static_cast<std::uint8_t>(pdos.size());
            return m_stEtherCATInterface.pSDOWrite(m_nSlaveID,index,0,FALSE,sizeof(count),&count,LLC_TIMEOUTRXM)>0;
        };
        if(!assign(0x1c12,m_vRxPDOs)||!assign(0x1c13,m_vTxPDOs))return REC_ControllerNotInit;
    }
    return InitPDOOffset();
}


int LLC_SlaveBase::InitPDOOffset()
{
    //虚拟模式不做操作
    if(m_bVirtual)
    {
        return REC_Successed;
    }
    auto bitCount=[](const std::vector<ESI_Parse::Pdo>& pdos){
        std::size_t count=0;
        for(const auto& pdo:pdos)for(const auto& entry:pdo.entries){
            if(entry.bitLen<0)return std::numeric_limits<std::size_t>::max();
            count+=static_cast<std::size_t>(entry.bitLen);
        }
        return count;
    };
    if(!m_pSlaves || bitCount(m_vRxPDOs)>m_pSlaves->Obits || bitCount(m_vTxPDOs)>m_pSlaves->Ibits ||
       (!m_vRxPDOs.empty()&&!m_pSlaves->outputs)||(!m_vTxPDOs.empty()&&!m_pSlaves->inputs))return REC_ParametersError;
    // 初始化RxPDO地址
    // RxPDO是主站发送给从站的数据，对应SOEM中的outputs
    uint8_t* pOutputBase =static_cast<uint8_t*>(m_pSlaves->outputs);
    // 加上该从站在首字节中的起始位偏移
    uint32_t nRxBitOffset =static_cast<uint32_t>(m_pSlaves->Ostartbit);
    for (size_t i = 0; i < m_vRxPDOs.size(); i++)
    {
        for (size_t j = 0;j < m_vRxPDOs[i].entries.size();j++)
        {
            auto& stEntry = m_vRxPDOs[i].entries[j];
            if (stEntry.index == 0)
            {
                stEntry.pDataPtr = nullptr;
            }
            else
            {
                stEntry.pDataPtr =pOutputBase + nRxBitOffset / 8;
            }
            // Entry在当前字节内的起始位
            stEntry.bitOffsetInByte =static_cast<int>(nRxBitOffset % 8);
            // 按Entry的实际位长度移动
            nRxBitOffset +=static_cast<uint32_t>(stEntry.bitLen);
        }
    }
    // 初始化TxPDO地址
    // TxPDO是从站发送给主站的数据，对应SOEM中的inputs
    uint8_t* pInputBase =static_cast<uint8_t*>(m_pSlaves->inputs);
    // 加上该从站在首字节中的起始位偏移
    uint32_t nTxBitOffset =static_cast<uint32_t>(m_pSlaves->Istartbit);
    for (size_t i = 0; i < m_vTxPDOs.size(); i++)
    {
        for (size_t j = 0;j < m_vTxPDOs[i].entries.size();j++)
        {
            auto& stEntry = m_vTxPDOs[i].entries[j];
            // Index为0表示填充数据
            if (stEntry.index == 0)
            {
                stEntry.pDataPtr = nullptr;
            }
            else
            {
                stEntry.pDataPtr =pInputBase + nTxBitOffset / 8;
            }
            // Entry在当前字节内的起始位
            stEntry.bitOffsetInByte = static_cast<int>(nTxBitOffset % 8);
            // 按Entry的实际位长度移动
            nTxBitOffset += static_cast<uint32_t>(stEntry.bitLen);
        }
    }
    std::printf("[PDO] slave %d offsets initialized: Rx %u bits/start %u, Tx %u bits/start %u\n",
                m_nSlaveID,static_cast<unsigned>(m_pSlaves->Obits),static_cast<unsigned>(m_pSlaves->Ostartbit),
                static_cast<unsigned>(m_pSlaves->Ibits),static_cast<unsigned>(m_pSlaves->Istartbit));
    return REC_Successed;
}

std::vector<ESI_Parse::Pdo>* LLC_SlaveBase::GetTXPDOList()
{
    return &m_vTxPDOs;
}
std::vector<ESI_Parse::Pdo>* LLC_SlaveBase::GetRXPDOList()
{
    return &m_vRxPDOs;
}


std::vector<CIA402Axis*> LLC_SlaveBase::GetCIA402Axis()
{
    // m_vAxes中的轴由LLC_SlaveBase负责管理
    for (CIA402Axis* pAxis : m_vAxes)
    {
        delete pAxis;
    }

    m_vAxes.clear();

    // 根据Index、SubIndex和BitLen查找PDO地址
    auto FindPDOAddress = [](const ESI_Parse::Pdo& vPDOs, int nIndex, int nSubIndex, int nBitLen) -> uint8_t*
    {
        for (const auto& stEntry : vPDOs.entries)
        {
            if (stEntry.index != nIndex)
            {
                continue;
            }

            if (stEntry.subIndex != nSubIndex)
            {
                continue;
            }

            if (stEntry.bitLen != nBitLen)
            {
                continue;
            }

            if (stEntry.pDataPtr == nullptr)
            {
                continue;
            }

            // 当前映射使用类型指针直接访问，因此必须按字节对齐
            if (stEntry.bitOffsetInByte != 0)
            {
                continue;
            }

            return reinterpret_cast<uint8_t*>(stEntry.pDataPtr);
        }
        return nullptr;
    };

    // m_nAxisSlot已经由外部配置完成，本函数不能重新初始化或修改
    for (size_t nSlot = 0; nSlot < m_vAxisPDOs.size(); nSlot++)
    {
        // m_nAxisSlot中保存的值就是轴ID
        const int nAxisID = m_vAxisPDOs[nSlot].nAxisID;
        if (nAxisID < 0)
        {
            continue;
        }
        const int nObjectOffset = nSlot * AXIS_OBJECT_INCREMENT;
        int nFindPDO[2]{0,0};
        ESI_Parse::Pdo stFindPDO[2];
        for (size_t i = 0; i < m_vTxPDOs.size(); i++)
        {
            if (m_vTxPDOs[i].index == m_vAxisPDOs[nSlot].nTxPDO)
            {
                nFindPDO[0] = 1;
                stFindPDO[0] = m_vTxPDOs[i];
            }
        }
        for (size_t i = 0; i < m_vRxPDOs.size(); i++)
        {
            if (m_vRxPDOs[i].index == m_vAxisPDOs[nSlot].nRxPDO)
            {
                nFindPDO[1] = 1;
                stFindPDO[1] = m_vRxPDOs[i];
            }
        }
        if((nFindPDO[0] + nFindPDO[1]) != 2)
        {
            printf("GETAxis--PDO NOT FIND");
            return m_vAxes;
        }
        ST_SMCInitMap stMap{};
        // RxPDO：主站发送给从站
        stMap.pControlword = reinterpret_cast<uint16_t*>(FindPDOAddress(stFindPDO[1], CONTROLWORD_INDEX + nObjectOffset, 0, 16));
        stMap.pTargetPosition = reinterpret_cast<int32_t*>(FindPDOAddress(stFindPDO[1], TARGET_POSITION_INDEX + nObjectOffset, 0, 32));
        stMap.pTargetVelocity = reinterpret_cast<int32_t*>(FindPDOAddress(stFindPDO[1], TARGET_VELOCITY_INDEX + nObjectOffset, 0, 32));
        stMap.pTargetTorque = reinterpret_cast<int16_t*>(FindPDOAddress(stFindPDO[1], TARGET_TORQUE_INDEX + nObjectOffset, 0, 16));
        stMap.pTargetModesOfOperation = reinterpret_cast<uint8_t*>(FindPDOAddress(stFindPDO[1], TARGET_OPERATION_MODE_INDEX + nObjectOffset, 0, 8));
        stMap.pDigitalOutputs = reinterpret_cast<uint32_t*>(FindPDOAddress(stFindPDO[1], DIGITAL_OUTPUTS_INDEX + nObjectOffset, DIGITAL_OUTPUTS_SUBINDEX, 32));
        stMap.pAddVelocityValue = reinterpret_cast<int32_t*>(FindPDOAddress(stFindPDO[1], ADD_VELOCITY_INDEX + nObjectOffset, 0, 32));
        stMap.pAddTorqueValue = reinterpret_cast<int16_t*>(FindPDOAddress(stFindPDO[1], ADD_TORQUE_INDEX + nObjectOffset, 0, 16));
        stMap.pTouchProbeFunction = reinterpret_cast<uint16_t*>(FindPDOAddress(stFindPDO[1], TOUCH_PROBE_FUNCTION_INDEX + nObjectOffset, 0, 16));

        // TxPDO：从站发送给主站
        stMap.pStatusWord = reinterpret_cast<uint16_t*>(FindPDOAddress(stFindPDO[0], STATUSWORD_INDEX + nObjectOffset, 0, 16));
        stMap.pErrorCode = reinterpret_cast<uint16_t*>(FindPDOAddress(stFindPDO[0], ERROR_CODE_INDEX + nObjectOffset, 0, 16));
        stMap.pActualPosition = reinterpret_cast<int32_t*>(FindPDOAddress(stFindPDO[0], ACTUAL_POSITION_INDEX + nObjectOffset, 0, 32));
        stMap.pActualVelocity = reinterpret_cast<int32_t*>(FindPDOAddress(stFindPDO[0], ACTUAL_VELOCITY_INDEX + nObjectOffset, 0, 32));
        stMap.pActualTorque = reinterpret_cast<int16_t*>(FindPDOAddress(stFindPDO[0], ACTUAL_TORQUE_INDEX + nObjectOffset, 0, 16));
        stMap.pActualModesOfOperation = reinterpret_cast<uint8_t*>(FindPDOAddress(stFindPDO[0], ACTUAL_OPERATION_MODE_INDEX + nObjectOffset, 0, 8));
        stMap.pDigitalInputs = reinterpret_cast<uint32_t*>(FindPDOAddress(stFindPDO[0], DIGITAL_INPUTS_INDEX + nObjectOffset, 0, 32));
        stMap.pTouchProbeStatus = reinterpret_cast<uint16_t*>(FindPDOAddress(stFindPDO[0], TOUCH_PROBE_STATUS_INDEX + nObjectOffset, 0, 16));

        // 非标准CiA 402对象，暂时不自动映射
        stMap.pJointActPosition = nullptr;

        if(nSlot<axisOverrides.size()){
            const auto& mapping=axisOverrides[nSlot].at("mapping");
            auto resolve=[&](const char* key,bool rx,int bits)->uint8_t*{
                if(!mapping.contains(key)||mapping[key].is_null())return nullptr;
                const auto& m=mapping[key];for(const auto& p:rx?m_vRxPDOs:m_vTxPDOs){
                    if(p.index!=m.at("pdo").get<int>())continue;
                    return FindPDOAddress(p,m.at("index").get<int>(),m.at("subIndex").get<int>(),bits);
                }return nullptr;
            };
            stMap.pControlword=reinterpret_cast<uint16_t*>(resolve("Controlword",true,16));
            stMap.pStatusWord=reinterpret_cast<uint16_t*>(resolve("StatusWord",false,16));
            stMap.pErrorCode=reinterpret_cast<uint16_t*>(resolve("ErrorCode",false,16));
            stMap.pTargetPosition=reinterpret_cast<int32_t*>(resolve("TargetPosition",true,32));
            stMap.pActualPosition=reinterpret_cast<int32_t*>(resolve("ActualPosition",false,32));
            stMap.pTargetVelocity=reinterpret_cast<int32_t*>(resolve("TargetVelocity",true,32));
            stMap.pActualVelocity=reinterpret_cast<int32_t*>(resolve("ActualVelocity",false,32));
            stMap.pTargetTorque=reinterpret_cast<int16_t*>(resolve("TargetTorque",true,16));
            stMap.pActualTorque=reinterpret_cast<int16_t*>(resolve("ActualTorque",false,16));
            stMap.pTargetModesOfOperation=reinterpret_cast<uint8_t*>(resolve("TargetModesOfOperation",true,8));
            stMap.pActualModesOfOperation=reinterpret_cast<uint8_t*>(resolve("ActualModesOfOperation",false,8));
            stMap.pDigitalInputs=reinterpret_cast<uint32_t*>(resolve("DigitalInputs",false,32));
            stMap.pDigitalOutputs=reinterpret_cast<uint32_t*>(resolve("DigitalOutputs",true,32));
        }
        // 控制字和状态字必须同时存在
        if (!m_bVirtual && (stMap.pControlword == nullptr || stMap.pStatusWord == nullptr))
        {
            // 配置要求生成轴，但PDO中缺少必要对象
            // 不修改预先配置好的m_nAxisSlot
            continue;
        }

        CIA402Axis* pAxis = new (std::nothrow) CIA402Axis(m_bVirtual);

        if (pAxis == nullptr)
        {
            continue;
        }

        // 初始化PDO映射
        if (pAxis->Axis_InitMap(stMap) != REC_Successed)
        {
            delete pAxis;
            continue;
        }

        // 使用m_nAxisSlot中预先配置的轴ID
        if (pAxis->Axis_SetAxisID(static_cast<uint16_t>(nAxisID)) != REC_Successed)
        {
            delete pAxis;
            continue;
        }

        // 设置轴控制周期，单位为ms
        if (pAxis->Axis_SetControlCycle(m_fAxisEtherCatCycleTime) != REC_Successed)
        {
            delete pAxis;
            continue;
        }

        m_vAxes.push_back(pAxis);
    }

    return m_vAxes;
}


int LLC_SlaveBase::SetEtherCatCycle(float fEtherCatCycle)
{
    m_fAxisEtherCatCycleTime = fEtherCatCycle;
   return REC_Successed;
    
}

int LLC_SlaveBase::InsertAxis(vector<ST_LLPC_AxisPDO> v_AxisPDOs)
{
    for (size_t i = 0; i < AXISSLOTSIZE; i++)
    {
        m_vAxisPDOs = v_AxisPDOs;
    }
    return REC_Successed;
}



int LLC_SlaveBase::ReadSDO(int nSlave, uint16_t index, uint8_t subidx, int nLen, int &Value)
{
    //虚拟模式不做操作
    if(m_bVirtual)
    {
        return REC_Successed;
    }
    int readData=0;
    // int nRet = readSDO_INT_Async(nSlave, index, subidx, &readData, &readState, &userData);
    int nRet = readSDO_INT(nSlave, index, subidx, &readData);
    Value = readData;
    // Value = nRet;
    return nRet;
}

void LLC_SlaveBase::MC_SlaveAxisRT()
{
    return;
}

int LLC_SlaveBase::WriteSDO(int nSlave, uint16_t index, uint8_t subidx, int Value, int nLen)
{
    //虚拟模式不做操作
    if(m_bVirtual)
    {
        return REC_Successed;
    }
    int nRet = writeSDO_INT(nSlave, index, subidx, Value);
    return nRet;
}

bool LLC_SlaveBase::IsSlaveOP()
{
    if (m_pSlaves == nullptr)
    {
        return false;
    }
    return (m_pSlaves->state == EC_STATE_OPERATIONAL);
}

int LLC_SlaveBase::FoEProgressHook(uint16 slave, int packetnumber, int datasize)
{
    LLC_SlaveBase* pSlave = s_pFoEUpdatingSlave;
    if (pSlave == nullptr)
    {
        return 0;
    }
    if (slave != pSlave->m_nSlaveID)
    {
        return 0;
    }
    if (pSlave->m_nFoETotalSize <= 0)
    {
        return 0;
    }
    int nTransferredSize = pSlave->m_nFoETotalSize - datasize;
    if (nTransferredSize < 0)
    {
        nTransferredSize = 0;
    }
    if (nTransferredSize > pSlave->m_nFoETotalSize)
    {
        nTransferredSize = pSlave->m_nFoETotalSize;
    }
    // 计算0~100的传输百分比
    double dProgress = static_cast<double>(nTransferredSize) * 100.0 / static_cast<double>(pSlave->m_nFoETotalSize);
    // 调用上层注册的进度回调
    if (pSlave->m_fnFoEProgressCallback)
    {
        pSlave->m_fnFoEProgressCallback(dProgress);
    }

    return 0;
}

int LLC_SlaveBase::WritePDO(uint16_t index, uint8_t subidx, int Value)
{
    //虚拟模式不做操作
    if(m_bVirtual)
    {
        return REC_Successed;
    }
    // RxPDO过程数据首地址
    unsigned char* pRxPDOBase = nullptr;
    size_t nRxBitOffset = 0;
    // 遍历全部RxPDO
    for (size_t i = 0; i < m_vRxPDOs.size(); ++i)
    {
        auto& entries = m_vRxPDOs[i].entries;
        for (size_t j = 0; j < entries.size(); ++j)
        {
            auto& entry = entries[j];
            // 保存当前Entry在整个Rx过程映像中的起始位
            size_t nCurrentEntryBitOffset = nRxBitOffset;
            // 累加位长度，不能在每个RxPDO开始时清零
            nRxBitOffset += static_cast<size_t>(entry.bitLen);
            // 保存RxPDO过程数据首地址
            if ((pRxPDOBase == nullptr) && (entry.pDataPtr != nullptr))
            {
                pRxPDOBase = reinterpret_cast<unsigned char*>(entry.pDataPtr);
            }
            if ((index != entry.index) || (subidx != entry.subIndex))
            {
                continue;
            }
            if ((pRxPDOBase == nullptr) || (entry.pDataPtr == nullptr) || (entry.bitLen == 0))
            {
                return REC_CmdFormatError;
            }
            if (entry.bitLen > sizeof(Value) * 8)
            {
                return REC_CmdFormatError;
            }
            uint32_t nRawValue = static_cast<uint32_t>(Value);
            // 1位PDO按照布尔值处理
            if (entry.bitLen == 1)
            {
                nRawValue = Value != 0 ? 1U : 0U;
            }
            // 按位写入目标Entry
            for (size_t nBit = 0; nBit < static_cast<size_t>(entry.bitLen); ++nBit)
            {
                // 计算当前位在整个Rx过程映像中的位置
                size_t nDestinationBit = nCurrentEntryBitOffset + nBit;
                // 计算目标字节位置
                size_t nDestinationByte = nDestinationBit / 8;
                // 计算目标位在字节中的位置
                size_t nBitInByte = nDestinationBit % 8;
                // 生成目标位掩码
                unsigned char nBitMask = static_cast<unsigned char>(1U << nBitInByte);
                // 写入目标位，同时保留同一字节中的其他输出
                if (((nRawValue >> nBit) & 0x01U) != 0)
                {
                    pRxPDOBase[nDestinationByte] |= nBitMask;
                }
                else
                {
                    pRxPDOBase[nDestinationByte] &= static_cast<unsigned char>(~nBitMask);
                }
            }
            return REC_Successed;
        }
    }

    // 没有找到对应Entry
    return REC_CmdFormatError;
}


void LLC_SlaveBase::SetEtherCATFuncCB(ST_EtherCATInterface stEtherCATInterface)
{
  m_stEtherCATInterface = stEtherCATInterface;
}


uint8_t LLC_SlaveBase::writeSDO_SHORT(int slave_no, uint16_t index, uint8_t subidx, short value)
{
  int ret;
  ret = m_stEtherCATInterface.pSDOWrite(slave_no, index, subidx, FALSE, sizeof(value), &value, LLC_TIMEOUTSAFE);
  return ret;
}
uint8_t LLC_SlaveBase::writeSDO_INT(int slave_no, uint16_t index, uint8_t subidx, int value)
{
  int ret;
  ret = m_stEtherCATInterface.pSDOWrite(slave_no, index, subidx, FALSE, sizeof(value), &value, LLC_TIMEOUTSAFE);
  return ret;
}
uint8_t LLC_SlaveBase::writeSDO_LONG(int slave_no, uint16_t index, uint8_t subidx, long value)
{
  int ret;
  ret = m_stEtherCATInterface.pSDOWrite(slave_no, index, subidx, FALSE, sizeof(value), &value, LLC_TIMEOUTSAFE);
  return ret;
}
uint8_t LLC_SlaveBase::writeSDO_UCHAR(int slave_no, uint16_t index, uint8_t subidx,unsigned char value)
{
  int ret;
  ret = m_stEtherCATInterface.pSDOWrite(slave_no, index, subidx, FALSE, sizeof(value), &value, LLC_TIMEOUTSAFE);
  return ret;
}
uint8_t LLC_SlaveBase::writeSDO_USHORT(int slave_no, uint16_t index, uint8_t subidx,unsigned short value)
{
  int ret;
  ret = m_stEtherCATInterface.pSDOWrite(slave_no, index, subidx, FALSE, sizeof(value), &value, LLC_TIMEOUTSAFE);
  return ret;
}
uint8_t LLC_SlaveBase::writeSDO_UINT(int slave_no, uint16_t index, uint8_t subidx,unsigned int value)
{
  int ret;
  ret = m_stEtherCATInterface.pSDOWrite(slave_no, index, subidx, FALSE, sizeof(value), &value, LLC_TIMEOUTSAFE);
  return ret;
}
uint8_t LLC_SlaveBase::writeSDO_ULONG(int slave_no, uint16_t index, uint8_t subidx,unsigned long value)
{
  int ret;
  ret = m_stEtherCATInterface.pSDOWrite(slave_no, index, subidx, FALSE, sizeof(value), &value, LLC_TIMEOUTSAFE);
  return ret;
}

// char LLC_SlaveBase::readSDO_CHAR(int slave_no, uint16_t index, uint8_t subidx)
// {
//     int ret, l;
//     char val;
//     l = sizeof(val);
//     ret = m_stEtherCATInterface.pSDORead(slave_no, index, subidx, FALSE, &l, &val, LLC_TIMEOUTRXM);
//     if ( ret <= 0 ) { // ret = Workcounter from last slave response
//         fprintf(stderr, "Failed to read from ret:%d, slave_no:%d, index:0x%04x, subidx:0x%02x\n", ret, slave_no, index, subidx);
//     }
//     return val;
// }
short LLC_SlaveBase::readSDO_SHORT(int slave_no, uint16_t index, uint8_t subidx)
{
    int ret, l;
    short val;
    l = sizeof(val);
    ret = m_stEtherCATInterface.pSDORead(slave_no, index, subidx, FALSE, &l, &val, LLC_TIMEOUTRXM);
    if ( ret <= 0 ) { // ret = Workcounter from last slave response
        fprintf(stderr, "Failed to read from ret:%d, slave_no:%d, index:0x%04x, subidx:0x%02x\n", ret, slave_no, index, subidx);
    }
    return val;
}
int LLC_SlaveBase::readSDO_INT(int slave_no, uint16_t index, uint8_t subidx, int *Value)
{
  int ret, l;
  int val;
  l = sizeof(val);
  ret = m_stEtherCATInterface.pSDORead(slave_no, index, subidx, FALSE, &l, Value, LLC_TIMEOUTRXM);
  if ( ret <= 0 ) { // ret = Workcounter from last slave response
    fprintf(stderr, "Failed to read from ret:%d, slave_no:%d, index:0x%04x, subidx:0x%02x\n", ret, slave_no, index, subidx);
  }
  return ret;
}
long LLC_SlaveBase::readSDO_LONG(int slave_no, uint16_t index, uint8_t subidx)
{
    int ret, l;
    long val;
    l = sizeof(val);
    ret = m_stEtherCATInterface.pSDORead(slave_no, index, subidx, FALSE, &l, &val, LLC_TIMEOUTRXM);
    if ( ret <= 0 ) { // ret = Workcounter from last slave response
        fprintf(stderr, "Failed to read from ret:%d, slave_no:%d, index:0x%04x, subidx:0x%02x\n", ret, slave_no, index, subidx);
    }
    return val;
}
unsigned char LLC_SlaveBase::readSDO_UCHAR(int slave_no, uint16_t index, uint8_t subidx)
{
    int ret, l;
    unsigned char val;
    l = sizeof(val);
    ret = m_stEtherCATInterface.pSDORead(slave_no, index, subidx, FALSE, &l, &val, LLC_TIMEOUTRXM);
    if ( ret <= 0 ) { // ret = Workcounter from last slave response
        fprintf(stderr, "Failed to read from ret:%d, slave_no:%d, index:0x%04x, subidx:0x%02x\n", ret, slave_no, index, subidx);
    }
    return val;
}
unsigned short LLC_SlaveBase::readSDO_USHORT(int slave_no, uint16_t index, uint8_t subidx)
{
    int ret, l;
    unsigned short val;
    l = sizeof(val);
    ret = m_stEtherCATInterface.pSDORead(slave_no, index, subidx, FALSE, &l, &val, LLC_TIMEOUTRXM);
    if ( ret <= 0 ) { // ret = Workcounter from last slave response
        fprintf(stderr, "Failed to read from ret:%d, slave_no:%d, index:0x%04x, subidx:0x%02x\n", ret, slave_no, index, subidx);
    }
    return val;
}
unsigned int LLC_SlaveBase::readSDO_UINT(int slave_no, uint16_t index, uint8_t subidx)
{
    int ret, l;
    unsigned int val;
    l = sizeof(val);
    ret = m_stEtherCATInterface.pSDORead(slave_no, index, subidx, FALSE, &l, &val, LLC_TIMEOUTRXM);
    if ( ret <= 0 ) { // ret = Workcounter from last slave response
        fprintf(stderr, "Failed to read from ret:%d, slave_no:%d, index:0x%04x, subidx:0x%02x\n", ret, slave_no, index, subidx);
    }
    return val;
}
unsigned long LLC_SlaveBase::readSDO_ULONG(int slave_no, uint16_t index, uint8_t subidx)
{
    int ret, l;
    unsigned long val;
    l = sizeof(val);
    ret = m_stEtherCATInterface.pSDORead(slave_no, index, subidx, FALSE, &l, &val, LLC_TIMEOUTRXM);
    if ( ret <= 0 ) { // ret = Workcounter from last slave response
        fprintf(stderr, "Failed to read from ret:%d, slave_no:%d, index:0x%04x, subidx:0x%02x\n", ret, slave_no, index, subidx);
    }
    return val;
}
