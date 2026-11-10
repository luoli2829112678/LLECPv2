#include "LLC_Rbt.h"
#include "LL_CpsService.h"
#include "LLC_Rbt_Bridge.h"
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace
{
bool ValidAxis(int axisId)
{
    return LLCRbt && axisId >= 0 && axisId < MaxAxisNum &&
           static_cast<size_t>(axisId) < LLCRbt->m_vAxis.size() &&
           static_cast<uint32_t>(axisId) < LLCRbt->m_nConfiguredAxisNum &&
           LLCRbt->m_vAxis[axisId] != nullptr;
}
void CheckResult(int result, const char* operation)
{
    if (result != REC_Successed)
        throw std::runtime_error(std::string(operation) + ": result=" + std::to_string(result));
}
void CheckSlave(int slave, int index, int subindex)
{
    if (!LLCRbt || slave < 1 || static_cast<size_t>(slave) > LLCRbt->m_vSlaves.size() ||
        !LLCRbt->m_vSlaves[slave - 1] || index <= 0 || index > 0xffff ||
        subindex < 0 || subindex > 0xff)
        throw std::invalid_argument("invalid slave/PDO address");
}
}
int LLC_Test(int param)
{
    const auto now = std::chrono::system_clock::now();
    const auto timestampUs = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
    std::printf("[时间=%lld us] LLC_test%d\n", static_cast<long long>(timestampUs), param);
    return 0;
}

//轴控制

int LLC_AxisEnable(int axisId)
{
    // const auto now = std::chrono::system_clock::now();
    // const auto timestampUs = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
    // std::printf("[时间=%lld us] LLC_AxisEnable axisId=%d\n", static_cast<long long>(timestampUs), axisId);
    axisId=LL_CpsService::Get().ResolveAxis(axisId);
    if (!ValidAxis(axisId)) return REC_ParametersError;
    std::vector<int> axes{axisId};
    return LLCRbt->AxisEnable(axes);
}

int LLC_AxisDisable(int axisId)
{
    axisId=LL_CpsService::Get().ResolveAxis(axisId);
    if (!ValidAxis(axisId)) return REC_ParametersError;
    std::vector<int> axes{axisId};
    return LLCRbt->AxisDisable(axes);
}
int LLC_AxisReset(int axisId)
{
    axisId=LL_CpsService::Get().ResolveAxis(axisId);
    if (!ValidAxis(axisId)) return REC_ParametersError;
    std::vector<int> axes{axisId};
    return LLCRbt->AxisReset(axes);
}
//轴运动
int LLC_AxisStop(int axisId)
{
    axisId=LL_CpsService::Get().ResolveAxis(axisId);
    if (!ValidAxis(axisId)) return REC_ParametersError;
    std::vector<int> axes{axisId};
    return LLCRbt->AxisStop(axes);
}
int LLC_AxisStartAnalogFocusing(int axisId)
{
    axisId=LL_CpsService::Get().ResolveAxis(axisId);
    if (!ValidAxis(axisId)) return REC_ParametersError;
    return LLCRbt->StartAnalogFocusing(axisId);
}

int LLC_AxisStopAnalogFocusing(int axisId)
{
    axisId=LL_CpsService::Get().ResolveAxis(axisId);
    if (!ValidAxis(axisId)) return REC_ParametersError;
    return LLCRbt->StopAnalogFocusing(axisId);
}

int LLC_AxisStartPEG(int axisId)
{
    axisId=LL_CpsService::Get().ResolveAxis(axisId);
    if (!ValidAxis(axisId)) return REC_ParametersError;
    return LLCRbt->StartPEG(axisId);
}

int LLC_AxisStopPEG(int axisId)
{
    axisId=LL_CpsService::Get().ResolveAxis(axisId);
    if (!ValidAxis(axisId)) return REC_ParametersError;
    return LLCRbt->StopPEG(axisId);
}

int LLC_AxisAbsMove(int axisId,double pos,double vel,double acc,double dec,double jerk)
{
    axisId=LL_CpsService::Get().ResolveAxis(axisId);
    if (!ValidAxis(axisId)) return REC_ParametersError;
    return LLCRbt->AxisAbsMove(axisId,pos,vel,acc,dec,jerk);
}
int LLC_AxisRelMove(int axisId,double pos,double vel,double acc,double dec,double jerk)
{
    axisId=LL_CpsService::Get().ResolveAxis(axisId);
    if (!ValidAxis(axisId)) return REC_ParametersError;
    return LLCRbt->AxisRelMove(axisId,pos,vel,acc,dec,jerk);
}
int LLC_AxisVelocityMove(int axisId,double vel,double acc,double dec,double jerk)
{
    axisId=LL_CpsService::Get().ResolveAxis(axisId);
    if (!ValidAxis(axisId)) return REC_ParametersError;
    if (!std::isfinite(vel)) return REC_ParametersError;
    const int direction = static_cast<int>(vel < 0 ? EN_Direction::enNegative : EN_Direction::enPositive);
    return LLCRbt->AxisVelocityMove(axisId,direction,std::fabs(vel),acc,dec,jerk);
}
int LLC_AxisMoveSet(int axisId)
{
    axisId=LL_CpsService::Get().ResolveAxis(axisId);
    if (!ValidAxis(axisId)) return REC_ParametersError;
    std::vector<int> axes{axisId};
    return LLCRbt->SetAxisMove(axes);
}

//轴设置
int LLC_AxisSetMovePos(int axisId,double pos)
{
    axisId=LL_CpsService::Get().ResolveAxis(axisId);
    if (!ValidAxis(axisId)) return REC_ParametersError;
    return LLCRbt->SetAxisMovePos(axisId,pos);
}


//轴读取
int LLC_AxisReadState(int axisId)
{
    axisId=LL_CpsService::Get().ResolveAxis(axisId);
    if (!ValidAxis(axisId)) return REC_ParametersError;
    return LLCRbt->AxisReadState(axisId);
}

int LLC_AxisSetMoveVel(int axisId, double vel)
{
    axisId=LL_CpsService::Get().ResolveAxis(axisId);
    if (!ValidAxis(axisId)) return REC_ParametersError;
    return LLCRbt->SetAxisMoveVel(axisId, vel);
}

int LLC_AxisSetMoveAcc(int axisId, double acc)
{
    axisId=LL_CpsService::Get().ResolveAxis(axisId);
    if (!ValidAxis(axisId)) return REC_ParametersError;
    return LLCRbt->SetAxisMoveAcc(axisId, acc);
}

int LLC_AxisSetMoveDec(int axisId, double dec)
{
    axisId=LL_CpsService::Get().ResolveAxis(axisId);
    if (!ValidAxis(axisId)) return REC_ParametersError;
    return LLCRbt->SetAxisMoveDec(axisId, dec);
}

int LLC_AxisSetMoveJerk(int axisId, double jerk)
{
    axisId=LL_CpsService::Get().ResolveAxis(axisId);
    if (!ValidAxis(axisId)) return REC_ParametersError;
    return LLCRbt->SetAxisMoveJerk(axisId, jerk);
}

int LLC_AxisEnableChecked(int axisId)
{
    CheckResult(LLC_AxisEnable(axisId), "AXIS_ENABLE");
    return REC_Successed;
}
int LLC_AxisDisableChecked(int axisId)
{
    CheckResult(LLC_AxisDisable(axisId), "AXIS_DISABLE");
    return REC_Successed;
}
double LLC_AxisGetActPos(int axisId)
{
    axisId=LL_CpsService::Get().ResolveAxis(axisId);
    if (!ValidAxis(axisId)) throw std::invalid_argument("AXIS_GETACTPOS: invalid axis");
    return LLCRbt->m_vAxis[axisId]->Axis_GetActualPosition();
}





int32_t LLC_SlavePDORead(int slave, int index, int subindex)
{
    CheckSlave(slave, index, subindex);
    const auto* pdos = LLCRbt->m_vSlaves[slave - 1]->GetTXPDOList();
    for (const auto& pdo : *pdos)
    {
        for (const auto& entry : pdo.entries)
        {
            if (entry.index != index || entry.subIndex != subindex) continue;
            if (!entry.pDataPtr || entry.bitLen <= 0 || entry.bitLen > 32 ||
                entry.bitOffsetInByte < 0 || entry.bitOffsetInByte > 7)
                throw std::runtime_error("SLAVE_PDO_READ: invalid/unmapped PDO (1..32 bits required)");
            const auto* bytes = static_cast<const uint8_t*>(entry.pDataPtr);
            uint32_t raw = 0;
            for (unsigned bit = 0; bit < static_cast<unsigned>(entry.bitLen); ++bit)
            {
                const unsigned source = static_cast<unsigned>(entry.bitOffsetInByte) + bit;
                raw |= static_cast<uint32_t>((bytes[source / 8] >> (source % 8)) & 1U) << bit;
            }
            // Raw bits: smaller PDOs are zero-extended; full DINT preserves its sign bit.
            int32_t value;
            std::memcpy(&value, &raw, sizeof(value));
            return value;
        }
    }
    throw std::runtime_error("SLAVE_PDO_READ: TX PDO not found");
}
int LLC_SlavePDOWrite(int slave, int index, int subindex, int32_t value)
{
    // 参数不合法时返回错误码，不抛异常。
    if (!LLCRbt || slave < 1 || static_cast<size_t>(slave) > LLCRbt->m_vSlaves.size())
    {
        return REC_ParametersError;
    }

    // 从站对象不存在，或者索引、子索引超出范围。
    if (!LLCRbt->m_vSlaves[slave - 1] || index <= 0 || index > 0xffff || subindex < 0 || subindex > 0xff)
    {
        return REC_ParametersError;
    }

    // 原样返回底层写入结果，不再调用CheckResult。
    return LLCRbt->WritePDO(slave, static_cast<uint16_t>(index), static_cast<uint8_t>(subindex), value);
}
