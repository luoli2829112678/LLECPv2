#include "LLC_Action_MotionSet.h"
#include "LL_Errorcode.h"
#include "LLC_Rbt.h"
#include "string.h"

bool action_AxisEnable(actionParam)
{
    int nRet = REC_Successed;
    do
    {
        if (event==nullptr){nRet= REC_CmdFormatError;break;}
        int AxisNum = event->param.size();
        vector<int> AxisList;
        AxisList.clear();
        for (int i = 0; i < AxisNum; i++)
        {
            AxisList.push_back(stoi(event->param[i]));
            if (AxisList.back()> LLCRbt->m_nConfiguredAxisNum -1){nRet= REC_ParametersError;break;}
        }
        nRet = LLCRbt->AxisEnable(AxisList);
    } while (false);
    event->result.push_back(to_string(nRet));
    return (nRet==REC_Successed);
}

bool action_AxisDisable(actionParam)
{
    int nRet = REC_Successed;
    do
    {
        if (event==nullptr){nRet= REC_CmdFormatError;break;}
        int AxisNum = event->param.size();
        vector<int> AxisList;
        AxisList.clear();
        for (int i = 0; i < AxisNum; i++)
        {
            AxisList.push_back(stoi(event->param[i]));
            if (AxisList.back()> LLCRbt->m_nConfiguredAxisNum -1){nRet= REC_ParametersError;break;}

        }
        nRet = LLCRbt->AxisDisable(AxisList);
    } while (false);
    event->result.push_back(to_string(nRet));
    return (nRet==REC_Successed);
}

bool action_AxisReset(actionParam)
{
    int nRet = REC_Successed;
    do
    {
        if (event==nullptr){nRet= REC_CmdFormatError;break;}
        int AxisNum = event->param.size();
        vector<int> AxisList;
        AxisList.clear();
        for (int i = 0; i < AxisNum; i++)
        {
            AxisList.push_back(stoi(event->param[i]));
            if (AxisList.back()> LLCRbt->m_nConfiguredAxisNum -1){nRet= REC_ParametersError;break;}
        }
        nRet = LLCRbt->AxisReset(AxisList);
    } while (false);
    event->result.push_back(to_string(nRet));
    return (nRet==REC_Successed);
}

bool action_AxisHome(actionParam)
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
        nRet = LLCRbt->AxisHome(AxisList);
    } while (false);
    event->result.push_back(to_string(nRet));
    return (nRet==REC_Successed);

}

bool action_AxisSetOffset(actionParam)
{
    int nRet = REC_Successed;
    do
    {
        if (event==nullptr){nRet= REC_CmdFormatError;break;}
        if (event->param.size() != 2){nRet= REC_CmdFormatError;break;}
        GET_AND_BREAK_NOT_INT(nAxisID, event->param[0]);
        GET_AND_BREAK_NOT_NUM(dOffset, event->param[1]);
        nRet = LLCRbt->AxisSetOffset(nAxisID,dOffset);
    } while (false);
    event->result.push_back(to_string(nRet));
    return (nRet==REC_Successed);
}

bool action_AxisSetModesOfOperation(actionParam)
{
    int nRet = REC_Successed;
    do
    {
        if (event==nullptr){nRet= REC_CmdFormatError;break;}
        if (event->param.size() != 2){nRet= REC_CmdFormatError;break;}
        GET_AND_BREAK_NOT_INT(nAxisID, event->param[0]);
        GET_AND_BREAK_NOT_INT(nMode, event->param[1]);
        nRet = LLCRbt->AxisSetModesOfOperation(nAxisID,nMode);
    } while (false);
    event->result.push_back(to_string(nRet));
    return (nRet==REC_Successed);
}






bool action_ControllerReset(actionParam)
{
    int nRet = REC_Successed;
    // 复位总线
    nRet = LLCRbt->ControllerReset();
    event->result.push_back(to_string(nRet));
    return REC_Successed == nRet;
}
