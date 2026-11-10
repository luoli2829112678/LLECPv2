#include "LLC_Rbt.h"

int LLC_Rbt::ResetControllerConfig()
{
    m_nSetConfiguredAxisNum = 0;
    m_nSetConfiguredSlaveNum = 0;
    v_AxisHardParam.clear();
    m_vSlaveConfigParam.clear();
    m_vIOConfig.clear();
    m_vAxisMtionParam.clear();
    m_vAxisMtionParam_Test[0].clear();
    m_vAxisMtionParam_Test[1].clear();
    m_vAxisMtionWaitTime_Test[0].clear();
    m_vAxisMtionWaitTime_Test[1].clear();
     
    return REC_Successed;
}

// 设置模拟
int LLC_Rbt::SetSimulationType(const int nSimulationType)
{
    //HSTR测试
    //HSTR_Init();
    //HSTR_CompileApplication();
    //HSTR_RunApplication(false);
    ResetControllerConfig();
    if ((nSimulationType != en_RealRobot) && (nSimulationType != en_SimRobot))
    {
        return REC_ParametersError;
    }
    m_stRbtEtherCATParams.bSimulationType = static_cast<bool>(nSimulationType);
    printf("[LLC.SetSimulationType]bVirtual=%d\n",static_cast<int>(m_stRbtEtherCATParams.bSimulationType));
    return REC_Successed;
}


//运动参数初始化
int LLC_Rbt::InitializationMotionParameters()
{
    m_vAxisMtionParam.push_back(AxisMtionParam());
    m_vAxisMtionParam_Test[0].push_back(AxisMtionParam());
    m_vAxisMtionParam_Test[1].push_back(AxisMtionParam());
    m_vAxisMtionWaitTime_Test[0].push_back(double());
    m_vAxisMtionWaitTime_Test[1].push_back(double());
    return REC_Successed;
}


// 轴参数注册
int LLC_Rbt::RegistrationAxis(int nAxisID,int nSlaveID,int TXpdo,int RXpdo,int nAxisType,int nHomePos,int nEncodeRatio,int nEncodeDirection,
                        double  dGearRatio,double  dVelocityScale,double  dTorqueScale,double  dSafeSpaceLimitUpper,double  dSafeSpaceLimitLower,
                        double  dMaxMotionVel,double  dMaxMotionAcc,double  dMaxMotionJerk,double  dTFFScale,double  dVFFScale,double  dJFFScale,
                        double  dStopAcc,double  dStopJerk)
{
    if(nAxisID < 0 || static_cast<std::size_t>(nAxisID) != v_AxisHardParam.size() || nSlaveID < 1 || static_cast<std::size_t>(nSlaveID) > m_vSlaveConfigParam.size() || v_AxisHardParam.size() >= MaxAxisNum)
        return REC_CmdFormatError;
    InitializationMotionParameters();
    //每个轴仅能初始化一次
    for (size_t i = 0; i < v_AxisHardParam.size(); i++)
    {
        if(v_AxisHardParam[i].nAxisID == nAxisID)
        {
            return REC_CmdFormatError;
        }
    }
    
    ST_LLPCAxisHardParam stAxisHardParam;
    stAxisHardParam.nAxisID                 = static_cast<int32_t>(nAxisID);
    stAxisHardParam.nAxisType                 = static_cast<int32_t>(nAxisType);
    stAxisHardParam.nHomePos                  = static_cast<int32_t>(nHomePos);
    stAxisHardParam.nEncodeRatio              = static_cast<int32_t>(nEncodeRatio);
    stAxisHardParam.nEncodeDirection          = static_cast<int32_t>(nEncodeDirection);
    stAxisHardParam.dGearRatio                = dGearRatio;
    stAxisHardParam.dVelocityScale            = dVelocityScale;
    stAxisHardParam.dTorqueScale              = dTorqueScale;
    stAxisHardParam.dSafeSpaceLimitUpper      = dSafeSpaceLimitUpper;
    stAxisHardParam.dSafeSpaceLimitLower      = dSafeSpaceLimitLower;
    stAxisHardParam.dMaxMotionVel             = dMaxMotionVel;
    stAxisHardParam.dMaxMotionAcc             = dMaxMotionAcc;
    stAxisHardParam.dMaxMotionJerk            = dMaxMotionJerk;
    stAxisHardParam.dTFFScale                 = dTFFScale;
    stAxisHardParam.dVFFScale                 = dVFFScale;
    stAxisHardParam.dJFFScale                 = dJFFScale;
    stAxisHardParam.dStopAcc                  = dStopAcc;
    stAxisHardParam.dStopJerk                 = dStopJerk;
    stAxisHardParam.nTxPDO                    = TXpdo;
    stAxisHardParam.nRxPDO                    = RXpdo;
    v_AxisHardParam.push_back(stAxisHardParam);
    m_nSetConfiguredAxisNum ++;
    //每个轴仅能初始化一次
    for (size_t i = 0; i < m_vSlaveConfigParam[nSlaveID -1].vAxisPDOs.size(); i++)
    {
        if(m_vSlaveConfigParam[nSlaveID -1].vAxisPDOs[i].nAxisID == nAxisID)
        {
            return REC_CmdFormatError;
        }
    }
    
    ST_LLPC_AxisPDO stAxisPDO;
    stAxisPDO.nAxisID = nAxisID;
    stAxisPDO.nRxPDO = RXpdo;
    stAxisPDO.nTxPDO = TXpdo;
    m_vSlaveConfigParam[nSlaveID -1].vAxisPDOs.push_back(stAxisPDO);
    return REC_Successed;
}

// 从站注册
int LLC_Rbt::RegistrationSlave(uint8_t nSlaveID,uint32_t nProductCode,uint32_t nRevisionNo,bool bEnableDC,uint32_t nShiftTime)
{
    if(nSlaveID < 1 || static_cast<std::size_t>(nSlaveID) != m_vSlaveConfigParam.size()+1 || nSlaveID > MAX_SLAVE_NUM) return REC_ParametersError;
    ST_LLPCSlaveConfigParam  SlaveConfigParam;
    SlaveConfigParam.nProductCode = nProductCode;
    SlaveConfigParam.nRevisionNo = nRevisionNo;
    SlaveConfigParam.bEnableDC = bEnableDC;
    SlaveConfigParam.nShiftTime = nShiftTime;
    SlaveConfigParam.nTxProcessData.clear();
    SlaveConfigParam.nRxProcessData.clear();
    m_vSlaveConfigParam.push_back(SlaveConfigParam);
    m_nSetConfiguredSlaveNum ++;
    return REC_Successed;
}

int LLC_Rbt::RegistrationIO(uint8_t nIOid,uint8_t nSlaveID,uint16_t nIndex,uint16_t nSubIndex,uint16_t nBit,bool bInverted)
{
    for(const auto& existing:m_vIOConfig) if(existing.m_IO_id==nIOid) return REC_ParametersError;
    ST_LLPC_IO_Config IO_Config;
    IO_Config.m_IO_id = nIOid;
    if (nSlaveID < 1 || nSlaveID > m_vSlaveConfigParam.size() || nBit > 7 || nIndex == 0) return REC_ParametersError;
    IO_Config.m_nSlaveID = nSlaveID - 1;
    IO_Config.m_nPDOIndex = nIndex;
    IO_Config.m_nPDOSubIndex = nSubIndex;
    IO_Config.m_bInverted = bInverted;
    IO_Config.m_nBit = nBit;

    m_vIOConfig.push_back(IO_Config);
    return REC_Successed;
}

//从站过程数据修改配置
int LLC_Rbt::ConfigureSlavePDO(uint8_t nSlaveID,vector<uint32_t>TxPDO,vector<uint32_t>RxPDO)
{
    if (nSlaveID == 0) 
    {
        return REC_ParametersError;
    }

    const size_t nSlaveIndex = static_cast<size_t>(nSlaveID - 1);  

    if (nSlaveIndex >= m_vSlaveConfigParam.size())  // 防止vector越界
    {
        return REC_ParametersError;
    }

    const bool bTxContainsZero = std::any_of(TxPDO.begin(), TxPDO.end(), [](uint32_t value) { return value == 0; });
    const bool bRxContainsZero = std::any_of(RxPDO.begin(), RxPDO.end(), [](uint32_t value) { return value == 0; });

    if (bTxContainsZero || bRxContainsZero)  // 0只能作为命令参数分隔符，不能作为PDO地址
    {
        return REC_ParametersError;
    }

    auto& stSlaveConfig = m_vSlaveConfigParam[nSlaveIndex]; 
    stSlaveConfig.nTxProcessData = TxPDO;                    
    stSlaveConfig.nRxProcessData = RxPDO;                    

    return REC_Successed;
}



