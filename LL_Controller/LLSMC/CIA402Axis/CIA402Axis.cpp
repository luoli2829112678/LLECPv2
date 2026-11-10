#include"CIA402Axis.h"
CIA402Axis::CIA402Axis(bool bIsVirtual) : m_stPDO_Virtual{}
{ 
   InitAxis();
   m_bVirtual = bIsVirtual;
   Axis_SetAxisState(EN_AxisMotionState::motionState_power_off);
   //map init
    m_st_map.pControlword             = &m_stPDO_Virtual.Controlword;
    m_st_map.pStatusWord              = &m_stPDO_Virtual.StatusWord;
    m_st_map.pErrorCode               = &m_stPDO_Virtual.ErrorCode;
    m_st_map.pTargetPosition          = &m_stPDO_Virtual.TargetPosition;
    m_st_map.pTargetVelocity          = &m_stPDO_Virtual.TargetVelocity;
    m_st_map.pTargetTorque            = &m_stPDO_Virtual.TargetTorque;
    m_st_map.pTargetModesOfOperation  = &m_stPDO_Virtual.TargetModesOfOperation;
    m_st_map.pActualPosition          = &m_stPDO_Virtual.ActualPosition;
    m_st_map.pJointActPosition        = &m_stPDO_Virtual.JointActPosition;
    m_st_map.pActualVelocity          = &m_stPDO_Virtual.ActualVelocity;
    m_st_map.pActualTorque            = &m_stPDO_Virtual.ActualTorque;
    m_st_map.pActualModesOfOperation  = &m_stPDO_Virtual.ActualModesOfOperation;
    m_st_map.pDigitalInputs           = &m_stPDO_Virtual.DigitalInputs;
    m_st_map.pDigitalOutputs          = &m_stPDO_Virtual.DigitalOutputs;
   DataSynchronization();
   m_bInit = true;
}
CIA402Axis::~CIA402Axis(){}

void CIA402Axis::InitAxis()
{
    m_bVirtual = false;
    m_bBusy = false;
    m_bInit = false;
    m_nAxisID = 0;
    m_nErrorCode = 0;
    m_dControlCycle = 0.001;
    m_bCyclicSyncMotion = false;
    m_nCyclicSyncMotionLosCount = 0;
    m_bSoftMotionSignal = false;
    return;
}

int CIA402Axis::Axis_InitMap(ST_SMCInitMap st_map)
{

    if(m_bVirtual)
    {
        return AEC_SUCCESSED;
    }
    m_st_map = st_map;
    //pdo镜像初始化防止飞车
    m_stMirrorPDO.Controlword = PDO_CONTROLWORD;
    m_stMirrorPDO.StatusWord = PDO_STATUSWORD;
    if(nullptr!=&PDO_ACTUALPOSITION)
    {
        m_stMirrorPDO.TargetPosition = PDO_ACTUALPOSITION;
        m_stMirrorPDO.ActualPosition = PDO_ACTUALPOSITION;
        PDO_TARGETPOSITION = PDO_ACTUALPOSITION;
    }
    if(nullptr!=&PDO_ACTUALTORQUE)
    {
        m_stMirrorPDO.TargetTorque = PDO_ACTUALTORQUE;
        m_stMirrorPDO.ActualTorque = PDO_ACTUALTORQUE;
    }
    if(nullptr!=&PDO_ACTVELOCITY)
    {
        m_stMirrorPDO.TargetVelocity = PDO_ACTVELOCITY;
        m_stMirrorPDO.ActualVelocity = PDO_ACTVELOCITY;
    }
    if(nullptr!=&PDO_ACTUALMODESOFOPERATION)
    {
        m_stMirrorPDO.TargetModesOfOperation = PDO_ACTUALMODESOFOPERATION;
        m_stMirrorPDO.ActualModesOfOperation = PDO_ACTUALMODESOFOPERATION;
    }
    if(nullptr!=&PDO_JOINT_ACTUALPOSITION)
    {
        m_stMirrorPDO.JointActPosition = PDO_JOINT_ACTUALPOSITION;
    }
    return AEC_SUCCESSED;
}
int CIA402Axis::Axis_SetAxisID(uint16_t id)
{
    m_nAxisID = id;
    return AEC_SUCCESSED;
}
int CIA402Axis::Axis_SetAxisState(EN_AxisMotionState enAxisState)
{
    //printf("Axisid:%d,Axis_SetAxisState:%d\n",nAxisID,enAxisState);
    m_enAxisMotionState = enAxisState;
    return AEC_SUCCESSED;
}
int CIA402Axis::Axis_SetOverride(ST_Factor stFactor)
{
    m_stSoftMotionEX.m_stFactor = stFactor;
    return AEC_SUCCESSED;
}
EN_AxisMotionState CIA402Axis::Axis_ReadAxisState()
{
    return m_enAxisMotionState;
}

bool CIA402Axis::Axis_CheckError(int& nErrorID)
{
    m_bError = Axis_PDOErrorCheck();
    if(m_bError)
    {
        Axis_SetAxisState(motionState_errorstop);
        m_nErrorCode = m_stMirrorPDO.ErrorCode;
        return true;
    }
    nErrorID = m_nErrorCode;
    return m_bError;
}
bool CIA402Axis::Axis_CheckError()
{
    m_bError = Axis_PDOErrorCheck();
    if(m_TonError.R_TRIG(m_bError) && m_bWriteLog)
    {
        printf("ErrorAxisID:%d,ErrorCode:%d.StatusWord:%d\n",m_nAxisID,m_stMirrorPDO.ErrorCode,m_stMirrorPDO.StatusWord);
        WriteErrorLog();
    }
    if(m_bError)
    {
        Axis_SetAxisState(motionState_errorstop);
        m_nErrorCode = m_stMirrorPDO.ErrorCode;
        return true;
    }
    return m_bError;
}

int CIA402Axis::Axis_ResetError()
{
    return AEC_SUCCESSED;
}

int CIA402Axis::Axis_SoftMotionRunDone()
{
    m_stSoftMotionEX.vMotionUint.begin()->SetFBStauts(false,true,false,false,AEC_SUCCESSED);
    m_stSoftMotionEX.vMotionUint.clear();
    return AEC_SUCCESSED;
}



int CIA402Axis::Axis_PushMotionUint(const ST_MotionUint STMotionUint)
{
    //立即中断当前运动，并开始新运动
    if(EN_BufferMode::enAborting == STMotionUint.BufferMode)
    {
        m_stSoftMotionEX.vMotionUint.clear();
        m_stSoftMotionEX.bReplan = true;
    }
    //首点位下发清理缓存
    if(motionState_standstill == m_enAxisMotionState)
    {
        //m_bMoving = true;
        m_stSoftMotionEX.bReplan = true;
        m_stSoftMotionEX.vMotionUint.clear();
    }
    if(enPositionPlanningMode != STMotionUint.PlanningMotionParam.PlanningMode)
    {
        //占用回调
        for(size_t i = 0;i < m_stSoftMotionEX.vMotionUint.size();i++)
        {
            m_stSoftMotionEX.vMotionUint[i].SetFBStauts(false,false,true,false,AEC_SUCCESSED);
        }
        m_stSoftMotionEX.vMotionUint.clear();
    }
    //缓存点位
    m_stSoftMotionEX.vMotionUint.push_back(STMotionUint);
    return AEC_SUCCESSED;
}

int CIA402Axis::Axis_GetMotionUint(ST_MotionUint& stMotionUint)
{
    int res = m_stSoftMotionEX.vMotionUint.size();
    if(AEC_SUCCESSED == res)
    {
        return AEC_SUCCESSED;
    }
    for(auto it = m_stSoftMotionEX.vMotionUint.begin();it<m_stSoftMotionEX.vMotionUint.end();it++)
    {
        stMotionUint = *it;
        if(it->bMotionDone)
        {
            continue;
        }
    }
    return AEC_SUCCESSED;
}

int CIA402Axis::Axis_GetMotionInitialParameters(ST_InterParams& stInitialParameters)
{
    switch (m_stMirrorPDO.ActualModesOfOperation)
    {
    case EN_ModesOfOperation::enModeCyclicSyncPosition:
        stInitialParameters.P = m_stMotorMotionData.stCommand.dPosition;
        stInitialParameters.V = m_stMotorMotionData.stCommand.dVelocity;
        break;
    case EN_ModesOfOperation::enModeCyclicSyncVelocity:
        stInitialParameters.P = m_stMotorMotionData.stCommand.dVelocity;
        stInitialParameters.V = m_stMotorMotionData.stCommand.dAcceleration;
        break;
    case EN_ModesOfOperation::enModeCyclicSyncTorque:
        stInitialParameters.P = m_stMotorMotionData.stCommand.dTorque;
        stInitialParameters.V =  m_stMotorMotionData.stCommand.dTorqueDot;
        break;
    default:
        //其他默认位置
        stInitialParameters.P = m_stMotorMotionData.stCommand.dPosition;
        stInitialParameters.V = m_stMotorMotionData.stCommand.dVelocity;
        break;
    }
    return AEC_SUCCESSED;
}


bool CIA402Axis::Axis_PDOErrorCheck()
{
    bool statuswordCheck = (m_stMirrorPDO.StatusWord & en_StatusWord_f) != 0;
    bool errorcodeCheck = m_stMirrorPDO.ErrorCode != 0;
    return statuswordCheck || errorcodeCheck;
}

void CIA402Axis::AxisMMOPositionSynchronization()
{
    int32_t tar;
    int16_t  tarT;
    //位置同步
    switch (m_stMirrorPDO.ActualModesOfOperation)
    {
    //其他两个兼着前馈作用，先暂时屏蔽
    case EN_ModesOfOperation::enModeCyclicSyncPosition:
        // Axis_PDO_ReadActualTorque(tarT);
        // Axis_PDO_SetTargetTorque(tarT);
        // Axis_PDO_ReadActualVelocity(tar);
        // Axis_PDO_SetTargetVelocity(tar);
        break;
    case EN_ModesOfOperation::enModeCyclicSyncVelocity:
        Axis_PDO_ReadActualPosition(tar);
        Axis_PDO_SetTargetPosition(tar);
        // Axis_PDO_ReadActualTorque(tarT);
        // Axis_PDO_SetTargetTorque(tarT);
        break;
    case EN_ModesOfOperation::enModeCyclicSyncTorque:
        Axis_PDO_ReadActualPosition(tar);
        Axis_PDO_SetTargetPosition(tar);
        // Axis_PDO_ReadActualVelocity(tar);
        // Axis_PDO_SetTargetVelocity(tar);
        break;
    default:
        //printf("Error! This mode is not supported:%d\n",nActModeOpration);
        return;
        break;
    }
    return;
}

void CIA402Axis::Axis_RT()
{
    //if(m_nAxisID == 1)
    {
        // printf("m_enAxisMotionState: %d     ",m_enAxisMotionState);
        // printf("ControlWord: %d     ", PDO_CONTROLWORD);
        // printf("StatusWord: %d     ", PDO_STATUSWORD);
        // printf("PDO_TARGETMODESOFOPERATION: %d     ", PDO_TARGETMODESOFOPERATION);
        // printf("PDO_ACTUALMODESOFOPERATION: %d     ", PDO_ACTUALMODESOFOPERATION);
        // printf("TargetPos: %f     ", m_stMotorMotionData.stCommand.dPosition);
        // printf("ActualPos: %f     \n", m_stMotorMotionData.stActual.dPosition);
        // printf("TargetPos: %d     ", PDO_TARGETPOSITION);
        // printf("ActualPos: %d     \n", PDO_ACTUALPOSITION);
        // printf("PDO_TARGETTORQUE: %d     ", PDO_TARGETTORQUE);
        // printf("PDO_ACTUALTORQUE: %d     ", PDO_ACTUALTORQUE);
        // printf("ErrorCode: %d\n", PDO_ERRORCODE);
        // printf("PDO_TARGETVELOCITY: %d     ", PDO_TARGETVELOCITY);
        //printf("PDO_ACTVELOCITY: %d     \n", PDO_ACTVELOCITY);
        // printf("PDO_TARGETTORQUE: %d     \n", PDO_TARGETTORQUE);
    //     printf("\n");
     }
    Axis_CheckError();
    AxisStatusCheck();
    AxisMMOPositionSynchronization();
    PDOsynchronization();
    DataSynchronization();
    Axis_SY_Feedforward();
    Axis_Simulation();
    if(m_stDualEncoderConfiguration.bDualEncoder)
    {
        Axis_DualEncoderCalculateAxisPosition();
    }
    VirtualPDOsynchronization();
    m_bCyclicSyncMotion = false;
    return;
}

void CIA402Axis::DataSynchronization()
{
    FrameDataCheck();
    //运动数据
    m_stMotorMotionData.stCommand.dPosition = m_stFrameData_now.dCmdPos;
    m_stMotorMotionData.stActual.dPosition  = m_stFrameData_now.dActPos;
    m_stMotorMotionData.stCommand.dTorque =   m_stFrameData_now.dCmdTorque;
    m_stMotorMotionData.stActual.dTorque =   m_stFrameData_now.dActTorque;
    //printf("AxisID:%d,dSetTorque:%f,dActPosition:%f,CmdTorPDO:%d,ActPDO:%d\n",m_nAxisID,dSetTorque,dActTorque,m_stMirrorPDO.TargetTorque,m_stMirrorPDO.ActualTorque);
    
    m_stMotorMotionData.stCommand.dVelocity = m_stFrameData_now.dCmdVel;
    m_stMotorMotionData.stActual.dVelocity =  m_stFrameData_now.dActVel;
    m_stMotorMotionData.stCommand.dAcceleration = m_stFrameData_now.dCmdAcc;
    m_stMotorMotionData.stActual.dAcceleration = m_stFrameData_now.dActAcc;
    m_stMotorMotionData.stCommand.dJerk = m_stFrameData_now.dCmdJerk;
    m_stMotorMotionData.stActual.dJerk = m_stFrameData_now.dActJerk;
    //状态
    //m_bMoving = m_enAxisMotionState > motionState_standstill;
    //配置
    m_bSoftMotionSignal = false;
    //对外数据同步
    m_stAxisData.stAxisConfig.stAxisConfiguration = m_stAxisConfiguration;
    m_stAxisData.stAxisConfig.stFeedforward = m_stFeedforward;
    m_stAxisData.stAxisConfig.stDualEncoderConfiguration = m_stDualEncoderConfiguration;
    m_stAxisData.stRealTimeData.stAxisData = m_stFrameData_now;
    m_stAxisData.stRealTimeData.stPDOData = m_stMirrorPDO;
    m_stAxisData.stRealTimeData.stAxisDualEncoderData = m_stAxisDualEncoderData;
    m_stAxisData.stRealTimeData.stAxisStauts.enAxisMotionState = m_enAxisMotionState;
    m_stAxisData.stRealTimeData.stAxisStauts.nAxisID = m_nAxisID;
    m_stAxisData.stRealTimeData.stAxisStauts.bError = m_bError;
    m_stAxisData.stRealTimeData.stAxisStauts.bErrorID = m_nErrorCode;
    m_stAxisData.stRealTimeData.stAxisStauts.bVirtual = m_bVirtual;
    m_stAxisData.stRealTimeData.stAxisStauts.bMoving  = m_enAxisMotionState > motionState_standstill;
    return;
}

void CIA402Axis::AxisStatusCheck()
{
    if((EN_AxisMotionState::motionState_synchronized_motion == m_enAxisMotionState)||(EN_AxisMotionState::motionState_stopping == m_enAxisMotionState))
    {
        if(m_bCyclicSyncMotion)
        {
            m_bCyclicSyncMotion = false;
        }
        else
        {
            m_nCyclicSyncMotionLosCount ++;
        }
        if(m_nCyclicSyncMotionLosCount>2)
        {
            Axis_SetAxisState(EN_AxisMotionState::motionState_standstill);
        }
    }
    else
    {
        m_nCyclicSyncMotionLosCount = 0;
    }
    return;
}

void CIA402Axis::Axis_Simulation()
{
    if(!m_bVirtual)return;
    m_stPDO_Virtual.ActualPosition = m_stPDO_Virtual.TargetPosition;
    m_stPDO_Virtual.ActualTorque = m_stPDO_Virtual.ActualTorque;
    m_stPDO_Virtual.ActualModesOfOperation = m_stPDO_Virtual.TargetModesOfOperation;
    m_stPDO_Virtual.ActualVelocity = m_stPDO_Virtual.TargetVelocity;
    return;
}



int CIA402Axis::Axis_InitAlign()
{
    int32_t nActPos;
    Axis_PDO_ReadActualPosition(nActPos);
    Axis_PDO_SetTargetPosition(nActPos);
    Axis_SetTargetVelocity(0);
    Axis_SetTargetTorque(0);
    m_stMotorMotionData.stCommand.dPosition = m_stMotorMotionData.stActual.dPosition;
    m_stMotorMotionData.stCommand.dVelocity = 0;
    m_stMotorMotionData.stCommand.dAcceleration = 0;
    m_stMotorMotionData.stCommand.dTorque = 0;
    m_stMotorMotionData.stCommand.dTorqueDot = 0;
    return 0;
}


int CIA402Axis::Axis_SY_Feedforward()
{
    if(0.001 < abs(m_stFeedforward.dVelocityFeedforward))
        Axis_SY_Feedforward_V();
    if(0.001 < abs(m_stFeedforward.dAccelerationFeedforward))
        Axis_SY_Feedforward_A();
    if(0.001 < abs(m_stFeedforward.dJerkFeedforward)) 
        Axis_SY_Feedforward_J();
    return AEC_SUCCESSED;
}

int CIA402Axis::Axis_SY_Feedforward_V()
{
    if(nullptr == &PDO_TARGET_VFF)
        return AEC_SUCCESSED;
    return Axis_PDO_SetVFF(m_stFrameData_now.dCmdVel * m_stFeedforward.dVelocityFeedforward);
}

int CIA402Axis::Axis_SY_Feedforward_A()
{
    if(nullptr == &PDO_TARGET_TFF)
        return AEC_SUCCESSED;
    return Axis_PDO_SetTFF(m_stFrameData_now.dCmdJerk * m_stFeedforward.dVelocityFeedforward);
}

int CIA402Axis::Axis_SY_Feedforward_J()
{
    return AEC_SUCCESSED;
}

void CIA402Axis::FrameDataCheck()
{
    if (!m_bInit) return;
    m_stFrameData_now.dCmdPos = m_stAxisConfiguration.nEncodeDirection * 
                        double(m_stMirrorPDO.TargetPosition - m_stAxisConfiguration.nEncodeHomePos) / 
                        double(m_stAxisConfiguration.nEncodeRatio * m_stAxisConfiguration.dGearRatio)*m_stAxisConfiguration.enAxisMoveDef;
    m_stFrameData_now.dActPos = m_stAxisConfiguration.nEncodeDirection * 
                                        double(m_stMirrorPDO.ActualPosition - m_stAxisConfiguration.nEncodeHomePos) / 
                                        double(m_stAxisConfiguration.nEncodeRatio * m_stAxisConfiguration.dGearRatio) * m_stAxisConfiguration.enAxisMoveDef;

    m_stFrameData_now.dCmdTorque =  m_stMirrorPDO.TargetTorque / m_stAxisConfiguration.dTorqueScale * m_stAxisConfiguration.nTorqueDirection; 
    m_stFrameData_now.dActTorque =  m_stMirrorPDO.ActualTorque / m_stAxisConfiguration.dTorqueScale * m_stAxisConfiguration.nTorqueDirection; 
    if(m_stMirrorPDO.ActualModesOfOperation == EN_ModesOfOperation::enModeCyclicSyncVelocity)
    {
        m_stFrameData_now.dActVel = m_stMirrorPDO.ActualVelocity / m_stAxisConfiguration.dVelocityScale;   
        m_stFrameData_now.dCmdVel = m_stMirrorPDO.TargetVelocity / m_stAxisConfiguration.dVelocityScale;   
    }
    else
    {
        m_stFrameData_now.dCmdVel=(m_stFrameData[0].dCmdPos-m_stFrameData[1].dCmdPos)/(m_dControlCycle);
        m_stFrameData_now.dActVel=(m_stFrameData[0].dActPos-m_stFrameData[nFilteringAlpha].dActPos)/(nFilteringAlpha*m_dControlCycle);
    }
    m_stFrameData_now.dCmdAcc=(m_stFrameData[0].dCmdVel-m_stFrameData[nFilteringAlpha].dCmdVel)/(nFilteringAlpha*m_dControlCycle);
    m_stFrameData_now.dActAcc=(m_stFrameData[0].dActPos-2*m_stFrameData[50].dActPos+m_stFrameData[100].dActPos)/((50.0*m_dControlCycle)*(50.0*m_dControlCycle));
    m_stFrameData_now.dCmdJerk=(m_stFrameData[0].dCmdAcc-m_stFrameData[1].dCmdAcc)/(m_dControlCycle);
    m_stFrameData_now.dActJerk=(m_stFrameData[0].dActAcc-m_stFrameData[nFilteringAlpha].dActAcc)/(nFilteringAlpha*m_dControlCycle);
    m_stFrameData_now.dCmdTorque_d1=(m_stFrameData[0].dCmdTorque-m_stFrameData[1].dCmdTorque)/(m_dControlCycle);
    m_stFrameData_now.dActTorque_d1=(m_stFrameData[0].dActTorque-m_stFrameData[nFilteringAlpha].dActTorque)/(nFilteringAlpha*m_dControlCycle);
    m_stFrameData_now.dCmdTorque_d2=(m_stFrameData[0].dCmdTorque_d1-m_stFrameData[1].dCmdTorque_d1)/(m_dControlCycle);
    m_stFrameData_now.dActTorque_d2=(m_stFrameData[0].dActTorque_d1-m_stFrameData[nFilteringAlpha].dActTorque_d1)/(nFilteringAlpha*m_dControlCycle);
    m_stFrameData_now.dCmdTorque_d3=(m_stFrameData[0].dCmdTorque_d2-m_stFrameData[1].dCmdTorque_d2)/(m_dControlCycle);
    m_stFrameData_now.dActTorque_d3=(m_stFrameData[0].dActTorque_d2-m_stFrameData[nFilteringAlpha].dActTorque_d2)/(nFilteringAlpha*m_dControlCycle);

    m_stFrameData_now.stPDO = m_stMirrorPDO;
    //2.054 us
    std::copy_backward(m_stFrameData, m_stFrameData + FRAME_DATA_SIZE - 1, m_stFrameData + FRAME_DATA_SIZE);
    m_stFrameData[0] = m_stFrameData_now; 
}
void CIA402Axis::WriteErrorLog()
{
    auto snapshot = std::make_shared<std::array<ST_FrameData, FRAME_DATA_SIZE>>(); // 创建一份帧数据快照

    std::copy_n(m_stFrameData, FRAME_DATA_SIZE, snapshot->begin());               // 把当前帧缓存复制到快照中

    std::reverse(snapshot->begin(), snapshot->end());                             // 反转顺序，让旧数据在前，新数据在后

    int axisID = m_nAxisID;                                                       // 保存当前轴 ID，避免线程里访问成员变量

    int axisErrorID = m_nErrorCode;                                               // 保存当前错误码，避免线程里访问成员变量

    std::string sErrorLogPath = "/usr/local/bin/Robot/LLCPS/ErrorLog/";           // 日志目录

    std::thread([snapshot, sErrorLogPath, axisID, axisErrorID]()                  // 开一个后台线程写日志
    {
        if (!CreateDirectoriesNoFilesystem(sErrorLogPath))                        // 创建日志目录
        {
            return;                                                               // 创建失败则退出
        }

        DeleteOldCsvFiles(sErrorLogPath, 100);                                    // 保证 csv 日志数量不超过 100 个

        auto now = std::chrono::system_clock::now();                              // 获取当前系统时间

        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;                                       // 获取当前时间的毫秒部分

        std::time_t tt = std::chrono::system_clock::to_time_t(now);               // 转成 time_t

        std::tm tmBuf;                                                            // 定义本地时间结构体

        std::memset(&tmBuf, 0, sizeof(tmBuf));                                    // 清空结构体

        localtime_r(&tt, &tmBuf);                                                 // 线程安全地转换成本地时间

        char szTime[32];                                                          // 保存年月日时分秒字符串

        std::memset(szTime, 0, sizeof(szTime));                                   // 清空字符串缓冲区

        std::strftime(szTime, sizeof(szTime), "%Y%m%d_%H%M%S", &tmBuf);          // 格式化时间，例如 20260623_153012

        char szMs[8];                                                             // 保存毫秒字符串

        std::memset(szMs, 0, sizeof(szMs));                                       // 清空毫秒缓冲区

        std::snprintf(szMs, sizeof(szMs), "%03ld", static_cast<long>(ms.count())); // 格式化毫秒，例如 007、035、128

        std::ostringstream ossFileName;                                           // 文件名字符串流

        ossFileName << sErrorLogPath                                             // 写入日志目录
                    << "AxisID_" << axisID                                       // 写入轴 ID
                    << "_ErrorCode_" << axisErrorID                              // 写入错误码
                    << "_" << szTime                                             // 写入年月日时分秒
                    << "_" << szMs                                               // 写入毫秒
                    << ".csv";                                                   // 写入 csv 后缀

        std::ofstream ofs(ossFileName.str().c_str(), std::ios::out | std::ios::trunc); // 打开 csv 文件

        if (!ofs.is_open())                                                       // 如果文件打开失败
        {
            return;                                                               // 直接退出
        }

        ofs << std::fixed << std::setprecision(6);                                // 设置浮点数保留 6 位小数

        ofs << "Index,"
            << "dCmdPos,dActPos,"
            << "dCmdVel,dActVel,"
            << "dCmdAcc,dActAcc,"
            << "dCmdJerk,dActJerk,"
            << "dCmdTorque,dActTorque,"
            << "dCmdTorque_d1,dActTorque_d1,"
            << "dCmdTorque_d2,dActTorque_d2,"
            << "dCmdTorque_d3,dActTorque_d3,"
            << "PDO_Controlword,"
            << "PDO_TargetPosition,"
            << "PDO_TargetVelocity,"
            << "PDO_TargetTorque,"
            << "PDO_TargetModesOfOperation,"
            << "PDO_DigitalOutputs,"
            << "PDO_TouchProbeFunction,"
            << "PDO_AddVelocityValue,"
            << "PDO_AddTorqueValue,"
            << "PDO_StatusWord,"
            << "PDO_ActualPosition,"
            << "PDO_ActualVelocity,"
            << "PDO_ActualTorque,"
            << "PDO_ActualModesOfOperation,"
            << "PDO_TouchProbeStatus,"
            << "PDO_DigitalInputs,"
            << "PDO_ErrorCode,"
            << "PDO_JointActPosition"
            << "\n";                                                              // 写入 csv 表头

        for (size_t i = 0; i < snapshot->size(); ++i)                              // 遍历快照中的每一帧数据
        {
            const ST_FrameData& f = (*snapshot)[i];                               // 取出当前帧数据

            const ST_CIA402_PDO& p = f.stPDO;                                     // 取出当前帧中的 PDO 数据

            ofs << i << ","                                                      // 写入序号
                << f.dCmdPos << "," << f.dActPos << ","                         // 写入命令位置和实际位置
                << f.dCmdVel << "," << f.dActVel << ","                         // 写入命令速度和实际速度
                << f.dCmdAcc << "," << f.dActAcc << ","                         // 写入命令加速度和实际加速度
                << f.dCmdJerk << "," << f.dActJerk << ","                       // 写入命令 jerk 和实际 jerk
                << f.dCmdTorque << "," << f.dActTorque << ","                   // 写入命令力矩和实际力矩
                << f.dCmdTorque_d1 << "," << f.dActTorque_d1 << ","             // 写入一阶力矩数据
                << f.dCmdTorque_d2 << "," << f.dActTorque_d2 << ","             // 写入二阶力矩数据
                << f.dCmdTorque_d3 << "," << f.dActTorque_d3 << ","             // 写入三阶力矩数据
                << p.Controlword << ","                                          // 写入控制字
                << p.TargetPosition << ","                                       // 写入目标位置
                << p.TargetVelocity << ","                                       // 写入目标速度
                << static_cast<int>(p.TargetTorque) << ","                       // 写入目标力矩
                << static_cast<unsigned int>(p.TargetModesOfOperation) << ","     // 写入目标运行模式
                << p.DigitalOutputs << ","                                       // 写入数字输出
                << p.TouchProbeFunction << ","                                   // 写入探针功能
                << p.AddVelocityValue << ","                                     // 写入附加速度
                << static_cast<int>(p.AddTorqueValue) << ","                     // 写入附加力矩
                << p.StatusWord << ","                                           // 写入状态字
                << p.ActualPosition << ","                                       // 写入实际位置
                << p.ActualVelocity << ","                                       // 写入实际速度
                << static_cast<int>(p.ActualTorque) << ","                       // 写入实际力矩
                << static_cast<unsigned int>(p.ActualModesOfOperation) << ","     // 写入实际运行模式
                << p.TouchProbeStatus << ","                                     // 写入探针状态
                << p.DigitalInputs << ","                                        // 写入数字输入
                << p.ErrorCode << ","                                            // 写入驱动错误码
                << p.JointActPosition                                            // 写入关节实际位置
                << "\n";                                                         // 换行
        }

        ofs.close();                                                              // 关闭文件
    }).detach();                                                                  // 分离线程，异步写日志
}

int CIA402Axis::CleanSoftMotionData()
{
    m_stSoftMotionEX.bReplan = false;
    m_stSoftMotionEX.vMotionUint.clear();
    m_stSoftMotionEX.dMotionTime = 0;
    return AEC_SUCCESSED;
}