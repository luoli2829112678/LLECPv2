

#include "LLC_guard.h"
#include "LLC_Rbt.h"
bool guard_SlavesOP(guardParam)
{
    bool bOP = LLCRbt->CheckIsSlaveInOprational();
    if(bOP)
    {
        std::vector<int> AxisList;
        AxisList.clear();
        for(uint32_t  i = 0;i<LLCRbt->m_nConfiguredAxisNum;i++)
        {
            AxisList.push_back(i);
        }
        LLCRbt->AxisInitReset(AxisList);
        LLCRbt->ControllerConfig();
        LLCRbt->InitSoftMotion();
    }
    return bOP && LLCRbt->guard_CheckIsSlaveConfigDone();
}
// 检测是否触发急停
bool guard_EmergencyStop(guardParam)
{
    return false;
}
// 检测是否急停处理完成
bool guard_EmergencyStopHandled(guardParam)
{
    return false;
}
// 检测是否看门狗超时
bool guard_WatchDog(guardParam)
{
    return false;
}
// 检测是否看门狗超时处理完成
bool guard_WatchDogHandled(guardParam)
{
    return false;
}
// 检测是否从站掉线
bool guard_SlaveDrop(guardParam)
{
    return LLCRbt->guard_CheckIsSlaveDrop();
}
// 检测是否报错
bool guard_RobotError(guardParam)
{
    return LLCRbt->guard_CheckIsError();
}
// 检测是否错误处理完成
bool guard_ErrorStoppingHandled(guardParam)
{
    return true;
}
//监测复位是否完成
bool guard_IsResetFinish(guardParam)
{
    return LLCRbt->guard_CheckResetFinish();
}

// 检测是否处于运动状态
bool guard_Moving(guardParam)
{
    return !LLCRbt->guard_CheckIsMoveDone();
}
// 检测是否处于准备就绪状态
bool guard_Standby(guardParam)
{
    return LLCRbt->guard_CheckIsMoveDone();
}