#include "CIA402Axis.h"
#include "Axis/AxisControl/MC_PowerOn.h"
#include "Axis/AxisControl/MC_Reset.h"
#include <stdexcept>
#include <iostream>
static void check(bool condition, const char *text) {
    if (!condition)
        throw std::runtime_error(text);
}
struct TestAxis : CIA402Axis {
    TestAxis() : CIA402Axis(false) {}
    using CIA402Axis::PDOsynchronization;
    using CIA402Axis::Axis_ResetError;
    void SoftwareFault(){m_bError=true;m_nErrorCode=123;Axis_SetAxisState(motionState_errorstop);}
};
int main() {
    try {
        ST_CIA402_PDO image{};
        image.StatusWord = 0x40;
        image.ActualPosition = 1234;
        ST_SMCInitMap map;
        map.pControlword = &image.Controlword;
        map.pStatusWord = &image.StatusWord;
        map.pErrorCode = &image.ErrorCode;
        map.pActualPosition = &image.ActualPosition;
        map.pTargetPosition = &image.TargetPosition;
        map.pTargetModesOfOperation = &image.TargetModesOfOperation;
        map.pActualModesOfOperation = &image.ActualModesOfOperation;
        TestAxis axis;
        check(axis.Axis_InitMap(map) == 0, "PDO map initialization failed");
        MC_PowerOn power;
        bool busy = false, done = false, error = false;
        int code = 0;
        auto tick = [&](bool execute) {
            power(&axis, execute, busy, done, error, code);
            axis.PDOsynchronization();
            check(!error, "Power function block failed");
        };
        tick(false);
        tick(true);
        check(image.TargetModesOfOperation == 8, "CSP operation mode did not reach output PDO");
        check(image.TargetPosition == image.ActualPosition, "Enable did not align target to actual position");
        tick(true);
        check(image.Controlword == 6, "Shutdown command did not reach output PDO");
        image.StatusWord = 0x21;
        axis.PDOsynchronization();
        tick(true);
        check(image.Controlword == 7, "Switch-on command did not reach output PDO");
        image.StatusWord = 0x23;
        axis.PDOsynchronization();
        tick(true);
        check(image.Controlword == 15, "Enable operation did not reach output PDO");
        image.StatusWord = 0x27;
        axis.PDOsynchronization();
        tick(true);
        check(done, "Enable was not completed from drive feedback");
        axis.SoftwareFault();image.StatusWord=0x40;image.ErrorCode=0;axis.PDOsynchronization();
        MC_Reset reset;reset(&axis,false,busy,done,error,code);
        for(int n=0;n<8&&!done;++n){reset(&axis,true,busy,done,error,code);axis.PDOsynchronization();}
        check(done&&!error&&!axis.Axis_GetErrorState(),"Software error did not reset without restarting master");
        axis.SoftwareFault();image.StatusWord=0x40;image.ErrorCode=0x1234;axis.PDOsynchronization();
        reset(&axis,false,busy,done,error,code);done=false;
        for(int n=0;n<8&&!done;++n){reset(&axis,true,busy,done,error,code);axis.PDOsynchronization();}
        check(done&&!error&&!axis.Axis_GetErrorState(),"Latched diagnostic prevented fault reset");
        image.StatusWord=0x08;image.ErrorCode=0x1234;axis.PDOsynchronization();
        check(axis.Axis_ResetError()!=0,"Active drive fault was incorrectly cleared");
        std::cout << "PASS: mapped operation mode, position alignment, 0x06/0x07/0x0F and drive-feedback "
                     "completion\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
