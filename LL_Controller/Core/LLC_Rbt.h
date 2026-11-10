//#pragma once
#ifndef _LLC_RobotBase_H
#define _LLC_RobotBase_H
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <regex>
#include <stdexcept>
#include <fstream>
#include <sys/syscall.h>
#include <sys/prctl.h>
#include <algorithm>   
#include <cctype>      
#include <filesystem>  
#include <utility>  
#include "ESI_Parse.h"
#include "LLC_main.h"
#include"LLSMC/SoftMotion/SoftMotion.h"
#include"LLSMC/SMbasic/SMbasic_stdafx.h"
#include "LLC_Slave_def.h"
#include "LLC_SlaveBase.h"
#include "LL_SHMDef.h"
#include "LL_Log.h"
#include"HSTR/HSTR.hpp"
#include "HSTR/third_party/nlohmann/json.hpp"
#include "LLC_Rbt_Status.h"
#include "LL_SingletonBase.h"
#include "LLC_IF.h"
#include "LLC_TcpClient.h"
#include "LL_MasterRegistry.h"
#include <future>
#include <mutex>
#include <thread>
#include "Monitor/LLC_JitterMonitor.h"
#include"IO/LLPC_IO.h"


class LLC_Rbt : public LLC_Rbt_Status
{
DECLARE_Singleton(LLC_Rbt)
public:
    LLC_Rbt(LLC_Rbt &&) = delete;
    LLC_Rbt(const LLC_Rbt &) = delete;
    LLC_Rbt &operator=(LLC_Rbt &&) = delete;
    LLC_Rbt &operator=(const LLC_Rbt &) = delete;


public://初始化相关
    // 初始化线程
    int Initialize_LLCThreads();
    int Initialize_LLCIF(LLC_HandlecmdCBFunc pLLC_DispatchCmdFunc);
    //初始化ESI（XML）文件
    int Initialize_ESI();
    int OnConnect(int s);
    int OnDisconnect(int s);
    int DispatchCmdFunc(int s,const string&cmd,const vector<string>&params,vector<string>&result,uint16_t nPort);
    //重置控制器配置
    int ResetControllerConfig();
    // 设置模拟机器人
    int SetSimulationType(const int nSimulationType);
    // 设置从站PDO映射，0为主站，配置主站时清除所有从站信息
    int InitSetConfigSlavePdo(int nSlaveCount, int nSlaveID, void* pSlaveST);
    //注册轴时参数vector初始化
    int InitializationMotionParameters();
    // 轴参数注册
    int RegistrationAxis(int nAxisID,int nSlaveID,int TXpdo,int RXpdo,int nAxisType,int nHomePos,int nEncodeRatio,int nEncodeDirection,
                        double  dGearRatio,double  dVelocityScale,double  dTorqueScale,double  dSafeSpaceLimitUpper,double  dSafeSpaceLimitLower,
                        double  dMaxMotionVel,double  dMaxMotionAcc,double  dMaxMotionJerk,double  dTFFScale,double  dVFFScale,double  dJFFScale,
                        double  dStopAcc,double  dStopJerk);

    // 从站注册
    int RegistrationSlave(uint8_t nSlaveID,uint32_t nProductCode,uint32_t nRevisionNo,bool bEnableDC,uint32_t nShiftTime);
    //io注册
    int RegistrationIO(uint8_t nIOid,uint8_t nSlaveID,uint16_t nIndex,uint16_t nSubIndex,uint16_t nBit,bool bInverted);
    //从站过程数据配置
    int ConfigureSlavePDO(uint8_t nSlaveID,vector<uint32_t>TxPDO,vector<uint32_t>RxPDO);
    // 检查所有从站是否进入OP状态
    bool IsSlaveInOprational();
    //掉线后清除配置
    int CleanSlaveConfig();
    int CleanAxisConfig();
    int CleanIOConfig();
    int CleanGroupConfig();

public://通信相关
    // 根据侦听端口获取LLCIFHandle对象
    LLCIFHandle* GetIFHandle(uint16_t nPort);
    // 获取CPS客户端(10003端口)对象
    TcpClient* GetCPSClient();
     // 初始化机器人信息读取命令列表
    void InitReadCmdList();
    // 发送cmd到CPS
    int SendCmd2CPS();
    // 发送命令执行结果到CPS8888端口
    void SendResult2CPSNormalPort(const int nResult);
    // 发送cmd到CPS，并等待执行结果
    int SendCmd2CPSAndWaitResult(const string& strCmd, string& strRst);private:
    // 处理命令
    //int Rbt_Handlecmd(const string& strCmd,const vector<string>& params,vector<string>& results);


    // 发送命令到CPS标志
    atomic<bool> m_bSendCmd2CPS10003{false};
    // 发送命令到CPS标志
    string m_sSendCmd2CPS;
    // 发送命令到CPS标志
    atomic<bool> m_bSendCmd2CPS8888{false};
    // 发送命令到CPS标志
    int m_nSendResult2CPS8888{};
    // 发送命令到CPS标志
    atomic<bool> m_bSendCmd2CPS8889{false};
    // 发送命令到CPS标志
    int m_nSendResult2CPS8889{};

    // 发送命令到CPS
    void SendCmd2CPS10003(string sCmd);
    // 发送命令到CPS
    void SendCmd2CPS8888(int nResult);
    // 发送命令到CPS
    void SendCmd2CPS8889(int nResult);

protected://配置参数
    int nECATStatusMachineLast{};
    double m_cycle_time{};
    // 应用配置参数
    ST_RbtAppParams m_stRbtAppParams{};
    ST_RbtEtherCATParams m_stRbtEtherCATParams{};
    vector<ST_LLPCAxisHardParam> v_AxisHardParam;
    //配置从站信息
    vector<ST_LLPCSlaveConfigParam> m_vSlaveConfigParam;
    //io配置
    vector<ST_LLPC_IO_Config> m_vIOConfig;
    //轴运动参数
    std::vector<AxisMtionParam> m_vAxisMtionParam;
    //回零参数
    ST_HomeParam m_stHomeParam[MaxAxisNum]{};
    //前馈值
    ST_Feedforward m_stFeedforward[MaxAxisNum]{};

    //调试运动参数
    std::vector<AxisMtionParam> m_vAxisMtionParam_Test[2];
    std::vector<double> m_vAxisMtionWaitTime_Test[2];
    //扫描从站信息
    vector<SlaveInfo> m_vScanSlaveInfo;
    //初始化的设备列表
    vector<std::unique_ptr<ESI_Parse>> m_vESI_Parse;
    
public://实时数据
    ST_RobotControlStatus m_stRobotControlStatus{};
    // 实时数据信息
    ST_ShmRobotRTStatus m_stShmRobotRTStatus{};
    //预配置轴数量
    int m_nSetConfiguredAxisNum{};
    //已配置轴数量
    uint32_t m_nConfiguredAxisNum{};
    //预配置从站数量
    int m_nSetConfiguredSlaveNum{};
    //已配置从站数量
    int m_nConfiguredSlaveNum{};
public://子轴和轴组
    //SMC驱动对象列表
    vector<CIA402Axis*> m_vAxis;
    // 从站轴对象列表
    vector<LLC_SlaveBase*> m_vSlaves;
    //LLPC_IO
    vector<LLPC_IO> m_vIOs;
    // 定义机器人轴组类
    vector<AxisGroup> m_vAxisGruop;
    
    SoftMotion LLCSoftMotion;
    int InitSoftMotion();
    int InitBasicFB();
    int InitIO();
    bool m_bEnableCPS{};

protected://功能块

    MC_InitResetAxis   m_FB_AxisInitReset[MaxAxisNum];
    MC_PowerOn         m_FB_AxisPowerOn[MaxAxisNum];
    MC_PowerOff        m_FB_AxisPowerOff[MaxAxisNum];
    MC_Reset           m_FB_AxisReset[MaxAxisNum];
    MC_Stop            m_FB_AxisStop[MaxAxisNum];
    MC_MoveAbsolute    m_FB_AxisAbsMove[MaxAxisNum];
    MC_MoveRelative    m_FB_AxisRelMove[MaxAxisNum];
    MC_MoveVelocity    m_FB_AxisMoveVelocity[MaxAxisNum];
    MC_MoveTorque      m_FB_AxisMoveTorque[MaxAxisNum];
    MC_Home            m_FB_AxisHome[MaxAxisNum];
    MC_SetPosition     m_FB_AxisSetPosition[MaxAxisNum];
    MC_SetModesOfOperation     m_FB_AxisSetModesOfOperation[MaxAxisNum];
    SMbasic*           m_pFB_AxisPTMove[MaxAxisNum];
    MC_FollowPosition   m_FB_FollowPosition[MaxAxisNum];
    MC_FollowVelocity   m_FB_FollowVelocity[MaxAxisNum];
    MC_FollowTorque   m_FB_FollowTorque[MaxAxisNum];

public://轴控制指令
    bool bAxisInitResetCmd[MaxAxisNum]{};
    bool bAxisEnableCmd[MaxAxisNum]{};
    bool bAxisDisableCmd[MaxAxisNum]{};
    bool bAxisResetCmd[MaxAxisNum]{};
    bool bAxisStopCmd[MaxAxisNum]{};
    bool bAxisAbsMoveCmd[MaxAxisNum]{};
    bool bAxisRelMoveCmd[MaxAxisNum]{};
    bool bAxisHomeCmd[MaxAxisNum]{};

    bool bAxisBackAndForthMoveCmd[MaxAxisNum]{};
    bool bAxisContinuousStepMoveCmd[MaxAxisNum]{};
    bool bAxisVelocityMoveCmd[MaxAxisNum]{};

protected://轴控制状态机
    EN_AxisControlFSM m_enAxisInitResetFSM[MaxAxisNum]{};
    EN_AxisControlFSM m_enAxisEnableFSM[MaxAxisNum]{};
    EN_AxisControlFSM m_enAxisDisableFSM[MaxAxisNum]{};
    EN_AxisControlFSM m_enAxisResetFSM[MaxAxisNum]{};
    EN_AxisControlFSM m_enAxisStopFSM[MaxAxisNum]{};
    EN_AxisControlFSM m_enAxisAbsMoveFSM[MaxAxisNum]{};
    EN_AxisControlFSM m_enAxisRelMoveFSM[MaxAxisNum]{};
    EN_AxisControlFSM m_enAxisHomeFSM[MaxAxisNum]{};

    int m_enAxisBackAndForthMoveFSM[MaxAxisNum]{};
    int m_enAxisContinuousStepMoveFSM[MaxAxisNum]{};
    int m_enAxisVelocityMoveFSM[MaxAxisNum]{};

protected://轴控制超时监测
    LLCTimer m_TonInitReset[MaxAxisNum];
    LLCTimer m_TonEnable[MaxAxisNum];
    LLCTimer m_TonDisable[MaxAxisNum];
    LLCTimer m_TonReset[MaxAxisNum];
    LLCTimer m_TonStop[MaxAxisNum];
    LLCTimer m_TonAxisAbsMove[MaxAxisNum];
    LLCTimer m_TonAxisRelMove[MaxAxisNum];

    LLCTimer m_TonAixsBackAndForthMove[MaxAxisNum];
    LLCTimer m_TonAxisContinuousStepMove[MaxAxisNum];
    LLCTimer m_TonAxisVelocityMove[MaxAxisNum];

protected://轴控制状态机执行
    void AxisControlFSM();

    void AxisInitResetFSM();
    void AxisEnableFSM();
    void AxisDisableFSM();
    void AxisResetFSM();
    void AxisStopFSM();
    void AxisAbsMoveFSM();
    void AxisRelMoveFSM();
    void AxisHomeFSM();
    
    void AxisBackAndForthMoveFSM();
    void AxisContinuousStepMoveFSM();
    void AxisVelocityMove();
    //Planned trajectory通用代理接口
    SMbasic* AxisPTMove(int nAxisID, bool bExecute, double dPosition, double dVelocity, double dAcceleration, double dDeceleration, double dJerk, EN_Direction enDirection, EN_BufferMode enBufferMode, bool &bDone, bool &bBusy, bool &bCommandAborted, bool &bError, int &nErrorID);
//WAYPOINT
protected:
    int m_nWayPoinCount{};
    int m_nWayPoinRunIndex{};
    bool m_bWayPoinError{};
    bool m_bWayPoinRunDone{};
    vector<ST_AxisWayPoint_Point> m_WayPointManager;
    
protected://指令执行相关
    // 命令执行
    LLC_HandlecmdCBFunc m_pLLC_DispatchCmdFunc;
    // 状态检测线程
    // 非实时命令执行线程
    void LLC_ExecHandleNotRealTimeCmd();
    // 处理普通运动指令：from port 8888
    int HandleNormalCmd(const string& strCmd,const vector<string>& params,vector<string>& results);
    // 跟随运动：from port 8889
    int HandleFollowMotionCmd(const string& strCmd,const vector<string>& params,vector<string>& results);
    // HSTR：from port 8890
    int HandleHSTRCmd(const string& strCmd,const vector<string>& params,vector<string>& results);
    // 处理机器人异步命令：from port 8891
    int HandleRbtAsyncCmd(const string& strCmd,const vector<string>& params,vector<string>& results);
    // 处理实时脚本指令：from port 8892
    //
    int HandleReplyCmd(const string& strCmd,const vector<string>& params,vector<string>& results);
    // LLCIFHandle列表
    std::map<uint16_t, LLCIFHandle> m_IFHandleMap;
    // 机器人信息读取命令列表
    std::map<string, LLC_CmdFunc> m_ReadCmdMap; //格式：<命令名称, 命令处理函数>
    // CPSClient变量
    TcpClient m_CPSClient;
public:
    ST_EtherCATMasterControl m_stEtherCATMasterControl{};
    // 读SDO
    static _ec_SDOread m_pFuncSDORead;
    // 写SDO
    static _ec_SDOwrite m_pFuncSDOWrite;


public://共享内存和波形
    // 共享内存
    ST_ShareMemory *m_pLLC_ShmRW = nullptr;
    // 共享内存-写区域
    ST_ShmCPS2LLCData  *m_pLLC_CPS2LLCMemory = nullptr;
    // 共享内存-读区域
    ST_ShmLLC2CPSData *m_pLLC_LLC2CPSMemory = nullptr;
    //PDO数量
    size_t m_nMutableShmPDONum{};

public: //** 初始化函数和插件接口函数
    // 初始化共享内存
    int Initialize_LLCShm();
    //获取共享内存写地址
    void* GetLLCWriteMemory();
    //获取共享内存读地址
    void* GetLLCReadMemory();
    // 设置总线参数
    int InitMaster(const string sNetPort,const int nCycleTime);
    // 获取总线周期时间
    int GetEcatCycleTime(float& fCycleTime);
    // 设置主站控制
    void SetMaster(ST_EtherCATMasterControl stEtherCATMasterControl);
    // 设置SDO读写函数
    static void SetSDOFuncCB(_ec_SDOread pSDORead, _ec_SDOwrite pSDOWrite);
    // 读取EtherCAT配置
    void ReadEtherCATConfig(bool& bIsSimulation, string& sNetPort, float& fCycle,vector<bool>& bEnableDC,vector<double>& dShiftTime);
    // 复位主站
    int ResetMaster();
    // 设置开启和关闭主站状态
    int SetRunControllerState(const int nState);
    //主站扫描从站
    int ScanSlave(vector<SlaveInfo>& vSlaveInfo);
    // 复位所有错误
    //int ResetControllerError();
    // 处理实时线程任务
    int HandleCyclicJob(long nCurTickUSEC);
    // 更新、处理总线相关
    int HandleEcatCheck(ecat_frame& update_ecat_shm);

public:
    //控制器指令
    int ControllerStop();
    int ControllerReset();
    int ControllerConfig();
//guard
public:
    bool guard_CheckIsSlaveDrop();
    bool guard_CheckIsError();
    bool guard_CheckIsMoveDone();
    bool guard_CheckResetFinish();
    bool guard_CheckIsSlaveConfigDone();

public: //** 控制器循环执行函数
    // 读取机器人位置信息
    void ReadRbtInfoFromAxisToShm();
    // 读取状态信息
    void ReadRbtInfoFromRbtStatusToShm();
    //读IO信息
    void ReadRbtInfoFromIOToShm();
    // 同步信息到8893
    void ReadRbtInfoTo8893();
public: //** 控制器状态检测
    // 检测CPS是否看门狗超时
    bool CheckCPSWatchDogTimeOut();
     //检测从站掉线
    bool CheckSlaveDropError();
    //检测急停
    bool CheckEmergencyStop();
    //监测脚本是否运行中
    bool CheckIsScriptMoving();
    // 检测急停处理完成
    bool CheckIsEmergencyStopHandled();
    // 检测是否退出急停到未上电
    bool CheckIsNotElectrifyInEmergencyStop();
    // 看门狗超时处理完成
    bool CheckIsWatchDogHandled();
    // 检查所有从站是否处于OP状态
    bool CheckIsSlaveInOprational();
    //急停处理
    bool HandleEmergencyStop();
    //LLPC系统错误监测
    bool CheckLLPCError();
public:
    // 上升沿检测读取波形数据
    LLCTimer m_R_Trig_ReadScopeInfo;
    // 力控标定定时器
    LLCTimer  m_Ton_SetCalibFTSensorParamsTime;
    // 看门狗计数初值
    int m_nWatchDog{};
    // 看门狗计数次数
    int m_nWatchDogCnt{};
    //  开关主站
    bool m_bMasterState{};
    // 需要上报8893的实时数据
    RobotRTData m_stRtData;
    string m_sRtData8893;

public://SDO 
    // SDO状态
    ST_SDOWorkState  m_stSDOWorkState{};
    // 温度电压读取定时器
    LLCTimer m_ReadTempVolTimer;
    // SDO读写定时器
    LLCTimer m_SDOReadTimmer;
    // SDO读写定时器
    LLCTimer m_SDOWriteTimmer;
    // 上升沿读取SDO
    LLCTimer m_R_Trig_ReadSDO;
    // 上升沿写入SDO
    LLCTimer m_R_Trig_WriteSDO;
    // 防止SDO同时发生
    std::mutex m_sdoMutex; 

    // 读SDO
    int ReadSdoFromSlave(int nSlave, int nIndex, int nSubIndex, int nSDOtype, string *sValue);
    // 写SDO
    int WriteSdoToSlave(int nSlave, int nIndex, int nSubIndex, int nSDOtype, string sValue);
    // 读取SDO
    int ReadSDO(int nSlave, uint16_t index, uint8_t subidx, int nSDOType, string &Value);
    // 写入SDO
    int WriteSDO(int nSlave, uint16_t index, uint8_t subidx, int nSDOType, string Value);

    // 写入PDO
    int WritePDO(int nSlave, uint16_t index, uint8_t subidx, int Value);
    //升级固件FOE
    int FOEwrite(int nSlave,const std::string& strFilePath, const std::string& strFoEFileName, uint32_t nPassword);
    //升级进度
    int GetFOEwriteProgress(double& dProgress);
    //FOE读取
    int FOEread(int nSlave,const std::string& strFilePath, const std::string& strFoEFileName, uint32_t nPassword);
    //升级进度
    int GetFOEreadProgress(double& dProgress);
    //升级ESI文件
    int UpdateESI(int nSlave,const std::string& strFilePath);
    //检查从站与配置文件是否一致
    bool CheckSlaveConfigConsistency();
private:
    JitterMonitor* m_pJitterMonitor; // 抖动监测器
    std::atomic<double> m_dFOEworkProgress{0.0};      // 固件升级进度，0~100，失败时为-1
    std::atomic<bool> m_bFoEWorking{false};    // 当前是否正在foe
    std::atomic<bool> m_bFoEWorkingDone{false};
public:    
    //8893数据推送通知
    std::mutex m_8893dataMutex;
    std::condition_variable m_8893dataCV;


    



public://轴控制
    int AxisInitReset(vector<int>AxisList);
    int AxisEnable(vector<int>AxisList);
    int AxisDisable(vector<int>AxisList);
    int AxisReset(vector<int>AxisList);
    int AxisStop(vector<int>AxisList);
    int AxisHome(vector<int>AxisList);
    int AxisSetOffset(int nAxisID,double dOffset);
    int AxisSetModesOfOperation(int nAxisID,int nModesOfOperation);
    int AxisReadState(int nAxisID);
    int SetAccFFScale(int nAxisID,double dScale);
    int SetVelFFScale(int nAxisID,double dScale);
    int SetJerkFFScale(int nAxisID,double dScale);
    //HUAYAN
    int StartAnalogFocusing(int nAxisID);
    int StopAnalogFocusing(int nAxisID);
    int StartPEG(int nAxisID);
    int StopPEG(int nAxisID);

    //轴运动
    int AxisAbsMove(int nAxisID,double dPos,double dVel,double dAcc,double dDec,double dJerk);
    int AxisRelMove(int nAxisID,double dPos,double dVel,double dAcc,double dDec,double dJerk);
    int AxisBackAndForthMove(int nAxisID,double dPos1,double dPos2,double dVel1,double dVel2,double dAcc1,double dAcc2,double dDec1,double dDec2,double dJerk1,double dJerk2,double dWaitTime);
    int AxisContinuousStepMove(int nAxisID,double dPos,double dVel,double dAcc,double dDec,double dJerk,double dWaitTime); 
    int AxisVelocityMove(int nAxisID,int nDir,double dVel,double dAcc,double dDec,double dJerk);

    int AxisFollowPosition(int nAxisID,double dPos);
    int AxisFollowVelocity(int nAxisID,double dVel);
    int AxisFollowTorque(int nAxisID,double dTor);

    //轴配置
    int SetAxisStopParam(int nAxisID,double dStopAcc,double dStopJerk);

    //设置IO
    int SetIO(int IOid,bool Var);


    //FSM
    int SetAxisMovePos(int nAxisID, double dPos);
    int SetAxisMoveVel(int nAxisID, double dVel);
    int SetAxisMoveAcc(int nAxisID, double dAcc);
    int SetAxisMoveDec(int nAxisID, double dDec);
    int SetAxisMoveJerk(int nAxisID, double dJerk);
    int SetAxisMove(vector<int> v_AxisID);

    int InitWaypoint();
    int PushWaypoint(vector<ST_AxisWayPointUnit> AxisPoints);
    int WayPointPushFinish(int nWayPointID);
    int ReadAxisInformation(string& sAxisInformation);
protected:
    void WayPoint_RT();



// HSTR programming runtime
public:
    int HSTR_TEST();
    //初始化HSTR
    int HSTR_Init();
    //创建应用
    int HSTR_CreateApplication(string AppName);
    // 切换应用。
    int HSTR_SelectedApplication(std::string AppName);

    int HSTR_AddProgram(const std::string& name);
    int HSTR_DeleteProgram(const std::string& name);

    int HSTR_AddFunction(const std::string& name);
    int HSTR_DeleteFunction(const std::string& name);

    int HSTR_AddTask(const std::string& name);
    int HSTR_DeleteTask(const std::string& name);

    // 管理枚举、结构体类型文件，
    int HSTR_AddTypeFile(const std::string& name);
    int HSTR_DeleteTypeFile(const std::string& name);
    int HSTR_AddGlobalVariableFile(const std::string& name);
    int HSTR_DeleteGlobalVariableFile(const std::string& name);

    // 编译当前应用，根据已加载的prj登记信息读取源文件和Task。
    int HSTR_CompileApplication();

    // 运行当前应用；debug=true时启用断点调试，仍需周期调用Cycle。
    int HSTR_RunApplication(bool debug = false);
    // 复位当前应用，停止执行并重置程序变量和Task统计。
    int HSTR_ResetProgram();
    // 添加断点，fileName为已登记的ST源文件名，line从1开始。
    int HSTR_AddBreakpoint(std::string fileName, uint32_t line);
    // 从断点继续执行。
    int HSTR_DebugContinue();
    // 单步步过：执行当前语句，停在同层或上层的下一暂停位置。
    int HSTR_DebugSingleStep();
    // 单步进入：当前语句必须是允许进入的独立函数或功能块调用。
    int HSTR_DebugStepInto();
    // 获取当前暂停位置，包含文件名、行号、调用深度、是否允许进入。
    int HSTR_GetDebugLocation(HSTR::DebugLocation& location);
    // 获取状态：-1未初始化，0未编译，1运行，2错误，3断点暂停，4就绪。
    int HSTR_GetApplicationStatus();
    // 移除断点，fileName为ST文件名，line从1开始。
    int HSTR_RemoveBreakpoint(std::string fileName, uint32_t line);
    // 停止当前应用，停止后不再执行ST，并复位程序变量和任务统计。
    int HSTR_StopApplication();
    // 读取变量，data为接收缓冲区，size必须与ST变量大小一致。
    int HSTR_ReadVariable(const std::string& variableName, void* data, size_t size);

    // 写入变量，data为待写入数据，size必须与ST变量大小一致。
    int HSTR_WriteVariable(const std::string& variableName, const void* data, size_t size);
    // 获取所有Task的执行次数、错过周期数、最近耗时和最大耗时。
    std::vector<HSTR::TaskStatistics> HSTR_GetTaskStatistics();
    // 获取当前选中的应用名称。
    string HSTR_GetSelectedApplication();
    // 获取最近一次运行错误，供前端展示。
    string HSTR_GetLastError();
    // 获取最近一次编译诊断，包括ST文件名、行号及编译错误。
    string HSTR_GetCompilerDiagnostics();
    // 获取各Task执行次数、错过周期数、执行耗时等统计。

    // 总线周期调用，执行当前Application中到期的Task。
    // 返回Application状态，-1表示Runtime未初始化。
    int HSTR_RT();
    HSTR::ApplicationStatus m_ApplicationStatus = HSTR::ApplicationStatus::NotCompiled;
protected:
    //执行的应用
    string m_sSelectedApp;
    string m_sApplicationPath = "runtime/Application";
    std::unique_ptr<HSTR::Runtime> m_pHSTRruntime;
    //允许app的字符
    std::regex m_NamePattern;
    //计时器
    uint64_t hstrLogicalTimeUs{};
    // 统一检查并执行文件新增、删除操作。
    int HSTR_EditProjectFile(const std::string& name, void (HSTR::Runtime::*operation)(const std::string&), const char* action);
    // 统一检查调试状态并执行调试命令。
    int HSTR_DebugExecute(void (HSTR::Runtime::*operation)(), const char* action, bool requireStepInto);

public:
    void SetCommandCallback(LLC_HandlecmdCBFunc callback) { m_pLLC_DispatchCmdFunc = callback; }
    bool InitializeController(const std::string& runtimeRoot = "runtime");
    void ShutdownController();
    std::recursive_mutex& ControllerMutex() { return m_controllerMutex; }
    bool AreSlavesConnected();
    bool HandleMasterCommand(const std::string& command, const std::vector<std::string>& parameters,
                             std::vector<std::string>& result);
    bool BindMaster(const std::string& id);
    LL::MasterRegistry& Masters() { return m_masters; }
    void SetSlaveInterface(const ST_EtherCATInterface& interface) { m_slaveInterface = interface; }
private:
    void ControllerCycle();
    bool ConnectActiveMaster();
    bool LaunchFileTransfer(bool write, int slave, const std::string& local,
                            const std::string& remote, std::uint32_t password);
    std::recursive_mutex m_controllerMutex;
    LL::MasterRegistry m_masters;
    std::string m_activeMaster;
    std::string m_runtimeRoot = "runtime";
    ST_EtherCATInterface m_slaveInterface{};
    bool m_initialized = false;
    int m_processLock = -1;
    std::atomic<bool> m_controllerRunning{false};
    std::thread m_controllerThread;
    std::future<int> m_fileTransfer;
    std::vector<std::uint8_t> m_virtualIOImage;
};





#define LLCRbt LLC_Rbt::instance()

#endif //_LLC_RobotBase_H
