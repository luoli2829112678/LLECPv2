#ifndef CIA402AXISDEF_H   // 如果还没定义这个宏
#define CIA402AXISDEF_H   // 定义宏，防止重复包含
#include"CIA402AxisDef.h"
#include"AxisErrorCode.h"
#include"AxisFriendList.h"
#define PDO_CONTROLWORD  *(m_st_map.pControlword)
#define PDO_STATUSWORD  *(m_st_map.pStatusWord)
#define PDO_TARGETPOSITION  *(m_st_map.pTargetPosition)
#define PDO_ACTUALPOSITION  *(m_st_map.pActualPosition)
#define PDO_TARGETTORQUE  *(m_st_map.pTargetTorque)
#define PDO_ACTUALTORQUE  *(m_st_map.pActualTorque)
#define PDO_TARGETVELOCITY *(m_st_map.pTargetVelocity)
#define PDO_ACTVELOCITY  *(m_st_map.pActualVelocity)
#define PDO_TARGETMODESOFOPERATION  *(m_st_map.pTargetModesOfOperation)
#define PDO_ACTUALMODESOFOPERATION  *(m_st_map.pActualModesOfOperation)
#define PDO_ERRORCODE  *(m_st_map.pErrorCode)
#define PDO_DIGITALOUTPUTS *(m_st_map.pDigitalOutputs) 
#define PDO_DIGITALINPUTS *(m_st_map.pDigitalInputs)
#define PDO_JOINT_ACTUALPOSITION  *(m_st_map.pJointActPosition)
#define PDO_TARGET_VFF  *(m_st_map.pAddVelocityValue)
#define PDO_TARGET_TFF  *(m_st_map.pAddTorqueValue)
//记录数据量
#define FRAME_DATA_SIZE 1000
class SoftMotion;
class AxisGroup;


class CIA402Axis 
{
    friend class LL_CpsService;
    AXIS_FRIEND_CLASSES
    friend SoftMotion;
    friend AxisGroup;
protected:
    //connfig
    bool m_bVirtual;
    bool m_bBusy;
    bool m_bInit;

    uint32_t m_nAxisID;
    bool m_bError;
    int m_nErrorCode;
    ST_CIA402_PDO m_stPDO_Virtual;
    //pdo同步
    //io映射镜像
    ST_CIA402_PDO m_stMirrorPDO;
    //IO映射
    ST_SMCInitMap m_st_map;
    //轴基本配置
    ST_SMCAxisConfiguration m_stAxisConfiguration;
    //双编码器
    ST_AxisDualEncoderConfiguration m_stDualEncoderConfiguration;
    ST_AxisDualEncoderData m_stAxisDualEncoderData;
    //前馈参数
    ST_Feedforward m_stFeedforward;

    //控制周期
    double m_dControlCycle;//ms
    //日志写入目录
    std::string sErrorLogPath = "/usr/local/bin/Robot/LLCPS/ErrorLog/";
    //滤波系数
    int nFilteringAlpha = 5;;
    //state
    EN_AxisMotionState m_enAxisMotionState;//sm
    //SoftMotion交换数据
    ST_SoftMotionData m_stSoftMotionEX;
    //插补帧
    ST_InterParams  m_stInterParamsData;
    // 限位回零的数据帧
    ST_RT_MotionPlanPointData m_stRTMotionPlanPointData;
    // 原始电机运动数据
    ST_MotorMotionData m_stMotorMotionData;
    // // 滤波后的电机运动数据
    // ST_MotorMotionData m_stMotorMotionDataFiltered;
public:
    //对外数据统一接口
    ST_AxisData m_stAxisData;
    // --- 控制字 / 状态字 ---
    int Axis_GetControlWord();          // 控制字（主站发往从站）
    int Axis_GetStatusWord();           // 状态字（从站反馈主站）
    // --- 目标运动参数 ---
    double Axis_GetSetPosition();       // 目标位置（工程单位）
    int    Axis_GetTargetPositionPDO(); // 目标位置（PDO映射整数值）
    double Axis_GetSetVelocity();       // 目标速度
    double Axis_GetSetAcceleration();       // 目标加速度
    double Axis_GetSetJerk();       // 目标加速度
    int    Axis_GetTargetVelocityPDO(); // 目标速度（PDO映射整数值）
    double Axis_GetSetTorque();         // 目标力矩
    int    Axis_GetTargetTorquePDO();   // 目标力矩（PDO映射整数值）
    // --- 实际运动参数 ---
    double Axis_GetActualPosition();       // 实际位置（工程单位）
    int    Axis_GetActualPositionPDO();    // 实际位置（PDO映射整数值）
    double Axis_GetActualVelocity();       // 实际速度
    double Axis_GetActualAcceleration(); 
    double Axis_GetActualJerk(); 
    int    Axis_GetActualVelocityPDO();    // 实际速度（PDO映射整数值）
    double Axis_GetActualTorque();         // 实际力矩
    int    Axis_GetActualTorquePDO();      // 实际力矩（PDO映射整数值）
    int Axis_GetDigitalOutputs();          // 数字输出
    int Axis_GetDigitalInputs();           // 数字输入

    // --- 模式与错误信息 ---
    int Axis_GetCommandOperationMode();     // 当前指令的操作模式
    int Axis_GetActualOperationMode();      // 从站反馈的操作模式
    bool Axis_GetErrorState();              // 是否错误
    int  Axis_GetAxisErrorID();             // 轴错误代码
    bool Axis_GetBusyState();               // 是否忙
    bool Axis_GetMovingState();             // 是否运动中
    bool Axis_GetSoftMotionMovingState();   // SoftMotion 是否运动中
    // --- 双编码器 ---
    ST_AxisDualEncoderConfiguration Axis_GetDualEncoderConfiguration(); // 双编码器配置
    ST_AxisDualEncoderData Axis_GetDualEncoderData();                   // 双编码器数据
    // --- 轴状态 ---
    bool Axis_GetVirtualState();                // 是否虚拟轴
    uint32_t Axis_GetAxisID();                  // 轴 ID
    EN_AxisMotionState Axis_GetAxisMotionState(); // 轴运动状态
    bool Axis_GetDualEncoderEnableState();      // 是否开启双编码校正
    // --- 配置 ---
    ST_SMCAxisConfiguration Axis_GetAxisConfiguration(); // 轴配置
    ST_Feedforward Axis_GetFeedforward();                // 前馈参数

//内部中间量
protected:
    bool m_bCyclicSyncMotion;
    int16_t m_nCyclicSyncMotionLosCount;
    bool m_bSoftMotionSignal;
    //写日志功能
    bool m_bWriteLog = true;
    SMCTimer m_TonError;
    ST_FrameData m_stFrameData_now;
    ST_FrameData m_stFrameData[FRAME_DATA_SIZE];

public:
    CIA402Axis(bool bVirtual);
    ~CIA402Axis();
    int Axis_InitMap(ST_SMCInitMap st_map);
    int Axis_SetAxisID(uint16_t id);
    //轴的实时函数
    void Axis_RT();
protected:
    int Axis_SetAxisState(EN_AxisMotionState enAxisMotionState);
    int Axis_SetOverride(ST_Factor stFactor);
    EN_AxisMotionState Axis_ReadAxisState();
    bool Axis_CheckError(int& nErrorID);
    bool Axis_CheckError();
    int Axis_ResetError();

    void Axis_SetCyclicSyncMotion();
    int Axis_InitAlign();

    //PDO镜像直接控制pdo

    void PDOsynchronization();
    //模拟PDO同步，这样子拷贝虚拟轴的轴对象时不会发生异常
    void VirtualPDOsynchronization();
    void AxisStatusCheck();
    // ===== 写入（RxPDO） =====
    int PDO_SetControlword(uint16_t Controlword);
    int PDO_SetTargetPosition(int32_t TargetPosition);
    int PDO_SetTargetVelocity(int32_t TargetVelocity);
    int PDO_SetTargetTorque(int16_t TargetTorque);
    int PDO_SetModesOfOperation(uint8_t mode);
    int PDO_SetVFF(int32_t VFF);
    int PDO_SetTFF(int16_t TFF);
    int PDO_SetDigitalOutputs(uint32_t DigitalOutputs);

    // ===== 读取（TxPDO） =====
    int PDO_ReadStatusWord(uint16_t& StatusWord);
    int PDO_ReadErrorCode(uint16_t& ErrorCode);
    int PDO_ReadActualPosition(int32_t& ActualPosition);
    int PDO_ReadActualJointPosition(int32_t& ActualJointPosition);
    int PDO_ReadActualVelocity(int32_t& ActualVelocity);
    int PDO_ReadActualTorque(int16_t& ActualTorque);
    int PDO_ReadModesOfOperation(uint8_t& mode);
    int PDO_ReadDigitalInputs(uint32_t& inputs);
    int PDO_ReadDigitalOutputs(uint32_t& outputs);

    // ===== 读回目标值缓存或虚拟轴调试 =====
    int PDO_ReadControlword(uint16_t& Controlword);
    int PDO_ReadTargetPosition(int32_t& TargetPosition);
    int PDO_ReadTargetVelocity(int32_t& TargetVelocity);
    int PDO_ReadTargetTorque(int16_t& TargetTorque);
    int PDO_ReadTargetModesOfOperation(uint8_t& mode);

    int Axis_ReadActPos(double& pos);



    //MC功能块控制
    int Axis_PDO_SetControlword(uint16_t Controlword);
    int Axis_PDO_SetTargetPosition(int32_t TargetPosition);
    int Axis_PDO_SetTargetVelocity(int32_t TargetVelocity);
    int Axis_PDO_SetTargetTorque(int16_t TargetTorque);
    int Axis_PDO_SetModesOfOperation(EN_ModesOfOperation mode);
    int Axis_PDO_SetVFF(int32_t VFF);
    int Axis_PDO_SetTFF(int16_t TFF);
    int Axis_PDO_SetDigitalOutputs(uint32_t DigitalOutputs);
    int Axis_PDO_ReadStatusWord(uint16_t& StatusWord);
    int Axis_PDO_ReadErrorCode(uint16_t& ErrorCode);
    int Axis_PDO_ReadActualPosition(int32_t& ActualPosition);
    int Axis_PDO_ReadActualVelocity(int32_t& ActualVelocity);
    int Axis_PDO_ReadActualTorque(int16_t& ActualTorque);
    int Axis_PDO_ReadModesOfOperation(EN_ModesOfOperation& mode);
    int Axis_PDO_ReadDigitalInputs(uint32_t& inputs);
    int Axis_PDO_ReadDigitalOutputs(uint32_t& outputs);
    int Axis_PDO_ReadControlword(uint16_t& Controlword);
    int Axis_PDO_ReadTargetPosition(int32_t& TargetPosition);
    int Axis_PDO_ReadTargetVelocity(int32_t& TargetVelocity);
    int Axis_PDO_ReadTargetTorque(int16_t& TargetTorque);
    int Axis_PDO_ReadTargetModesOfOperation(uint8_t& mode);



    int Axis_PushMotionUint(const ST_MotionUint STMotionUint);
    int Axis_GetMotionUint(ST_MotionUint& stMotionUint);
    int Axis_GetMotionInitialParameters(ST_InterParams& stInitialParameters);
    int Axis_SoftMotionRunDone();
    int Axis_DualEncoderCalculateAxisPosition();
    int Axis_SY_Feedforward();
    int Axis_SY_Feedforward_V();
    int Axis_SY_Feedforward_A();
    int Axis_SY_Feedforward_J();

public:
    //Config
    //设置轴类型直线轴，旋转轴，旋转轴在计算减速比时会叠加360
    int Axis_SetAxisType(EN_AxisMoveDef enAxisType);
    //设置减速比，调用即时生效*******减速比定义：在设置的编码器分辨率下一个编码器分辨率周期下的运动距离
    int Axis_SetGearRatio(double dGearRatio);
    //设置编码器分辨率，调用即时生效
    int Axis_SetEncodeRatio(uint32_t nEncodeRatio);
    //设置编码器方向实时生效
    int Axis_SetEncodeDirection(int nEncodeDirection);
    //设置编码器原点，实时生效
    int Axis_SetEncodeHomePos(int32_t nEncodeHomePos);
    //设置电流常数，实时生效
    int Axis_SetTorqueScale(double dTorqueScale);
    //设置速度常数
    int Axis_SetVelocityScale(double dVelocityScale);
    //设置电流方向，实时生效
    int Axis_SetTorqueDirection(int nTorqueDirection);
    //设置硬件边界
    int Axis_SetSWLimit(double dPositive,double dNegative);
    //设置控制周期(ms)
    int Axis_SetControlCycle(double dControlCycle);
    //驱动赋错误码
    int Axis_SetDriveErrorCode(int ErrorCode);
    //前馈设置

    int Axis_SetFeedforward(ST_Feedforward stFeedforward);
    //设置电流前馈，实时生效
    int Axis_SetTFFScale(double TFFScale);
    //设置速度前馈常数
    int Axis_SetVFFScale(double VFFScale);


protected:
    int Axis_SetMotionPlanner(ST_InterParams stMotionPlanner);
    int Axis_SetTargetPosition(double TargetPosition);
    int Axis_SetTargetVelocity(double TargetVelocity);
    int Axis_SetTargetTorque(double TargetTorque);
    int Axis_SetVFF(double dVelocity);
    int Axis_SetTFF(double dTorque);
//CIA402AxisGeneral
protected:
    //不同操作模式下其他的位置同步
    void AxisMMOPositionSynchronization();
    bool Axis_PDOErrorCheck();
    void DataSynchronization();
    //虚拟轴系统
    void Axis_Simulation();
    //数据计算
    void FrameDataCheck();
    //初始化轴
    void InitAxis();
    //写入错误日志
    void WriteErrorLog();

    //softmotion
    int CleanSoftMotionData();
};


#endif