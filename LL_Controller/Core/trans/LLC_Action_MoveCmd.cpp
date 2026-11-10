
#include "LLC_Action_MoveCmd.h"
#include "LL_Errorcode.h"
#include "LLC_Rbt.h"


// 停止运动
bool action_ControllerStop(actionParam)
{
    int nRet = REC_Successed;
    do
    {
        if (event==nullptr){nRet= REC_CmdFormatError;break;}
        if (event->param.size() != 0){nRet= REC_CmdFormatError;break;}
        LLCRbt->ControllerStop();
    } while (false);
    event->result.push_back(to_string(nRet));
    return (nRet==REC_Successed);
}
bool action_AxisStop(actionParam)
{
    int nRet = REC_Successed;
    do
    {
        if (event==nullptr){nRet= REC_CmdFormatError;break;}
        int AxisNum = event->param.size();
        if (0 == AxisNum){nRet= REC_ParametersError;break;}
        vector<int> AxisList;
        for (int i = 0; i < AxisNum; i++)
        {
            AxisList.push_back(stoi(event->param[i]));
            if (AxisList.back()> LLCRbt->m_nConfiguredAxisNum -1){nRet= REC_ParametersError;break;}
        }
        nRet = LLCRbt->AxisStop(AxisList);
    } while (false);
    event->result.push_back(to_string(nRet));
    return (nRet==REC_Successed);
}
bool action_AxisAbsMove(actionParam)
{
    int nRet = REC_Successed;
    do
    {
        if (event==nullptr){nRet= REC_CmdFormatError;break;}
        if (event->param.size() != 6){nRet= REC_CmdFormatError;break;}
        GET_AND_BREAK_NOT_INT(nAxisID, event->param[0]);
        GET_AND_BREAK_NOT_NUM(dPos, event->param[1]);
        GET_AND_BREAK_NOT_NUM(dVel, event->param[2]);
        GET_AND_BREAK_NOT_NUM(dAcc, event->param[3]);
        GET_AND_BREAK_NOT_NUM(dDec, event->param[4]);
        GET_AND_BREAK_NOT_NUM(dJerk, event->param[5]);
        nRet = LLCRbt->AxisAbsMove(nAxisID,dPos,dVel,dAcc,dDec,dJerk);
    } while (false);
    event->result.push_back(to_string(nRet));
    return (nRet==REC_Successed);
}
bool action_AxisRelMove(actionParam)
{
    int nRet = REC_Successed;
    do
    {
        if (event==nullptr){nRet= REC_CmdFormatError;break;}
        if (event->param.size() != 6){nRet= REC_CmdFormatError;break;}
        GET_AND_BREAK_NOT_INT(nAxisID, event->param[0]);
        GET_AND_BREAK_NOT_NUM(dPos, event->param[1]);
        GET_AND_BREAK_NOT_NUM(dVel, event->param[2]);
        GET_AND_BREAK_NOT_NUM(dAcc, event->param[3]);
        GET_AND_BREAK_NOT_NUM(dDec, event->param[4]);
        GET_AND_BREAK_NOT_NUM(dJerk, event->param[5]);
        nRet = LLCRbt->AxisRelMove(nAxisID,dPos,dVel,dAcc,dDec,dJerk);
    } while (false);
    event->result.push_back(to_string(nRet));
    return (nRet==REC_Successed);
}

bool action_AxisBackAndForthMove(actionParam)
{
    int nRet = REC_Successed;
    do
    {
        if (event==nullptr){nRet= REC_CmdFormatError;break;}
        if (event->param.size() != 12){nRet= REC_CmdFormatError;break;}
        GET_AND_BREAK_NOT_INT(nAxisID, event->param[0]);
        GET_AND_BREAK_NOT_NUM(dPos1, event->param[1]);
        GET_AND_BREAK_NOT_NUM(dPos2, event->param[2]);
        GET_AND_BREAK_NOT_NUM(dVel1, event->param[3]);
        GET_AND_BREAK_NOT_NUM(dVel2, event->param[4]);
        GET_AND_BREAK_NOT_NUM(dAcc1, event->param[5]);
        GET_AND_BREAK_NOT_NUM(dAcc2, event->param[6]);
        GET_AND_BREAK_NOT_NUM(dDec1, event->param[7]);
        GET_AND_BREAK_NOT_NUM(dDec2, event->param[8]);
        GET_AND_BREAK_NOT_NUM(dJerk1, event->param[9]);
        GET_AND_BREAK_NOT_NUM(dJerk2, event->param[10]);
        GET_AND_BREAK_NOT_NUM(dWaitTime, event->param[11]);
        nRet = LLCRbt->AxisBackAndForthMove(nAxisID,dPos1,dPos2,dVel1,dVel2,dAcc1,dAcc2,dDec1,dDec2,dJerk1,dJerk2,dWaitTime);
    } while (false);
    event->result.push_back(to_string(nRet));
    return (nRet==REC_Successed);
}
bool action_AxisContinuousStepMove(actionParam)
{
    int nRet = REC_Successed;
    do
    {
        if (event==nullptr){nRet= REC_CmdFormatError;break;}
        if (event->param.size() != 7){nRet= REC_CmdFormatError;break;}
        GET_AND_BREAK_NOT_INT(nAxisID, event->param[0]);
        GET_AND_BREAK_NOT_NUM(dPos, event->param[1]);
        GET_AND_BREAK_NOT_NUM(dVel, event->param[2]);
        GET_AND_BREAK_NOT_NUM(dAcc, event->param[3]);
        GET_AND_BREAK_NOT_NUM(dDec, event->param[4]);
        GET_AND_BREAK_NOT_NUM(dJerk, event->param[5]);
        GET_AND_BREAK_NOT_NUM(dWaitTime, event->param[6]);
        nRet = LLCRbt->AxisContinuousStepMove(nAxisID,dPos,dVel,dAcc,dDec,dJerk,dWaitTime);
    } while (false);
    event->result.push_back(to_string(nRet));
    return (nRet==REC_Successed);
}

bool action_AxisVelocityMove(actionParam)
{
    int nRet = REC_Successed;
    do
    {
        if (event==nullptr){nRet= REC_CmdFormatError;break;}
        if (event->param.size() != 6){nRet= REC_CmdFormatError;break;}
        GET_AND_BREAK_NOT_INT(nAxisID, event->param[0]);
        GET_AND_BREAK_NOT_NUM(nDir, event->param[1]);
        GET_AND_BREAK_NOT_NUM(dVel, event->param[2]);
        GET_AND_BREAK_NOT_NUM(dAcc, event->param[3]);
        GET_AND_BREAK_NOT_NUM(dDec, event->param[4]);
        GET_AND_BREAK_NOT_NUM(dJerk, event->param[5]);
        nRet = LLCRbt->AxisVelocityMove(nAxisID,nDir,dVel,dAcc,dDec,dJerk);
    } while (false);
    event->result.push_back(to_string(nRet));
    return (nRet==REC_Successed);
}


// 启动模拟量对焦
bool action_StartAnalogFocusing(actionParam)
{
    int nRet = REC_Successed;
    do
    {
        if (event == nullptr) { nRet = REC_CmdFormatError; break; }
        if (event->param.size() != 1) { nRet = REC_CmdFormatError; break; }

        int nAxisID = stoi(event->param[0]);
        nRet = LLCRbt->StartAnalogFocusing(nAxisID);

    } while (false);

    event->result.push_back(to_string(nRet));
    return (nRet == REC_Successed);
}


// 停止模拟量对焦
bool action_StopAnalogFocusing(actionParam)
{
    int nRet = REC_Successed;
    do
    {
        if (event == nullptr) { nRet = REC_CmdFormatError; break; }
        if (event->param.size() != 1) { nRet = REC_CmdFormatError; break; }

        int nAxisID = stoi(event->param[0]);
        nRet = LLCRbt->StopAnalogFocusing(nAxisID);

    } while (false);

    event->result.push_back(to_string(nRet));
    return (nRet == REC_Successed);
}


// 启动 PEG
bool action_StartPEG(actionParam)
{
    int nRet = REC_Successed;
    do
    {
        if (event == nullptr) { nRet = REC_CmdFormatError; break; }
        if (event->param.size() != 1) { nRet = REC_CmdFormatError; break; }

        int nAxisID = stoi(event->param[0]);
        nRet = LLCRbt->StartPEG(nAxisID);

    } while (false);

    event->result.push_back(to_string(nRet));
    return (nRet == REC_Successed);
}


// 停止 PEG
bool action_StopPEG(actionParam)
{
    int nRet = REC_Successed;
    do
    {
        if (event == nullptr) { nRet = REC_CmdFormatError; break; }
        if (event->param.size() != 1) { nRet = REC_CmdFormatError; break; }

        int nAxisID = stoi(event->param[0]);
        nRet = LLCRbt->StopPEG(nAxisID);

    } while (false);

    event->result.push_back(to_string(nRet));
    return (nRet == REC_Successed);
}

bool action_SetAxisMove(actionParam)
{
    int nRet = REC_Successed;
    do
    {
        if (event == nullptr) { nRet = REC_CmdFormatError; break; }
        if (event->param.size() < 1) { nRet = REC_CmdFormatError; break; }
        std::vector<int> v_Axes;
        for (size_t i = 0; i < event->param.size(); i++)
        {
            v_Axes.push_back(stoi(event->param[i]));
        }
        nRet = LLCRbt->SetAxisMove(v_Axes);

    } while (false);
    event->result.push_back(to_string(nRet));
    return (nRet == REC_Successed);
}

bool action_InitWayPoint(actionParam)
{
    int nRet = REC_Successed;
    do
    {
        if (event == nullptr) { nRet = REC_CmdFormatError; break; }
        if (event->param.size() != 0) { nRet = REC_CmdFormatError; break; }
        nRet = LLCRbt->InitWaypoint();

    } while (false);
    event->result.push_back(to_string(nRet));
    return (nRet == REC_Successed);
}

bool action_PushWayPoint(actionParam)
{
    int nRet = REC_Successed;
    do
    {
        if (event == nullptr) { nRet = REC_CmdFormatError; break; }
        if (event->param.size() % 6 != 0) { nRet = REC_CmdFormatError; break; }//必须下发完整
        int nPointNum = event->param.size()/6;
        std::vector<ST_AxisWayPointUnit> AxisPoints;
        for (int i = 0; i < nPointNum; i++)
        {   
            ST_AxisWayPointUnit AxisPoint;
            AxisPoint.nAxisID = stoi(event->param[i * 6 + 0]);
            AxisPoint.stAxisMtionParam.dPos = stof(event->param[i * 6 + 1]);
            AxisPoint.stAxisMtionParam.dVel = stof(event->param[i * 6 + 2]);
            AxisPoint.stAxisMtionParam.dAcc = stof(event->param[i * 6 + 3]);
            AxisPoint.stAxisMtionParam.dDec = stof(event->param[i * 6 + 4]);
            AxisPoint.stAxisMtionParam.dJerk = stof(event->param[i * 6 + 5]);
            AxisPoints.push_back(AxisPoint);
        }
        nRet = LLCRbt->PushWaypoint(AxisPoints);
    } while (false);
    event->result.push_back(to_string(nRet));
    return (nRet == REC_Successed);
}

bool action_WayPointPushFinish(actionParam)
{
    int nRet = REC_Successed;
    do
    {
        if (event == nullptr) { nRet = REC_CmdFormatError; break; }
        if (event->param.size() != 1) { nRet = REC_CmdFormatError; break; }
        int nWayPointID = stoi(event->param[0]);
        nRet = LLCRbt->WayPointPushFinish(nWayPointID);

    } while (false);
    event->result.push_back(to_string(nRet));
    return (nRet == REC_Successed);
}

bool action_AxisFollowPosition(actionParam)
{
    int nRet = REC_Successed;
    do
    {
        if (event == nullptr) { nRet = REC_CmdFormatError; break; }
        if (event->param.size() != 2) { nRet = REC_CmdFormatError; break; }
        int nAxisID = stof(event->param[0]);
        double dPosition = stof(event->param[1]);
        nRet = LLCRbt->AxisFollowPosition(nAxisID,dPosition);

    } while (false);
    event->result.push_back(to_string(nRet));
    return (nRet == REC_Successed);
}
bool action_AxisFollowVelocity(actionParam)
{
    int nRet = REC_Successed;
    do
    {
        if (event == nullptr) { nRet = REC_CmdFormatError; break; }
        if (event->param.size() != 2) { nRet = REC_CmdFormatError; break; }
        int nAxisID = stof(event->param[0]);
        double dVelocity = stof(event->param[1]);
        nRet = LLCRbt->AxisFollowVelocity(nAxisID,dVelocity);

    } while (false);
    event->result.push_back(to_string(nRet));
    return (nRet == REC_Successed);
}
bool action_AxisFollowTorque(actionParam)
{
    int nRet = REC_Successed;
    do
    {
        if (event == nullptr) { nRet = REC_CmdFormatError; break; }
        if (event->param.size() != 2) { nRet = REC_CmdFormatError; break; }
        int nAxisID = stof(event->param[0]);
        double dTorque = stof(event->param[1]);
        nRet = LLCRbt->AxisFollowTorque(nAxisID,dTorque);

    } while (false);
    event->result.push_back(to_string(nRet));
    return (nRet == REC_Successed);
}