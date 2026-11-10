#include "ESI_Parse.h"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <iostream>
#include <sstream>

// MSVC-compatible aliases for nested data structures.
// They avoid name lookup differences when member functions are defined outside ESI_Parse.
using VendorInfo = ESI_Parse::VendorInfo;
using GroupInfo = ESI_Parse::GroupInfo;
using StateMachineTimeout = ESI_Parse::StateMachineTimeout;
using MailboxTimeout = ESI_Parse::MailboxTimeout;
using SyncManager = ESI_Parse::SyncManager;
using PdoEntry = ESI_Parse::PdoEntry;
using Pdo = ESI_Parse::Pdo;
using SdoWrite = ESI_Parse::SdoWrite;
using ObjectSubItem = ESI_Parse::ObjectSubItem;
using ObjectInfo = ESI_Parse::ObjectInfo;
using DataType = ESI_Parse::DataType;
using InitCmd = ESI_Parse::InitCmd;
using ModuleInfo = ESI_Parse::ModuleInfo;
using EepromInfo = ESI_Parse::EepromInfo;
using DcOpMode = ESI_Parse::DcOpMode;
using DiagMessage = ESI_Parse::DiagMessage;
using ModuleClass = ESI_Parse::ModuleClass;
using SlotInfo = ESI_Parse::SlotInfo;
using SlotsInfo = ESI_Parse::SlotsInfo;
using Device = ESI_Parse::Device;


namespace
{
int parseSubIndexFromText(const std::string& text)
{
    // 只解析 "SubIndex 033"、"SubIndex 001"、"SubIndex 000" 这类显式 SubIndex 名称。
    // 不要把 "SS2_1" 误解析成 21。
    std::string lowered = text;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    if (lowered.find("subindex") == std::string::npos && lowered.find("sub index") == std::string::npos) {
        return -1;
    }

    std::string digits;
    for (char ch : text) {
        if (std::isdigit(static_cast<unsigned char>(ch))) digits.push_back(ch);
    }
    if (digits.empty()) return -1;
    try {
        return std::stoi(digits, nullptr, 10);
    } catch (...) {
        return -1;
    }
}

std::string normalizeNameForMatch(std::string value)
{
    value.erase(std::remove_if(value.begin(), value.end(), [](unsigned char c) {
        return std::isspace(c) != 0;
    }), value.end());
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

void mergeObjectSubItem(ESI_Parse::ObjectInfo& obj, const ESI_Parse::ObjectSubItem& sub)
{
    auto it = obj.subItems.find(sub.subIdx);
    if (it == obj.subItems.end()) {
        obj.subItems[sub.subIdx] = sub;
        return;
    }

    ESI_Parse::ObjectSubItem& dst = it->second;
    if (dst.name.empty()) dst.name = sub.name;
    if (dst.type.empty()) dst.type = sub.type;
    if (dst.bitSize == 0) dst.bitSize = sub.bitSize;
    if (dst.bitOffset == 0) dst.bitOffset = sub.bitOffset;
    dst.byteOffset = dst.bitOffset / 8;
    dst.bitOffsetInByte = dst.bitOffset % 8;
    if (dst.access.empty()) dst.access = sub.access;
    if (dst.pdoMapping.empty()) dst.pdoMapping = sub.pdoMapping;
    if (dst.defaultData.empty()) dst.defaultData = sub.defaultData;
    if (dst.comment.empty()) dst.comment = sub.comment;
    if (!dst.minValue && sub.minValue) dst.minValue = sub.minValue;
    if (!dst.maxValue && sub.maxValue) dst.maxValue = sub.maxValue;
    if (!dst.defaultValue && sub.defaultValue) dst.defaultValue = sub.defaultValue;
}

int findSubIndexByName(const ESI_Parse::ObjectInfo& obj, const std::string& name)
{
    const std::string wanted = normalizeNameForMatch(name);
    if (wanted.empty()) return -1;

    for (const auto& kv : obj.subItems) {
        if (normalizeNameForMatch(kv.second.name) == wanted) return kv.first;
    }

    return -1;
}
}

bool ESI_Parse::loadFromFile(const std::string& filename)
{
    doc.reset();
    vendor = VendorInfo{};
    groups.clear();
    devices.clear();
    device = Device{};

    pugi::xml_parse_result result = doc.load_file(filename.c_str());
    if (!result) {
        std::cout << "XML parsed error: " << result.description() << std::endl;
        return false;
    }
    return parseDocument();
}

bool ESI_Parse::loadFromString(const std::string& content)
{
    doc.reset();
    vendor = VendorInfo{};
    groups.clear();
    devices.clear();
    device = Device{};

    pugi::xml_parse_result result = doc.load_string(content.c_str());
    if (!result) {
        std::cout << "XML parsed error: " << result.description() << std::endl;
        return false;
    }
    return parseDocument();
}

int ESI_Parse::hexStringToInt(const std::string& hexStr) const
{
    if (hexStr.empty()) return 0;

    std::string str = hexStr;
    str.erase(std::remove_if(str.begin(), str.end(), [](unsigned char c) {
        return std::isspace(c) != 0;
    }), str.end());

    if (str.empty()) return 0;

    try {
        if (str.rfind("#x", 0) == 0 || str.rfind("#X", 0) == 0) {
            return static_cast<int>(std::stoul(str.substr(2), nullptr, 16));
        }
        if (str.rfind("0x", 0) == 0 || str.rfind("0X", 0) == 0) {
            return static_cast<int>(std::stoul(str.substr(2), nullptr, 16));
        }
        return std::stoi(str, nullptr, 10);
    } catch (...) {
        return 0;
    }
}

std::string ESI_Parse::cleanHexString(const std::string& hexStr) const
{
    std::string str = hexStr;
    if (str.rfind("#x", 0) == 0 || str.rfind("#X", 0) == 0) return str.substr(2);
    if (str.rfind("0x", 0) == 0 || str.rfind("0X", 0) == 0) return str.substr(2);
    return str;
}

bool ESI_Parse::xmlBool(const pugi::xml_attribute& attr) const
{
    if (!attr) return false;
    std::string value = attr.as_string();
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return !(value.empty() || value == "0" || value == "false" || value == "no");
}

void ESI_Parse::appendUniqueInt(std::vector<int>& values, int value) const
{
    if (std::find(values.begin(), values.end(), value) == values.end()) {
        values.push_back(value);
    }
}

std::string ESI_Parse::formatHex(int value, int width) const
{
    std::ostringstream oss;
    oss << "16#" << std::hex << std::uppercase
        << std::setw(width) << std::setfill('0') << value
        << std::dec << std::nouppercase << std::setfill(' ');
    return oss.str();
}

bool ESI_Parse::parseDocument()
{
    try {
        pugi::xml_node root = doc.child("EtherCATInfo");
        if (!root) {
            std::cerr << "No EtherCATInfo root node found\n";
            return false;
        }

        parseVendor(root);

        pugi::xml_node desc = root.child("Descriptions");
        if (!desc) {
            std::cerr << "No Descriptions node found\n";
            return false;
        }

        parseGroups(desc);

        std::vector<ModuleInfo> moduleLibrary;
        pugi::xml_node modulesNode = desc.child("Modules");
        if (modulesNode) {
            moduleLibrary = parseModules(modulesNode);
        }

        pugi::xml_node devicesNode = desc.child("Devices");
        if (devicesNode) {
            for (pugi::xml_node devNode = devicesNode.child("Device"); devNode; devNode = devNode.next_sibling("Device")) {
                Device parsedDevice = parseDevice(devNode, moduleLibrary);
                devices.push_back(parsedDevice);
            }
        }

        if (!devices.empty()) {
            device = devices.front();
        }

        return true;
    } catch (const std::exception& e) {
        std::cerr << "Parse error: " << e.what() << std::endl;
        return false;
    }
}

void ESI_Parse::parseVendor(pugi::xml_node root)
{
    pugi::xml_node vendorNode = root.child("Vendor");
    if (!vendorNode) return;

    if (vendorNode.attribute("FileVersion")) vendor.fileVersion = vendorNode.attribute("FileVersion").as_string();
    if (vendorNode.child("Id")) vendor.id = static_cast<uint32_t>(hexStringToInt(vendorNode.child("Id").child_value()));
    if (vendorNode.child("Name")) vendor.name = vendorNode.child("Name").child_value();
    if (vendorNode.child("ImageData16x14")) vendor.imageData = vendorNode.child("ImageData16x14").child_value();
}

void ESI_Parse::parseGroups(pugi::xml_node desc)
{
    pugi::xml_node groupsNode = desc.child("Groups");
    if (!groupsNode) return;

    for (pugi::xml_node groupNode = groupsNode.child("Group"); groupNode; groupNode = groupNode.next_sibling("Group")) {
        GroupInfo group;
        if (groupNode.attribute("SortOrder")) group.sortOrder = groupNode.attribute("SortOrder").as_int();
        if (groupNode.child("Type")) group.type = groupNode.child("Type").child_value();
        if (groupNode.child("Name")) group.name = groupNode.child("Name").child_value();
        if (groupNode.child("ImageData16x14")) group.imageData = groupNode.child("ImageData16x14").child_value();
        groups.push_back(group);
    }
}

ESI_Parse::Device ESI_Parse::parseDevice(pugi::xml_node deviceNode, const std::vector<ModuleInfo>& moduleLibrary)
{
    Device outDevice;

    if (deviceNode.attribute("Physics")) outDevice.physics = deviceNode.attribute("Physics").as_string();

    pugi::xml_node typeNode = deviceNode.child("Type");
    if (typeNode) {
        outDevice.type = typeNode.child_value();
        if (typeNode.attribute("ProductCode")) {
            outDevice.productCode = static_cast<uint32_t>(hexStringToInt(typeNode.attribute("ProductCode").as_string()));
        }
        if (typeNode.attribute("RevisionNo")) {
            outDevice.revisionNo = static_cast<uint32_t>(hexStringToInt(typeNode.attribute("RevisionNo").as_string()));
        }
        if (typeNode.attribute("SerialNo")) outDevice.serialNo = typeNode.attribute("SerialNo").as_string();
    }

    if (deviceNode.child("Name")) outDevice.name = deviceNode.child("Name").child_value();
    if (deviceNode.child("URL")) outDevice.url = deviceNode.child("URL").child_value();
    if (deviceNode.child("GroupType")) outDevice.groupType = deviceNode.child("GroupType").child_value();

    parseInfo(deviceNode, outDevice);
    parseProfile(deviceNode, outDevice);

    for (pugi::xml_node fmmuNode = deviceNode.child("Fmmu"); fmmuNode; fmmuNode = fmmuNode.next_sibling("Fmmu")) {
        outDevice.fmmus.push_back(fmmuNode.child_value());
    }

    int smIndex = 0;
    for (pugi::xml_node smNode = deviceNode.child("Sm"); smNode; smNode = smNode.next_sibling("Sm")) {
        SyncManager sm;
        sm.index = smIndex++;
        sm.name = smNode.child_value();
        if (smNode.attribute("MinSize")) sm.minSize = smNode.attribute("MinSize").as_int();
        if (smNode.attribute("MaxSize")) sm.maxSize = smNode.attribute("MaxSize").as_int();
        if (smNode.attribute("DefaultSize")) sm.defaultSize = smNode.attribute("DefaultSize").as_int();
        if (smNode.attribute("StartAddress")) sm.startAddress = hexStringToInt(smNode.attribute("StartAddress").as_string());
        if (smNode.attribute("ControlByte")) sm.controlByte = static_cast<uint8_t>(hexStringToInt(smNode.attribute("ControlByte").as_string()));
        if (smNode.attribute("Enable")) sm.enable = xmlBool(smNode.attribute("Enable"));
        outDevice.syncManagers.push_back(sm);
    }

    for (pugi::xml_node pdoNode = deviceNode.child("RxPdo"); pdoNode; pdoNode = pdoNode.next_sibling("RxPdo")) {
        Pdo pdo;
        parsePdo(pdoNode, pdo);
        if (pdoNode.attribute("Sm")) pdo.sm = pdoNode.attribute("Sm").as_int();
        if (pdoNode.attribute("Fixed")) pdo.fixed = xmlBool(pdoNode.attribute("Fixed"));
        if (pdoNode.attribute("Mandatory")) pdo.mandatory = xmlBool(pdoNode.attribute("Mandatory"));
        outDevice.rxPdos.push_back(pdo);
    }

    for (pugi::xml_node pdoNode = deviceNode.child("TxPdo"); pdoNode; pdoNode = pdoNode.next_sibling("TxPdo")) {
        Pdo pdo;
        parsePdo(pdoNode, pdo);
        if (pdoNode.attribute("Sm")) pdo.sm = pdoNode.attribute("Sm").as_int();
        if (pdoNode.attribute("Fixed")) pdo.fixed = xmlBool(pdoNode.attribute("Fixed"));
        if (pdoNode.attribute("Mandatory")) pdo.mandatory = xmlBool(pdoNode.attribute("Mandatory"));
        outDevice.txPdos.push_back(pdo);
    }

    parseMailbox(deviceNode, outDevice);
    parseDc(deviceNode, outDevice);
    parseSlots(deviceNode, outDevice);
    parseEeprom(deviceNode, outDevice);

    expandSlotModules(outDevice, moduleLibrary);
    buildDefaultPdoSelection(outDevice.rxPdos);
    buildDefaultPdoSelection(outDevice.txPdos);

    return outDevice;
}

void ESI_Parse::parseInfo(pugi::xml_node deviceNode, Device& outDevice)
{
    pugi::xml_node infoNode = deviceNode.child("Info");
    if (!infoNode) return;

    pugi::xml_node timeoutNode = infoNode.child("StateMachine").child("Timeout");
    if (!timeoutNode) return;

    if (timeoutNode.child("PreopTimeout")) outDevice.stateMachineTimeout.preopTimeout = timeoutNode.child("PreopTimeout").text().as_int();
    if (timeoutNode.child("SafeopOpTimeout")) outDevice.stateMachineTimeout.safeopOpTimeout = timeoutNode.child("SafeopOpTimeout").text().as_int();
    if (timeoutNode.child("BackToInitTimeout")) outDevice.stateMachineTimeout.backToInitTimeout = timeoutNode.child("BackToInitTimeout").text().as_int();
    if (timeoutNode.child("BackToSafeopTimeout")) outDevice.stateMachineTimeout.backToSafeopTimeout = timeoutNode.child("BackToSafeopTimeout").text().as_int();
}

void ESI_Parse::parseMailbox(pugi::xml_node deviceNode, Device& outDevice)
{
    pugi::xml_node timeoutNode = deviceNode.child("Mailbox").child("Timeout");
    if (!timeoutNode) return;

    if (timeoutNode.child("RequestTimeout")) outDevice.mailboxTimeout.requestTimeout = timeoutNode.child("RequestTimeout").text().as_int();
    if (timeoutNode.child("ResponseTimeout")) outDevice.mailboxTimeout.responseTimeout = timeoutNode.child("ResponseTimeout").text().as_int();
}

void ESI_Parse::parseProfile(pugi::xml_node deviceNode, Device& outDevice)
{
    pugi::xml_node profileNode = deviceNode.child("Profile");
    if (!profileNode) return;

    if (profileNode.child("ProfileNo")) outDevice.profileNo = profileNode.child("ProfileNo").text().as_int();

    for (pugi::xml_node channelNode = profileNode.child("ChannelInfo"); channelNode; channelNode = channelNode.next_sibling("ChannelInfo")) {
        if (outDevice.profileNo == 0 && channelNode.child("ProfileNo")) {
            outDevice.profileNo = channelNode.child("ProfileNo").text().as_int();
        }
    }

    pugi::xml_node dictNode = profileNode.child("Dictionary");
    if (dictNode) {
        parseDataTypes(dictNode, outDevice.dataTypes);
        parseObjects(dictNode, outDevice.dataTypes, outDevice.objects);
    }

    pugi::xml_node diagNode = profileNode.child("DiagMessages");
    if (diagNode) parseDiagMessages(diagNode, outDevice.diagMessages);
}

void ESI_Parse::parseDataTypes(pugi::xml_node dictNode, std::map<std::string, DataType>& outDataTypes)
{
    pugi::xml_node dataTypesNode = dictNode.child("DataTypes");
    if (!dataTypesNode) return;

    for (pugi::xml_node dtNode = dataTypesNode.child("DataType"); dtNode; dtNode = dtNode.next_sibling("DataType")) {
        DataType dt;
        if (dtNode.child("Name")) dt.name = dtNode.child("Name").child_value();
        if (dtNode.child("BaseType")) dt.baseType = dtNode.child("BaseType").child_value();
        if (dtNode.child("BitSize")) dt.bitSize = dtNode.child("BitSize").text().as_int();

        for (pugi::xml_node subNode = dtNode.child("SubItem"); subNode; subNode = subNode.next_sibling("SubItem")) {
            ObjectSubItem sub;
            if (subNode.attribute("SubIdx")) sub.subIdx = subNode.attribute("SubIdx").as_int();
            if (subNode.child("SubIdx")) sub.subIdx = subNode.child("SubIdx").text().as_int();
            if (subNode.child("Name")) sub.name = subNode.child("Name").child_value();
            if (subNode.child("Type")) sub.type = subNode.child("Type").child_value();
            if (subNode.child("BitSize")) sub.bitSize = subNode.child("BitSize").text().as_int();
            if (subNode.child("BitOffs")) sub.bitOffset = subNode.child("BitOffs").text().as_int();
            sub.byteOffset = sub.bitOffset / 8;
            sub.bitOffsetInByte = sub.bitOffset % 8;
            if (subNode.child("Comment")) sub.comment = subNode.child("Comment").child_value();
            pugi::xml_node flagsNode = subNode.child("Flags");
            if (flagsNode.child("Access")) sub.access = flagsNode.child("Access").child_value();
            if (flagsNode.child("PdoMapping")) sub.pdoMapping = flagsNode.child("PdoMapping").child_value();
            dt.subItems.push_back(sub);
        }

        for (pugi::xml_node enumNode = dtNode.child("EnumInfo"); enumNode; enumNode = enumNode.next_sibling("EnumInfo")) {
            dt.enumInfos.emplace_back(enumNode.child("Text").child_value(), enumNode.child("Enum").child_value());
        }

        pugi::xml_node arrayNode = dtNode.child("ArrayInfo");
        if (arrayNode) {
            if (arrayNode.child("LBound")) dt.lBound = arrayNode.child("LBound").text().as_int();
            if (arrayNode.child("Elements")) dt.elements = arrayNode.child("Elements").text().as_int();
        }

        if (!dt.name.empty()) outDataTypes[dt.name] = dt;
    }
}

void ESI_Parse::parseObjects(pugi::xml_node dictNode,
                             const std::map<std::string, DataType>& dataTypes,
                             std::vector<ObjectInfo>& outObjects)
{
    pugi::xml_node objectsNode = dictNode.child("Objects");
    if (!objectsNode) return;

    for (pugi::xml_node objNode = objectsNode.child("Object"); objNode; objNode = objNode.next_sibling("Object")) {
        ObjectInfo obj;
        pugi::xml_node indexNode = objNode.child("Index");
        if (indexNode) {
            obj.index = hexStringToInt(indexNode.child_value());
            if (indexNode.attribute("DependOnSlot")) obj.dependOnSlot = xmlBool(indexNode.attribute("DependOnSlot"));
        }
        if (objNode.attribute("DependOnSlot")) obj.dependOnSlot = xmlBool(objNode.attribute("DependOnSlot"));
        if (objNode.child("Name")) obj.name = objNode.child("Name").child_value();
        if (objNode.child("Type")) obj.type = objNode.child("Type").child_value();
        if (objNode.child("BitSize")) obj.bitSize = objNode.child("BitSize").text().as_int();

        pugi::xml_node flagsNode = objNode.child("Flags");
        if (flagsNode.child("Access")) obj.access = flagsNode.child("Access").child_value();

        // 关键修正：Object 本身通常只写 Type，例如 DT7001。
        // 真正的 SDO SubIndex 定义在 DataTypes/DataType/SubItem 中。
        // 因此必须先按 obj.type 找到 DataType，再把 SubItem 全部挂到 obj.subItems。
        auto dtIt = dataTypes.find(obj.type);
        if (dtIt != dataTypes.end()) {
            for (const auto& sub : dtIt->second.subItems) {
                mergeObjectSubItem(obj, sub);
            }
        }

        pugi::xml_node infoNode = objNode.child("Info");
        if (infoNode) {
            if (infoNode.child("DefaultData")) obj.defaultData = infoNode.child("DefaultData").child_value();

            int fallbackSubIdx = 0;
            for (pugi::xml_node subInfo = infoNode.child("SubItem"); subInfo; subInfo = subInfo.next_sibling("SubItem")) {
                ObjectSubItem sub;
                sub.subIdx = -1;

                if (subInfo.attribute("SubIdx")) sub.subIdx = subInfo.attribute("SubIdx").as_int();
                if (subInfo.child("SubIdx")) sub.subIdx = subInfo.child("SubIdx").text().as_int();

                if (subInfo.child("Name")) {
                    sub.name = subInfo.child("Name").child_value();
                    if (sub.subIdx < 0) {
                        int idxFromMatchedName = findSubIndexByName(obj, sub.name);
                        if (idxFromMatchedName >= 0) sub.subIdx = idxFromMatchedName;
                    }
                    if (sub.subIdx < 0) {
                        int idxFromName = parseSubIndexFromText(sub.name);
                        if (idxFromName >= 0) sub.subIdx = idxFromName;
                    }
                }

                if (sub.subIdx < 0) sub.subIdx = fallbackSubIdx;
                ++fallbackSubIdx;

                if (subInfo.child("Type")) sub.type = subInfo.child("Type").child_value();
                if (subInfo.child("BitSize")) sub.bitSize = subInfo.child("BitSize").text().as_int();
                if (subInfo.child("BitOffs")) sub.bitOffset = subInfo.child("BitOffs").text().as_int();
                sub.byteOffset = sub.bitOffset / 8;
                sub.bitOffsetInByte = sub.bitOffset % 8;
                if (subInfo.child("Comment")) sub.comment = subInfo.child("Comment").child_value();

                pugi::xml_node subFlagsNode = subInfo.child("Flags");
                if (subFlagsNode.child("Access")) sub.access = subFlagsNode.child("Access").child_value();
                if (subFlagsNode.child("PdoMapping")) sub.pdoMapping = subFlagsNode.child("PdoMapping").child_value();

                pugi::xml_node subInnerInfo = subInfo.child("Info");
                if (subInnerInfo.child("DefaultData")) sub.defaultData = subInnerInfo.child("DefaultData").child_value();
                if (subInnerInfo.child("Comment")) sub.comment = subInnerInfo.child("Comment").child_value();
                if (subInnerInfo.child("DefaultValue")) sub.defaultValue = subInnerInfo.child("DefaultValue").text().as_int();
                if (subInnerInfo.child("MinValue")) sub.minValue = subInnerInfo.child("MinValue").text().as_int();
                if (subInnerInfo.child("MaxValue")) sub.maxValue = subInnerInfo.child("MaxValue").text().as_int();

                mergeObjectSubItem(obj, sub);
            }
        }

        outObjects.push_back(obj);
    }
}

void ESI_Parse::parsePdo(pugi::xml_node pdoNode, Pdo& pdo) const
{
    pugi::xml_node pdoIndexNode = pdoNode.child("Index");
    if (pdoIndexNode) {
        pdo.index = hexStringToInt(pdoIndexNode.child_value());
        if (pdoIndexNode.attribute("DependOnSlot")) pdo.dependOnSlot = xmlBool(pdoIndexNode.attribute("DependOnSlot"));
    }
    if (pdoNode.attribute("DependOnSlot")) pdo.dependOnSlot = xmlBool(pdoNode.attribute("DependOnSlot"));
    if (pdoNode.attribute("Default")) pdo.defaultSelected = xmlBool(pdoNode.attribute("Default"));
    if (pdoNode.attribute("DefaultSelected")) pdo.defaultSelected = xmlBool(pdoNode.attribute("DefaultSelected"));
    if (pdoNode.child("Default")) {
        std::string value = pdoNode.child("Default").text().as_string();
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        pdo.defaultSelected = !(value.empty() || value == "0" || value == "false" || value == "no");
    }
    if (pdoNode.child("Name")) pdo.name = pdoNode.child("Name").child_value();

    // 解析 PDO 互斥关系。部分厂商 XML 使用 <Exclude>，部分使用 <ExcludePdo>。
    // TwinCAT 中显示为“排除 16#xxxx”的信息就来自这里。
    const char* excludeTags[] = {"Exclude", "ExcludePdo", "Excluded", "ExcludedPdo"};
    for (const char* tag : excludeTags) {
        for (pugi::xml_node exNode = pdoNode.child(tag); exNode; exNode = exNode.next_sibling(tag)) {
            int excludeIndex = 0;
            if (exNode.attribute("Index")) excludeIndex = hexStringToInt(exNode.attribute("Index").as_string());
            else excludeIndex = hexStringToInt(exNode.child_value());
            if (excludeIndex != 0) appendUniqueInt(pdo.excludes, excludeIndex);
        }
    }

    int currentBitOffset = 0;
    for (pugi::xml_node entryNode = pdoNode.child("Entry"); entryNode; entryNode = entryNode.next_sibling("Entry")) {
        PdoEntry entry;
        pugi::xml_node entryIndexNode = entryNode.child("Index");
        if (entryIndexNode) {
            entry.index = hexStringToInt(entryIndexNode.child_value());
            if (entryIndexNode.attribute("DependOnSlot")) entry.dependOnSlot = xmlBool(entryIndexNode.attribute("DependOnSlot"));
        }
        if (entryNode.attribute("DependOnSlot")) entry.dependOnSlot = xmlBool(entryNode.attribute("DependOnSlot"));
        if (entryNode.child("SubIndex")) entry.subIndex = entryNode.child("SubIndex").text().as_int();
        if (entryNode.child("BitLen")) entry.bitLen = entryNode.child("BitLen").text().as_int();
        if (entryNode.child("Name")) entry.name = entryNode.child("Name").child_value();
        if (entryNode.child("DataType")) entry.dataType = entryNode.child("DataType").child_value();

        entry.padding = (entry.index == 0);
        entry.bitOffset = currentBitOffset;
        entry.byteOffset = currentBitOffset / 8;
        entry.bitOffsetInByte = currentBitOffset % 8;
        currentBitOffset += entry.bitLen;

        pdo.entries.push_back(entry);
    }

    pdo.bitSize = currentBitOffset;
    pdo.byteSize = (currentBitOffset + 7) / 8;
}

void ESI_Parse::parseDc(pugi::xml_node deviceNode, Device& outDevice)
{
    pugi::xml_node dcNode = deviceNode.child("Dc");
    if (!dcNode) return;

    for (pugi::xml_node opModeNode = dcNode.child("OpMode"); opModeNode; opModeNode = opModeNode.next_sibling("OpMode")) {
        DcOpMode mode;
        if (opModeNode.child("Name")) mode.name = opModeNode.child("Name").child_value();
        if (opModeNode.child("Desc")) mode.desc = opModeNode.child("Desc").child_value();
        if (opModeNode.child("AssignActivate")) mode.assignActivate = static_cast<uint16_t>(hexStringToInt(opModeNode.child("AssignActivate").child_value()));
        if (opModeNode.child("CycleTimeSync0")) mode.cycleTimeSync0 = opModeNode.child("CycleTimeSync0").text().as_int();
        if (opModeNode.child("CycleTimeSync1")) mode.cycleTimeSync1 = opModeNode.child("CycleTimeSync1").text().as_int();
        outDevice.dcOpModes.push_back(mode);
    }
}

void ESI_Parse::parseSlots(pugi::xml_node deviceNode, Device& outDevice)
{
    pugi::xml_node slotsNode = deviceNode.child("Slots");
    if (!slotsNode) return;

    if (slotsNode.attribute("DownloadModuleIdentList")) outDevice.slotsInfo.downloadModuleIdentList = xmlBool(slotsNode.attribute("DownloadModuleIdentList"));
    if (slotsNode.attribute("IdentifyModuleBy")) outDevice.slotsInfo.identifyModuleBy = slotsNode.attribute("IdentifyModuleBy").as_string();
    if (slotsNode.attribute("SlotIndexIncrement")) outDevice.slotsInfo.slotIndexIncrement = hexStringToInt(slotsNode.attribute("SlotIndexIncrement").as_string());
    if (slotsNode.attribute("SlotPdoIncrement")) outDevice.slotsInfo.slotPdoIncrement = hexStringToInt(slotsNode.attribute("SlotPdoIncrement").as_string());

    for (pugi::xml_node slotNode = slotsNode.child("Slot"); slotNode; slotNode = slotNode.next_sibling("Slot")) {
        SlotInfo slot;
        if (slotNode.attribute("MinInstances")) slot.minInstances = slotNode.attribute("MinInstances").as_int();
        if (slotNode.attribute("MaxInstances")) slot.maxInstances = slotNode.attribute("MaxInstances").as_int();
        if (slotNode.attribute("TreeView")) slot.treeView = slotNode.attribute("TreeView").as_string();
        if (slotNode.child("Name")) slot.name = slotNode.child("Name").child_value();

        for (pugi::xml_node miNode = slotNode.child("ModuleIdent"); miNode; miNode = miNode.next_sibling("ModuleIdent")) {
            int moduleIdent = hexStringToInt(miNode.child_value());
            appendUniqueInt(slot.moduleIdents, moduleIdent);

            if (miNode.attribute("Default")) {
                std::string defaultValue = miNode.attribute("Default").as_string();
                if (defaultValue.rfind("#x", 0) == 0 || defaultValue.rfind("#X", 0) == 0 ||
                    defaultValue.rfind("0x", 0) == 0 || defaultValue.rfind("0X", 0) == 0) {
                    slot.defaultModuleIdent = hexStringToInt(defaultValue);
                } else if (xmlBool(miNode.attribute("Default"))) {
                    slot.defaultModuleIdent = moduleIdent;
                }
                slot.hasDefaultModuleIdent = true;
                appendUniqueInt(slot.moduleIdents, slot.defaultModuleIdent);
            }
        }

        if (!slot.hasDefaultModuleIdent && slot.moduleIdents.size() == 1) {
            slot.defaultModuleIdent = slot.moduleIdents.front();
            slot.hasDefaultModuleIdent = true;
        }

        for (pugi::xml_node mcNode = slotNode.child("ModuleClass"); mcNode; mcNode = mcNode.next_sibling("ModuleClass")) {
            ModuleClass mc;
            if (mcNode.child("ClassName")) mc.className = mcNode.child("ClassName").child_value();
            if (mcNode.child("Name")) mc.name = mcNode.child("Name").child_value();
            if (mcNode.child("ImageData16x14")) mc.imageData = mcNode.child("ImageData16x14").child_value();
            outDevice.slotsInfo.moduleClasses.push_back(mc);
        }

        outDevice.slotsInfo.slots.push_back(slot);
    }
}

void ESI_Parse::parseEeprom(pugi::xml_node deviceNode, Device& outDevice)
{
    pugi::xml_node eepromNode = deviceNode.child("Eeprom");
    if (!eepromNode) return;

    if (eepromNode.child("ByteSize")) outDevice.eeprom.byteSize = eepromNode.child("ByteSize").text().as_int();
    if (eepromNode.child("ConfigData")) outDevice.eeprom.configData = eepromNode.child("ConfigData").child_value();
    if (eepromNode.child("BootStrap")) outDevice.eeprom.bootStrap = eepromNode.child("BootStrap").child_value();
}

void ESI_Parse::parseDiagMessages(pugi::xml_node diagNode, std::vector<DiagMessage>& outMessages)
{
    for (pugi::xml_node msgNode = diagNode.child("DiagMessage"); msgNode; msgNode = msgNode.next_sibling("DiagMessage")) {
        DiagMessage msg;
        if (msgNode.child("TextId")) msg.textId = msgNode.child("TextId").text().as_int();
        if (msgNode.child("MessageText")) msg.messageText = msgNode.child("MessageText").child_value();
        outMessages.push_back(msg);
    }
}

std::vector<ESI_Parse::ModuleInfo> ESI_Parse::parseModules(pugi::xml_node modulesNode)
{
    std::vector<ModuleInfo> parsedModules;

    for (pugi::xml_node moduleNode = modulesNode.child("Module"); moduleNode; moduleNode = moduleNode.next_sibling("Module")) {
        ModuleInfo module;

        pugi::xml_node typeNode = moduleNode.child("Type");
        if (typeNode) {
            module.type = typeNode.child_value();
            if (typeNode.attribute("ModuleIdent")) module.moduleIdent = hexStringToInt(typeNode.attribute("ModuleIdent").as_string());
            if (typeNode.attribute("ModuleClass")) module.moduleClass = typeNode.attribute("ModuleClass").as_string();
        }

        if (moduleNode.child("Name")) module.name = moduleNode.child("Name").child_value();
        if (moduleNode.attribute("ModulePdoGroup")) module.modulePdoGroup = moduleNode.attribute("ModulePdoGroup").as_int();

        for (pugi::xml_node pdoNode = moduleNode.child("RxPdo"); pdoNode; pdoNode = pdoNode.next_sibling("RxPdo")) {
            Pdo pdo;
            parsePdo(pdoNode, pdo);
            if (pdoNode.attribute("Sm")) pdo.sm = pdoNode.attribute("Sm").as_int();
            if (pdoNode.attribute("Fixed")) pdo.fixed = xmlBool(pdoNode.attribute("Fixed"));
            if (pdoNode.attribute("Mandatory")) pdo.mandatory = xmlBool(pdoNode.attribute("Mandatory"));
            module.rxPdos.push_back(pdo);
        }

        for (pugi::xml_node pdoNode = moduleNode.child("TxPdo"); pdoNode; pdoNode = pdoNode.next_sibling("TxPdo")) {
            Pdo pdo;
            parsePdo(pdoNode, pdo);
            if (pdoNode.attribute("Sm")) pdo.sm = pdoNode.attribute("Sm").as_int();
            if (pdoNode.attribute("Fixed")) pdo.fixed = xmlBool(pdoNode.attribute("Fixed"));
            if (pdoNode.attribute("Mandatory")) pdo.mandatory = xmlBool(pdoNode.attribute("Mandatory"));
            module.txPdos.push_back(pdo);
        }

        for (pugi::xml_node initNode = moduleNode.child("InitCmd"); initNode; initNode = initNode.next_sibling("InitCmd")) {
            InitCmd cmd;
            if (initNode.child("Transition")) cmd.transition = initNode.child("Transition").child_value();
            if (initNode.child("Index")) {
                cmd.index = hexStringToInt(initNode.child("Index").child_value());
                if (initNode.child("Index").attribute("DependOnSlot")) cmd.dependOnSlot = xmlBool(initNode.child("Index").attribute("DependOnSlot"));
            }
            if (initNode.child("SubIndex")) cmd.subIndex = initNode.child("SubIndex").text().as_int();
            if (initNode.child("Data")) cmd.data = initNode.child("Data").child_value();
            if (initNode.child("Comment")) cmd.comment = initNode.child("Comment").child_value();
            module.initCmds.push_back(cmd);
        }

        pugi::xml_node dictNode = moduleNode.child("Profile").child("Dictionary");
        if (dictNode) {
            parseDataTypes(dictNode, module.dataTypes);
            parseObjects(dictNode, module.dataTypes, module.objects);
        }

        parsedModules.push_back(module);
    }

    return parsedModules;
}

ESI_Parse::Pdo ESI_Parse::makeSlotPdo(const Pdo& source,
                                      int slotNo,
                                      const SlotInfo& slot,
                                      int pdoIncrement,
                                      int indexIncrement) const
{
    Pdo result = source;

    if (result.dependOnSlot) {
        result.index += slotNo * pdoIncrement;
        result.dependOnSlot = false;
    }

    for (auto& excludeIndex : result.excludes) {
        if (excludeIndex != 0) excludeIndex += slotNo * pdoIncrement;
    }

    for (auto& entry : result.entries) {
        if (entry.dependOnSlot && entry.index != 0) {
            entry.index += slotNo * indexIncrement;
            entry.dependOnSlot = false;
        }
    }

    std::ostringstream oss;
    oss << result.name;
    if (!slot.name.empty()) oss << " [" << slot.name << "]";
    else oss << " [Slot " << (slotNo + 1) << "]";
    result.name = oss.str();

    return result;
}

void ESI_Parse::expandSlotModules(Device& outDevice, const std::vector<ModuleInfo>& moduleLibrary) const
{
    if (outDevice.slotsInfo.slots.empty() || moduleLibrary.empty()) return;

    const int pdoIncrement = outDevice.slotsInfo.slotPdoIncrement;
    const int indexIncrement = outDevice.slotsInfo.slotIndexIncrement;

    for (size_t slotNo = 0; slotNo < outDevice.slotsInfo.slots.size(); ++slotNo) {
        const SlotInfo& slot = outDevice.slotsInfo.slots[slotNo];

        for (int moduleIdent : slot.moduleIdents) {
            auto it = std::find_if(moduleLibrary.begin(), moduleLibrary.end(), [moduleIdent](const ModuleInfo& module) {
                return module.moduleIdent == moduleIdent;
            });

            if (it == moduleLibrary.end()) continue;

            const ModuleInfo& module = *it;
            outDevice.modules.push_back(module);

            for (const auto& pdo : module.rxPdos) {
                outDevice.rxPdos.push_back(makeSlotPdo(pdo, static_cast<int>(slotNo), slot, pdoIncrement, indexIncrement));
            }

            for (const auto& pdo : module.txPdos) {
                outDevice.txPdos.push_back(makeSlotPdo(pdo, static_cast<int>(slotNo), slot, pdoIncrement, indexIncrement));
            }

            for (auto cmd : module.initCmds) {
                if (cmd.dependOnSlot && cmd.index != 0) {
                    cmd.index += static_cast<int>(slotNo) * indexIncrement;
                    cmd.dependOnSlot = false;
                }
                outDevice.initCmds.push_back(cmd);
            }

            for (auto obj : module.objects) {
                if (obj.dependOnSlot && obj.index != 0) {
                    obj.index += static_cast<int>(slotNo) * indexIncrement;
                    obj.dependOnSlot = false;
                }
                outDevice.objects.push_back(obj);
            }

            for (const auto& dt : module.dataTypes) {
                outDevice.dataTypes[dt.first] = dt.second;
            }
        }
    }
}

void ESI_Parse::printOneDeviceSummary(const Device& outDevice, size_t deviceIndex) const
{
    std::cout << "[" << deviceIndex << "] " << outDevice.name << "\n";
    std::cout << "    Type: " << outDevice.type << "\n";
    std::cout << "    Product Code: 0x" << std::hex << std::uppercase
              << std::setw(8) << std::setfill('0') << outDevice.productCode
              << std::dec << std::nouppercase << std::setfill(' ') << "\n";
    std::cout << "    Revision: " << outDevice.revisionNo << "\n";
    std::cout << "    Physics: " << outDevice.physics << "\n";
    std::cout << "    Group Type: " << outDevice.groupType << "\n";
    std::cout << "    FMMU: " << outDevice.fmmus.size()
              << ", SM: " << outDevice.syncManagers.size()
              << ", RxPDO: " << outDevice.rxPdos.size()
              << ", TxPDO: " << outDevice.txPdos.size()
              << ", Objects: " << outDevice.objects.size()
              << ", DC Modes: " << outDevice.dcOpModes.size() << "\n";
}


void ESI_Parse::printPdoList(const std::vector<Pdo>& pdos, const std::string& directionName) const
{
    for (const auto& pdo : pdos) {
        std::cout << "    " << directionName << " " << formatHex(pdo.index);
        if (!pdo.name.empty()) std::cout << " " << pdo.name;
        if (!pdo.excludes.empty()) {
            std::cout << " (排除 ";
            for (size_t i = 0; i < pdo.excludes.size(); ++i) {
                if (i > 0) std::cout << ", ";
                std::cout << formatHex(pdo.excludes[i]);
            }
            std::cout << ")";
        }
        std::cout << " | SM=" << pdo.sm
                  << " | entries=" << pdo.entries.size()
                  << " | bits=" << pdo.bitSize
                  << " | bytes=" << pdo.byteSize;
        if (pdo.fixed) std::cout << " | Fixed";
        if (pdo.mandatory) std::cout << " | Mandatory";
        if (pdo.defaultSelected) std::cout << " | Default";
        if (pdo.selected) std::cout << " | Selected";
        if (!pdo.selectable) std::cout << " | Disabled";
        if (!pdo.excludedBy.empty()) {
            std::cout << " | ExcludedBy=";
            for (size_t i = 0; i < pdo.excludedBy.size(); ++i) {
                if (i > 0) std::cout << ",";
                std::cout << formatHex(pdo.excludedBy[i]);
            }
        }
        std::cout << "\n";

        for (const auto& entry : pdo.entries) {
            std::cout << "        ";
            if (entry.padding) {
                std::cout << "<Padding>";
            } else {
                std::cout << std::left << std::setw(32) << entry.name << std::right;
            }
            std::cout << "  " << std::left << std::setw(8) << entry.dataType << std::right
                      << "  " << formatHex(entry.index) << ":" << formatHex(entry.subIndex, 2)
                      << "  bitLen=" << entry.bitLen
                      << "  offset=" << entry.byteOffset << "." << entry.bitOffsetInByte
                      << "\n";
        }
    }
}


bool ESI_Parse::pdoConflict(const Pdo& lhs, const Pdo& rhs)
{
    if (lhs.index == 0 || rhs.index == 0 || lhs.index == rhs.index) return false;

    auto lhsExcludesRhs = std::find(lhs.excludes.begin(), lhs.excludes.end(), rhs.index) != lhs.excludes.end();
    auto rhsExcludesLhs = std::find(rhs.excludes.begin(), rhs.excludes.end(), lhs.index) != rhs.excludes.end();
    return lhsExcludesRhs || rhsExcludesLhs;
}

void ESI_Parse::recomputePdoSelectable(std::vector<Pdo>& pdos)
{
    for (auto& pdo : pdos) {
        pdo.selectable = true;
        pdo.excludedBy.clear();
    }

    for (const auto& selectedPdo : pdos) {
        if (!selectedPdo.selected) continue;

        for (auto& candidate : pdos) {
            if (candidate.index == selectedPdo.index) continue;
            if (!candidate.selected && pdoConflict(selectedPdo, candidate)) {
                candidate.selectable = false;
                if (std::find(candidate.excludedBy.begin(), candidate.excludedBy.end(), selectedPdo.index) == candidate.excludedBy.end()) {
                    candidate.excludedBy.push_back(selectedPdo.index);
                }
            }
        }
    }
}

void ESI_Parse::buildDefaultPdoSelection(std::vector<Pdo>& pdos)
{
    for (auto& pdo : pdos) {
        pdo.selected = false;
        pdo.selectable = true;
        pdo.excludedBy.clear();
    }

    if (pdos.empty()) return;

    bool hasConflictRule = false;
    for (size_t i = 0; i < pdos.size(); ++i) {
        for (size_t j = i + 1; j < pdos.size(); ++j) {
            if (pdoConflict(pdos[i], pdos[j])) {
                hasConflictRule = true;
                break;
            }
        }
        if (hasConflictRule) break;
    }

    bool hasExplicitDefault = false;
    for (const auto& pdo : pdos) {
        if (pdo.mandatory || pdo.defaultSelected) {
            hasExplicitDefault = true;
            break;
        }
    }

    // 没有 Exclude 关系时，通常是 Beckhoff ELxxxx 这类通道型 IO，多个 PDO 可以全部启用。
    // 没有显式默认项且没有互斥规则时，直接全选，方便生成完整 0x1C12 / 0x1C13。
    const bool selectAllWhenNoConflict = !hasExplicitDefault && !hasConflictRule;

    for (auto& pdo : pdos) {
        const bool wantSelect = pdo.mandatory || pdo.defaultSelected || selectAllWhenNoConflict;
        if (!wantSelect) continue;

        bool blockedBySelectedMandatory = false;
        std::vector<size_t> selectedConflicts;

        for (size_t i = 0; i < pdos.size(); ++i) {
            if (!pdos[i].selected) continue;
            if (!pdoConflict(pdo, pdos[i])) continue;

            if (pdos[i].mandatory && !pdo.mandatory) {
                blockedBySelectedMandatory = true;
                break;
            }
            selectedConflicts.push_back(i);
        }

        if (blockedBySelectedMandatory) continue;

        for (size_t conflictIndex : selectedConflicts) {
            if (!pdos[conflictIndex].mandatory) {
                pdos[conflictIndex].selected = false;
            }
        }

        pdo.selected = true;
    }

    // 有互斥规则但 XML 没给 Default/Mandatory 时，至少选一个，避免空配置。
    if (hasConflictRule) {
        bool anySelected = false;
        for (const auto& pdo : pdos) {
            if (pdo.selected) {
                anySelected = true;
                break;
            }
        }
        if (!anySelected) {
            pdos.front().selected = true;
        }
    }

    recomputePdoSelectable(pdos);
}

bool ESI_Parse::setPdoSelected(std::vector<Pdo>& pdos, int targetIndex, bool selected, std::string* errorMessage)
{
    auto targetIt = std::find_if(pdos.begin(), pdos.end(), [targetIndex](const Pdo& pdo) {
        return pdo.index == targetIndex;
    });

    if (targetIt == pdos.end()) {
        if (errorMessage) *errorMessage = "PDO index not found";
        return false;
    }

    if (!selected) {
        if (targetIt->mandatory) {
            if (errorMessage) *errorMessage = "Mandatory PDO cannot be deselected";
            return false;
        }
        targetIt->selected = false;
        recomputePdoSelectable(pdos);
        return true;
    }

    for (auto& pdo : pdos) {
        if (!pdo.selected) continue;
        if (pdo.index == targetIndex) continue;
        if (!pdoConflict(*targetIt, pdo)) continue;

        if (pdo.mandatory) {
            if (errorMessage) *errorMessage = "Target PDO conflicts with a mandatory selected PDO";
            return false;
        }
    }

    for (auto& pdo : pdos) {
        if (!pdo.selected) continue;
        if (pdo.index == targetIndex) continue;
        if (pdoConflict(*targetIt, pdo)) {
            pdo.selected = false;
        }
    }

    targetIt->selected = true;
    recomputePdoSelectable(pdos);
    return true;
}

bool ESI_Parse::selectPdo(std::vector<Pdo>& pdos, int targetIndex, std::string* errorMessage)
{
    return setPdoSelected(pdos, targetIndex, true, errorMessage);
}

bool ESI_Parse::deselectPdo(std::vector<Pdo>& pdos, int targetIndex, std::string* errorMessage)
{
    return setPdoSelected(pdos, targetIndex, false, errorMessage);
}

std::vector<int> ESI_Parse::getSelectedPdoIndexes(const std::vector<Pdo>& pdos)
{
    std::vector<int> indexes;
    for (const auto& pdo : pdos) {
        if (pdo.selected) indexes.push_back(pdo.index);
    }
    return indexes;
}

std::vector<ESI_Parse::SdoWrite> ESI_Parse::buildPdoAssignWrites(int assignIndex, const std::vector<Pdo>& pdos)
{
    std::vector<SdoWrite> writes;
    std::vector<int> selectedIndexes = getSelectedPdoIndexes(pdos);

    writes.push_back({assignIndex, 0x00, 0x00, 8, "clear PDO assign"});

    for (size_t i = 0; i < selectedIndexes.size(); ++i) {
        std::ostringstream oss;
        oss << "assign PDO 0x" << std::hex << std::uppercase << selectedIndexes[i];
        writes.push_back({assignIndex, static_cast<int>(i + 1), selectedIndexes[i], 16, oss.str()});
    }

    writes.push_back({assignIndex, 0x00, static_cast<int>(selectedIndexes.size()), 8, "enable PDO assign"});
    return writes;
}

std::vector<ESI_Parse::SdoWrite> ESI_Parse::buildRxPdoAssignWrites(const Device& outDevice)
{
    return buildPdoAssignWrites(0x1C12, outDevice.rxPdos);
}

std::vector<ESI_Parse::SdoWrite> ESI_Parse::buildTxPdoAssignWrites(const Device& outDevice)
{
    return buildPdoAssignWrites(0x1C13, outDevice.txPdos);
}

void ESI_Parse::printConfig() const
{
    std::cout << "====================================\n";
    std::cout << "EtherCAT ESI Configuration\n";
    std::cout << "====================================\n\n";
    std::cout << "Vendor: " << vendor.name << " (ID: " << vendor.id << ")\n";
    if (!vendor.fileVersion.empty()) std::cout << "  File Version: " << vendor.fileVersion << "\n";
    std::cout << "Groups: " << groups.size() << "\n";
    std::cout << "Devices: " << devices.size() << "\n\n";

    for (size_t i = 0; i < devices.size(); ++i) {
        printOneDeviceSummary(devices[i], i);
    }

    std::cout << "====================================\n";
}

const ESI_Parse::VendorInfo& ESI_Parse::getVendor() const { return vendor; }
const std::vector<ESI_Parse::GroupInfo>& ESI_Parse::getGroups() const { return groups; }
const std::vector<ESI_Parse::Device>& ESI_Parse::getDevices() const { return devices; }
size_t ESI_Parse::getDeviceCount() const { return devices.size(); }

const ESI_Parse::Device* ESI_Parse::getDeviceConfig(size_t index) const
{
    if (index >= devices.size()) return nullptr;
    return &devices[index];
}

#include <optional>  // 提供 std::optional 和 std::nullopt

const ESI_Parse::Device* ESI_Parse::findDeviceConfig(uint32_t productCode, std::optional<uint32_t> revisionNo) const
{
    for (const auto& dev : devices)          // 依次遍历所有设备配置
    {
        if (dev.productCode != productCode)  // 产品代码不匹配
        {
            continue;                        // 继续检查下一个设备
        }

        if (revisionNo.has_value() &&        // 调用者指定了版本号
            dev.revisionNo != revisionNo.value()) // 但设备版本号不匹配
        {
            continue;                        // 继续检查下一个设备
        }

        return &dev;                          // 产品代码和版本条件均满足
    }

    return nullptr;                          // 没有找到匹配设备
}

// const ESI_Parse::Device& ESI_Parse::getDevice() const { return device; }
// const ESI_Parse::StateMachineTimeout& ESI_Parse::getStateMachineTimeout() const { return device.stateMachineTimeout; }
// const ESI_Parse::MailboxTimeout& ESI_Parse::getMailboxTimeout() const { return device.mailboxTimeout; }
// const std::vector<std::string>& ESI_Parse::getFmmus() const { return device.fmmus; }
// const std::vector<ESI_Parse::SyncManager>& ESI_Parse::getSyncManagers() const { return device.syncManagers; }
// const std::vector<ESI_Parse::Pdo>& ESI_Parse::getRxPdos() const { return device.rxPdos; }
// const std::vector<ESI_Parse::Pdo>& ESI_Parse::getTxPdos() const { return device.txPdos; }
// const std::vector<ESI_Parse::ObjectInfo>& ESI_Parse::getObjects() const { return device.objects; }
// const std::vector<ESI_Parse::ModuleInfo>& ESI_Parse::getModules() const { return device.modules; }
// const std::map<std::string, ESI_Parse::DataType>& ESI_Parse::getDataTypes() const { return device.dataTypes; }
// const ESI_Parse::EepromInfo& ESI_Parse::getEeprom() const { return device.eeprom; }
// const std::vector<ESI_Parse::DcOpMode>& ESI_Parse::getDcOpModes() const { return device.dcOpModes; }
// const std::vector<ESI_Parse::DiagMessage>& ESI_Parse::getDiagMessages() const { return device.diagMessages; }
// const ESI_Parse::SlotsInfo& ESI_Parse::getSlotsInfo() const { return device.slotsInfo; }
// const std::vector<ESI_Parse::InitCmd>& ESI_Parse::getInitCmds() const { return device.initCmds; }

ESI_Parse::ObjectInfo* ESI_Parse::findObject(int index)
{
    auto it = std::find_if(device.objects.begin(), device.objects.end(), [index](const ObjectInfo& obj) {
        return obj.index == index;
    });
    if (it == device.objects.end()) return nullptr;
    return &(*it);
}

ESI_Parse::Pdo* ESI_Parse::findPdo(const std::string& name, bool isRx)
{
    auto& list = isRx ? device.rxPdos : device.txPdos;
    auto it = std::find_if(list.begin(), list.end(), [&name](const Pdo& pdo) {
        return pdo.name == name;
    });
    if (it == list.end()) return nullptr;
    return &(*it);
}

ESI_Parse::ModuleInfo* ESI_Parse::findModule(const std::string& type)
{
    auto it = std::find_if(device.modules.begin(), device.modules.end(), [&type](const ModuleInfo& module) {
        return module.type == type;
    });
    if (it == device.modules.end()) return nullptr;
    return &(*it);
}

ESI_Parse::ModuleInfo* ESI_Parse::findModuleByIdent(int ident)
{
    auto it = std::find_if(device.modules.begin(), device.modules.end(), [ident](const ModuleInfo& module) {
        return module.moduleIdent == ident;
    });
    if (it == device.modules.end()) return nullptr;
    return &(*it);
}

void ESI_Parse::addRxPdos(const std::vector<Pdo>& pdos)
{
    device.rxPdos.insert(device.rxPdos.end(), pdos.begin(), pdos.end());
    if (!devices.empty()) devices.front().rxPdos = device.rxPdos;
}

void ESI_Parse::addTxPdos(const std::vector<Pdo>& pdos)
{
    device.txPdos.insert(device.txPdos.end(), pdos.begin(), pdos.end());
    if (!devices.empty()) devices.front().txPdos = device.txPdos;
}

size_t ESI_Parse::getRxEntryPdoSize() const
{
    size_t size = 0;
    for (const auto& pdo : device.rxPdos) size += pdo.entries.size();
    return size;
}

size_t ESI_Parse::getTxEntryPdoSize() const
{
    size_t size = 0;
    for (const auto& pdo : device.txPdos) size += pdo.entries.size();
    return size;
}



uint32_t ESI_Parse::getVendorId() const { return vendor.id; }
uint32_t ESI_Parse::getProductCode() const { return device.productCode; }
std::string ESI_Parse::getVendorName() const { return vendor.name; }
std::string ESI_Parse::getProductName() const { return device.name; }


std::vector<int> GetAxisChannel(std::vector<int> vPdoList)
{
    std::vector<int> vChannel;
    vChannel.clear();
    bool Channel_TX[4] = {0,0,0,0};
    bool Channel_RX[4] = {0,0,0,0};
    for (size_t i = 0; i < vPdoList.size(); i++)
    {
        if(0x6040 == vPdoList[i])
        {
            Channel_RX[0] = true;
        }
        if(0x6840 == vPdoList[i])
        {
            Channel_RX[1] = true;
        }
        if(0x6041 == vPdoList[i])
        {
            Channel_TX[0] = true;
        }
        if(0x6841 == vPdoList[i])
        {
            Channel_TX[1] = true;
        }
    }
    for (size_t i = 0; i < 4; i++)
    {
        if(Channel_TX[i] && Channel_RX[i])
        {
            vChannel.push_back(i);
        }
    }
    return vChannel;
}