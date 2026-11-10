// 此文件由CMake自动生成，请勿手动修改。
#pragma once
#include <cstdio>

#include "AxisGroupAlgorithmBase.h"
#include "AxisGroupAlgorithmDef.h"
#include "AxisGroupAlgorithm_XY.h"
#include "MotionAlgorithm/MotionAlgorithm.h"
#include "MotionAlgorithm/MotionDefine.h"

// 根据轴组模型创建算法对象，调用方负责释放返回的对象
inline AxisGroupAlgorithmBase* GenerativeModelAlgorithm(ENAxisGroupModel enGroupModel, ST_AxisGroupDH stDH)
{
    // 初始化算法对象指针
    AxisGroupAlgorithmBase* pAxisGroupAlgorithm = nullptr;

    // 暂未接入DH配置,显式标记参数已处理,避免未使用参数警告
    (void)stDH;

    // 根据轴组模型选择算法
    switch (enGroupModel)
    {
    case AxisGroupModel_null:
        // 未配置有效模型
        std::printf("无效的轴组模型\n");
        break;

    case AxisGroupModel_xy_Cartesian:
        pAxisGroupAlgorithm = new AxisGroupAlgorithm_XY_Cartesian();
        break;

    default:
        // 当前尚未支持的模型
        std::printf("不支持的轴组模型\n");
        break;
    }

    // 返回创建的算法对象,未创建成功时返回nullptr
    return pAxisGroupAlgorithm;
}
