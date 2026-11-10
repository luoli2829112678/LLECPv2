#ifndef _LLC_CMD_MSG_H
#define _LLC_CMD_MSG_H


//Msg Cmd
#define Cmd_UnInitialized                       "UnInitialized"
#define Cmd_ConnectSlaves                       "ConnectSlaves"
#define Cmd_DisconnectSlaves                    "DisconnectSlaves"

//新接口，符合认证标准的名称
#define Cmd_ReadOperationMode                   "ReadOperationMode"
#define Cmd_SetOperationMode                    "SetOperationMode"
#define Cmd_CycleHandle                         "CycleHandle"

#define Cmd_RobotEnable                         "GrpEnable"
#define Cmd_RobotDisable                        "GrpDisable"

// //添加
#define Cmd_ControllerStop                           "ControllerStop"   //停止
#define Cmd_ControllerReset                          "ControllerReset"  //复位
#define Cmd_AxisStop                            "AxisStop"



//***************************************************************************//
//** 定时状态检测
//***************************************************************************//
//Timer Cmd
// 检测是否触发急停 
#define Cmd_CheckIs_EmergencyStop               "CheckIs_EmergencyStop"
// 检测是否看门狗超时   
#define Cmd_CheckIs_WatchDog                    "CheckIs_WatchDog"
// 检测是否从站掉线 
#define Cmd_CheckIs_SlaveDrop                   "CheckIs_SlaveDrop"
// 检测是否报错 
#define Cmd_CheckIs_RobotError                  "CheckIs_RobotError "
//监测错误是否复位完成
#define Cmd_CheckIs_ResetFinish                  "CheckIs_ResetFinish "
// 检测是否急停处理完成 
#define Cmd_CheckIs_EmergencyStopHandled        "CheckIs_EmergencyStopHandled "
// 检测是否本体未上电   
#define Cmd_CheckIs_NotElectrify                "CheckIs_NotElectrify"
// 检测是否本体未上电   
#define Cmd_CheckIs_NotElectrifyInEmergencyStop "CheckIs_NotElectrifyInEmergencyStop"
// 检测是否本体上电完成 
#define Cmd_CheckIs_NotElectrifyHandled         "CheckIs_NotElectrifyHandled"
// 检测是否看门狗超时处理完成   
#define Cmd_CheckIs_WatchDogHandled             "CheckIs_WatchDogHandled "
// 检测是否错误处理完成 
#define Cmd_CheckIs_ErrorStoppingHandled        "CheckIs_ErrorStoppingHandled"
// 检测是否所有从站进入OP   
#define Cmd_CheckIs_SlavesOP                    "CheckIs_SlavesOP"
// 检测是否进入去使能   
#define Cmd_CheckIs_Disabled                    "CheckIs_Disabled"
// 检测是否进入使能 
#define Cmd_CheckIs_Enabled                     "CheckIs_Enabled "
// 检查是否处于准备就绪状态 
#define Cmd_CheckIs_Standby                     "CheckIs_Standby"
// 检测是否处于运动状态 
#define Cmd_CheckIs_Moving                      "CheckIs_Moving"
// 检测是否处于暂停中状态
#define Cmd_CheckIs_Pausing                     "CheckIs_Pausing"
// 检测是否继续完成
#define Cmd_CheckIs_ContinueDone                "CheckIs_ContinueDone"

//***************************************************************************//
//** 硬件配置
//***************************************************************************//
// 设置开启关闭机器人状态
#define	Cmd_StartMaster                         "StartMaster"
#define	Cmd_CloseMaster                         "CloseMaster"
#define	Cmd_ScanSlave                       "ScanSlave"
#define	Cmd_ReloadESI                           "ReloadESI"

// 设置模拟机器人   
#define	Cmd_SetSimulationType                   "SetSimulationType"
#define Cmd_SetEcatCycleTime                    "SetEcatCycleTime"
#define Cmd_SetIsConfigDC                       "SetIsConfigDC"
// 设置主站     
#define	Cmd_InitMaster                          "InitMaster"
//注册从站
#define	Cmd_RegistrationSlave                  "RegistrationSlave"   
//配置数据通道
#define	Cmd_ConfigureSlavePDO                   "ConfigureSlavePDO"
//注册轴
#define	Cmd_RegistrationAxis                    "RegistrationAxis"    
//注册IO
#define	Cmd_RegistrationIO                      "RegistrationIO"
// 设置机器人类型   
#define	Cmd_SetRobotModel                       "SetRobotModel"
// 设置机器人控制类型   
#define	Cmd_SetRobotControlType                 "SetRobotControlType"
// 设置从站类型 
#define	Cmd_SetSlaveType                        "SetSlaveType"

//** 其他
// 读取SDO
#define	Cmd_ReadSDO								"ReadSDO"
// 写入SDO
#define	Cmd_WriteSDO							"WriteSDO"
//升级固件
#define	Cmd_FOEwrite						"FOEwrite"
//获取升级固件进度
#define	Cmd_GetFOEwriteProgress			"GetFOEwriteProgress"
//读取固件
#define	Cmd_FOEread						"FOEread"
//获取读取进度
#define	Cmd_GetFOEreadProgress			"GetFOEreadProgress"
//升级xml
#define	Cmd_UpdateESI			        "UpdateESI"

//写入PDO
#define	Cmd_WritePDO							"WritePDO"

#define Cmd_SetIO                               "SetIO"


//***************************************************************************//
//** 运动指令
//***************************************************************************//
// 停止运动
#define	Cmd_GrpStop						        "GrpStop"
// 暂停运动
#define	Cmd_GrpInterrupt						"GrpInterrupt"
// 继续运动
#define	Cmd_GrpContinue						    "GrpContinue"

// dumpInfo 保存状态机转换表到文件指令
#define Cmd_DumpInfo                                "dumpInfo"


//轴控制指令
#define Cmd_AxisEnable                              "AxisEnable"
#define Cmd_AxisDisable                             "AxisDisable"
#define Cmd_AxisReset                               "AxisReset"
#define Cmd_AxisSetOffset                           "AxisSetOffset"
#define Cmd_AxisSetModesOfOperation                 "AxisSetModesOfOperation"
#define Cmd_AxisHome                                "AxisHome"
#define Cmd_AxisSetHomeParam                        "AxisSetHomeParam"
#define Cmd_AxisReadHomeParam                       "AxisReadHomeParam"
#define Cmd_AxisInitMotionMonitor                   "AxisInitMotionMonitor"

//轴运动指令
#define Cmd_AxisAbsMove                             "AxisAbsMove"
#define Cmd_AxisRelMove                             "AxisRelMove"
#define Cmd_AxisBackAndForthMove                    "AxisBackAndForthMove"
#define Cmd_AxisContinuousStepMove                  "AxisContinuousStepMove"
#define Cmd_AxisVelocityMove                        "AxisVelocityMove"




   

#define Cmd_SetAxisMovePos                              "SetAxisMovePos"
#define Cmd_SetAxisStopParam                            "SetAxisStopParam"
#define Cmd_SetAxisMoveVel                              "SetAxisMoveVel"
#define Cmd_SetAxisMoveAcc                              "SetAxisMoveAcc"
#define Cmd_SetAxisMoveDec                              "SetAxisMoveDec"
#define Cmd_SetAxisMoveJerk                             "SetAxisMoveJerk"
#define Cmd_SetAxisMove                                 "SetAxisMove"

#define Cmd_AxisFollowPosition                          "AxisFollowPosition"
#define Cmd_AxisFollowVelocity                          "AxisFollowVelocity"
#define Cmd_AxisFollowTorque                            "AxisFollowTorque"


#define Cmd_SetAccFFScale                               "SetAccFFScale"
#define Cmd_SetVelFFScale                               "SetVelFFScale"
#define Cmd_SetJerkFFScale                              "SetJerkFFScale"


#define Cmd_InitWayPoint                                "InitWayPoint"
#define Cmd_PushWayPoint                                "PushWayPoint"
#define Cmd_WayPointPushFinish                          "WayPointPushFinish"

//8894
#define Cmd_ReadAxisInformation                  "ReadAxisInformation"
#define Cmd_ReadGroupInformation                  "ReadGroupInformation"


//实时脚本


//HSTR
// Application管理
#define Cmd_HSTR_CreateApplication                 "HSTR_CreateApplication"
#define Cmd_HSTR_SelectedApplication               "HSTR_SelectedApplication"
#define Cmd_HSTR_AddProgram                        "HSTR_AddProgram"
#define Cmd_HSTR_DeleteProgram                     "HSTR_DeleteProgram"
#define Cmd_HSTR_AddFunction                       "HSTR_AddFunction"
#define Cmd_HSTR_DeleteFunction                    "HSTR_DeleteFunction"
#define Cmd_HSTR_AddTask                           "HSTR_AddTask"
#define Cmd_HSTR_DeleteTask                        "HSTR_DeleteTask"
#define Cmd_HSTR_AddTypeFile                       "HSTR_AddTypeFile"
#define Cmd_HSTR_DeleteTypeFile                    "HSTR_DeleteTypeFile"
#define Cmd_HSTR_AddGlobalVariableFile             "HSTR_AddGlobalVariableFile"
#define Cmd_HSTR_DeleteGlobalVariableFile          "HSTR_DeleteGlobalVariableFile"
#define Cmd_HSTR_CompileApplication                "HSTR_CompileApplication"
#define Cmd_HSTR_RunApplication                    "HSTR_RunApplication"
#define Cmd_HSTR_ResetProgram                      "HSTR_ResetProgram"
#define Cmd_HSTR_AddBreakpoint                     "HSTR_AddBreakpoint"
#define Cmd_HSTR_DebugContinue                     "HSTR_DebugContinue"
#define Cmd_HSTR_DebugSingleStep                   "HSTR_DebugSingleStep"
#define Cmd_HSTR_DebugStepInto                     "HSTR_DebugStepInto"
#define Cmd_HSTR_GetDebugLocation                  "HSTR_GetDebugLocation"
#define Cmd_HSTR_GetApplicationStatus              "HSTR_GetApplicationStatus"
#define Cmd_HSTR_RemoveBreakpoint                  "HSTR_RemoveBreakpoint"
#define Cmd_HSTR_StopApplication                   "HSTR_StopApplication"
#define Cmd_HSTR_ReadVariable                      "HSTR_ReadVariable"
#define Cmd_HSTR_WriteVariable                     "HSTR_WriteVariable"
#define Cmd_HSTR_GetSelectedApplication            "HSTR_GetSelectedApplication"
#define Cmd_HSTR_GetLastError                      "HSTR_GetLastError"
#define Cmd_HSTR_GetCompilerDiagnostics            "HSTR_GetCompilerDiagnostics"

#endif

#define Cmd_RegistrationMaster "RegistrationMaster"
#define Cmd_UnregisterMaster "UnregisterMaster"
#define Cmd_SelectMaster "SelectMaster"
#define Cmd_ListMasters "ListMasters"
#define Cmd_ReadMasterStatus "ReadMasterStatus"
#define Cmd_ReadSlaveStates "ReadSlaveStates"
#define Cmd_SetSlaveState "SetSlaveState"
#define Cmd_StartRegisteredMaster "StartRegisteredMaster"
#define Cmd_StopRegisteredMaster "StopRegisteredMaster"
