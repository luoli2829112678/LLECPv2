#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

#include "pugixml.h"


std::vector<int> GetAxisChannel(std::vector<int> vPdoList);

using EtherCATVariant = std::variant<bool,        // BOOL
    uint8_t,     // USINT, BYTE
    uint16_t,    // UINT, WORD
    uint32_t,    // UDINT, DWORD
    uint64_t,    // ULINT, LWORD
    int8_t,      // SINT
    int16_t,     // INT
    int32_t,     // DINT
    int64_t,     // LINT
    float,       // REAL
    double,      // LREAL
    std::string  // STRING
>;

class EtherCATValueVariant
{
private:
    EtherCATVariant value_;
    std::string originalType_;

public:
    explicit EtherCATValueVariant(const std::string& xmlType) : originalType_(xmlType)
    {
        if (xmlType == "BOOL") value_ = false;
        else if (xmlType == "USINT" || xmlType == "BYTE") value_ = uint8_t{0};
        else if (xmlType == "UINT" || xmlType == "WORD") value_ = uint16_t{0};
        else if (xmlType == "UDINT" || xmlType == "DWORD") value_ = uint32_t{0};
        else if (xmlType == "ULINT" || xmlType == "LWORD") value_ = uint64_t{0};
        else if (xmlType == "SINT") value_ = int8_t{0};
        else if (xmlType == "INT") value_ = int16_t{0};
        else if (xmlType == "DINT") value_ = int32_t{0};
        else if (xmlType == "LINT") value_ = int64_t{0};
        else if (xmlType == "REAL") value_ = 0.0f;
        else if (xmlType == "LREAL") value_ = 0.0;
        else value_ = std::string{};
    }

    template <typename T>
    bool set(T val)
    {
        if (!std::holds_alternative<T>(value_)) return false;
        value_ = val;
        return true;
    }

    template <typename T>
    std::optional<T> get() const
    {
        if (!std::holds_alternative<T>(value_)) return std::nullopt;
        return std::get<T>(value_);
    }

    std::vector<uint8_t> toBytes() const
    {
        return std::visit([](auto&& arg) -> std::vector<uint8_t> {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, std::string>) {
                return std::vector<uint8_t>(arg.begin(), arg.end());
            } else {
                std::vector<uint8_t> bytes(sizeof(T));
                std::memcpy(bytes.data(), &arg, sizeof(T));
                return bytes;
            }
        }, value_);
    }

    void fromBytes(const std::vector<uint8_t>& data)
    {
        std::visit([&](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, std::string>) {
                value_ = std::string(data.begin(), data.end());
            } else if (data.size() >= sizeof(T)) {
                T val{};
                std::memcpy(&val, data.data(), sizeof(T));
                value_ = val;
            }
        }, value_);
    }

    std::string toString() const
    {
        return std::visit([](auto&& arg) -> std::string {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, bool>) return arg ? "true" : "false";
            else if constexpr (std::is_arithmetic_v<T>) return std::to_string(arg);
            else return arg;
        }, value_);
    }
};

class ESI_Parse
{
public:
    ESI_Parse() = default;
    ~ESI_Parse() = default;

    bool loadFromFile(const std::string& filename);
    bool loadFromString(const std::string& content);
    void printConfig() const;

    struct VendorInfo
    {
        uint32_t id = 0;
        std::string name;
        std::string fileVersion;
        std::string imageData;
    };

    struct GroupInfo
    {
        std::string type;
        std::string name;
        int sortOrder = 0;
        std::string imageData;
    };

    struct StateMachineTimeout
    {
        int preopTimeout = 0;
        int safeopOpTimeout = 0;
        int backToInitTimeout = 0;
        int backToSafeopTimeout = 0;
    };

    struct MailboxTimeout
    {
        int requestTimeout = 0;
        int responseTimeout = 0;
    };

    struct SyncManager
    {
        int index = 0;
        std::string name;
        int minSize = 0;
        int maxSize = 0;
        int defaultSize = 0;
        int startAddress = 0;
        uint8_t controlByte = 0;
        bool enable = false;
    };

    struct PdoEntry
    {
        int index = 0;
        int subIndex = 0;
        int bitLen = 0;
        int bitOffset = 0;      // Entry 在当前 PDO 内的 bit 偏移，方便直接生成过程数据映射
        int byteOffset = 0;     // Entry 在当前 PDO 内的 byte 偏移
        int bitOffsetInByte = 0;// Entry 在当前 byte 内的 bit 偏移
        std::string name;
        std::string dataType;
        bool dependOnSlot = false;
        bool padding = false;   // Index=0 的 Entry 视为填充位
        void* pDataPtr;//数据对应的地址，在从站初始化后初始化该指针
    };

    struct Pdo
    {
        uint32_t index = 0;
        std::string name;
        int sm = 0;
        bool fixed = false;
        bool mandatory = false;
        bool defaultSelected = false; // XML 中 Default=1/true 时置位；主站可优先选择
        bool selected = false;        // 当前是否被选中；用于生成 0x1C12 / 0x1C13
        bool selectable = true;       // 当前是否允许被用户选中；被互斥 PDO 排除时为 false
        bool dependOnSlot = false;
        int bitSize = 0;        // 当前 PDO 总 bit 长度
        int byteSize = 0;       // 当前 PDO 总 byte 长度，向上取整
        std::vector<int> excludes;   // 当前 PDO 排斥的 PDO Index，例如 TwinCAT 显示“排除 16#1701”
        std::vector<int> excludedBy; // 当前 PDO 被哪些已选 PDO 排斥；用于界面置灰提示
        std::vector<PdoEntry> entries;
    };

    struct SdoWrite
    {
        int index = 0;
        int subIndex = 0;
        int value = 0;
        int bitLen = 16;
        std::string comment;
    };

    struct ObjectSubItem
    {
        int subIdx = 0;
        std::string name;
        std::string type;
        int bitSize = 0;
        int bitOffset = 0;       // 子项在对象数据类型内的 bit 偏移
        int byteOffset = 0;      // 子项在对象数据类型内的 byte 偏移
        int bitOffsetInByte = 0; // 子项在当前 byte 内的 bit 偏移
        std::string access;
        std::string pdoMapping;   // r/t/rw 等 PDO 映射权限，来自 Flags/PdoMapping
        std::string defaultData;
        std::string comment;
        std::optional<int> minValue;
        std::optional<int> maxValue;
        std::optional<int> defaultValue;
    };

    struct ObjectInfo
    {
        int index = 0;
        std::string name;
        std::string type;
        int bitSize = 0;
        std::string access;
        std::string defaultData;
        bool dependOnSlot = false;
        std::map<int, ObjectSubItem> subItems;
    };

    struct DataType
    {
        std::string name;
        std::string baseType;
        int bitSize = 0;
        std::vector<ObjectSubItem> subItems;
        std::vector<std::pair<std::string, std::string>> enumInfos;
        int lBound = 0;
        int elements = 0;
    };

    struct InitCmd
    {
        std::string transition;
        int index = 0;
        int subIndex = 0;
        std::string data;
        std::string comment;
        bool dependOnSlot = false;
    };

    struct ModuleInfo
    {
        std::string type;
        std::string name;
        std::string moduleClass;
        int moduleIdent = 0;
        int modulePdoGroup = 0;
        std::vector<Pdo> rxPdos;
        std::vector<Pdo> txPdos;
        std::vector<InitCmd> initCmds;
        std::vector<ObjectInfo> objects;
        std::map<std::string, DataType> dataTypes;
    };

    struct EepromInfo
    {
        int byteSize = 0;
        std::string configData;
        std::string bootStrap;
    };

    struct DcOpMode
    {
        std::string name;
        std::string desc;
        uint16_t assignActivate = 0;
        int cycleTimeSync0 = 0;
        int cycleTimeSync1 = 0;
    };

    struct DiagMessage
    {
        int textId = 0;
        std::string messageText;
    };

    struct ModuleClass
    {
        std::string className;
        std::string name;
        std::string imageData;
    };

    struct SlotInfo
    {
        std::string name;
        int minInstances = 0;
        int maxInstances = 0;
        std::string treeView;
        bool hasDefaultModuleIdent = false;
        int defaultModuleIdent = 0;
        std::vector<int> moduleIdents;
    };

    struct SlotsInfo
    {
        bool downloadModuleIdentList = false;
        std::string identifyModuleBy;
        int slotIndexIncrement = 0;
        int slotPdoIncrement = 0;
        std::vector<ModuleClass> moduleClasses;
        std::vector<SlotInfo> slots;
    };

    struct Device
    {
        // Missing attribute keeps the historic dynamic CoE behavior.
        bool supportsPdoUpload = true;
        std::string physics;
        std::string type;
        std::string name;
        uint32_t productCode = 0;
        uint32_t revisionNo;
        std::string serialNo;
        std::string groupType;
        std::string url;
        int profileNo = 0;

        StateMachineTimeout stateMachineTimeout;
        MailboxTimeout mailboxTimeout;
        std::vector<std::string> fmmus;
        std::vector<SyncManager> syncManagers;
        std::vector<InitCmd> initCmds;
        std::vector<Pdo> rxPdos;
        std::vector<Pdo> txPdos;
        std::vector<ObjectInfo> objects;
        std::map<std::string, DataType> dataTypes;
        std::vector<ModuleInfo> modules;
        EepromInfo eeprom;
        std::vector<DcOpMode> dcOpModes;
        std::vector<DiagMessage> diagMessages;
        SlotsInfo slotsInfo;
        
    };

    // Backward-compatible alias for older code that used DeviceConfig.
    using DeviceConfig = Device;

    // 获取 ESI 文件中的厂商信息，例如厂商 ID、厂商名称等。
    const VendorInfo& getVendor() const;

    // 获取 ESI 文件中定义的设备组（Group）列表。
    const std::vector<GroupInfo>& getGroups() const;

    // 获取已解析到的全部设备配置。
    // 一个 ESI XML 文件中可能包含多个 Device 节点。
    const std::vector<Device>& getDevices() const;

    // 获取当前已解析的设备数量。
    size_t getDeviceCount() const;

    // 按下标获取设备配置。
    // index 从 0 开始；若 index 越界，建议返回 nullptr。
    const Device* getDeviceConfig(size_t index) const;

    // 根据产品代码和可选的修订号查找设备配置。
    // revisionNo 有值时：同时匹配 ProductCode 和 RevisionNo。
    // revisionNo 无值时：仅按 ProductCode 匹配。
    // 未找到时返回 nullptr。
    const Device* findDeviceConfig(
        uint32_t productCode,
        std::optional<uint32_t> revisionNo) const;

    // ==================== 兼容旧版单设备接口 ====================
    // 以下接口默认返回“第一个解析到的 Device”的信息。
    // 若 ESI 中存在多个设备，建议新代码优先通过 getDeviceConfig()
    // 或 findDeviceConfig() 获取指定设备。

    // 获取第一个设备的完整配置。
    // 注意：当没有任何设备时，该函数应抛出异常或进行明确保护，
    // 因为返回引用不能表示“未找到”。
    const Device& getDevice() const;

    // 获取第一个设备的状态机超时参数。
    const StateMachineTimeout& getStateMachineTimeout() const;

    // 获取第一个设备的邮箱通信超时参数。
    const MailboxTimeout& getMailboxTimeout() const;

    // 获取第一个设备的 FMMU（Fieldbus Memory Management Unit）配置列表。
    const std::vector<std::string>& getFmmus() const;

    // 获取第一个设备的同步管理器（Sync Manager）配置列表。
    const std::vector<SyncManager>& getSyncManagers() const;

    // 获取第一个设备的 RxPDO 列表。
    // 对 EtherCAT 从站而言，RxPDO 表示主站写入从站的数据。
    const std::vector<Pdo>& getRxPdos() const;

    // 获取第一个设备的 TxPDO 列表。
    // 对 EtherCAT 从站而言，TxPDO 表示从站发送给主站的数据。
    const std::vector<Pdo>& getTxPdos() const;

    // 获取第一个设备的 CoE 对象字典信息。
    const std::vector<ObjectInfo>& getObjects() const;

    // 获取第一个设备的模块（Module）配置列表。
    const std::vector<ModuleInfo>& getModules() const;

    // 获取第一个设备的数据类型定义表。
    // key 通常为数据类型名称，value 为对应的数据类型描述。
    const std::map<std::string, DataType>& getDataTypes() const;

    // 获取第一个设备的 EEPROM 配置信息。
    const EepromInfo& getEeprom() const;

    // 获取第一个设备支持的分布式时钟（DC）运行模式。
    const std::vector<DcOpMode>& getDcOpModes() const;

    // 获取第一个设备的诊断信息定义列表。
    const std::vector<DiagMessage>& getDiagMessages() const;

    // 获取第一个设备的插槽（Slots）配置信息。
    // 通常用于模块化从站的模块插拔、插槽数量和限制规则。
    const SlotsInfo& getSlotsInfo() const;

    // 获取第一个设备的初始化命令列表。
    // 这些命令通常用于从站状态切换期间的 SDO/寄存器初始化。
    const std::vector<InitCmd>& getInitCmds() const;

    // ==================== 查找与修改接口 ====================

    // 在第一个设备的对象字典中按 Object Index 查找对象。
    // 找到后返回可修改指针；未找到时返回 nullptr。
    ObjectInfo* findObject(int index);

    // 在第一个设备的 PDO 列表中按名称查找 PDO。
    // isRx = true：在 RxPDO 中查找；
    // isRx = false：在 TxPDO 中查找。
    // 未找到时返回 nullptr。
    Pdo* findPdo(const std::string& name, bool isRx = true);

    // 在第一个设备的模块列表中按模块类型名称查找模块。
    // 未找到时返回 nullptr。
    ModuleInfo* findModule(const std::string& type);

    // 在第一个设备的模块列表中按模块 Ident 编号查找模块。
    // Ident 一般用于唯一标识某一种可插拔模块。
    // 未找到时返回 nullptr。
    ModuleInfo* findModuleByIdent(int ident);

    // 向第一个设备的 RxPDO 列表追加 PDO。
    void addRxPdos(const std::vector<Pdo>& pdos);

    // 向第一个设备的 TxPDO 列表追加 PDO。
    void addTxPdos(const std::vector<Pdo>& pdos);

    // 获取第一个设备中全部 RxPDO Entry 的总位长度。
    // 返回值单位建议统一为 bit。
    size_t getRxEntryPdoSize() const;

    // 获取第一个设备中全部 TxPDO Entry 的总位长度。
    // 返回值单位建议统一为 bit。
    size_t getTxEntryPdoSize() const;

    // ==================== PDO 选择与映射构建 ====================

    // 按 PDO 的默认属性初始化选择状态。
    // 通常根据 fixed、mandatory、selected、selectable 等字段决定。
    static void buildDefaultPdoSelection(std::vector<Pdo>& pdos);

    // 选择指定 PDO，并校验其选择条件。
    // targetIndex 为 PDO 的 Object Index，例如 0x1600、0x1A00。
    // 若存在 excludes、excludedBy 等冲突关系，则返回 false，
    // 并通过 errorMessage 返回具体原因。
    static bool selectPdo(
        std::vector<Pdo>& pdos,
        int targetIndex,
        std::string* errorMessage = nullptr);

    // 取消选择指定 PDO，并校验该操作是否允许。
    // 若 PDO 是固定 PDO 或被其他 PDO 依赖，则应返回 false。
    static bool deselectPdo(
        std::vector<Pdo>& pdos,
        int targetIndex,
        std::string* errorMessage = nullptr);

    // 设置指定 PDO 的选中状态。
    // selected = true：选择 PDO；
    // selected = false：取消选择 PDO。
    static bool setPdoSelected(
        std::vector<Pdo>& pdos,
        int targetIndex,
        bool selected,
        std::string* errorMessage = nullptr);

    // 获取当前处于选中状态的全部 PDO Index 列表。
    static std::vector<int> getSelectedPdoIndexes(const std::vector<Pdo>& pdos);

    // 根据当前选中的 PDO 构建 PDO Assignment 对象写入命令。
    // assignIndex 通常为：
    // - 0x1C12：RxPDO Assignment；
    // - 0x1C13：TxPDO Assignment。
    // 返回的 SdoWrite 列表可直接用于写入从站对象字典。
    static std::vector<SdoWrite> buildPdoAssignWrites(
        int assignIndex,
        const std::vector<Pdo>& pdos);

    // 根据设备的 RxPDO 配置，构建写入 0x1C12 的 SDO 命令。
    // outDevice 表示目标从站设备配置。
    static std::vector<SdoWrite> buildRxPdoAssignWrites(
        const Device& outDevice);

    // 根据设备的 TxPDO 配置，构建写入 0x1C13 的 SDO 命令。
    // outDevice 表示目标从站设备配置。
    static std::vector<SdoWrite> buildTxPdoAssignWrites(
        const Device& outDevice);

    // ==================== 常用设备基础信息 ====================

    // 获取第一个设备的厂商 ID。
    uint32_t getVendorId() const;

    // 获取第一个设备的产品代码 ProductCode。
    uint32_t getProductCode() const;

    // 获取第一个设备的厂商名称。
    std::string getVendorName() const;

    // 获取第一个设备的产品名称。
    std::string getProductName() const;

private:
    int hexStringToInt(const std::string& hexStr) const;
    std::string cleanHexString(const std::string& hexStr) const;
    bool xmlBool(const pugi::xml_attribute& attr) const;
    void appendUniqueInt(std::vector<int>& values, int value) const;
    std::string formatHex(int value, int width = 4) const;

    bool parseDocument();
    void parseVendor(pugi::xml_node root);
    void parseGroups(pugi::xml_node desc);
    Device parseDevice(pugi::xml_node deviceNode, const std::vector<ModuleInfo>& moduleLibrary);
    void parseInfo(pugi::xml_node deviceNode, Device& outDevice);
    void parseMailbox(pugi::xml_node deviceNode, Device& outDevice);
    void parseProfile(pugi::xml_node deviceNode, Device& outDevice);
    void parseDataTypes(pugi::xml_node dictNode, std::map<std::string, DataType>& outDataTypes);
    void parseObjects(pugi::xml_node dictNode, const std::map<std::string, DataType>& dataTypes, std::vector<ObjectInfo>& outObjects);
    void parsePdo(pugi::xml_node pdoNode, Pdo& pdo) const;
    void parseDc(pugi::xml_node deviceNode, Device& outDevice);
    void parseSlots(pugi::xml_node deviceNode, Device& outDevice);
    void parseEeprom(pugi::xml_node deviceNode, Device& outDevice);
    void parseDiagMessages(pugi::xml_node diagNode, std::vector<DiagMessage>& outMessages);
    std::vector<ModuleInfo> parseModules(pugi::xml_node modulesNode);
    Pdo makeSlotPdo(const Pdo& source, int slotNo, const SlotInfo& slot, int pdoIncrement, int indexIncrement) const;
    void expandSlotModules(Device& outDevice, const std::vector<ModuleInfo>& moduleLibrary) const;
    void printOneDeviceSummary(const Device& outDevice, size_t deviceIndex) const;
    void printPdoList(const std::vector<Pdo>& pdos, const std::string& directionName) const;
    static bool pdoConflict(const Pdo& lhs, const Pdo& rhs);
    static void recomputePdoSelectable(std::vector<Pdo>& pdos);

    // ESI_Parse only owns global XML-level data and the parsed device list.
    pugi::xml_document doc;
    VendorInfo vendor;
    std::vector<GroupInfo> groups;
    std::vector<Device> devices;
    Device device;
};
