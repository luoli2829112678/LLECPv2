#pragma once
#ifndef _LLC_DEF_H
#define _LLC_DEF_H

#include <stdint.h>
#include <vector>
#include <string>
#include <atomic>
#include <functional>
using namespace std;

#define MAX_PDO_SIZE 4096

#define Cmd_PrintPDO     "PrintPDO"

#define PORT_NORMALCMD      8888
#define PORT_FOLLOWMOTIONCMD    8889
#define PORT_HSTRCMD    8890
#define PORT_ASYNCCMD       8891
#define PORT_RTDATARPT      8893
#define PORT_REPLY          8894


enum EN_FOEworkType
{
    EN_FOEworkTypeNULL = 0,
    EN_FOEworkTypeFOEread = 1,
    EN_FOEworkTypeFOEwrite = 2,
    EN_FOEworkTypeWriteESI = 3,
};

typedef struct 
{
    string DriverName;
    uint32_t nProductCode;
    uint32_t nRevisionNo;
} SlaveInfo;

struct ST_FOE_Config
{
    std::atomic<bool> bFoECmd{false};   // 请求FoE独占EtherCAT
    EN_FOEworkType enFOEworkType = EN_FOEworkTypeNULL;

    int nSlave;
    string strFilePath;
    string strFoEFileName; 
    uint32_t nPassword;

    string strXmlPath;
};

typedef struct ecat_frame
{
    int nEtherCATMasterState;
    long long send_frame_count=0;
    long long send_frame_sec_count=0;
    long long frame_lost_count=0;
    long long rece_frame_error_count=0;
    long long send_frame_error_count=0;
    bool is_error_state=false;
    bool linkDisconnected=false;
    int error_code=0;
    int error_slaveID=0;
    int error_type = 0;
    int foe_file_size;
    int foe_send_size;
    double foeSendDataPercentage;
    double foeSendDataDone;
    // bool reset=false;
} ecat_frame;



using _ec_SDOread = std::function<int(uint16_t slave, uint16_t index, uint8_t subindex, uint8_t CA, int *psize, void *p, int timeout)>;
using _ec_SDOwrite = std::function<int(uint16_t slave, uint16_t index, uint8_t subindex, uint8_t CA, int psize, const void *p, int timeout)>;
// EtherCAT状态操作
using _ec_writestate = std::function<int(uint16_t slave)>;
using _ec_statecheck = std::function<uint16_t(uint16_t slave, uint16_t reqstate, int timeout)>;
using _ec_readstate = std::function<int(void)>;
// EEPROM读取
using _ec_readeeprom = std::function<uint32_t(uint16_t slave, uint16_t eeproma, int timeout)>;
// Fixed Position Write
using _ec_FPWR = std::function<int(uint16_t ADP, uint16_t ADO, uint16_t length, void *data, int timeout)>;
// FoE
using _ec_FOEwrite = std::function<int(uint16_t slave, char *filename, uint32_t password, int psize, void *p, int timeout)>;

using _ec_FOEdefinehook = std::function<int(void *hook)>;

struct ST_EtherCATInterface
{
    _ec_SDOread pSDORead = nullptr;                  // SDO读取接口
    _ec_SDOwrite pSDOWrite = nullptr;                // SDO写入接口

    _ec_writestate pWriteState = nullptr;            // EtherCAT状态写入接口
    _ec_statecheck pStateCheck = nullptr;            // EtherCAT状态检查接口
    _ec_readstate pReadState = nullptr;              // EtherCAT状态读取接口

    _ec_readeeprom pReadEEPROM = nullptr;            // EEPROM读取接口

    _ec_FPWR pFPWR = nullptr;                        // Fixed Position Write接口

    _ec_FOEwrite pFOEWrite = nullptr;                // FoE文件写入接口
    _ec_FOEdefinehook pFOEDefineHook = nullptr;      // FoE进度Hook注册接口
};
using _ec_MasterFunc = std::function<bool()>;
using _ec_FoEWriteFunc = std::function<bool(int nSlave, const std::string& strFilePath, const std::string& strFoEFileName, uint32_t nPassword)>;
using _ec_FoEReadFunc = std::function<bool(int nSlave, const std::string& strFilePath, const std::string& strFoEFileName, uint32_t nPassword)>;
using _ec_FoEWriteESIFunc = std::function<bool(int nSlave, const std::string& strESIPath)>;
using _ec_ScanSlaveFunc = std::function<bool(vector<SlaveInfo>& vSlaveInfo)>;


struct ST_EtherCATMasterControl
{
    _ec_MasterFunc pConnectSlaves;
    _ec_MasterFunc pDisconnectSlaves;
    _ec_MasterFunc pResetSlaves;
    _ec_ScanSlaveFunc pScanSlaves;
    _ec_FoEWriteFunc pFoEWrite;
    _ec_FoEReadFunc pFoERead;
    _ec_FoEWriteESIFunc m_pFoEWriteESI;
};




using LLC_HandlecmdCBFunc = std::function<int(const string& strCmd,const vector<string>& params,vector<string>& results)>;

using plugin_InitLLCMainFunc = std::function<int(LLC_HandlecmdCBFunc)>;

using plugin_SetMasterFunc = std::function<int(ST_EtherCATMasterControl stEtherCATMasterControl)>;
using plugin_SetEtherCATInterfaceFunc = std::function<int(ST_EtherCATInterface)>;

using plugin_ResetSlaveInfoFunc = std::function<int(int)>;
//using plugin_SetSlaveInfoFunc = std::function<int(int,void* pSlaveST,ST_slaveInfo&)>;
using plugin_SetSlaveInfoFunc = std::function<int(int,int,void* pSlaveST)>;

using plugin_ReadEtherCATConfigFunc = std::function<int(bool& bIsSimulation, string& sNetPort, float& fCycle,vector<bool>& bEnableDC,vector<double>& dShiftTime)>;
using plugin_HandleCyclicJobFunc = std::function<int(long)>;
using plugin_HandleEcatCheckFunc = std::function<int(ecat_frame& ecat_frame)>;

//using plugin_SetLLC_HandlecmdCBFunc = std::function<int(LLC_HandlecmdCBFunc)>;
using LLC_CmdFunc = std::function<int(const vector<string>& params,vector<string>& results)>;





#endif
inline constexpr const char* IFEnd_Flag = ";";
inline constexpr const char* IFSplit_Flag = ",";
