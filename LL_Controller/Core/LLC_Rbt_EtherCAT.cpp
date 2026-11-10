
#include "LLC_Rbt.h"

int LLC_Rbt::ScanSlave(vector<SlaveInfo>& vSlaveInfo)
{
    vSlaveInfo.clear();
    m_vScanSlaveInfo.clear();
    if(m_stRbtEtherCATParams.bSimulationType)
    {
        return REC_Successed;
    }
    if (!m_stEtherCATMasterControl.pScanSlaves || !m_stEtherCATMasterControl.pScanSlaves(m_vScanSlaveInfo)) return REC_ControllerNotInit;
    for (size_t i = 0; i < m_vScanSlaveInfo.size(); i++)
    {
        vSlaveInfo.push_back(m_vScanSlaveInfo[i]);
    }
    return 0;
}

int LLC_Rbt::ReadSdoFromSlave(int nSlave, int nIndex, int nSubIndex, int nSDOtype, std::string* sValue)
{
    if (sValue == nullptr || m_pFuncSDORead == nullptr) return REC_CmdFormatError;

    std::lock_guard<std::mutex> lock(m_sdoMutex);

    if (!m_R_Trig_ReadSDO.R_TRIG(m_stSDOWorkState.bExecute) || m_stSDOWorkState.bBusy) return REC_TimeOutError;

    m_stSDOWorkState.bBusy = true;
    m_stSDOWorkState.bDone = false;
    m_stSDOWorkState.bError = false;

    auto Finish = [&](int nResult, bool bError)
    {
        m_stSDOWorkState.bDone = true;
        m_stSDOWorkState.bError = bError;
        m_stSDOWorkState.bBusy = false;
        m_stSDOWorkState.bExecute = false;
        m_R_Trig_ReadSDO.R_TRIG(false);
        return nResult;
    };

    int l = 0;
    int nRet = 0;
    void* pValue = nullptr;

    uint8_t nByteValue = 0;
    uint16_t nWordValue = 0;
    uint32_t nDwordValue = 0;
    uint64_t nLwordValue = 0;
    float fRealValue = 0.0F;
    double dLrealValue = 0.0;
    std::array<char, 256> szStringValue{};

    switch (nSDOtype)
    {
        case EN_SDOType::enum_SDOType_STRING:
            l = static_cast<int>(szStringValue.size());
            pValue = szStringValue.data();
            break;

        case EN_SDOType::enum_SDOType_BYTE:
            l = sizeof(nByteValue);
            pValue = &nByteValue;
            break;

        case EN_SDOType::enum_SDOType_WORD:
            l = sizeof(nWordValue);
            pValue = &nWordValue;
            break;

        case EN_SDOType::enum_SDOType_DWORD:
            l = sizeof(nDwordValue);
            pValue = &nDwordValue;
            break;

        case EN_SDOType::enum_SDOType_LWORD:
            l = sizeof(nLwordValue);
            pValue = &nLwordValue;
            break;

        case EN_SDOType::enum_SDOType_REAL:
            l = sizeof(fRealValue);
            pValue = &fRealValue;
            break;

        case EN_SDOType::enum_SDOType_LREAL:
            l = sizeof(dLrealValue);
            pValue = &dLrealValue;
            break;

        default:
            return Finish(REC_CmdFormatError, true);
    }

    nRet = m_pFuncSDORead(nSlave, nIndex, nSubIndex, FALSE, &l, pValue, EC_TIMEOUTRET);

    if (nRet <= 0) return Finish(REC_TimeOutError, true);

    switch (nSDOtype)
    {
        case EN_SDOType::enum_SDOType_STRING:
        {
            size_t nLength = l > 0 ? std::min(static_cast<size_t>(l), szStringValue.size()) : 0;
            auto itEnd = std::find(szStringValue.begin(), szStringValue.begin() + nLength, '\0');
            sValue->assign(szStringValue.begin(), itEnd);
            break;
        }

        case EN_SDOType::enum_SDOType_BYTE:
            *sValue = std::to_string(nByteValue);
            break;

        case EN_SDOType::enum_SDOType_WORD:
            *sValue = std::to_string(nWordValue);
            break;

        case EN_SDOType::enum_SDOType_DWORD:
            *sValue = std::to_string(nDwordValue);
            break;

        case EN_SDOType::enum_SDOType_LWORD:
            *sValue = std::to_string(nLwordValue);
            break;

        case EN_SDOType::enum_SDOType_REAL:
        {
            std::ostringstream stream;
            stream << std::setprecision(std::numeric_limits<float>::max_digits10) << fRealValue;
            *sValue = stream.str();
            break;
        }

        case EN_SDOType::enum_SDOType_LREAL:
        {
            std::ostringstream stream;
            stream << std::setprecision(std::numeric_limits<double>::max_digits10) << dLrealValue;
            *sValue = stream.str();
            break;
        }

        default:
            return Finish(REC_CmdFormatError, true);
    }

    return Finish(REC_Successed, false);
}

int LLC_Rbt::WriteSdoToSlave(int nSlave, int nIndex, int nSubIndex, int nSDOtype, std::string sValue)
{
    if (m_pFuncSDOWrite == nullptr) return REC_CmdFormatError;

    std::lock_guard<std::mutex> lock(m_sdoMutex);

    if (!m_R_Trig_WriteSDO.R_TRIG(m_stSDOWorkState.bExecute) || m_stSDOWorkState.bBusy)
    {
        m_R_Trig_WriteSDO.R_TRIG(false);
        return REC_TimeOutError;
    }

    m_stSDOWorkState.bBusy = true;
    m_stSDOWorkState.bDone = false;
    m_stSDOWorkState.bError = false;

    auto Finish = [&](int nResult, bool bError)
    {
        m_stSDOWorkState.bDone = true;
        m_stSDOWorkState.bError = bError;
        m_stSDOWorkState.bBusy = false;
        m_stSDOWorkState.bExecute = false;
        m_R_Trig_WriteSDO.R_TRIG(false);
        return nResult;
    };

    auto ParseUnsignedValue = [&](uint64_t nMaxValue, uint64_t& nResult)
    {
        try
        {
            size_t nFirst = sValue.find_first_not_of(" \t\r\n");
            if (nFirst == std::string::npos || sValue[nFirst] == '-') return false;

            size_t nPosition = 0;
            nResult = std::stoull(sValue, &nPosition, 0);

            if (nPosition != sValue.size()) return false;
            if (nResult > nMaxValue) return false;

            return true;
        }
        catch (...)
        {
            return false;
        }
    };

    auto ParseRealValue = [&](float& fResult)
    {
        try
        {
            size_t nPosition = 0;
            fResult = std::stof(sValue, &nPosition);

            if (nPosition != sValue.size()) return false;
            if (!std::isfinite(fResult)) return false;

            return true;
        }
        catch (...)
        {
            return false;
        }
    };

    auto ParseLrealValue = [&](double& dResult)
    {
        try
        {
            size_t nPosition = 0;
            dResult = std::stod(sValue, &nPosition);

            if (nPosition != sValue.size()) return false;
            if (!std::isfinite(dResult)) return false;

            return true;
        }
        catch (...)
        {
            return false;
        }
    };

    int l = 0;
    int nRet = 0;
    void* pValue = nullptr;
    uint64_t nParsedValue = 0;

    uint8_t nByteValue = 0;
    uint16_t nWordValue = 0;
    uint32_t nDwordValue = 0;
    uint64_t nLwordValue = 0;
    float fRealValue = 0.0F;
    double dLrealValue = 0.0;

    switch (nSDOtype)
    {
        case EN_SDOType::enum_SDOType_STRING:
            if (sValue.size() > static_cast<size_t>(std::numeric_limits<int>::max())) return Finish(REC_CmdFormatError, true);
            l = static_cast<int>(sValue.size());
            pValue = sValue.data();
            break;

        case EN_SDOType::enum_SDOType_BYTE:
            if (!ParseUnsignedValue(std::numeric_limits<uint8_t>::max(), nParsedValue)) return Finish(REC_CmdFormatError, true);
            nByteValue = static_cast<uint8_t>(nParsedValue);
            l = sizeof(nByteValue);
            pValue = &nByteValue;
            break;

        case EN_SDOType::enum_SDOType_WORD:
            if (!ParseUnsignedValue(std::numeric_limits<uint16_t>::max(), nParsedValue)) return Finish(REC_CmdFormatError, true);
            nWordValue = static_cast<uint16_t>(nParsedValue);
            l = sizeof(nWordValue);
            pValue = &nWordValue;
            break;

        case EN_SDOType::enum_SDOType_DWORD:
            if (!ParseUnsignedValue(std::numeric_limits<uint32_t>::max(), nParsedValue)) return Finish(REC_CmdFormatError, true);
            nDwordValue = static_cast<uint32_t>(nParsedValue);
            l = sizeof(nDwordValue);
            pValue = &nDwordValue;
            break;

        case EN_SDOType::enum_SDOType_LWORD:
            if (!ParseUnsignedValue(std::numeric_limits<uint64_t>::max(), nParsedValue)) return Finish(REC_CmdFormatError, true);
            nLwordValue = nParsedValue;
            l = sizeof(nLwordValue);
            pValue = &nLwordValue;
            break;

        case EN_SDOType::enum_SDOType_REAL:
            if (!ParseRealValue(fRealValue)) return Finish(REC_CmdFormatError, true);
            l = sizeof(fRealValue);
            pValue = &fRealValue;
            break;

        case EN_SDOType::enum_SDOType_LREAL:
            if (!ParseLrealValue(dLrealValue)) return Finish(REC_CmdFormatError, true);
            l = sizeof(dLrealValue);
            pValue = &dLrealValue;
            break;

        default:
            return Finish(REC_CmdFormatError, true);
    }

    nRet = m_pFuncSDOWrite(nSlave, nIndex, nSubIndex, FALSE, l, pValue, EC_TIMEOUTRET);

    if (nRet <= 0) return Finish(REC_TimeOutError, true);

    return Finish(REC_Successed, false);
}

int LLC_Rbt::ReadSDO(int nSlave, uint16_t index, uint8_t subidx, int nSDOType, string &Value)
{

    m_stSDOWorkState.bExecute = true;
    int nSDOReadState = ReadSdoFromSlave(nSlave, 
                                    index, 
                                    subidx, 
                                    nSDOType, 
                                    &Value);
    if(m_stSDOWorkState.bDone)
    {
        m_stSDOWorkState.bDone = false;
    }
    return nSDOReadState;

}

int LLC_Rbt::WriteSDO(int nSlave, uint16_t index, uint8_t subidx, int nSDOType, string Value)
{

     m_stSDOWorkState.bExecute = true;
    int nSDOWriteState = WriteSdoToSlave(nSlave, 
                                    index,
                                    subidx, 
                                    nSDOType, 
                                    Value);
    if(m_stSDOWorkState.bDone)
    {
        m_stSDOWorkState.bDone = false;
    }
    return nSDOWriteState;
}

int LLC_Rbt::WritePDO(int nSlave, uint16_t index, uint8_t subidx, int Value)
{
    //状态保护
    if(m_stShmRobotRTStatus.stECATStatus.nECATStatusMachine < enCDSState_WatchDog)
    {
        return REC_TimeOutError;
    }
    if(nSlave > m_vSlaves.size())
    {
        return REC_ParametersError;
    }
    return m_vSlaves[nSlave - 1]->WritePDO(index, subidx, Value);
}

int LLC_Rbt::FOEwrite(int nSlave, const std::string& strFilePath, const std::string& strFoEFileName, uint32_t nPassword)
{
    if (!m_stEtherCATMasterControl.pFoEWrite || !m_stEtherCATMasterControl.pFoEWrite(nSlave,strFilePath,strFoEFileName,nPassword)) { m_bFoEWorking=false; return REC_ControllerNotInit; }
    return REC_Successed;
}

int LLC_Rbt::UpdateESI(int nSlave,const std::string& strFilePath)
{
    if (!m_stEtherCATMasterControl.m_pFoEWriteESI || !m_stEtherCATMasterControl.m_pFoEWriteESI(nSlave,strFilePath)) return REC_ParametersError;
    return REC_Successed;
}

int LLC_Rbt::GetFOEwriteProgress(double& dProgress)
{
    dProgress = m_dFOEworkProgress.load(std::memory_order_relaxed);
    return REC_Successed;
}
//FOE读取
int LLC_Rbt::FOEread(int nSlave,const std::string& strFilePath, const std::string& strFoEFileName, uint32_t nPassword)
{
    if (!m_stEtherCATMasterControl.pFoERead || !m_stEtherCATMasterControl.pFoERead(nSlave,strFilePath,strFoEFileName,nPassword)) { m_bFoEWorking=false; return REC_ControllerNotInit; }
    return REC_Successed;
}
//升级进度
int LLC_Rbt::GetFOEreadProgress(double& dProgress)
{
    dProgress = m_dFOEworkProgress.load(std::memory_order_relaxed);
    return REC_Successed;
}

bool LLC_Rbt::CheckSlaveConfigConsistency()
{
    if(m_stRbtEtherCATParams.bSimulationType)
    {
        return true;
    }
    vector<SlaveInfo> vSlaveInfo;
    if (ScanSlave(vSlaveInfo) != REC_Successed) return false;
    if(vSlaveInfo.size() != m_vSlaveConfigParam.size())
    {
        printf("从站数量与实际不一致！配置数量：%d,实际数量：%d\n",m_vSlaveConfigParam.size(),vSlaveInfo.size());
        return false;
    }
    for (size_t i = 0; i < vSlaveInfo.size(); i++)
    {
        if((vSlaveInfo[i].nProductCode!=m_vSlaveConfigParam[i].nProductCode)||(vSlaveInfo[i].nRevisionNo!=m_vSlaveConfigParam[i].nRevisionNo))
        {
            printf("从站配置与实际不一致！从站ID：%d,配置ProductCode：%d,实际ProductCode：%d,配置RevisionNo：%d,实际RevisionNo：%d\n"
                    ,i+1,m_vSlaveConfigParam[i].nProductCode,vSlaveInfo[i].nProductCode,m_vSlaveConfigParam[i].nRevisionNo,vSlaveInfo[i].nRevisionNo);
            return false; 
        }
    }
    return true;
}