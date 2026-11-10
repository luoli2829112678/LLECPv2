#ifndef _LLC_Slave_def_H
#define _LLC_Slave_def_H

#include <string.h>
#include "LLC_Robot_def.h"
#include "ESI_Parse.h"

#define ModeOP_Position 8
#define ModeOP_Tor 10

#pragma pack(push,1)

    // CiA 402标准对象Index
    constexpr int CONTROLWORD_INDEX = 0x6040;
    constexpr int STATUSWORD_INDEX = 0x6041;
    constexpr int ERROR_CODE_INDEX = 0x603F;
    constexpr int TARGET_POSITION_INDEX = 0x607A;
    constexpr int TARGET_VELOCITY_INDEX = 0x60FF;
    constexpr int TARGET_TORQUE_INDEX = 0x6071;
    constexpr int TARGET_OPERATION_MODE_INDEX = 0x6060;
    constexpr int ACTUAL_POSITION_INDEX = 0x6064;
    constexpr int ACTUAL_VELOCITY_INDEX = 0x606C;
    constexpr int ACTUAL_TORQUE_INDEX = 0x6077;
    constexpr int ACTUAL_OPERATION_MODE_INDEX = 0x6061;
    constexpr int DIGITAL_INPUTS_INDEX = 0x60FD;
    constexpr int DIGITAL_OUTPUTS_INDEX = 0x60FE;
    constexpr int DIGITAL_OUTPUTS_SUBINDEX = 0x01;
    constexpr int ADD_VELOCITY_INDEX = 0x60B1;
    constexpr int ADD_TORQUE_INDEX = 0x60B2;
    constexpr int TOUCH_PROBE_FUNCTION_INDEX = 0x60B8;
    constexpr int TOUCH_PROBE_STATUS_INDEX = 0x60B9;
    // CiA 402多轴对象字典偏移
    constexpr int AXIS_OBJECT_INCREMENT = 0x0800;

struct ST_LLPC_AxisPDO
{
    int nAxisID;
    uint32_t nTxPDO;
    uint32_t nRxPDO;
	ST_LLPC_AxisPDO():nAxisID(-1),nTxPDO(0),nRxPDO(0)
	{
	}
};



struct ST_LLPCSlaveConfigParam
{
    uint32_t nProductCode;
    uint32_t nRevisionNo;
	bool bEnableDC;
	uint32_t nShiftTime;
	vector<uint32_t> nTxProcessData;
	vector<uint32_t> nRxProcessData;
    vector<ST_LLPC_AxisPDO> vAxisPDOs;
	ST_LLPCSlaveConfigParam():nProductCode(0),nRevisionNo(0),bEnableDC(false),nShiftTime(0)
	{
		nTxProcessData.clear();
		nRxProcessData.clear();
        vAxisPDOs.clear();
	}
};






#pragma pack(pop)

#endif //_LLC_Slave_def_H