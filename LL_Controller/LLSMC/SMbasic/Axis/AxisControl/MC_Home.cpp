#include"MC_Home.h"
MC_Home::MC_Home(/* args */)
{
    m_bExecute =false;
    m_bBusy = false;
    m_bDone = false;
    m_bError = false;
    m_nErrorID = false;
}

MC_Home::~MC_Home()
{
}

void MC_Home::operator()(CIA402Axis* axis)
{
    m_pCIA402Axis = axis;
    this->Execute();
}
void MC_Home::operator()(CIA402Axis* axis,bool bExecute,bool &bBusy,bool &bCommandAborted,bool &bDone, bool &bError,int &nErrorID)
{
    m_pCIA402Axis = axis;
    this->operator()(axis,bExecute,0,bBusy,bCommandAborted,bDone,bError,nErrorID);
}

void MC_Home::operator()(CIA402Axis* axis, bool bExecute, double dPos, bool& bBusy, bool& bCommandAborted, bool& bDone, bool& bError, int& nErrorID)
{
    m_pCIA402Axis = axis; // 保存轴对象
    m_bExecute = bExecute; // 保存执行信号

    if (!bExecute)
    {
        m_fsHome = ReadyHome; // 恢复初始阶段
        m_bBusy = false; // 清除执行状态
        m_bError = false; // 清除错误状态
        m_bDone = false; // 清除完成状态
        m_nErrorID = SMEC_SUCCESSED; // 清除错误码
        m_Timer.R_TRIG(false); // 复位上升沿检测
        m_Timer.Ton(false, SMC_TIME_OUT); // 复位超时计时
    }
    else
    {
        this->Execute(); // 每周期推进内部状态机
    }

    bBusy = m_bBusy; // 输出执行状态
    bCommandAborted = false; // 当前实现尚未提供命令中止检测
    bDone = m_bDone; // 输出完成状态
    bError = m_bError; // 输出错误状态
    nErrorID = m_nErrorID; // 输出错误码
}

void MC_Home::Execute()
{
    if(nullptr == m_pCIA402Axis)
    {
        m_bError = true;
        m_nErrorID = SMEC_INVALID_AXIS;
        return;
    }
    uint16_t nControlWord = 0;
    uint16_t nStatusWord = 0;
    EN_ModesOfOperation  nModesOfOperation;
    m_pCIA402Axis->Axis_PDO_ReadStatusWord(nStatusWord);
    m_pCIA402Axis->Axis_PDO_ReadControlword(nControlWord);
    m_pCIA402Axis->Axis_PDO_ReadModesOfOperation(nModesOfOperation);
    m_bBusy = false;
    m_bError = false;
    m_bDone = false;
    m_nErrorID = SMEC_SUCCESSED;
    if(!m_bExecute)
    {
       m_Timer.R_TRIG(m_bExecute);
       m_fsHome = ReadyHome;
       return;
    }
    //检测复位中报错
    uint16_t unStatuBit;
    if(m_pCIA402Axis->Axis_CheckError())
    {
        m_fsHome = HomeError;
        //反馈错误状态
        m_bError = true;
        m_Timer.Ton(false, SMC_TIME_OUT);
    } 
    switch (m_fsHome)
    {
        case ReadyHome:
            if(m_Timer.R_TRIG(m_bExecute))
            {
                //检测轴是否为使能状态
                if(motionState_standstill != m_pCIA402Axis->Axis_ReadAxisState())
                {
                    //此状态不允许找零，也不切状态机，功能块置为error
                    m_fsHome = HomeError;
                    //切换到错误状态
                    m_bError = true;
                    m_Timer.Ton(false, SMC_TIME_OUT);
                    break;
                }
                //虚拟轴找零完成
                if((m_pCIA402Axis->Axis_GetVirtualState())||(en_StatusWord_ms2 == (nStatusWord & en_StatusWord_ms2)))
                {
                    //反馈完成状态
                    m_bDone = true;
                    //同步状态机
                    m_pCIA402Axis->Axis_SetAxisState(EN_AxisMotionState::motionState_standstill);
                    m_fsHome = HomeFinish;
                    m_Timer.Ton(false, SMC_TIME_OUT);
                    break;
                }
                //切换轴状态
                m_pCIA402Axis->Axis_SetAxisState(EN_AxisMotionState::motionState_homing);
                //切换到找零模式
                m_pCIA402Axis->Axis_PDO_SetModesOfOperation(EN_ModesOfOperation ::enModeHoming);
                //初始化控制字
                m_fsHome = StartHome;
                m_bBusy = true;
                m_Timer.Ton(false, SMC_TIME_OUT);
                break;
            }
            break;
        case StartHome:
            m_bBusy = true;
            //监测操作模式是否切到6
            //超时检测
            if(m_Timer.Ton(true, SMC_TIME_OUT))
            {
                m_fsHome = HomeError;
                m_bError = true;
                m_nErrorID = SMEC_TIMEOUT;
                m_Timer.Ton(false, SMC_TIME_OUT);
                break;
            }
            if(EN_ModesOfOperation::enModeHoming == nModesOfOperation)
            {
                m_pCIA402Axis->Axis_PDO_SetControlword(nControlWord | en_ControlWord_oms0);
                m_fsHome = Homing;
                m_Timer.Ton(false, SMC_TIME_OUT);
            }
            break;
        case Homing:
            m_bBusy = true;
            if(m_pCIA402Axis->Axis_CheckError())
            {
                m_fsHome = HomeError;
                m_bError = true;
                m_Timer.Ton(false, SMC_TIME_OUT);
                break;
            }
            if(en_StatusWord_ms2 == (nStatusWord & en_StatusWord_ms2))
            {
                //同步状态机
                m_pCIA402Axis->Axis_SetAxisState(EN_AxisMotionState::motionState_standstill);
                m_pCIA402Axis->Axis_InitAlign();//位置对齐
                m_pCIA402Axis->Axis_PDO_SetModesOfOperation(EN_ModesOfOperation ::enModeCyclicSyncPosition);//切换回位置模式
                m_fsHome = HomeFinish;
                m_Timer.Ton(false, SMC_TIME_OUT);
                break;
            }
            break;
        case HomeFinish:
            m_bBusy = false;
            m_pCIA402Axis->Axis_SetAxisState(EN_AxisMotionState::motionState_standstill);
            nControlWord =  nControlWord & (~en_ControlWord_oms0);
            m_pCIA402Axis->Axis_PDO_SetControlword(nControlWord);
            //反馈完成状态
            m_bDone = true;
            if(m_Timer.F_TRIG(m_bExecute))
            {
                m_nErrorID = SMEC_SUCCESSED;
                m_fsHome = ReadyHome;
                break;
            }
            break;
        case HomeError:
            nControlWord = en_ControlWord_Init;
            m_pCIA402Axis->Axis_PDO_SetControlword(nControlWord);
            if(m_Timer.F_TRIG(m_bExecute))
            {
                m_nErrorID = SMEC_SUCCESSED;
                m_fsHome = ReadyHome;
                break;
            }
    }
}

