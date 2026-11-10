#pragma once
#define SMC_TIME_OUT 500 * 1000
//Functional Status of MC_InitResetAxis 

enum FS_InitResetAxis
{
    ReadyInitResetAxis,
    InitResetting,
    InitResetFinish,
    InitResetError,
};
//Functional Status of MC_ClearFault
enum FS_ClearFault
{
    ReadyClearFault,
    StartClearFault,
    ClearFaulting,
    ClearFaultFinish,
    ClearFaultError,
};
//Functional Status of MC_Reset
enum FS_Reset
{
    ReadyReset,
    StartReset,
    Reseting,
    ResetFinish,
    ResetError,
};
//Functional Status of MC_PowerOn
enum FS_PowerOn
{
    ReadyPowerOn,
    StartPowerOn,
    PowerOning_0x06,
    PowerOning_0x07,
    PowerOning_0x0F,
    PowerOnFinish,
    PowerOnError,
};
//Functional Status of MC_PowerOff
enum FS_PowerOff
{
    ReadyPowerOff,
    StartPowerOff,
    PowerOff_0x07,
    PowerOff_0x06,
    PowerOffFinish,
    PowerOffError,
};
//Functional Status of MC_Home

enum FS_HomeLimit
{
    ReadyHomeLimit,
    StartHomeLimit,
    LimitIOTrigger,
    HomeIOTrigger,
    HomeLimitFinish,
    HomeLimitError,
};

enum FS_HomeLimitStop
{
    ReadyHomeLimitStop,
    StartHomeLimitStop,
    HomeLimitStopFinish,
    HomeLimitStopError,
};


enum FS_Home
{
    ReadyHome,
    StartHome,
    Homing,
    HomeFinish,
    HomeError,
};

enum FS_Motion
{
    ReadyMotion,
    InMotion,
    MotionFinish,
    MotionError,
};


