#pragma once
#include <cstdint>
#include <string>
#include <vector>


// 只暴露 HSTR 需要的最小接口（按需加）

int LLC_Test(int param);

//轴控制
int LLC_AxisEnable(int axisId);
int LLC_AxisDisable(int axisId);
int LLC_AxisReset(int axisId);


//轴运动
int LLC_AxisStop(int axisId);
int LLC_AxisAbsMove(int axisId,double pos,double vel,double acc,double dec,double jerk);
int LLC_AxisRelMove(int axisId,double pos,double vel,double acc,double dec,double jerk);
int LLC_AxisVelocityMove(int axisId,double vel,double acc,double dec,double jerk);
int LLC_AxisMoveSet(int axisId);
// 模拟对焦
int LLC_AxisStartAnalogFocusing(int axis_id);
int LLC_AxisStopAnalogFocusing(int axis_id);
// PEG
int LLC_AxisStartPEG(int axis_id);
int LLC_AxisStopPEG(int axis_id);


//轴设置
int LLC_AxisSetMovePos(int axisId,double pos);
int LLC_AxisSetMoveVel(int axisId,double vel);
int LLC_AxisSetMoveAcc(int axisId,double acc);
int LLC_AxisSetMoveDec(int axisId,double dec);
int LLC_AxisSetMoveJerk(int axisId,double jerk);

//轴读取
int LLC_AxisReadState(int axisId);

// HSTR legacy VOID commands throw on rejection instead of silently losing status.
int LLC_AxisEnableChecked(int axisId);
int LLC_AxisDisableChecked(int axisId);
double LLC_AxisGetActPos(int axisId);
int32_t LLC_SlavePDORead(int slave, int index, int subindex);
int LLC_SlavePDOWrite(int slave, int index, int subindex, int32_t value);
