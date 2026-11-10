
#include "LLC_Action_ConfigHard.h"
#include "LL_Errorcode.h"
#include "LLC_Rbt.h"

// 设置模拟机器人
bool action_SetSimulationType(actionParam)
{
    int nRet = REC_Successed;
    do
    {
        if (event==nullptr){nRet= REC_CmdFormatError;break;}
        if (event->param.size() != 1){nRet= REC_CmdFormatError;break;}
        GET_AND_BREAK_NOT_INT(nSimulationType, event->param[0]);
        nRet = LLCRbt->SetSimulationType(nSimulationType);
        printf("SetSimulationType %d\n",nSimulationType);
    } while (false);
    event->result.push_back(to_string(nRet));
    return (nRet==REC_Successed);
}

bool action_RegistrationSlave(actionParam)
{
    int nRet = REC_Successed;
    do
    {
        if (event==nullptr){nRet= REC_CmdFormatError;break;}
        if (event->param.size() != 5){nRet= REC_CmdFormatError;break;}
        GET_AND_BREAK_NOT_INT(nSlaveID, event->param[0]);
        GET_AND_BREAK_NOT_INT(nProductCode, event->param[1]);
        GET_AND_BREAK_NOT_INT(nRevisionNo, event->param[2]);
        GET_AND_BREAK_NOT_INT(bEnableDC, event->param[3]);
        GET_AND_BREAK_NOT_INT(nShiftTime, event->param[4]);
        nRet = LLCRbt->RegistrationSlave(nSlaveID,nProductCode,nRevisionNo,(bool)bEnableDC,nShiftTime);
        printf("RegistrationSlave %d params:\n",nRet);
        printf("ProductCode=%d\n", nProductCode);
        printf("RevisionNo=%d\n", nRevisionNo);
    } while (false);
    event->result.push_back(to_string(nRet));
    return (nRet==REC_Successed);
}

bool action_ConfigureSlavePDO(actionParam)
{

    int nRet = REC_Successed;
    do
    {
        if (event==nullptr){nRet= REC_CmdFormatError;break;}
        if (event->param.size() < 3){nRet= REC_CmdFormatError;break;}
        GET_AND_BREAK_NOT_INT(nSlaveID, event->param[0]);
        GET_AND_BREAK_NOT_INT(TxPDONum, event->param[1]);
        GET_AND_BREAK_NOT_INT(RxPDONum, event->param[2]);
        int nPdoNum = TxPDONum + RxPDONum;
        if (event->param.size() < 3 + nPdoNum){nRet= REC_CmdFormatError;break;}

        std::vector<uint32_t> TxPDO;  
        std::vector<uint32_t> RxPDO; 
        TxPDO.clear();
        TxPDO.clear();
        for (int i = 3; i < 3 + TxPDONum; i++)
        {
            GET_AND_BREAK_NOT_INT(nVar, event->param[i])
            TxPDO.push_back(static_cast<uint32_t>(nVar));
        }
        for (int i = 3 + TxPDONum; i < 3 + nPdoNum; i++)
        {
            GET_AND_BREAK_NOT_INT(nVar, event->param[i])
            RxPDO.push_back(static_cast<uint32_t>(nVar));
        }

        if (nRet != REC_Successed)  
        {
            break;
        }
        nRet = LLCRbt->ConfigureSlavePDO(static_cast<uint8_t>(nSlaveID), TxPDO, RxPDO);
        printf("ConfigureSlavePDO: result=%d, SlaveID=%d, TxPDO=%zu, RxPDO=%zu\n",nRet, nSlaveID, TxPDO.size(), RxPDO.size());
    } while (false);

    event->result.push_back(std::to_string(nRet));
    return nRet == REC_Successed;
}



bool action_RegistrationAxis(actionParam)
{
    int nRet = REC_Successed;
    do
    {
        if (event==nullptr){nRet= REC_CmdFormatError;break;}
        if (event->param.size() != 21){nRet= REC_CmdFormatError;break;}
        GET_AND_BREAK_NOT_INT(nAxisID, event->param[0]);
        GET_AND_BREAK_NOT_INT(nSlaveID, event->param[1]);
        GET_AND_BREAK_NOT_INT(nTxPDO, event->param[2]);
        GET_AND_BREAK_NOT_INT(nRxPDO, event->param[3]);
        GET_AND_BREAK_NOT_INT(nAxisType, event->param[4]);
        GET_AND_BREAK_NOT_INT(nHomePos, event->param[5]);
        GET_AND_BREAK_NOT_INT(nEncodeRatio, event->param[6]);
        GET_AND_BREAK_NOT_INT(nEncodeDirection, event->param[7]);
        GET_AND_BREAK_NOT_NUM(dGearRatio,              event->param[8]);
        GET_AND_BREAK_NOT_NUM(dVelocityScale,    event->param[9]);
        GET_AND_BREAK_NOT_NUM(dVTorqueScale,    event->param[10]);
        GET_AND_BREAK_NOT_NUM(dSafeSpaceLimitUpper,    event->param[11]);
        GET_AND_BREAK_NOT_NUM(dSafeSpaceLimitLower,    event->param[12]);
        GET_AND_BREAK_NOT_NUM(dMaxMotionVel,           event->param[13]);
        GET_AND_BREAK_NOT_NUM(dMaxMotionAcc,           event->param[14]);
        GET_AND_BREAK_NOT_NUM(dMaxMotionJerk,          event->param[15]);
        GET_AND_BREAK_NOT_NUM(dTorqueFeedforwardScale, event->param[16]);
        GET_AND_BREAK_NOT_NUM(dVelocityFeedforwardScale,event->param[17]);
        GET_AND_BREAK_NOT_NUM(dJerkFeedforwardScale,   event->param[18]);
        GET_AND_BREAK_NOT_NUM(dStopAcc,   event->param[19]);
        GET_AND_BREAK_NOT_NUM(dStopJerk,   event->param[20]);
        nRet = LLCRbt->RegistrationAxis(nAxisID,nSlaveID,nTxPDO,nRxPDO,nAxisType,nHomePos,nEncodeRatio,nEncodeDirection,
                dGearRatio,dVelocityScale,dVTorqueScale,dSafeSpaceLimitUpper,dSafeSpaceLimitLower,dMaxMotionVel,dMaxMotionAcc,
                dMaxMotionJerk,dTorqueFeedforwardScale,dVelocityFeedforwardScale,dJerkFeedforwardScale,dStopAcc,dStopJerk);
        printf("RegistrationAxis %d params:\n",nRet);
        printf("nAxisType=%d\n", nAxisType);
        printf("nHomePos=%d\n", nHomePos);
        printf("nEncodeRatio=%d\n", nEncodeRatio);
        printf("nEncodeDirection=%d\n", nEncodeDirection);

        printf("dGearRatio=%f\n", dGearRatio);
        printf("dVelocityScale=%f\n", dVelocityScale);
        printf("dVTorqueScale=%f\n", dVTorqueScale);
        printf("dSafeSpaceLimitUpper=%f\n", dSafeSpaceLimitUpper);
        printf("dSafeSpaceLimitLower=%f\n", dSafeSpaceLimitLower);
        printf("dMaxMotionVel=%f\n", dMaxMotionVel);
        printf("dMaxMotionAcc=%f\n", dMaxMotionAcc);
        printf("dMaxMotionJerk=%f\n", dMaxMotionJerk);
        printf("dTorqueFeedforwardScale=%f\n", dTorqueFeedforwardScale);
        printf("dVelocityFeedforwardScale=%f\n", dVelocityFeedforwardScale);
        printf("dJerkFeedforwardScale=%f\n", dJerkFeedforwardScale);
        printf("dStopAcc=%f\n", dStopAcc);
        printf("dStopJerk=%f\n", dStopJerk);
    } while (false);
    event->result.push_back(to_string(nRet));
    return (nRet==REC_Successed);
}

// 设置总线周期时间
bool action_InitMaster(actionParam)
{
    int nRet = REC_Successed; 
    do
    {
        if (event==nullptr){nRet= REC_CmdFormatError;break;}
        if (event->param.size() != 2){nRet= REC_CmdFormatError;break;}
        GET_AND_BREAK_NOT_INT(nCycleTime, event->param[1]);
        nRet = LLCRbt->InitMaster(event->param[0],nCycleTime);
        printf("SetEcatCycleTime %d\n",nCycleTime);
    } while (false);
    event->result.push_back(to_string(nRet));
    return (nRet==REC_Successed);
}

bool action_RegistrationIO(actionParam)
{
    if (event == nullptr) return false; // 事件为空时直接返回，避免访问空指针
    int nRet = REC_Successed; // 默认执行成功
    do
    {
        if (event->param.size() != 6) { nRet = REC_CmdFormatError; break; } // 必须包含6个参数
        GET_AND_BREAK_NOT_INT(nIOid, event->param[0]); // IO编号
        GET_AND_BREAK_NOT_INT(nSlaveID, event->param[1]); // 从站编号
        GET_AND_BREAK_NOT_INT(nIndex, event->param[2]); // PDO对象索引
        GET_AND_BREAK_NOT_INT(nSubIndex, event->param[3]); // PDO对象子索引
        GET_AND_BREAK_NOT_INT(nBit, event->param[4]); // IO位偏移
        GET_AND_BREAK_NOT_INT(nInverted, event->param[5]); // 取反标志：0不取反，1取反
        if (nIOid < 0 || nIOid > UINT8_MAX || nSlaveID < 0 || nSlaveID > UINT8_MAX ||
            nIndex < 0 || nIndex > UINT16_MAX || nSubIndex < 0 || nSubIndex > UINT16_MAX ||
            nBit < 0 || nBit > 7 || (nInverted != 0 && nInverted != 1))
        {
            nRet = REC_CmdFormatError; // 参数超出允许范围
            break;
        }
        nRet = LLCRbt->RegistrationIO(static_cast<uint8_t>(nIOid), static_cast<uint8_t>(nSlaveID), static_cast<uint16_t>(nIndex), static_cast<uint16_t>(nSubIndex), static_cast<uint16_t>(nBit), nInverted != 0); // 注册IO
    } while (false);
    event->result.push_back(to_string(nRet)); // 返回执行结果
    return nRet == REC_Successed; // 判断是否执行成功
}