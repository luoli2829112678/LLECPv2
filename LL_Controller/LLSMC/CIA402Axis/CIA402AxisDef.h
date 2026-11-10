#pragma once
#include"../SMCAlgorithm/MotionAlgorithm/MotionAlgorithm.h"
#include <vector>
#include <stdint.h> 
#include <string.h>
#include<time.h>
#include <cstdlib>
#include <cstdio>
#include <math.h>
#include <functional>
#include <thread>       
#include <array>        
#include <algorithm>    
#include <fstream>      
#include <sstream>      
#include <iomanip>     
#include <chrono>       
#include <ctime>        
#include <string>       
#include <sys/stat.h>      // mkdir、stat、S_ISREG
#include <sys/types.h>     // 系统类型
#include <dirent.h>        // opendir、readdir、closedir
#include <unistd.h>        // unlink
#include <cerrno>          // errno
#include <cstring>         // strcmp
#include <memory>          // shared_ptr、make_shared


struct ST_CIA402_PDO
{
    uint16_t   Controlword;
    int32_t    TargetPosition;
    int32_t    TargetVelocity;
    int16_t    TargetTorque;
    uint8_t    TargetModesOfOperation;
    uint32_t   DigitalOutputs;
    uint32_t   TouchProbeFunction;
    int32_t   AddVelocityValue;
    int16_t   AddTorqueValue;

    uint16_t   StatusWord;
    int32_t    ActualPosition;
    int32_t    ActualVelocity;
    int16_t    ActualTorque;
    uint8_t    ActualModesOfOperation;
    uint32_t   TouchProbeStatus;
    uint32_t   DigitalInputs;
    uint16_t   ErrorCode;
    //双编码器
    int32_t   JointActPosition;
    ST_CIA402_PDO()
        : Controlword(0),
          TargetPosition(0),
          TargetVelocity(0),
          TargetTorque(0),
          TargetModesOfOperation(0),
          DigitalOutputs(0),
          TouchProbeFunction(0),
          AddVelocityValue(0),
          AddTorqueValue(0),
          StatusWord(0),
          ActualPosition(0),
          ActualVelocity(0),
          ActualTorque(0),
          ActualModesOfOperation(0),
          TouchProbeStatus(0),
          DigitalInputs(0),
          ErrorCode(0),
          JointActPosition(0)
    {}
};

//该映射遵照CanOpen402协议
struct ST_SMCInitMap
{
    uint16_t*   pControlword;
    uint16_t*   pStatusWord;
    uint16_t*   pErrorCode;
    int32_t*    pTargetPosition;
    int32_t*    pTargetVelocity;
    int16_t*    pTargetTorque;
    uint8_t*    pTargetModesOfOperation;
    int32_t*    pActualPosition;
    int32_t*    pActualVelocity;
    int16_t*    pActualTorque;
    uint8_t*    pActualModesOfOperation;
    uint32_t*   pDigitalInputs;
    uint32_t*   pDigitalOutputs;  
    
    uint16_t*   pTouchProbeFunction;
    int32_t*   pAddVelocityValue;
    int16_t*   pAddTorqueValue;
    uint16_t*   pTouchProbeStatus;

    int32_t*    pJointActPosition;
    ST_SMCInitMap()
    {
        pControlword = nullptr;
        pStatusWord = nullptr;
        pErrorCode = nullptr;
        pTargetPosition = nullptr;
        pTargetVelocity = nullptr;
        pTargetTorque = nullptr;
        pTargetModesOfOperation = nullptr;
        pActualPosition = nullptr;
        pActualVelocity = nullptr;
        pActualTorque = nullptr;
        pActualModesOfOperation = nullptr;
        pDigitalInputs = nullptr;
        pDigitalOutputs = nullptr;
        pTouchProbeFunction = nullptr;
        pAddVelocityValue = nullptr;
        pAddTorqueValue = nullptr;
        pTouchProbeStatus = nullptr;
        pJointActPosition = nullptr;
    }
};

typedef enum
{
    en_ControlWord_Init                     = 0, 
    en_ControlWord_so                       = 1 << 0,  
    en_ControlWord_ev                       = 1 << 1,
    en_ControlWord_qs                       = 1 << 2,
    en_ControlWord_eo                       = 1 << 3,
    en_ControlWord_oms0                     = 1 << 4,
    en_ControlWord_oms1                     = 1 << 5,
    en_ControlWord_oms2                     = 1 << 6,
    en_ControlWord_fr                       = 1 << 7,
    en_ControlWord_h                        = 1 << 8,
    en_ControlWord_oms3                     = 1 << 9,
    en_ControlWord_r                        = 1 << 10,
    en_ControlWord_ms1                      = 1 << 11,
    en_ControlWord_ms2                      = 1 << 12,
    en_ControlWord_ms3                      = 1 << 13,
    en_ControlWord_ms4                      = 1 << 14,
    en_ControlWord_ms5                      = 1 << 15,

} EN_SMC_ControlWord;

typedef enum// : EC_T_USHORT
{
	en_StatusWord_rtso                      = 1 << 0,
	en_StatusWord_so                        = 1 << 1,
	en_StatusWord_oe                        = 1 << 2,
	en_StatusWord_f                         = 1 << 3,
	en_StatusWord_ve                        = 1 << 4,
	en_StatusWord_qs                        = 1 << 5,
	en_StatusWord_sod                       = 1 << 6,
	en_StatusWord_w                         = 1 << 7,
	en_StatusWord_ms                        = 1 << 8,
	en_StatusWord_rm                        = 1 << 9,
	en_StatusWord_tr                        = 1 << 10,
	en_StatusWord_ila                       = 1 << 11,
	en_StatusWord_oms1                      = 1 << 12,
	en_StatusWord_oms2                      = 1 << 13,
	en_StatusWord_ms1                       = 1 << 14,
	en_StatusWord_ms2                       = 1 << 15,
} EN_SMC_StatusWord;




// 运动方向
enum EN_Direction
{
    enPositive = 1,   // 正方向
    enNegative = -1,   // 负方向
    //enTorque  = 2    // 保持当前方向（有些厂商支持）
};

//运动模式
enum EN_AxisMoveType
{
    enMoveTypeNull,
    enAbsoluteMotion,
    enRelativMotion,
};

// 缓冲模式
enum EN_BufferMode
{
    enAborting = 0,   // 立即中断当前运动，执行新指令
    enBuffered = 1,   // 缓冲，等前一运动完成后再执行
    enBlendingLow = 2, // 融合过渡，低优先级平滑衔接
    enBlendingHigh = 3 // 融合过渡，高优先级平滑衔接
};

struct ST_PlanningMotionParam
{

    double tar;
    double tar_dot;
    double ptar_ddot;
    double ntar_ddot;
    double tar_dddot;
    double tar_ddddot;
    EN_PlanningMode PlanningMode;
    EN_Direction  Direction;
    // == 运算符重载
    bool operator==(const ST_PlanningMotionParam& other) const
    {
        auto eq = [](double a, double b) { return std::fabs(a - b) < 1e-5;};

        return eq(tar,  other.tar)  &&
               eq(tar_dot,  other.tar_dot)  &&
               eq(ptar_ddot,  other.ptar_ddot)  &&
               eq(ntar_ddot,  other.ntar_ddot)  &&
               eq(tar_dddot, other.tar_dddot) &&
               eq(tar_ddddot, other.tar_ddddot) &&
               (PlanningMode == other.PlanningMode) &&
               (Direction    == other.Direction);
    }
    bool operator!=(const ST_PlanningMotionParam& other) const
    {
        return !(*this == other);
    }
    ST_PlanningMotionParam()
        : tar(0.0),
          tar_dot(0.0),
          ptar_ddot(0.0),
          ntar_ddot(0.0),
          tar_dddot(0.0),
          tar_ddddot(1e8),
          PlanningMode{},
          Direction{}
    {}
};



// 单个电机运动单元
struct ST_MotorMotionUnit
{
    double dPosition;        // 位置
    double dVelocity;        // 速度
    double dAcceleration;    // 加速度
    double dJerk;            // 加加速度
    double dSnap;            // 加加加速度

    double dTorque;          // 力矩
    double dTorqueDot;       // 力矩一阶导数
    double dTorqueDdot;      // 力矩二阶导数
    double dTorqueDddot;     // 力矩三阶导数
    double dTorqueDdddot;    // 力矩四阶导数

    // 默认构造函数
    ST_MotorMotionUnit()
        : dPosition(0.0),
          dVelocity(0.0),
          dAcceleration(0.0),
          dJerk(0.0),
          dSnap(0.0),
          dTorque(0.0),
          dTorqueDot(0.0),
          dTorqueDdot(0.0),
          dTorqueDddot(0.0),
          dTorqueDdddot(0.0)
    {
    }
};

// 电机运动数据：命令值 + 实际值
struct ST_MotorMotionData
{
    ST_MotorMotionUnit stCommand;   // 命令运动单元
    ST_MotorMotionUnit stActual;    // 实际运动单元

    // 默认构造函数
    ST_MotorMotionData()
        : stCommand(),
          stActual()
    {
    }
};


//前馈
struct ST_Feedforward
{
    double dVelocityFeedforward;
    double dAccelerationFeedforward;
    double dJerkFeedforward;
    ST_Feedforward():dVelocityFeedforward(1.0),dAccelerationFeedforward(1.0),dJerkFeedforward(1.0){}
};

struct ST_MotionUint
{
    ST_PlanningMotionParam  PlanningMotionParam;
    EN_BufferMode BufferMode;
    EN_AxisMoveType MoveType;
    void* fbID;//功能块指针
    //回调函数
    std::function<void(bool, bool, bool, bool, int)> fCallback;
    bool bMotion;
    bool bMotionDone;
    void SetFBStauts(bool bBusy,bool bDone,bool bCommandAborted,bool bError,int nErrorID)
    {
        bMotionDone = bDone;
        bMotion = bBusy;
        if (fCallback) 
            fCallback(bBusy,bDone,bCommandAborted,bError,nErrorID);
        return;
    }
    ST_MotionUint():bMotionDone(false),bMotion(false){}
};

struct ST_Factor
{
    double dVelFactor;
    double dAccFactor;
    double dJerkFactor;
    ST_Factor()
    {
        dVelFactor = 1;
        dAccFactor = 1;
        dJerkFactor = 1;
    }
    // == 运算符重载
    bool operator==(const ST_Factor& other) const
    {
        auto eq = [](double a, double b) { return std::fabs(a - b) < 1e-9; };

        return eq(dVelFactor,  other.dVelFactor)  &&
               eq(dAccFactor,  other.dAccFactor)  &&
               eq(dJerkFactor,  other.dJerkFactor);
    }
    bool operator!=(const ST_Factor& other) const
    {
        return !(*this == other);
    }
};

struct ST_SoftMotionData
{
    //待运动点位
    std::vector<ST_MotionUint>vMotionUint;
    //运动参数
    ST_PlanningMotionParam stSoftMotionMotionParam;
    //运动时间帧
    double dMotionTime;
    //减速比 
    ST_Factor m_stFactor;
    //重新规划标志
    bool bReplan;
};

struct ST_RT_MotionPlanPointData
{
    double dStartPostion;
    double dTargetPostion;
    double dStartVelocity;
    double dTargetVelocity;
    double dMaxVel;
    double dMaxAcc;
    double dJerk;
    double dStartTime;
    double dEcatCycle;
    int nMotionFrameCounts;
    STMotionFrame stMotionFrame;
    ST_InterParams stStopMotionFrame;
    double dStopMotionTime;

    ST_RT_MotionPlanPointData()
    {
        dStartPostion = 0;
        dTargetPostion = 0;
        dStartVelocity = 0;
        dTargetVelocity = 0;
        dMaxVel = 0;
        dMaxAcc = 0;
        dJerk =0;
        dStartTime = 0;
        dEcatCycle = 0.001;
        nMotionFrameCounts = 0;
        dStopMotionTime = 0;

    }
};

enum EN_AxisMoveDef
{
    en_NullAxisType = 0,
    enLinearAxis = 1,
    enRotationAxis = 360,
};


struct ST_SMCAxisConfiguration
{
    //轴类型 直线/旋转
    EN_AxisMoveDef enAxisMoveDef;
    double dGearRatio;
    uint32_t nEncodeRatio;
    int nEncodeDirection;
    int nEncodeHomePos;
    double dTorqueScale;
    double dVelocityScale;
    int nTorqueDirection;
    double dPositive;
    double dNegative;
    ST_SMCAxisConfiguration()
    {
        enAxisMoveDef = enLinearAxis;
        dGearRatio = 1;
        nEncodeRatio = 131072;
        nEncodeDirection = 1;
        nEncodeHomePos = 0;
        dTorqueScale = 1;
        nTorqueDirection = 1;
        dNegative = 0;
        dPositive = 0;
    }
};

enum EN_EncoderType
{
    enEncoderType_SingleEncoder = 0,
    enEncoderType_SingleEncoderOutput = 1,
    enEncoderType_DualEncoder = 2,
    enEncoderType_DualEncoder_ = 3,
    enEncoderType_singleLap = 4,
};

struct ST_AxisDualEncoderConfiguration
{
    //双编码器
    bool bDualEncoder;
    int nEncoderType;
    bool bIsSaveDoubleEncoder;
    int32_t nJointHomeEncoder;
    int nJointSideEncoderDirection;
    int32_t nJointSideEncoderRatio;
    double dJointSideGearRatio;
    int32_t nDcsSaveJointEncoder;

    ST_AxisDualEncoderConfiguration()
        : nEncoderType(0),
          bDualEncoder(false),
          bIsSaveDoubleEncoder(false),
          nJointHomeEncoder(0),
          nJointSideEncoderDirection(0),
          nJointSideEncoderRatio(0),
          dJointSideGearRatio(0),
          nDcsSaveJointEncoder(0)
    {
    }
};

struct ST_AxisDualEncoderData
{
    int32_t nJointEncodeLowbit;
    int32_t nActMultiEncoder;
    double dJointActPos;
    int nRealTimeCalculateJointEncode;
    int32_t nCalculateHomeEncoder;

    ST_AxisDualEncoderData()
        : nJointEncodeLowbit(0),
          nActMultiEncoder(0),
          dJointActPos(0),
          nRealTimeCalculateJointEncode(0),
          nCalculateHomeEncoder(0)
    {
    }
};


enum EN_ModesOfOperation
{
    enModeManufacturerSpecificMin = -128,// 厂商自定义模式起始值

    enModeNoModeAssigned = 0,            // 无模式变化/未指定模式
    enModeProfilePosition = 1,           // 位置模式
    enModeVelocity = 2,                  // 速度模式
    enModeProfileVelocity = 3,           // 简单速度模式
    enModeProfileTorque = 4,             // 扭矩模式
    enModeReserved5 = 5,                 // 保留
    enModeHoming = 6,                    // 回零模式
    enModeInterpolatedPosition = 7,      // 插值位置模式
    enModeCyclicSyncPosition = 8,        // 同步位置模式（周期）
    enModeCyclicSyncVelocity = 9,        // 同步速度模式（周期）
    enModeCyclicSyncTorque = 10,         // 同步扭矩模式（周期）
    enModeCyclicSyncJointTorque = 11,    // 同步关节力矩模式(周期)

    enModeManufacturerSpecificMax = 127  // 厂商自定义最大值
    // 11 到 127 保留
};

//OpenPLC标准轴状态
enum EN_AxisMotionState
{
    motionState_power_off           = 0,  // Disabled
    motionState_errorstop           = 1,  // Error Stop
    motionState_stopping            = 2,  // Stopping
    motionState_standstill          = 3,  // Standstill
    motionState_discrete_motion     = 4,  // 单步运动
    motionState_continuous_motion   = 5,  // 连续运动
    motionState_synchronized_motion = 6,  // 同步运动
    motionState_homing              = 7   // 回零
};



struct ST_FrameData
{
    // ===== Position =====
    double dCmdPos;        // 命令位置
    double dActPos;        // 实际位置

    // ===== Velocity =====
    double dCmdVel;        // 命令速度
    double dActVel;        // 实际速度

    // ===== Acceleration =====
    double dCmdAcc;        // 命令加速度
    double dActAcc;        // 实际加速度

    // ===== Jerk (加加速度) =====
    double dCmdJerk;       // 命令加加速度
    double dActJerk;       // 实际加加速度

    // ===== Torque =====
    double dCmdTorque;    // 命令电流
    double dActTorque;    // 实际电流

    // ===== Torque 1st Derivative =====
    double dCmdTorque_d1; // 命令电流一阶导 (di/dt)
    double dActTorque_d1; // 实际电流一阶导

    // ===== Torque 2nd Derivative =====
    double dCmdTorque_d2; // 命令电流二阶导
    double dActTorque_d2; // 实际电流二阶导

    // ===== Torque 3rd Derivative =====
    double dCmdTorque_d3; // 命令电流三阶导
    double dActTorque_d3; // 实际电流三阶导

    ST_CIA402_PDO stPDO;
    ST_FrameData()
        : dCmdPos(0), dActPos(0),
          dCmdVel(0), dActVel(0),
          dCmdAcc(0), dActAcc(0),
          dCmdJerk(0), dActJerk(0),
          dCmdTorque(0), dActTorque(0),
          dCmdTorque_d1(0), dActTorque_d1(0),
          dCmdTorque_d2(0), dActTorque_d2(0),
          dCmdTorque_d3(0), dActTorque_d3(0)
    {}
};

class SMCTimer 
{
public:
    bool bInit_IN;
    long nInit_PT = 0;  // 以微秒为单位
    struct timespec nInit_ET = {0, 0};

	bool bLast_CLK = false;
public:
	// 定时器
    bool Ton(bool IN, int PT, int& ET) 
    {
        if (!IN)
        {
            ET = 0;
            bInit_IN = false;
            nInit_PT = 0;
            nInit_ET = {0, 0};
            return false;
        }
        // 检测上升沿
        if (IN && !bInit_IN)
        {
            nInit_PT = PT;
            clock_gettime(CLOCK_MONOTONIC, &nInit_ET);
        }
        if (IN)
        {
            bInit_IN = IN;
            struct timespec now;
            clock_gettime(CLOCK_MONOTONIC, &now);
            // 计算经过的时间（微秒）
            long elapsed = (now.tv_sec - nInit_ET.tv_sec) * 1000000 + 
                           (now.tv_nsec - nInit_ET.tv_nsec) / 1000;
            ET = elapsed;
            return ET > nInit_PT;
        }
        return false;
    }

	bool Ton(bool IN, int PT) 
    {
		int ET = 0;
        if (!IN)
        {
            bInit_IN = false;
            nInit_PT = 0;
            nInit_ET = {0, 0};
            return false;
        }
        // 检测上升沿
        if (IN && !bInit_IN)
        {
            nInit_PT = PT;
            clock_gettime(CLOCK_MONOTONIC, &nInit_ET);
        }
        if (IN)
        {
            bInit_IN = IN;
            struct timespec now;
            clock_gettime(CLOCK_MONOTONIC, &now);
            // 计算经过的时间（微秒）
            long elapsed = (now.tv_sec - nInit_ET.tv_sec) * 1000000 + 
                           (now.tv_nsec - nInit_ET.tv_nsec) / 1000;
            ET = elapsed;
            return ET > nInit_PT;
        }
        return false;
    }
	// 上升沿检测
	bool R_TRIG(bool CLK)
	{
		if (CLK && !bLast_CLK)
		{
			bLast_CLK = CLK;
			return true;
		}
		else
		{
			bLast_CLK = CLK;
			return false;
		}
	}
	// 下降沿检测
    bool F_TRIG(bool CLK)
    {
        if (!CLK && bLast_CLK) // 从 true 变为 false
        {
            bLast_CLK = CLK;
            return true;
        }
        else
        {
            bLast_CLK = CLK;
            return false;
        }
    }
};

struct ST_AxisStauts
{
    int nAxisID;
    EN_AxisMotionState  enAxisMotionState;
    bool bMoving;
    bool bError;
    bool bErrorID;
    bool bVirtual;
};

struct ST_AxisConfig
{
    //轴基本配置
    ST_SMCAxisConfiguration stAxisConfiguration;
    //轴前馈配置
    ST_Feedforward stFeedforward;
    //轴双编配置
    ST_AxisDualEncoderConfiguration stDualEncoderConfiguration;


};

struct ST_RealTimeData
{
    //402数据
    ST_AxisStauts stAxisStauts;
    //轴基础实时参数
    ST_FrameData stAxisData;
    //轴PDO实时参数
    ST_CIA402_PDO stPDOData;
    //轴双编实时数据
    ST_AxisDualEncoderData stAxisDualEncoderData;

};

struct ST_AxisData
{
    //轴配置
    ST_AxisConfig stAxisConfig;
    //轴实时数据
    ST_RealTimeData stRealTimeData;
};


struct ST_LogFileInfo
{
    std::string sFilePath;     // 日志文件完整路径
    time_t tModifyTime;        // 文件最后修改时间
};


inline bool CreateDirectoriesNoFilesystem(const std::string& sDirPath)
{
    if (sDirPath.empty())                              // 如果目录路径为空
    {
        return false;                                  // 直接返回失败
    }

    std::string sPath = sDirPath;                      // 拷贝一份路径，避免修改原始参数

    if (sPath.size() > 1 && sPath[sPath.size() - 1] == '/') // 如果路径末尾有 /
    {
        sPath.erase(sPath.size() - 1);                 // 删除末尾的 /
    }

    for (size_t i = 1; i <= sPath.size(); ++i)          // 从第 1 个字符开始逐级扫描路径
    {
        if (i == sPath.size() || sPath[i] == '/')       // 遇到 / 或者到达路径末尾
        {
            std::string sSubPath = sPath.substr(0, i); // 截取当前层级目录

            if (sSubPath.empty() || sSubPath == "/")   // 空目录或者根目录不需要创建
            {
                continue;                              // 跳过
            }

            if (::mkdir(sSubPath.c_str(), 0755) != 0)  // 创建当前层级目录
            {
                if (errno != EEXIST)                   // 如果错误不是“目录已存在”
                {
                    return false;                      // 说明创建失败
                }
            }
        }
    }

    return true;                                       // 所有目录创建成功
}


inline bool IsCsvFileName(const std::string& sFileName)
{
    const std::string sSuffix = ".csv";                // CSV 文件后缀

    if (sFileName.size() < sSuffix.size())             // 文件名长度小于 ".csv"
    {
        return false;                                  // 肯定不是 csv 文件
    }

    return sFileName.compare(sFileName.size() - sSuffix.size(),
                             sSuffix.size(),
                             sSuffix) == 0;            // 判断文件名最后是否为 ".csv"
}


inline void DeleteOldCsvFiles(const std::string& sDirPath, size_t nMaxFileCount)
{
    std::vector<ST_LogFileInfo> vecFiles;              // 保存目录下所有 csv 文件信息

    DIR* pDir = ::opendir(sDirPath.c_str());           // 打开日志目录

    if (pDir == nullptr)                               // 如果目录打开失败
    {
        return;                                        // 直接返回
    }

    struct dirent* pEntry = nullptr;                   // 目录项指针

    while ((pEntry = ::readdir(pDir)) != nullptr)       // 循环读取目录下的文件
    {
        const char* pName = pEntry->d_name;            // 获取文件名

        if (std::strcmp(pName, ".") == 0 || std::strcmp(pName, "..") == 0)
        {
            continue;                                  // 跳过当前目录和上级目录
        }

        std::string sFileName = pName;                 // 转成 std::string

        if (!IsCsvFileName(sFileName))                 // 如果不是 .csv 文件
        {
            continue;                                  // 跳过
        }

        std::string sFullPath = sDirPath;              // 准备拼接完整路径

        if (!sFullPath.empty() && sFullPath[sFullPath.size() - 1] != '/')
        {
            sFullPath += "/";                          // 如果目录末尾没有 /，补一个 /
        }

        sFullPath += sFileName;                        // 拼接完整文件路径

        struct stat stFileInfo;                        // 文件状态结构体

        if (::stat(sFullPath.c_str(), &stFileInfo) != 0)
        {
            continue;                                  // 获取文件信息失败，跳过
        }

        if (!S_ISREG(stFileInfo.st_mode))              // 如果不是普通文件
        {
            continue;                                  // 跳过
        }

        ST_LogFileInfo stLogFile;                      // 定义日志文件信息结构体
        stLogFile.sFilePath = sFullPath;               // 保存完整路径
        stLogFile.tModifyTime = stFileInfo.st_mtime;   // 保存最后修改时间

        vecFiles.push_back(stLogFile);                 // 加入文件列表
    }

    ::closedir(pDir);                                  // 关闭目录

    if (vecFiles.size() < nMaxFileCount)               // 如果文件数量没有达到上限
    {
        return;                                        // 不需要删除
    }

    std::sort(vecFiles.begin(), vecFiles.end(),
        [](const ST_LogFileInfo& a, const ST_LogFileInfo& b)
        {
            return a.tModifyTime < b.tModifyTime;      // 按修改时间从旧到新排序
        });

    size_t nNeedDelete = vecFiles.size() - nMaxFileCount + 1; // 删除后给新文件留一个位置

    for (size_t i = 0; i < nNeedDelete; ++i)            // 删除最旧的若干文件
    {
        ::unlink(vecFiles[i].sFilePath.c_str());       // 删除文件
    }
}