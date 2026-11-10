
#include "LLC_Action.h"
#include "LL_Errorcode.h"
#include "LLC_Rbt.h"


bool LLCIsAllParamInt(std::vector<std::string>params)
{
    for (size_t i = 0; i < params.size(); i++)
    {
        if (!LL_isInt(params[i].c_str()))
        {
            return false;
        }
    }
    return true;
}

bool action_noTrans(actionParam)
{
    string strMsg=event->cmd;
    strMsg.append(" forbidden in ");
    strMsg.append(currentStateData);
    strMsg.append(" state");

    event->result.push_back(to_string(REC_ControllerStateRefuseCmd));
    event->result.push_back(strMsg);
    return false;
}
//////////////////////////////////////////////////////////////////////////
bool action_CycleHandleUnInitialized(actionParam)
{
    return false;
}
bool action_CycleHandlePreInitializing(actionParam)
{
    return false;
}
bool action_CycleHandleNotElectrify(actionParam)
{
    return false;
}
bool action_CycleHandleEmergencyStop(actionParam)
{
    return false;
}
bool action_CycleHandleControllerNotConnect(actionParam)
{
    return false;
}
bool action_CycleHandlePreControllerConnecting(actionParam)
{
    return false;
}
bool action_CycleHandleWatchDog(actionParam)
{
    //if(0==LLCRbt->GrpReset(true))
    //{
        //LLCRbt->SetWatchDogReset(false);
    //}
    event->result.push_back(to_string(REC_Successed));
    return false;
}
bool action_CycleHandleDisabled(actionParam)
{
    return false;
}
bool action_CycleHandleHomeStep2(actionParam)
{
    return false;
}
bool action_CycleHandleBrake(actionParam)
{
    return false;
}
bool action_CycleHandlePreEnabling(actionParam)
{
    //LLCRbt->GrpPowerOn();   //进行使能
    event->result.push_back(to_string(REC_Successed));
    return false;
}
bool action_CycleHandleStandBy(actionParam)
{
    return false;
}
bool action_CycleHandlePreDisabling(actionParam)
{
    //LLCRbt->GrpPowerOff(); //添加
    event->result.push_back(to_string(REC_Successed));
    return false;
}
bool action_CycleHandleMoving(actionParam)
{
    return false;
}
bool action_CycleHandleStopping(actionParam)
{
    return false;
}
bool action_CycleHandlePausing(actionParam)
{
    return false;
}
bool action_CycleHandleErrorStopping(actionParam)
{
    return false;
}
bool action_CycleHandleError(actionParam)
{
    return false;
}
bool action_CycleHandlePreResettingError(actionParam)
{
    //LLCRbt->GrpReset();
    event->result.push_back(to_string(REC_Successed));
    return false;
}
bool action_CycleHandlePreFreeDrive(actionParam)
{
    return false;
}
bool action_CycleHandleFreeDrive(actionParam)
{
    return false;
}
bool action_CycleHandlePreDisFreeDrive(actionParam)
{
    return false;
}
bool action_CycleHandleSafeGuard(actionParam)
{
    return false;
}
bool action_CycleHandleHandleEmergencyStop(actionParam)
{
    return false;
}
bool action_CycleHandleHandlePausing(actionParam)
{
    return false;
}
bool action_CycleHandleHandleContinue(actionParam)
{
    return false;
}