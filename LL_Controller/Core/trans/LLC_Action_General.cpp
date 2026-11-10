
#include "LL_Errorcode.h"
#include "LLC_Rbt.h"



bool action_LLCReadSDO(actionParam)
{
    int nRet = REC_Successed;
    string value = "";
    vector<int> nData;
    do
    {
        if (event==nullptr){nRet= REC_CmdFormatError;break;}
        if (event->param.size() != 4){nRet= REC_CmdFormatError;break;}

        GET_AND_BREAK_NOT_INT(nSlaveID, event->param[0])
        GET_AND_BREAK_NOT_INT(nSDOIndex, event->param[1])
        GET_AND_BREAK_NOT_INT(nSDOSubIndex, event->param[2])
        GET_AND_BREAK_NOT_INT(nSDOType, event->param[3])

        nRet = LLCRbt->ReadSDO(nSlaveID, nSDOIndex, nSDOSubIndex, nSDOType, value);
    } while (false);
    event->result.push_back(to_string(nRet));
    if(nRet == REC_Successed)
    {
        event->result.push_back(value);
    }
    return (nRet==REC_Successed);
}

bool action_LLCWriteSDO(actionParam)
{
    int nRet = REC_Successed;
    do
    {
        if (event==nullptr){nRet= REC_CmdFormatError;break;}
        if (event->param.size() < 5){nRet= REC_CmdFormatError;break;}
        GET_AND_BREAK_NOT_INT(nSlaveID, event->param[0])
        GET_AND_BREAK_NOT_INT(nSDOIndex, event->param[1])
        GET_AND_BREAK_NOT_INT(nSDOSubIndex, event->param[2])
        GET_AND_BREAK_NOT_INT(nSDOType, event->param[3])
        nRet = LLCRbt->WriteSDO(nSlaveID, nSDOIndex, nSDOSubIndex, nSDOType,event->param[4]);
    } while (false);
    event->result.push_back(to_string(nRet));
    return (nRet==REC_Successed);
}

bool action_LLCWritePDO(actionParam)
{
    int nRet = REC_Successed;
    do
    {
        if (event==nullptr){nRet= REC_CmdFormatError;break;}
        if (event->param.size() < 4){nRet= REC_CmdFormatError;break;}
        GET_AND_BREAK_NOT_INT(nSlaveID, event->param[0])
        GET_AND_BREAK_NOT_INT(nPDOIndex, event->param[1])
        GET_AND_BREAK_NOT_INT(nPDOSubIndex, event->param[2])
        GET_AND_BREAK_NOT_INT(nValue, event->param[3])

        nRet = LLCRbt->WritePDO(nSlaveID, nPDOIndex, nPDOSubIndex, nValue);
    } while (false);
    event->result.push_back(to_string(nRet));
    return (nRet==REC_Successed);
}

//升级固件
bool action_FOEwrite(actionParam)
{
    int nRet = REC_Successed;
    do
    {
        if (event==nullptr){nRet= REC_CmdFormatError;break;}
        if (event->param.size() != 4){nRet= REC_CmdFormatError;break;}
        GET_AND_BREAK_NOT_INT(nSlaveID, event->param[0])
        string strFilePath = event->param[1];
        string strFoEFileName = event->param[2];
        GET_AND_BREAK_NOT_INT(nPassword, event->param[3])
        nRet = LLCRbt->FOEwrite(nSlaveID, strFilePath, strFoEFileName, nPassword);
    } while (false);
    event->result.push_back(to_string(nRet));
    return (nRet==REC_Successed);
}
//获取升级固件进度
bool action_GetFOEwriteProgress(actionParam)
{
    int nRet = REC_Successed;
    double dProgress = 0;
    do
    {
        if (event==nullptr){nRet= REC_CmdFormatError;break;}
        if (event->param.size() != 0){nRet= REC_CmdFormatError;break;}
        nRet = LLCRbt->GetFOEwriteProgress(dProgress);
    } while (false);
    event->result.push_back(to_string(nRet));
    event->result.push_back(to_string(dProgress));
    return (nRet==REC_Successed);
}

bool action_FOEread(actionParam)
{
    int nRet = REC_Successed;
    do
    {
        if (event==nullptr){nRet= REC_CmdFormatError;break;}
        if (event->param.size() != 4){nRet= REC_CmdFormatError;break;}
        GET_AND_BREAK_NOT_INT(nSlaveID, event->param[0])
        string strFilePath = event->param[1];
        string strFoEFileName = event->param[2];
        GET_AND_BREAK_NOT_INT(nPassword, event->param[3])
        nRet = LLCRbt->FOEread(nSlaveID, strFilePath, strFoEFileName, nPassword);
    } while (false);
    event->result.push_back(to_string(nRet));
    return (nRet==REC_Successed);
}
//获取升级固件进度
bool action_GetFOEreadProgress(actionParam)
{
    int nRet = REC_Successed;
    double dProgress = 0;
    do
    {
        if (event==nullptr){nRet= REC_CmdFormatError;break;}
        if (event->param.size() != 0){nRet= REC_CmdFormatError;break;}
        nRet = LLCRbt->GetFOEreadProgress(dProgress);
    } while (false);
    event->result.push_back(to_string(nRet));
    event->result.push_back(to_string(dProgress));
    return (nRet==REC_Successed);
}

bool action_UpdateESI(actionParam)
{
    int nRet = REC_Successed;
    do
    {
        if (event==nullptr){nRet= REC_CmdFormatError;break;}
        if (event->param.size() != 2){nRet= REC_CmdFormatError;break;}
        GET_AND_BREAK_NOT_INT(nSlaveID, event->param[0])
        string strFilePath = event->param[1];
        nRet = LLCRbt->UpdateESI(nSlaveID, strFilePath);
    } while (false);
    event->result.push_back(to_string(nRet));
    return (nRet==REC_Successed);
}

bool action_ReadAxisInformation(actionParam)
{
    int nRet = REC_Successed;
    string sAxisInformation;
    nRet = LLCRbt->ReadAxisInformation(sAxisInformation);
    event->result.push_back(to_string(nRet));
    event->result.push_back(sAxisInformation);
    return (nRet==REC_Successed);
}

bool action_SetIO(actionParam)
{
    if (event == nullptr) return false; // 事件为空时直接返回
    int nRet = REC_Successed; // 默认执行成功
    do
    {
        if (event->param.size() != 2) { nRet = REC_CmdFormatError; break; } // 检查参数数量
        GET_AND_BREAK_NOT_INT(nIOid, event->param[0]); // 获取IO编号
        GET_AND_BREAK_NOT_INT(nVar, event->param[1]); // 获取IO状态：0或1
        if (nIOid < 0 || (nVar != 0 && nVar != 1)) { nRet = REC_CmdFormatError; break; } // 检查参数范围
        nRet = LLCRbt->SetIO(nIOid, nVar != 0); // 将整数状态转换为bool并设置IO
    } while (false);
    event->result.push_back(to_string(nRet)); // 保存执行结果
    return nRet == REC_Successed; // 返回是否执行成功
}