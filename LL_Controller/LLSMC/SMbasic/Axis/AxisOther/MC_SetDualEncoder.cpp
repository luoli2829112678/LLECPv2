#include"MC_SetDualEncoder.h"
#include <cmath>
#include <stdint.h>

namespace
{
bool IsValidMotorScale(const ST_SMCAxisConfiguration& stAxisConfiguration)
{
    return (stAxisConfiguration.nEncodeRatio != 0) &&
           (stAxisConfiguration.dGearRatio != 0.0) &&
           (stAxisConfiguration.nEncodeDirection != 0) &&
           (stAxisConfiguration.enAxisMoveDef != en_NullAxisType);
}

double MotorEncoderToJointDegree(int32_t nEncoderDelta, const ST_SMCAxisConfiguration& stAxisConfiguration)
{
    return stAxisConfiguration.nEncodeDirection *
           static_cast<double>(nEncoderDelta) *
           stAxisConfiguration.enAxisMoveDef /
           (static_cast<double>(stAxisConfiguration.nEncodeRatio) * stAxisConfiguration.dGearRatio);
}

double JointEncoderToJointDegree(int32_t nJointEncoder,
                                 const ST_AxisDualEncoderConfiguration& stDualEncoderConfiguration,
                                 const ST_SMCAxisConfiguration& stAxisConfiguration)
{
    return stDualEncoderConfiguration.nJointSideEncoderDirection *
           (static_cast<double>(nJointEncoder) -
            static_cast<double>(stDualEncoderConfiguration.nJointHomeEncoder)) *
           stAxisConfiguration.enAxisMoveDef /
           static_cast<double>(stDualEncoderConfiguration.nJointSideEncoderRatio);
}

int32_t CalculateMotorHomeEncoder(int32_t nActualPosition,
                                  double dJointDegree,
                                  const ST_SMCAxisConfiguration& stAxisConfiguration)
{
    const double dMotorEncoderDelta =
        dJointDegree *
        (static_cast<double>(stAxisConfiguration.nEncodeRatio) * stAxisConfiguration.dGearRatio) /
        (stAxisConfiguration.nEncodeDirection * stAxisConfiguration.enAxisMoveDef);

    return nActualPosition - static_cast<int32_t>(std::round(dMotorEncoderDelta));
}
}

MC_SetDualEncoder::MC_SetDualEncoder(/* args */)
{
}

MC_SetDualEncoder::~MC_SetDualEncoder()
{
}

void MC_SetDualEncoder::operator()(CIA402Axis* axis)
{
    m_pCIA402Axis = axis;
    this->Execute();
}

void MC_SetDualEncoder::operator()(CIA402Axis* axis,bool bExecute,
        ST_AxisDualEncoderConfiguration stAxisDualEncoderConfiguration,
        bool &bDone, bool &bError,int &nErrorID)
{
    m_bExecute = bExecute;

    if(!bExecute)
    {
        m_bBusy = false;
        m_bError = false;
        m_bDone = false;
        m_nErrorID = SMEC_SUCCESSED;
        bDone = m_bDone;
        bError = m_bError;
        nErrorID = m_nErrorID;
        return;
    }

    if(nullptr == axis)
    {
        m_bBusy = false;
        m_bError = true;
        m_bDone = false;
        m_nErrorID = SMEC_INVALID_AXIS;
        bDone = m_bDone;
        bError = m_bError;
        nErrorID = m_nErrorID;
        return;
    }

    if(m_bBusy && (nullptr != m_pCIA402Axis) && (axis != m_pCIA402Axis))
    {
        m_bBusy = false;
        m_bError = true;
        m_bDone = false;
        m_nErrorID = SMEC_ABNORMAL_AXIS_CHANGE;
        bDone = m_bDone;
        bError = m_bError;
        nErrorID = m_nErrorID;
        return;
    }

    m_pCIA402Axis = axis;
    m_stAxisDualEncoderConfiguration = stAxisDualEncoderConfiguration;
    // bDualEncoder 是运行时开关，应由编码器类型统一推导，避免入参默认值覆盖掉开关。
    m_stAxisDualEncoderConfiguration.bDualEncoder =
        (m_stAxisDualEncoderConfiguration.nEncoderType != enEncoderType_SingleEncoder);
    m_pCIA402Axis->m_stDualEncoderConfiguration = m_stAxisDualEncoderConfiguration;

    this->Execute();
    bDone = m_bDone;
    bError = m_bError;
    nErrorID = m_nErrorID;
}


void MC_SetDualEncoder::Execute()
{
    if (nullptr == m_pCIA402Axis)
    {
        m_bBusy = false;
        m_bError = true;
        m_bDone = false;
        m_nErrorID = SMEC_INVALID_AXIS;
        return;
    }

    if (m_pCIA402Axis->Axis_GetVirtualState())
    {
        m_bBusy = false;
        m_bError = false;
        m_bDone = true;
        m_nErrorID = SMEC_SUCCESSED;
        return;
    }

    ST_SMCAxisConfiguration& stAxisConfiguration = m_pCIA402Axis->m_stAxisConfiguration;
    ST_AxisDualEncoderConfiguration& stDualEncoderConfiguration = m_pCIA402Axis->m_stDualEncoderConfiguration;
    ST_AxisDualEncoderData& stDualEncoderData = m_pCIA402Axis->m_stAxisDualEncoderData;

    if (!IsValidMotorScale(stAxisConfiguration))
    {
        m_bBusy = false;
        m_bError = true;
        m_bDone = false;
        m_nErrorID = SMEC_INVALID_AXIS;
        return;
    }

    m_bBusy = true;
    m_bError = false;
    m_bDone = false;
    m_nErrorID = SMEC_SUCCESSED;

    // 先重建当前双编/单圈多圈值；后续零点计算只使用实时成员，不再依赖 m_stAxisData 的旧快照。
    m_pCIA402Axis->Axis_DualEncoderCalculateAxisPosition();

    const int32_t nActualPosition = m_pCIA402Axis->Axis_GetActualPositionPDO();
    double dJointSideDegree = 0.0;

    switch (stDualEncoderConfiguration.nEncoderType)
    {
    case enEncoderType_SingleEncoder:
        // E 系列单编码器不做双编补偿，保持原本电机侧零点，只计算当前关节角用于统一日志。
        stDualEncoderData.nCalculateHomeEncoder = stAxisConfiguration.nEncodeHomePos;
        dJointSideDegree = MotorEncoderToJointDegree(
            nActualPosition - stAxisConfiguration.nEncodeHomePos,
            stAxisConfiguration);
        break;

    case enEncoderType_SingleEncoderOutput:
    case enEncoderType_DualEncoder:
    case enEncoderType_DualEncoder_:
        if ((stDualEncoderConfiguration.nJointSideEncoderRatio <= 0) ||
            (stDualEncoderConfiguration.nJointSideEncoderDirection == 0))
        {
            m_bBusy = false;
            m_bError = true;
            m_bDone = false;
            m_nErrorID = SMEC_INVALID_AXIS;
            return;
        }
        dJointSideDegree = JointEncoderToJointDegree(
            stDualEncoderData.nRealTimeCalculateJointEncode,
            stDualEncoderConfiguration,
            stAxisConfiguration);
        stDualEncoderData.nCalculateHomeEncoder = CalculateMotorHomeEncoder(
            nActualPosition,
            dJointSideDegree,
            stAxisConfiguration);
        break;

    case enEncoderType_singleLap:
        // single-lap 已在 Axis_DualEncoderCalculateAxisPosition 中恢复输入侧多圈值。
        dJointSideDegree = MotorEncoderToJointDegree(
            stDualEncoderData.nRealTimeCalculateJointEncode - stAxisConfiguration.nEncodeHomePos,
            stAxisConfiguration);
        stDualEncoderData.nCalculateHomeEncoder = CalculateMotorHomeEncoder(
            nActualPosition,
            dJointSideDegree,
            stAxisConfiguration);
        break;

    default:
        m_bBusy = false;
        m_bError = true;
        m_bDone = false;
        m_nErrorID = SMEC_INVALID_AXIS;
        return;
    }

    // 通知实时周期把当前恢复后的多圈编码器值保存到 DCS，下一次上电可用它恢复圈号。
    stDualEncoderConfiguration.bIsSaveDoubleEncoder =
        (stDualEncoderConfiguration.nEncoderType != enEncoderType_SingleEncoder);
    stDualEncoderConfiguration.bDualEncoder =
        (stDualEncoderConfiguration.nEncoderType != enEncoderType_SingleEncoder);

    printf("------------------------------SetActPosition:%f--------------------\n",dJointSideDegree);
    m_pCIA402Axis->Axis_SetEncodeHomePos(stDualEncoderData.nCalculateHomeEncoder);

    m_pCIA402Axis->m_stAxisData.stAxisConfig.stAxisConfiguration = stAxisConfiguration;
    m_pCIA402Axis->m_stAxisData.stAxisConfig.stDualEncoderConfiguration = stDualEncoderConfiguration;
    m_pCIA402Axis->m_stAxisData.stRealTimeData.stAxisDualEncoderData = stDualEncoderData;

    m_bBusy = false;
    m_bError = false;
    m_bDone = true;
    m_nErrorID = SMEC_SUCCESSED;
    return;
}
