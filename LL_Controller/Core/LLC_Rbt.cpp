#include "LLC_Rbt.h"
#include "LL_CpsService.h"
#include "LL_CpsData.h"
#include "LLC_IF.h"

#include <unistd.h>
#include "LLC_Robot_def.h"
#include "LLC_DataSheetSrv.h"
#include <chrono>
#include <string>
#include <vector>


Build_Singleton(LLC_Rbt)
_ec_SDOread LLC_Rbt::m_pFuncSDORead=nullptr;
_ec_SDOwrite LLC_Rbt::m_pFuncSDOWrite=nullptr;
LLC_Rbt::LLC_Rbt():m_pLLC_DispatchCmdFunc(nullptr), m_CPSClient("127.0.0.1", 10003),m_bEnableCPS(false),
m_cycle_time(1.0),m_pJitterMonitor(nullptr)
{
    vector<uint16_t> vListenPort = {PORT_NORMALCMD,  PORT_FOLLOWMOTIONCMD, PORT_HSTRCMD, PORT_ASYNCCMD, PORT_REPLY};
    for (size_t i = 0; i < vListenPort.size(); i++)
    {
        LLCIFHandle ifhandle(vListenPort[i]);
        m_IFHandleMap.insert(std::make_pair(vListenPort[i], ifhandle));
    }
    m_R_Trig_ReadSDO.R_TRIG(false);
    m_pJitterMonitor = new JitterMonitor(1000);
    m_vAxis.reserve(MaxAxisNum);
}
LLC_Rbt::~LLC_Rbt()
{
}

// 初始化LLC线程
int LLC_Rbt::Initialize_LLCThreads() {
    if (m_controllerRunning.exchange(true)) return REC_Successed;
    try { m_controllerThread = std::thread([this] { ControllerCycle(); }); }
    catch (...) { m_controllerRunning = false; return REC_ControllerNotInit; }
    return REC_Successed;
}




// 初始化LLCIF接口
int LLC_Rbt::Initialize_LLCIF(LLC_HandlecmdCBFunc pLLC_DispatchCmdFunc)
{
    m_pLLC_DispatchCmdFunc = pLLC_DispatchCmdFunc;
    //初始化命令列表
    InitReadCmdList();
    //连接CPS 10003
    m_CPSClient.Connect();
    //初始化Socket侦听
    for (auto& it : m_IFHandleMap)
    {
        const int listenResult=LLNet_Listen(it.first, &(it.second));
        if(listenResult != 0)  //只在此IP侦听
        {
            int nErr=listenResult;
            printf("listen IF protocol %d port error:%d, %s\n", it.first, nErr, strerror(nErr));
            return nErr;
        }
        printf("listen IF protocol %d port\n", it.first);
    }
    //初始化实时数据上报Socket侦听
    LLC_DataSheetSrv dataSheet;
    int nCT = 1000;  //1ms发送一次
    dataSheet.SetSrv(nCT, false);
    const int listenResult=LLNet_Listen(PORT_RTDATARPT, &dataSheet);
    if(listenResult != 0)
    {
        int nErr=listenResult;
        printf("listen LL protocol %d port error:%d, %s\n", PORT_RTDATARPT, nErr, strerror(nErr));
        return nErr;
    }
    printf("datasheet port %d, frequency: %d us\n", PORT_RTDATARPT, nCT);
    return REC_Successed;
}


int LLC_Rbt::Initialize_ESI()
{
     namespace fs = std::filesystem;              // 文件系统命名空间别名
    const fs::path directoryPath = LL::CpsDataRoot() / "ESI"; // ESI文件目录
    fs::create_directories(directoryPath);
    if(!m_vSlaves.empty())for(auto& parser:m_vESI_Parse)m_retiredESI.push_back(std::move(parser));
    m_vESI_Parse.clear();                       
    std::vector<fs::path> xmlFiles;              // 保存找到的XML文件路径
    try
    {
        if (!fs::exists(directoryPath))
        {
            std::cerr << "ESI文件夹不存在: " << directoryPath << '\n';
            return REC_ControllerNotInit;
        }
        if (!fs::is_directory(directoryPath))
        {
            std::cerr << "指定路径不是文件夹: " << directoryPath << '\n';
            return REC_ControllerNotInit;
        }
        for (const auto& entry : fs::directory_iterator(directoryPath))
        {
            if (!entry.is_regular_file())
            {
                continue;
            }
            std::string extension = entry.path().extension().string();
            std::transform(extension.begin(), extension.end(), extension.begin(),
                [](unsigned char ch)
                {
                    return static_cast<char>(std::tolower(ch));
                });

            if (extension == ".xml")
            {
                xmlFiles.push_back(entry.path());
            }
        }
    }
    catch (const fs::filesystem_error& error)
    {
        std::cerr << "遍历ESI文件夹失败: " << error.what() << '\n';
        return REC_ControllerNotInit;
    }
    std::sort(xmlFiles.begin(), xmlFiles.end()); // 固定XML加载顺序
    m_vESI_Parse.reserve(xmlFiles.size());       // unique_ptr支持移动，可以正常预分配
    size_t successCount = 0;                     // 加载成功数量
    size_t failedCount = 0;                      // 加载失败数量
    for (const auto& xmlPath : xmlFiles)
    {
        const std::string fileName = xmlPath.filename().string();
        auto esi = std::make_unique<ESI_Parse>(); // 在堆上创建ESI_Parse对象

        if (!esi->loadFromFile(xmlPath.string()))
        {
            ++failedCount;
            std::cerr << "加载失败: " << fileName << '\n';
            continue;
        }
        m_vESI_Parse.emplace_back(std::move(esi)); // 转移智能指针，不复制ESI_Parse
        ++successCount;
        std::cout << "加载成功: " << fileName << '\n';
    }
    std::cout << "共扫描XML文件: " << xmlFiles.size() << "，加载成功: " << successCount
              << "，加载失败: " << failedCount << "，共加载: " << m_vESI_Parse.size() << '\n';
    return failedCount == 0 ? REC_Successed : REC_ControllerNotInit;     
}

// 初始化共享内存
int LLC_Rbt::Initialize_LLCShm()
{
    char sSharedMemReadName[100] = "/LLECP_Controller_SharedMemory";

    int fd = shm_open(sSharedMemReadName, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    if (fd == -1)
    {
        return REC_SHMNULL;
    }
	int nSHMLen = sizeof(ST_ShareMemory);//sizeof(*m_pShmRW)
    if (ftruncate(fd, nSHMLen) != 0) { close(fd); return REC_SHMNULL; }
    size_t nSize = sizeof(*m_pLLC_ShmRW);
    m_pLLC_ShmRW = (ST_ShareMemory *)mmap(0, nSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
	if (m_pLLC_ShmRW==nullptr||m_pLLC_ShmRW==MAP_FAILED)
	{
		m_pLLC_ShmRW=nullptr;
		return REC_SHMNULL;
	}
	new (m_pLLC_ShmRW) ST_ShareMemory{};
	m_pLLC_CPS2LLCMemory = &(m_pLLC_ShmRW->ShmCPS2LLC);
    m_pLLC_LLC2CPSMemory = &(m_pLLC_ShmRW->ShmLLC2CPS);
    //初始化控制器状态
    m_pLLC_LLC2CPSMemory->st_StatusArea.nrECAT_Status[enECAT_LLC_Machine] = enCDSState_UnInitialized;
    return REC_Successed;
}
// 获取共享内存写地址
void* LLC_Rbt::GetLLCWriteMemory()
{
    return &(m_pLLC_ShmRW->ShmCPS2LLC);
}
// 获取共享内存读地址
void* LLC_Rbt::GetLLCReadMemory()
{
    return &(m_pLLC_ShmRW->ShmLLC2CPS);
}


// 设置总线周期时间,单位：us(微秒)
int LLC_Rbt::InitMaster(const string sNetPort,const int nCycleTime)
{
    auto registered = m_masters.Get(m_activeMaster);
    if (!registered || registered->Config().endpoint != sNetPort || nCycleTime < 100 ||
        static_cast<unsigned>(nCycleTime) != registered->Config().cycleUs) return REC_ControllerNotInit;
    m_cycle_time = static_cast<float>(nCycleTime) / 1000.0;
    
    // 重新创建JitterMonitor对象
    if (m_pJitterMonitor) {
        delete m_pJitterMonitor;
    }
    m_pJitterMonitor = new JitterMonitor(m_cycle_time * 1000.0);
    m_stRbtEtherCATParams.sEthernetPort = sNetPort;
    m_stRbtEtherCATParams.dEtherCatTimeCycle = m_cycle_time / MS_TO_SECOND; // 与总线周期保持一致
    m_stRobotControlStatus.dEtherCatTimeCycle = m_cycle_time; 
    return REC_Successed;
}
// 获取总线周期时间
int LLC_Rbt::GetEcatCycleTime(float& fCycleTime)
{
    fCycleTime = static_cast<float>(m_cycle_time);
    return REC_Successed;
}

// 设置开启和关闭主站函数
void LLC_Rbt::SetMaster(ST_EtherCATMasterControl stEtherCATMasterControl)
{
    m_stEtherCATMasterControl = stEtherCATMasterControl;
}
// 设置SDO函数
void LLC_Rbt::SetSDOFuncCB(_ec_SDOread pSDORead, _ec_SDOwrite pSDOWrite)
{
    m_pFuncSDORead = pSDORead;
    m_pFuncSDOWrite = pSDOWrite;
}

// 读取DC配置
void LLC_Rbt::ReadEtherCATConfig(bool& bIsSimulation, string& sNetPort, float& fCycle,vector<bool>& bEnableDC,vector<double>& dShiftTime)
{
    bEnableDC.clear();
    dShiftTime.clear();
    bIsSimulation = m_stRbtEtherCATParams.bSimulationType;
    for (size_t i = 0; i < m_vSlaveConfigParam.size(); i++)
    {
        bEnableDC.push_back(m_vSlaveConfigParam[i].bEnableDC);
        dShiftTime.push_back(m_vSlaveConfigParam[i].nShiftTime);
    }
    sNetPort = m_stRbtEtherCATParams.sEthernetPort;
    fCycle = m_cycle_time;
}
// 复位主站
int LLC_Rbt::ResetMaster()
{
    //控制器复位会调用
    if(m_stEtherCATMasterControl.pResetSlaves == nullptr)
    {
        return -1;
    }
    m_stEtherCATMasterControl.pResetSlaves();
    return REC_Successed;
}

// 设置启动和关闭运行控制器
int LLC_Rbt::SetRunControllerState(const int nState)
{
    m_bFoEWorking = false;
    if (m_stEtherCATMasterControl.pConnectSlaves == nullptr || m_stEtherCATMasterControl.pDisconnectSlaves == nullptr)
    {
        return -1;
    }
    switch(nState)
    {
        case enum_StatusLow:
            m_bMasterState = false;
            // 关闭控制器时，先去使能
            if (m_fsmRbt.currentState->unStateCode <= enCDSState_ControllerNotConnect)
            {
                return REC_ControllerAlreadyClosed;
            }
            if(!m_stEtherCATMasterControl.pDisconnectSlaves())
            {
                return REC_ControllerAlreadyClosed;
            }
            break;
        case enum_StatusHigh:
            if(!CheckSlaveConfigConsistency())
            {
                printf("从站一致性检测失败，启动主站失败\n");
                m_stRobotControlStatus.bStatus_ControllerError = true;
                m_stShmRobotRTStatus.nErrorID = CEC_SlaveDropped;
                m_stRobotControlStatus.bStatus_SlaveDropError = true;
                CheckSlaveDropError();
                return REC_ControllerNotInit;
            }
            if(!m_stEtherCATMasterControl.pConnectSlaves())
            {
                m_stRobotControlStatus.bStatus_ControllerError = true;
                m_stRobotControlStatus.bStatus_SlaveDropError = true;
                CheckSlaveDropError();
                return REC_ControllerNotInit;
            }
            m_bMasterState = true;
            // 复位总线
            ResetMaster();
            for (size_t i = 0; i < m_vAxisGruop.size(); i++)
            {
                m_vAxisGruop[i].ResetError();
            }
            break;
        default:
            return -1;
    }
    
    return REC_Successed;
}

void LLC_Rbt::ReadRbtInfoFromIOToShm()
{
    for(uint32_t i = 0;i < m_vIOs.size();i++)
    {
        bool value;
        m_vIOs[i].IO_Read(value);
        m_pLLC_LLC2CPSMemory->ioValue[i] = value;
    }
}

// 读取信息
void LLC_Rbt::ReadRbtInfoFromAxisToShm()
{

    // 读取错误码
    m_pLLC_LLC2CPSMemory->st_StatusArea.nError_Code = m_stShmRobotRTStatus.nErrorID;
    //printf("ErrorCode:%d\n",m_pLLC_LLC2CPSMemory->st_StatusArea.nError_Code);
    // 读取错误轴ID
    m_pLLC_LLC2CPSMemory->st_StatusArea.nError_AxisID = m_stShmRobotRTStatus.nErrorAxisID;
    // 读取发送帧数量
    m_pLLC_LLC2CPSMemory->st_StatusArea.nECAT_SendFrameCNT = m_stShmRobotRTStatus.nECAT_SendFrameCNT;
    // 读取发送帧频率
    m_pLLC_LLC2CPSMemory->st_StatusArea.nECAT_SendFramesPerSecond = m_stShmRobotRTStatus.nECAT_SendFramesPerSecond;
    // 备用
    m_pLLC_LLC2CPSMemory->st_StatusArea.nECAT_Reserve = m_stShmRobotRTStatus.nECAT_Reserve;
    // 读取丢帧数量
    m_pLLC_LLC2CPSMemory->st_StatusArea.nECAT_TotalLostFrames = m_stShmRobotRTStatus.nECAT_TotalLostFrames;
    // 读取TxPdo错误数量
    m_pLLC_LLC2CPSMemory->st_StatusArea.nECAT_TxErrorCNT = m_stShmRobotRTStatus.nECAT_TxErrorCNT;
    // 读取RxPdo错误数量
    m_pLLC_LLC2CPSMemory->st_StatusArea.nECAT_RxErrorCNT = m_stShmRobotRTStatus.nECAT_RxErrorCNT;
    //FOE烧录进度
    m_pLLC_LLC2CPSMemory->st_StatusArea.dFOEworkProgress = m_dFOEworkProgress;
    //foe烧录完成
    m_pLLC_LLC2CPSMemory->st_StatusArea.nFoeOperationCompleted = m_bFoEWorkingDone;
    //读取轴参数
    for(uint32_t i = 0;i < m_nConfiguredAxisNum;i++)
    {
        m_pLLC_LLC2CPSMemory->stAxisData[i].nAxisID = i;
        m_pLLC_LLC2CPSMemory->stAxisData[i].nAxisStatus = (int)m_vAxis[i]->Axis_GetAxisMotionState();
        m_pLLC_LLC2CPSMemory->stAxisData[i].nAxisMoveState = (int)m_vAxis[i]->Axis_GetMovingState();
        m_pLLC_LLC2CPSMemory->stAxisData[i].nAxisErrorCode = m_vAxis[i]->Axis_GetAxisErrorID();
        m_pLLC_LLC2CPSMemory->stAxisData[i].dCmdPos = m_vAxis[i]->Axis_GetSetPosition();
        m_pLLC_LLC2CPSMemory->stAxisData[i].dCmdVel = m_vAxis[i]->Axis_GetSetVelocity();
        m_pLLC_LLC2CPSMemory->stAxisData[i].dCmdAcc = m_vAxis[i]->Axis_GetSetAcceleration();
        m_pLLC_LLC2CPSMemory->stAxisData[i].dCmdJerk = m_vAxis[i]->Axis_GetSetAcceleration();
        m_pLLC_LLC2CPSMemory->stAxisData[i].dCmdTor = m_vAxis[i]->Axis_GetSetTorque();
        m_pLLC_LLC2CPSMemory->stAxisData[i].nCmdAxisModesOfOperation = m_vAxis[i]->Axis_GetCommandOperationMode();
        m_pLLC_LLC2CPSMemory->stAxisData[i].dActPos = m_vAxis[i]->Axis_GetActualPosition();
        m_pLLC_LLC2CPSMemory->stAxisData[i].dActVel = m_vAxis[i]->Axis_GetActualVelocity();
        m_pLLC_LLC2CPSMemory->stAxisData[i].dActAcc = m_vAxis[i]->Axis_GetActualAcceleration();
        m_pLLC_LLC2CPSMemory->stAxisData[i].dActJerk = m_vAxis[i]->Axis_GetActualJerk();
        m_pLLC_LLC2CPSMemory->stAxisData[i].dActTor = m_vAxis[i]->Axis_GetActualTorque();
        m_pLLC_LLC2CPSMemory->stAxisData[i].nActAxisModesOfOperation = m_vAxis[i]->Axis_GetActualOperationMode();

    }
    if(m_stRbtEtherCATParams.bSimulationType)
        return ;
    // 当前写入的共享内存 PDO 数量
    size_t nPDOCount = 0;

    // 共享内存中 PDO 槽位的最大数量
    const size_t nMaxPDOCount =sizeof(m_pLLC_LLC2CPSMemory->stPDOData) /sizeof(m_pLLC_LLC2CPSMemory->stPDOData[0]);

    // 遍历全部已配置从站
    for (size_t i = 0;i < static_cast<size_t>(m_nConfiguredSlaveNum) && i < m_vSlaves.size();++i)
    {
        // 获取当前从站对象
        auto* pSlave = m_vSlaves[i];
        // 从站对象无效，跳过
        if (pSlave == nullptr)
            continue;
        /*
        * 处理一个方向的PDO列表。
        * TxPDO和RxPDO必须分别调用一次。
        * 每次调用时，nSlaveBitOffset都会重新从0开始。
        */
        auto CopyPDOListToSharedMemory =[&](const auto* pPDOList)
            {
                // PDO列表无效，直接返回
                if (pPDOList == nullptr)
                    return;
                // 当前方向在本从站过程数据区中的累计位偏移
                size_t nSlaveBitOffset = 0;
                // 遍历当前方向的全部PDO
                for (const auto& pdo : *pPDOList)
                {
                    // 遍历当前PDO中的全部Entry
                    for (const auto& entry : pdo.entries)
                    {
                        // 记录当前Entry的起始位偏移
                        const size_t nEntryStartBit = nSlaveBitOffset;
                        // 无论Entry是否有效，都要推进位偏移
                        nSlaveBitOffset += static_cast<size_t>(entry.bitLen);
                        // Entry没有数据地址或长度为0，跳过
                        if (entry.pDataPtr == nullptr || entry.bitLen == 0)
                            continue;
                        // 计算保存Entry数据需要的字节数
                        const size_t nPDOByteSize = (static_cast<size_t>(entry.bitLen) + 7U) / 8U;
                        // Entry超过共享内存单个PDO值的容量，跳过
                        if (nPDOByteSize > sizeof(UN_PDOValue))
                            continue;
                        // 防止超出共享内存PDO数组范围
                        if (nPDOCount >= nMaxPDOCount)
                            return;
                        // 获取当前共享内存PDO槽位
                        ST_PDO_Unit& destination = m_pLLC_LLC2CPSMemory->stPDOData[nPDOCount];
                        // 清空整个槽位
                        std::memset(&destination, 0, sizeof(destination));
                        // 保存从站ID
                        destination.nSlaveID = i + 1;
                        // 保存对象字典索引
                        destination.nIndex = entry.index;
                        destination.nSubIndex = entry.subIndex;
                        // 当前 Entry 所在的首字节地址；它已经包含字节偏移。
                        const auto* pSourceBytes = reinterpret_cast<const unsigned char*>(entry.pDataPtr);

                        // 将目标联合体作为字节数组访问。
                        auto* pDestinationBytes = reinterpret_cast<unsigned char*>(&destination.unPDOValue);

                        // 只取首字节内部的起始位偏移，范围为 0～7；截图中为 0。
                        const size_t nStartBitInByte = static_cast<size_t>(entry.bitOffsetInByte);

                        // 获取当前 Entry 需要复制的总位数；截图中为 32。
                        const size_t nBitCount = static_cast<size_t>(entry.bitLen);

                        // 逐位复制当前 Entry 的数据。
                        for (size_t nBit = 0; nBit < nBitCount; ++nBit)
                        {
                            // 相对于 entry.pDataPtr 计算位置，只加字节内部的偏移。
                            const size_t nSourceBit = nStartBitInByte + nBit;

                            // 除以 8 得到源字节编号，取余得到该字节内的位编号。
                            const unsigned char nSourceByte = pSourceBytes[nSourceBit / 8U];

                            // 将所需位移到最低位，再通过掩码取得 0 或 1。
                            const unsigned char nBitValue =static_cast<unsigned char>((nSourceByte >> (nSourceBit % 8U)) & 0x01U);

                            // 将这一位写入目标；前面的 memset 必须保留，确保目标初始为 0。
                            pDestinationBytes[nBit / 8U] |=static_cast<unsigned char>(nBitValue << (nBit % 8U));
                        }
                        // 指向下一个共享内存PDO槽位
                        ++nPDOCount;
                    }
                }
            };

        // 处理TXpdo
        CopyPDOListToSharedMemory(pSlave->GetTXPDOList());

        // 处理RXPDO
        CopyPDOListToSharedMemory(pSlave->GetRXPDOList());
    }

    // for (size_t i = 0; i <= 8 ; ++i)
    // {
    //     printf("Slave[%zu] = %d    ", i, static_cast<int>(m_pLLC_LLC2CPSMemory->stPDOData[i].unPDOValue.int32Value));
    // }
    // printf("\n");
}


// 更新处理总线数据
int LLC_Rbt::HandleEcatCheck(ecat_frame& update_ecat_shm)
{
    // if(update_ecat_shm.send_frame_count == 0){
    // printf("总线负载： 发送帧数量： %d, 每秒发送帧数量： %d,\
    //     丢帧数量： %d, 接受PDO错误数量: %d, 发送PDO错误数量: %d\n",
    //     update_ecat_shm.send_frame_count, update_ecat_shm.send_frame_sec_count,
    //     update_ecat_shm.frame_lost_count, update_ecat_shm.rece_frame_error_count, update_ecat_shm.send_frame_error_count);
    // }

    m_stShmRobotRTStatus.nECAT_SendFrameCNT = update_ecat_shm.send_frame_count;
    m_stShmRobotRTStatus.nECAT_SendFramesPerSecond = update_ecat_shm.send_frame_sec_count;
    m_stShmRobotRTStatus.nECAT_Reserve = 0;
    m_stShmRobotRTStatus.nECAT_TotalLostFrames = update_ecat_shm.frame_lost_count;
    m_stShmRobotRTStatus.nECAT_TxErrorCNT = update_ecat_shm.rece_frame_error_count;
    m_stShmRobotRTStatus.nECAT_RxErrorCNT = update_ecat_shm.send_frame_error_count;
    m_dFOEworkProgress = update_ecat_shm.foeSendDataPercentage;
    m_bFoEWorkingDone = update_ecat_shm.foeSendDataDone;
    m_stShmRobotRTStatus.stECATStatus.nECATStatusMachine = update_ecat_shm.nEtherCATMasterState;
    if(m_bFoEWorkingDone)
    {
        m_bFoEWorking = false;
    }
    if(update_ecat_shm.linkDisconnected && m_bMasterState && (m_fsmRbt.currentState->unStateCode >= enCDSState_WatchDog) )
    {
        // 设置控制器为错误状态
        m_stRobotControlStatus.bStatus_ControllerError = true;
        //m_pLLC_LLC2CPSMemory->st_StatusArea.nError_Code = update_ecat_shm.error_code;
        m_pLLC_LLC2CPSMemory->st_StatusArea.nError_AxisID = update_ecat_shm.error_slaveID;
        m_stRobotControlStatus.bStatus_SlaveDropError = true;
        //状态重置
        // 前期调试用
        return 1;
    }
    if (m_pJitterMonitor == nullptr) 
    {
        return -1; // 监测器未初始化
    }
    // 1. 更新周期时间戳和抖动计算
    m_pJitterMonitor->updateCycleTimestamp();

    // 2. 检查是否触发报警
    bool alarm_triggered = m_pJitterMonitor->checkAndTriggerAlarm();
    // 3.共享内存
    // 4. 如果触发报警且真机状态，执行保护动作
    if (alarm_triggered && m_stRbtEtherCATParams.bSimulationType == false) 
    {
        // 错误处理流程
        // return REC_Jittermonitoring; // 表示触发了报警
        return REC_RobotSlavesVerError;
    }
    return REC_Successed;   
}

// 读取状态信息
void LLC_Rbt::ReadRbtInfoFromRbtStatusToShm()
{

    // 设置看门狗数据
    m_pLLC_LLC2CPSMemory->st_StatusArea.hstrApplicationStatus = static_cast<int>(m_ApplicationStatus);
    m_pLLC_LLC2CPSMemory->st_StatusArea.readCNT ++;
    // 设置版本号
    m_pLLC_LLC2CPSMemory->st_StatusArea.nVersion = Version;

    // 去使能状态
    // 准备就绪状态
    m_pLLC_LLC2CPSMemory->st_StatusArea.nrRobot_Status[enRS_StandBy] = static_cast<int>(m_stShmRobotRTStatus.stRobotStatus.bStandBy);
    // 运动状态
    m_pLLC_LLC2CPSMemory->st_StatusArea.nrRobot_Status[enRS_Moving] = static_cast<int>(m_stShmRobotRTStatus.stRobotStatus.bMoving);
    // 使能状态
    // 停止中状态
    m_pLLC_LLC2CPSMemory->st_StatusArea.nrRobot_Status[enRS_Stopping] = static_cast<int>(m_stShmRobotRTStatus.stRobotStatus.bStopping);
    // 错误停止状态
    m_pLLC_LLC2CPSMemory->st_StatusArea.nrRobot_Status[enRS_ErrorStop] = static_cast<int>(m_stShmRobotRTStatus.stRobotStatus.bErrorStop);
    // 错误状态
    m_pLLC_LLC2CPSMemory->st_StatusArea.nrRobot_Status[enRS_Error] = static_cast<int>(m_stShmRobotRTStatus.stRobotStatus.bError);



    // 总线运行
    m_pLLC_LLC2CPSMemory->st_StatusArea.nrECAT_Status[enECAT_BUS_Running] = static_cast<int>(m_stShmRobotRTStatus.stECATStatus.bECATBUSRunning);
    // 控制器状态机
    m_pLLC_LLC2CPSMemory->st_StatusArea.nrECAT_Status[enECAT_LLC_Machine] = m_fsmRbt.currentState->unStateCode;
    // 模拟状态
    m_pLLC_LLC2CPSMemory->st_StatusArea.nrECAT_Status[enECAT_SimulationMode] = static_cast<int>(m_stShmRobotRTStatus.stECATStatus.bECATSimulationMode);

    
    return;
}
// 同步 8893 数据
void LLC_Rbt::ReadRbtInfoTo8893()
{
    m_stRtData.nStatusMachine = m_fsmRbt.currentState->unStateCode;
    m_stRtData.CNT = m_pLLC_LLC2CPSMemory->st_StatusArea.readCNT;
    for (size_t i = 0; i < m_vAxis.size(); i++)
    {
        auto& rt  = m_stRtData.AxisData[i];
        auto& ax  = m_vAxis[i];

        rt.nAxisStatus              = ax->Axis_GetAxisMotionState();
        rt.nAxisMoveState           = ax->Axis_GetMovingState();
        rt.nAxisErrorCode           = ax->Axis_GetAxisErrorID();

        rt.nCmdAxisModesOfOperation = ax->Axis_GetCommandOperationMode();
        rt.nActAxisModesOfOperation = ax->Axis_GetActualOperationMode();

        rt.dCmdPos = ax->Axis_GetSetPosition();
        rt.dCmdVel = ax->Axis_GetSetVelocity();
        rt.dCmdAcc = ax->Axis_GetSetAcceleration();
        rt.dCmdJerk = ax->Axis_GetSetJerk();
        rt.dCmdTor = ax->Axis_GetSetTorque();

        rt.dActPos = ax->Axis_GetActualPosition();
        rt.dActVel = ax->Axis_GetActualVelocity();
        rt.dActAcc = ax->Axis_GetActualAcceleration();
        rt.dActJerk = ax->Axis_GetActualJerk();
        rt.dActTor = ax->Axis_GetActualTorque();
    }
    { std::lock_guard<std::mutex> lock(m_8893dataMutex);
    m_sRtData8893 = string(reinterpret_cast<char*>(&m_stRtData), sizeof(RobotRTData)); }
    //通知8893线程数据发送
    m_8893dataCV.notify_one();
}

int LLC_Rbt::ReadAxisInformation(string& sAxisInformation)
{
    const auto* data = reinterpret_cast<const unsigned char*>(&m_pLLC_LLC2CPSMemory->stAxisData);
    constexpr char digits[]="0123456789abcdef";
    sAxisInformation.clear();
    for(std::size_t i=0;i<sizeof(m_pLLC_LLC2CPSMemory->stAxisData);++i){sAxisInformation+=digits[data[i]>>4];sAxisInformation+=digits[data[i]&15];}
    return REC_Successed;
}

// 循环处理线程
int LLC_Rbt::HandleCyclicJob(long nCurTickUSEC)
{
    //获取当前系统时间
    m_stRbtAppParams.nSystemTime_TaskStart = 0;
    // 获取任务间隔时间
    m_stRbtAppParams.nSystemTime_TaskCycleTime =  m_stRbtAppParams.nSystemTime_TaskStart -  m_stRbtAppParams.nSystemTime_LastTaskStart;
    // 获取最大任务执行间隔时间
    if (m_stRbtAppParams.nSystemTime_TaskCycleTime >   m_stRbtAppParams.nSystemTime_MaxTaskCycleTime)
    {
        m_stRbtAppParams.nSystemTime_MaxTaskCycleTime =   m_stRbtAppParams.nSystemTime_TaskCycleTime;
    }
    // 获取最小任务执行间隔时间
    if (m_stRbtAppParams.nSystemTime_TaskCycleTime <  m_stRbtAppParams.nSystemTime_MinTaskCycleTime)
    {
        m_stRbtAppParams.nSystemTime_MinTaskCycleTime =  m_stRbtAppParams.nSystemTime_TaskCycleTime;
    }
    // 获取上一周期任务时间
    m_stRbtAppParams.nSystemTime_LastTaskStart =  m_stRbtAppParams.nSystemTime_TaskStart;

    //SoftMotionRun运行
    LLCSoftMotion.SoftMotionRun();

    // 循环处理轴组相关信息
    for (size_t i = 0; i < m_vAxisGruop.size(); i++)
    {
        m_vAxisGruop[i].AxisGruop_RT();
    }
    for (size_t i = 0; i < m_vSlaves.size(); i++)
    {
        m_vSlaves[i]->MC_SlaveAxisRT();
    }
    //轴操作
    AxisControlFSM();
    // 读取轴组实际信息写入共享内存
    ReadRbtInfoFromAxisToShm();
    // 读取机器人控制状态写入共享内存
    ReadRbtInfoFromRbtStatusToShm();
    //读取IO写入共享内存
    ReadRbtInfoFromIOToShm();
    // 发送数据到8893端口
    ReadRbtInfoTo8893();
    //script
    WayPoint_RT();
    //实时脚本

    //HSTR
    // HSTR采用总线扫描次数驱动。
    HSTR_RT();
    
    // 发送消息到CPS
    SendCmd2CPS();
    // 获取任务执行结束时间
    m_stRbtAppParams.nSystemTime_TaskEnd = 0;
	// 计算任务耗时
	m_stRbtAppParams.nSystemTime_TaskExecTime =  m_stRbtAppParams.nSystemTime_TaskEnd -  m_stRbtAppParams.nSystemTime_TaskStart;
	// 获取任务最大执行周期
	if (m_stRbtAppParams.nSystemTime_TaskExecTime >  m_stRbtAppParams.nSystemTime_MaxTaskExecTime)
    {
        m_stRbtAppParams.nSystemTime_MaxTaskExecTime =  m_stRbtAppParams.nSystemTime_TaskExecTime;
    }
	// 获取任务最小执行周期
	if (m_stRbtAppParams.nSystemTime_TaskExecTime <  m_stRbtAppParams.nSystemTime_MinTaskExecTime)
    {
        m_stRbtAppParams.nSystemTime_MinTaskExecTime =  m_stRbtAppParams.nSystemTime_TaskExecTime;
    }
    return REC_Successed;
}

/**
 * @brief 处理延时类命令
 * @param 无
 * @return 无
 */
void LLC_Rbt::LLC_ExecHandleNotRealTimeCmd()
{

    return;
}

// 检测看门狗超时
bool LLC_Rbt::CheckCPSWatchDogTimeOut()
{
    if(!m_bEnableCPS)
    {
        return false;
    }
    if(m_nWatchDog == m_pLLC_CPS2LLCMemory->writeCNT)
    {
        m_nWatchDogCnt ++;
        if(m_nWatchDogCnt > 250)
        {
            //m_vAxisGruop[i].m_stMotionCtlCmdStatus.bCmdGroupStop = true;
        }
        //等待停止/去使能/关闭零力完成
        if(m_nWatchDogCnt > 300)
        {
            m_nWatchDogCnt = 0;
            m_nWatchDog = m_pLLC_CPS2LLCMemory->writeCNT;
            m_pLLC_LLC2CPSMemory->st_StatusArea.readCNT = 0;
            //在系统控制板未修复前不要放开这个，否则无法上电
            SetRunControllerState(false);
            return true;
        }
    }
    else
    {
        m_nWatchDog = m_pLLC_CPS2LLCMemory->writeCNT;
        m_nWatchDogCnt = 0;
    }
    return false;
}
// 检测从站掉线
bool LLC_Rbt::CheckSlaveDropError()
{
    if(m_bFoEWorking)
        return false;
    if (m_stRobotControlStatus.bStatus_SlaveDropError)
    {
        if(m_stShmRobotRTStatus.stECATStatus.nECATStatusMachine == enCDSState_PreResettingError)
        {
            SetRunControllerState(false);
        }
        else
        {
            //关闭零力示教去使能
            //状态重置
            m_stRobotControlStatus.bStatus_ControllerError = true;
            m_stShmRobotRTStatus.nErrorID = CEC_SlaveDropped;
        }
        return true;
    }
    return false;
}

bool LLC_Rbt::CheckEmergencyStop()
{
    return LL_CpsService::Get().Emergency() || enum_StatusHigh == m_pLLC_CPS2LLCMemory->st_BoxInfo.nIsEStop;
}
bool LLC_Rbt::CheckIsEmergencyStopHandled()
{

    return false;
}

bool LLC_Rbt::CheckIsNotElectrifyInEmergencyStop()
{
    return enum_StatusLow == m_pLLC_CPS2LLCMemory->st_BoxInfo.nIsEStop;
}

bool LLC_Rbt::CheckIsWatchDogHandled()
{
    return false;
}
// 检查所有从站是否处于OP状态,初始化完成后不再调用
bool LLC_Rbt::CheckIsSlaveInOprational()
{
    // 检查所有从站是否处于OP状态
    return IsSlaveInOprational();
}
// 处理急停
bool LLC_Rbt::HandleEmergencyStop()
{
    m_stEtherCATMasterControl.pDisconnectSlaves();
    return true;
}


int LLC_Rbt::InitSetConfigSlavePdo(int nSlaveCount,int nSlaveID, void* pSlaveST)
{
    const ESI_Parse::Device * dev = nullptr;
    ec_slavet* pSlaves = (ec_slavet*)pSlaveST;
    bool bFindDevice = false;
    //虚拟模式直接生成
    if(m_stRbtEtherCATParams.bSimulationType)
    {
        m_nConfiguredAxisNum = 0;
        m_nConfiguredSlaveNum =0;
        //清空从站指针
        for (size_t i = 0; i < m_vSlaves.size(); i++)
        {
            //释放内存时调用从站析构函数释放轴内存
            delete m_vSlaves[i];
            m_vSlaves[i] = nullptr;
        }
        
        m_vAxis.clear();
        m_vSlaves.clear();
        m_vIOs.clear();
        for (int i = 0; i < m_nSetConfiguredSlaveNum; i++)
        {
            bFindDevice = false;
            for (size_t j = 0; j < m_vESI_Parse.size(); j++)
            {
                dev = m_vESI_Parse[j]->findDeviceConfig(m_vSlaveConfigParam[i].nProductCode,m_vSlaveConfigParam[i].nRevisionNo);
                //匹配到了对应的设备
                if(nullptr != dev)
                {
                    bFindDevice = true;
                    LLC_SlaveBase* pAxisSlave = new LLC_SlaveBase();
                    pAxisSlave->SetEtherCATFuncCB(m_slaveInterface);
                    //设置配置文件
                    pAxisSlave->SetDeviceConfig(dev);
                    //初始化从站
                    if (pAxisSlave->InitSlave(i,true,m_stRbtEtherCATParams.dEtherCatTimeCycle,m_vSlaveConfigParam[i],pSlaves,false) != REC_Successed) { delete pAxisSlave; return REC_ControllerNotInit; }
                    //注册轴
                    pAxisSlave->InsertAxis(m_vSlaveConfigParam[i].vAxisPDOs);
                    //提取轴
                    vector<CIA402Axis*> Axes= pAxisSlave->GetCIA402Axis();
                    //期望轴与实际轴不一致
                    if(m_vSlaveConfigParam[i].vAxisPDOs.size() != Axes.size())
                    {
                        return CEC_SlaveDropped;
                    }
                    for (size_t AxisNum = 0; AxisNum < Axes.size(); AxisNum ++)
                    {
                        //设置轴ID
                        Axes[AxisNum]->Axis_SetAxisID(m_vAxis.size());
                        m_vAxis.push_back(Axes[AxisNum]);
                        printf("InitAxisSUCC,AxisID:%d\n",AxisNum);
                    }
                    m_nConfiguredAxisNum = m_nConfiguredAxisNum + Axes.size();

                    m_vSlaves.push_back(pAxisSlave);
                    m_nConfiguredSlaveNum = m_nConfiguredSlaveNum + 1;
                    
                }
                if(bFindDevice)
                {
                    break;;
                }
            }
            //没·找到设备
            if(!bFindDevice)
            {
                printf("-----------未找到设备:ProductCode:%d    RevisionNo:%d-------------\n",m_vSlaveConfigParam[i].nProductCode,m_vSlaveConfigParam[i].nRevisionNo);
                return CEC_SlaveDropped;
            }

        }
        return REC_Successed;
    }
    LLC_SlaveBase* pAxisSlave = nullptr;
    if (nSlaveID == 0) return CleanSlaveConfig();
    if (!pSlaves || nSlaveID < 1 || static_cast<std::size_t>(nSlaveID) > m_vSlaveConfigParam.size()) return REC_ParametersError;
    //设备不一致
    if((m_vSlaveConfigParam[nSlaveID -1].nProductCode != pSlaves->eep_id) || (m_vSlaveConfigParam[nSlaveID -1].nRevisionNo != pSlaves->eep_rev))
    {
        printf("-----------EtherCAT拓扑不一致:ConfigProductCode:%d    ConfigRevisionNo:%d-------------\n",m_vSlaveConfigParam[nSlaveID -1].nProductCode,m_vSlaveConfigParam[nSlaveID -1].nRevisionNo);
        printf("-----------EtherCAT拓扑不一致:ActProductCode:%d    ACTRevisionNo:%d-------------\n",pSlaves->eep_id,pSlaves->eep_rev);
        return CEC_SlaveDropped;
    }
    bFindDevice = false;
    for (size_t j = 0; j < m_vESI_Parse.size(); j++)
    {
        dev = m_vESI_Parse[j]->findDeviceConfig(pSlaves->eep_id,pSlaves->eep_rev);
        //匹配到了对应的设备
        if(nullptr != dev)
        {
            bFindDevice = true;
            LLC_SlaveBase* pAxisSlave = new LLC_SlaveBase();
                    pAxisSlave->SetEtherCATFuncCB(m_slaveInterface);
            //设置配置文件
            pAxisSlave->SetDeviceConfig(dev);
            //初始化从站
            if (pAxisSlave->InitSlave(nSlaveID,false,m_stRbtEtherCATParams.dEtherCatTimeCycle,m_vSlaveConfigParam[nSlaveID -1],pSlaves,false) != REC_Successed) { delete pAxisSlave; return REC_ControllerNotInit; }
            //注册轴
            pAxisSlave->InsertAxis(m_vSlaveConfigParam[nSlaveID - 1].vAxisPDOs);
            //提取轴
            vector<CIA402Axis*> Axes= pAxisSlave->GetCIA402Axis();
            //期望轴与实际轴不一致
            if(m_vSlaveConfigParam[nSlaveID - 1].vAxisPDOs.size() != Axes.size())
            {
                return CEC_SlaveDropped;
            }
            for (size_t AxisNum = 0; AxisNum < Axes.size(); AxisNum ++)
            {
                //设置轴ID
                Axes[AxisNum]->Axis_SetAxisID(m_vAxis.size());
                m_vAxis.push_back(Axes[AxisNum]);
                printf("InitAxisSUCC,AxisID:%d\n",AxisNum);
            }
            m_nConfiguredAxisNum = m_nConfiguredAxisNum + Axes.size();
            m_vSlaves.push_back(pAxisSlave);
            m_nConfiguredSlaveNum++;
            return REC_Successed;
        }
    }
    printf("-----------未找到设备:ProductCode:%d    RevisionNo:%d-------------\n",pSlaves->eep_id,pSlaves->eep_rev);
    return CEC_SlaveDropped;
}



// 检查所有从站是否进入OP状态
bool LLC_Rbt::IsSlaveInOprational()
{
    if (m_stRbtEtherCATParams.bSimulationType)
    {
        return true;
    }
    for (size_t i = 0; i < m_vSlaves.size(); i++)
    {
        if(m_vSlaves[i] != nullptr)
        {
            if(!m_vSlaves[i]->IsSlaveOP())
            {
                return false;
            }
        }
    }
    return true;
}

//掉线后清除配置
int LLC_Rbt::CleanSlaveConfig() {
    LL_CpsService::Get().InvalidateAxes();
    LLCSoftMotion.RegisterAxis({}); LLCSoftMotion.RegisterAxisGroup({});
    m_vAxis.clear(); m_vIOs.clear(); m_vAxisGruop.clear();
    for (auto* slave : m_vSlaves) delete slave;
    m_vSlaves.clear();m_retiredESI.clear(); m_nConfiguredSlaveNum=0; m_nConfiguredAxisNum=0;
    return REC_Successed;
}
int LLC_Rbt::CleanAxisConfig()
{
    m_vAxis.clear();
    return REC_Successed;
}
int LLC_Rbt::CleanIOConfig()
{
    m_vIOs.clear();
    return REC_Successed;
}
int LLC_Rbt::CleanGroupConfig()
{
    return REC_Successed;
}






int LLC_Rbt::ControllerReset()
{
    //复位主站
    LLCRbt->ResetMaster();
    //复位控制器
    m_stShmRobotRTStatus.nErrorID = 0;
    m_stShmRobotRTStatus.nErrorAxisID = 0;
    m_stRobotControlStatus.bStatus_SlaveDropError = false;
    m_stRobotControlStatus.bStatus_ControllerError = false;
    m_stRobotControlStatus.bStatus_AxisError = false;
    //复位轴
    for (size_t i = 0; i < m_vAxis.size(); i++)
    {
        bAxisResetCmd[i] = true;
    }
    //复位轴组
    for (size_t i = 0; i < m_vAxisGruop.size(); i++)
    {
        //bAxisGroupResetCmd[i] = true;
    }
    //复位脚本
    InitWaypoint();
    //复位实时脚本
    HSTR_ResetProgram();
    return REC_Successed;
}

int LLC_Rbt::ControllerStop()
{
    //停止轴组
    for (size_t i = 0; i < m_vAxisGruop.size(); i++)
    {
        //bAxisGroupResetCmd[i] = true;
    }
    //停止脚本
    InitWaypoint();
    //停止实时脚本
    HSTR_StopApplication();
    //停止轴
    std::vector<int> AxisList;
    for (size_t i = 0; i < m_vAxis.size(); i++)
    {
        AxisList.push_back(i);
    }
    AxisStop(AxisList);
    return REC_Successed;
}
//进op设置（所有轴配置完毕）
int LLC_Rbt::ControllerConfig()
{
    //配置轴
    for (uint32_t i = 0; i < m_nConfiguredAxisNum; i++)
    {
        //编码器方向
        m_vAxis[i]->Axis_SetEncodeDirection(v_AxisHardParam[i].nEncodeDirection);
        //编码器分辨率
        m_vAxis[i]->Axis_SetEncodeRatio(v_AxisHardParam[i].nEncodeRatio);
        //电机减速比
        m_vAxis[i]->Axis_SetGearRatio(v_AxisHardParam[i].dGearRatio);
        //原点配置
        m_vAxis[i]->Axis_SetEncodeHomePos(v_AxisHardParam[i].nHomePos);
        //设置边界
        m_vAxis[i]->Axis_SetSWLimit(v_AxisHardParam[i].dSafeSpaceLimitUpper,v_AxisHardParam[i].dSafeSpaceLimitLower);
        //设置电流系数
        m_vAxis[i]->Axis_SetTFFScale(v_AxisHardParam[i].dTFFScale);
        //设置速度系数
        m_vAxis[i]->Axis_SetVFFScale(v_AxisHardParam[i].dVFFScale);
        //设置电流前馈系数
        m_vAxis[i]->Axis_SetTorqueScale(v_AxisHardParam[i].dTorqueScale);
        //设置速度前馈系数
        m_vAxis[i]->Axis_SetVelocityScale(v_AxisHardParam[i].dVelocityScale);
    }
    return REC_Successed;
}


int LLC_Rbt::InitSoftMotion()
{
    std::vector<CIA402Axis *> v_pAxis;
    for (size_t i = 0; i < m_vAxis.size(); i++)
    {
        v_pAxis.push_back(m_vAxis[i]);
    }
    std::vector<AxisGroup *> v_pAxisGruop;
    for (size_t i = 0; i < m_vAxisGruop.size(); i++)
    {
        v_pAxisGruop.push_back(&m_vAxisGruop[i]);
    }
    
    LLCSoftMotion.RegisterAxis(v_pAxis);
    LLCSoftMotion.RegisterAxisGroup(v_pAxisGruop);
    LLCSoftMotion.SetSoftMotionCycle(m_stRbtEtherCATParams.dEtherCatTimeCycle);
    return REC_Successed;
}

int LLC_Rbt::InitIO()
{
    if(m_stRbtEtherCATParams.bSimulationType) {
        m_vIOs.clear();m_virtualIOImage.assign(m_vIOConfig.size(),0);
        for(std::size_t i=0;i<m_vIOConfig.size();++i){
            const auto& config=m_vIOConfig[i];
            LLPC_IO io(config.m_nSlaveID,config.m_nPDOIndex,config.m_nPDOSubIndex,config.m_nBit);
            io.IO_SetID(config.m_IO_id);io.IO_SetInverted(config.m_bInverted);io.IO_SetPDOPtr(&m_virtualIOImage[i]);m_vIOs.push_back(io);
        }
        return REC_Successed;
    }
    std::vector<LLPC_IO> vIOs; // 暂存初始化结果，全部成功后再更新m_vIOs
    vIOs.reserve(m_vIOConfig.size()); // 提前分配空间

    for (const auto& config : m_vIOConfig) // 遍历全部IO配置
    {
        if (config.m_IO_id < 0 || config.m_nBit > 7 || config.m_nPDOIndex == 0)
            return -1; // IO编号、位偏移或对象索引无效

        for (const auto& io : vIOs) // 检查IO编号是否重复
        {
            if (io.IO_GetID() == config.m_IO_id)
                return -1; // 不允许重复IO编号
        }

        if (config.m_nSlaveID >= m_nConfiguredSlaveNum || config.m_nSlaveID >= m_vSlaves.size())
            return -1; // 从站下标越界

        auto* pSlave = m_vSlaves[config.m_nSlaveID]; // 获取配置指定的从站
        if (pSlave == nullptr)
            return -1; // 从站对象无效

        const ESI_Parse::PdoEntry* pMatchedEntry = nullptr; // 保存匹配到的PDO条目
        bool bAmbiguous = false; // 标记是否存在重复映射

        auto FindEntry = [&](const std::vector<ESI_Parse::Pdo>* pPdos) // 在指定方向的PDO列表中查找
        {
            if (pPdos == nullptr)
                return; // 当前方向没有PDO列表

            for (const auto& pdo : *pPdos) // 遍历PDO
            {
                if (!pdo.selected)
                    continue; // 只使用已选中并参与映射的PDO

                for (const auto& entry : pdo.entries) // 遍历PDO中的对象条目
                {
                    if (entry.padding || entry.index != config.m_nPDOIndex || entry.subIndex != config.m_nPDOSubIndex)
                        continue; // 跳过填充条目以及索引不匹配的条目

                    if (pMatchedEntry != nullptr)
                        bAmbiguous = true; // 同一对象存在多处映射，当前配置无法区分
                    else
                        pMatchedEntry = &entry; // 保存首次匹配的条目
                }
            }
        };

        FindEntry(pSlave->GetTXPDOList()); // 查找输入PDO
        FindEntry(pSlave->GetRXPDOList()); // 查找输出PDO

        if (bAmbiguous || pMatchedEntry == nullptr)
            return -1; // 映射不唯一或找不到配置对象

        if (pMatchedEntry->pDataPtr == nullptr || pMatchedEntry->bitLen <= config.m_nBit)
            return -1; // 映射指针未初始化或配置位超出对象长度

        if (pMatchedEntry->bitOffsetInByte < 0 || pMatchedEntry->bitOffsetInByte > 7)
            return -1; // PDO条目的起始位偏移无效

        const unsigned int nBitOffset = static_cast<unsigned int>(pMatchedEntry->bitOffsetInByte) + config.m_nBit; // 计算实际位偏移
        auto* pData = static_cast<uint8_t*>(pMatchedEntry->pDataPtr) + nBitOffset / 8; // 定位实际所在字节，不重复累加byteOffset

        vIOs.emplace_back(config.m_nSlaveID, config.m_nPDOIndex, config.m_nPDOSubIndex, static_cast<uint16_t>(nBitOffset % 8)); // 创建IO并保存字节内位偏移
        auto& io = vIOs.back(); // 引用刚创建的IO
        io.IO_SetID(config.m_IO_id); // 保存配置中的IO编号
        io.IO_SetInverted(config.m_bInverted); // 保存取反属性
        io.IO_SetPDOPtr(pData); // 绑定实际映射地址
    }

    m_vIOs.swap(vIOs); // 全部成功后替换原IO列表
    return REC_Successed; // 初始化成功
}

int LLC_Rbt::InitBasicFB()
{
    // m_FB_AxisInitReset.clear();
    // m_FB_AxisPowerOn.clear();
    // m_FB_AxisPowerOff.clear();
    // m_FB_AxisReset.clear();
    // for (int i = 0; i < m_nSetConfiguredAxisNum; i++)
    // {
    //     m_FB_AxisInitReset.push_back(MC_InitResetAxis());
    //     m_FB_AxisPowerOn.push_back(MC_PowerOn());
    //     m_FB_AxisPowerOff.push_back(MC_PowerOff());
    //     m_FB_AxisReset.push_back(MC_Reset());
    //     m_FB_AxisStop.push_back(MC_Stop());
    //     //m_FB_AxisAbsMove.push_back(MC_MoveAbsolute());
    //     m_FB_AxisRelMove.push_back(MC_MoveRelative());
    //     m_FB_AxisMoveVelocity.push_back(MC_MoveVelocity());
    //     m_FB_AxisSetPosition.push_back(MC_SetPosition());
    //     m_FB_AxisHome.push_back(MC_Home());
    // }
    return REC_Successed;
}

int LLC_Rbt::SetIO(int IOid, bool Var)
{
    if (IOid < 0) return -1; // IO编号无效
    for (auto& io : m_vIOs) // 引用遍历已注册的IO
    {
        if (io.IO_GetID() != IOid) continue; // 编号不匹配，继续查找
        if (!io.IO_IsValid()) return -1; // IO映射无效
        return io.IO_Write(Var) ? REC_Successed : -1; // 写入并返回结果
    }
    return -1; // 未找到指定编号的IO
}
