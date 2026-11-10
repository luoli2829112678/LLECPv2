#include"CIA402Axis.h"
#include <cmath>
#include <stdint.h>

namespace
{
int32_t NormalizeEncoderLowBit(int32_t nEncoderValue, int32_t nEncoderRatio)
{
    if (nEncoderRatio <= 0)
    {
        return 0;
    }

    int32_t nLowBit = nEncoderValue % nEncoderRatio;
    if (nLowBit < 0)
    {
        nLowBit += nEncoderRatio;
    }
    return nLowBit;
}

int32_t BuildNearestMultiTurnEncoder(int32_t nSavedEncoder, int32_t nLowBit, int32_t nEncoderRatio)
{
    if (nEncoderRatio <= 0)
    {
        return nSavedEncoder;
    }

    // 用保存到 DCS 的多圈值确定当前圈号，再在邻近圈内找距离最近的组合值。
    // 这里必须用浮点除法；原来的整数除法会把负值和小数圈截断，导致零点偏移。
    const double dSavedTurns = static_cast<double>(nSavedEncoder) / static_cast<double>(nEncoderRatio);
    const int nBaseTurn = static_cast<int>(std::floor(dSavedTurns));

    int32_t nBestEncoder = nBaseTurn * nEncoderRatio + nLowBit;
    double dBestDistance = std::fabs(static_cast<double>(nBestEncoder) - static_cast<double>(nSavedEncoder));

    for (int nTurn = nBaseTurn - 1; nTurn <= nBaseTurn + 2; ++nTurn)
    {
        const int32_t nCandidate = nTurn * nEncoderRatio + nLowBit;
        const double dDistance = std::fabs(static_cast<double>(nCandidate) - static_cast<double>(nSavedEncoder));
        if (dDistance < dBestDistance)
        {
            nBestEncoder = nCandidate;
            dBestDistance = dDistance;
        }
    }

    return nBestEncoder;
}
}

int CIA402Axis::Axis_DualEncoderCalculateAxisPosition()
{
    if (m_bVirtual)
    {
        return AEC_SUCCESSED;
    }

    const int nEncoderType = m_stDualEncoderConfiguration.nEncoderType;
    int32_t nEncoderRatio = 0;
    int32_t nLowBit = 0;
    int32_t nRealtimeEncoder = 0;

    switch (nEncoderType)
    {
    case enEncoderType_SingleEncoder:
        // 单编码器不需要重建多圈值，只保持零点数据与轴配置一致，避免配置双编时改坏 E 系列零点。
        m_stAxisDualEncoderData.nCalculateHomeEncoder = m_stAxisConfiguration.nEncodeHomePos;
        if (nullptr != m_st_map.pActualPosition)
        {
            m_stAxisDualEncoderData.nRealTimeCalculateJointEncode = *m_st_map.pActualPosition;
        }
        break;

    case enEncoderType_SingleEncoderOutput:
        // 输出侧单圈编码器由电机实际位置提供低位，用 DCS 保存值恢复多圈。
        if ((nullptr == m_st_map.pActualPosition) || (m_stDualEncoderConfiguration.nJointSideEncoderRatio <= 0))
        {
            return AEC_SUCCESSED;
        }
        nEncoderRatio = m_stDualEncoderConfiguration.nJointSideEncoderRatio;
        nLowBit = NormalizeEncoderLowBit(*m_st_map.pActualPosition, nEncoderRatio);
        nRealtimeEncoder = BuildNearestMultiTurnEncoder(m_stDualEncoderConfiguration.nDcsSaveJointEncoder,
                                                         nLowBit,
                                                         nEncoderRatio);
        m_stAxisDualEncoderData.nJointEncodeLowbit = nLowBit;
        m_stAxisDualEncoderData.nActMultiEncoder = nLowBit;
        m_stAxisDualEncoderData.nRealTimeCalculateJointEncode = nRealtimeEncoder;
        break;

    case enEncoderType_DualEncoder:
    case enEncoderType_DualEncoder_:
        // 真双编码器由关节侧 PDO 提供低位，用 DCS 保存值恢复关节侧多圈。
        if ((nullptr == m_st_map.pJointActPosition) || (m_stDualEncoderConfiguration.nJointSideEncoderRatio <= 0))
        {
            return AEC_SUCCESSED;
        }
        nEncoderRatio = m_stDualEncoderConfiguration.nJointSideEncoderRatio;
        nLowBit = NormalizeEncoderLowBit(*m_st_map.pJointActPosition, nEncoderRatio);
        nRealtimeEncoder = BuildNearestMultiTurnEncoder(m_stDualEncoderConfiguration.nDcsSaveJointEncoder,
                                                         nLowBit,
                                                         nEncoderRatio);
        m_stAxisDualEncoderData.nJointEncodeLowbit = nLowBit;
        m_stAxisDualEncoderData.nActMultiEncoder = nLowBit;
        m_stAxisDualEncoderData.nRealTimeCalculateJointEncode = nRealtimeEncoder;
        break;

    case enEncoderType_singleLap:
        // 输入侧 single-lap 编码器只可信任单圈低位，需要按电机编码器比例恢复输入侧多圈。
        if ((nullptr == m_st_map.pActualPosition) || (0 == m_stAxisConfiguration.nEncodeRatio))
        {
            return AEC_SUCCESSED;
        }
        nEncoderRatio = static_cast<int32_t>(m_stAxisConfiguration.nEncodeRatio);
        nLowBit = NormalizeEncoderLowBit(*m_st_map.pActualPosition, nEncoderRatio);
        nRealtimeEncoder = BuildNearestMultiTurnEncoder(m_stDualEncoderConfiguration.nDcsSaveJointEncoder,
                                                         nLowBit,
                                                         nEncoderRatio);
        m_stAxisDualEncoderData.nJointEncodeLowbit = nLowBit;
        m_stAxisDualEncoderData.nActMultiEncoder = nLowBit;
        m_stAxisDualEncoderData.nRealTimeCalculateJointEncode = nRealtimeEncoder;
        break;

    default:
        return AEC_SUCCESSED;
    }

    if (m_stDualEncoderConfiguration.bIsSaveDoubleEncoder)
    {
        m_stDualEncoderConfiguration.nDcsSaveJointEncoder =
            m_stAxisDualEncoderData.nRealTimeCalculateJointEncode;
    }

    m_stAxisData.stRealTimeData.stAxisDualEncoderData = m_stAxisDualEncoderData;
    m_stAxisData.stAxisConfig.stAxisConfiguration = m_stAxisConfiguration;
    m_stAxisData.stAxisConfig.stDualEncoderConfiguration = m_stDualEncoderConfiguration;
    return AEC_SUCCESSED;
}
