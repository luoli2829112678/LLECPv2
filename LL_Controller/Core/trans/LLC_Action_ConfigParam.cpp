
#include "LLC_Action_ConfigParam.h"
#include "LL_Errorcode.h"
#include "LLC_Rbt.h"
#include "string.h"

bool action_SetAxisStopParam(actionParam)
{
    int nRet = REC_Successed;
    do
    {
        if (event==nullptr){nRet= REC_CmdFormatError;break;}
        if (event->param.size() != 3){nRet= REC_ParametersError;break;}
        GET_AND_BREAK_NOT_INT(nAxisID, event->param[0]);
        GET_AND_BREAK_NOT_NUM(dStopAcc, event->param[1]);
        GET_AND_BREAK_NOT_NUM(dStopJerk, event->param[2]);
        nRet = LLCRbt->SetAxisStopParam(nAxisID,dStopAcc,dStopJerk);
    } while (false);
    event->result.push_back(to_string(nRet));
    return (nRet==REC_Successed);
}

// 设置轴目标位置
bool action_SetAxisMovePos(actionParam)
{
    int nRet = REC_Successed;
    do {
        if (!event) { nRet = REC_CmdFormatError; break; }
        if (event->param.size() != 2) { nRet = REC_ParametersError; break; }

        int nAxisID = stoi(event->param[0]);
        double dPos    = stod(event->param[1]);
        nRet = LLCRbt->SetAxisMovePos(nAxisID, dPos);
    } while (false);

    event->result.push_back(to_string(nRet));
    return nRet == REC_Successed;
}

// 设置轴运动速度
bool action_SetAxisMoveVel(actionParam)
{
    int nRet = REC_Successed;
    do {
        if (!event) { nRet = REC_CmdFormatError; break; }
        if (event->param.size() != 2) { nRet = REC_ParametersError; break; }

        nRet = LLCRbt->SetAxisMoveVel(
            stoi(event->param[0]),
            stod(event->param[1])
        );
    } while (false);

    event->result.push_back(to_string(nRet));
    return nRet == REC_Successed;
}

// 设置轴加速度
bool action_SetAxisMoveAcc(actionParam)
{
    int nRet = REC_Successed;
    do {
        if (!event) { nRet = REC_CmdFormatError; break; }
        if (event->param.size() != 2) { nRet = REC_ParametersError; break; }

        nRet = LLCRbt->SetAxisMoveAcc(
            stoi(event->param[0]),
            stod(event->param[1])
        );
    } while (false);

    event->result.push_back(to_string(nRet));
    return nRet == REC_Successed;
}

// 设置轴减速度
bool action_SetAxisMoveDec(actionParam)
{
    int nRet = REC_Successed;
    do {
        if (!event) { nRet = REC_CmdFormatError; break; }
        if (event->param.size() != 2) { nRet = REC_ParametersError; break; }

        nRet = LLCRbt->SetAxisMoveDec(
            stoi(event->param[0]),
            stod(event->param[1])
        );
    } while (false);

    event->result.push_back(to_string(nRet));
    return nRet == REC_Successed;
}

// 设置轴加加速度（Jerk）
bool action_SetAxisMoveJerk(actionParam)
{
    int nRet = REC_Successed;
    do {
        if (!event) { nRet = REC_CmdFormatError; break; }
        if (event->param.size() != 2) { nRet = REC_ParametersError; break; }

        nRet = LLCRbt->SetAxisMoveJerk(
            stoi(event->param[0]),
            stod(event->param[1])
        );
    } while (false);

    event->result.push_back(to_string(nRet));
    return nRet == REC_Successed;
}






bool action_SetAccFFScale(actionParam)
{
    int nRet = REC_Successed;
    do {
        if (!event) { nRet = REC_CmdFormatError; break; }
        if (event->param.size() != 2) { nRet = REC_ParametersError; break; }

        nRet = LLCRbt->SetAccFFScale(
            stoi(event->param[0]),
            (stof(event->param[1]))
        );
    } while (false);

    event->result.push_back(to_string(nRet));
    return nRet == REC_Successed;
}
bool action_SetVelFFScale(actionParam)
{
    int nRet = REC_Successed;
    do {
        if (!event) { nRet = REC_CmdFormatError; break; }
        if (event->param.size() != 2) { nRet = REC_ParametersError; break; }

        nRet = LLCRbt->SetVelFFScale(
            stoi(event->param[0]),
            (stof(event->param[1]))
        );
    } while (false);

    event->result.push_back(to_string(nRet));
    return nRet == REC_Successed;
}
bool action_SetJerkFFScale(actionParam)
{
    int nRet = REC_Successed;
    do {
        if (!event) { nRet = REC_CmdFormatError; break; }
        if (event->param.size() != 2) { nRet = REC_ParametersError; break; }

        nRet = LLCRbt->SetJerkFFScale(
            stoi(event->param[0]),
            (stof(event->param[1]))
        );
    } while (false);
    event->result.push_back(to_string(nRet));
    return nRet == REC_Successed;
}