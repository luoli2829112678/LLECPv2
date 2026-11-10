#ifndef _LL_ROBOT_STRUCT_H
#define _LL_ROBOT_STRUCT_H

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

#include "LL_Errorcode.h"
#include "LL_PublicConstant.h"
#include "LL_String.h"
#include "json/cJSON.h"

using namespace std;

typedef struct _ST_JSONOperator
{
    static int get_str_value(const cJSON* itemObj, string& value)
    {
        if (!itemObj || !cJSON_IsString(itemObj) || !itemObj->valuestring)
        {
            return REC_ParamError;
        }
        value = itemObj->valuestring;
        return REC_Successed;
    }

    static int get_int_value(const cJSON* itemObj, int& value)
    {
        if (!itemObj)
        {
            return REC_ParamError;
        }
        if (cJSON_IsNumber(itemObj))
        {
            value = itemObj->valueint;
            return REC_Successed;
        }
        return cJSON_IsString(itemObj) && itemObj->valuestring && LL_Str2Int(itemObj->valuestring, value)
            ? REC_Successed : REC_ParamError;
    }

    static int get_uint32_value(const cJSON* itemObj, std::uint32_t& value)
    {
        if (!itemObj)
        {
            return REC_ParamError;
        }
        if (cJSON_IsNumber(itemObj))
        {
            if (itemObj->valuedouble < 0 || itemObj->valuedouble > 4294967295.0)
            {
                return REC_ParamError;
            }
            value = static_cast<std::uint32_t>(itemObj->valuedouble);
            return static_cast<double>(value) == itemObj->valuedouble ? REC_Successed : REC_ParamError;
        }
        if (!cJSON_IsString(itemObj) || !itemObj->valuestring)
        {
            return REC_ParamError;
        }
        errno = 0;
        char* end = nullptr;
        const unsigned long long parsed = std::strtoull(itemObj->valuestring, &end, 0);
        if (ERANGE == errno || end == itemObj->valuestring || '\0' != *end || parsed > 0xFFFFFFFFULL)
        {
            return REC_ParamError;
        }
        value = static_cast<std::uint32_t>(parsed);
        return REC_Successed;
    }

    static int get_short_int_value(const cJSON* itemObj, short int& value)
    {
        if (!itemObj)
        {
            return REC_ParamError;
        }
        if (cJSON_IsNumber(itemObj))
        {
            value = static_cast<short int>(itemObj->valueint);
            return REC_Successed;
        }
        return cJSON_IsString(itemObj) && itemObj->valuestring && LL_Str2ShortInt(itemObj->valuestring, value)
            ? REC_Successed : REC_ParamError;
    }

    static int get_double_value(const cJSON* itemObj, double& value)
    {
        if (!itemObj)
        {
            return REC_ParamError;
        }
        if (cJSON_IsNumber(itemObj))
        {
            value = itemObj->valuedouble;
            return REC_Successed;
        }
        return cJSON_IsString(itemObj) && itemObj->valuestring && LL_Str2Num(itemObj->valuestring, value)
            ? REC_Successed : REC_ParamError;
    }
} ST_JSONOperator;

template <typename T>
struct ST_VectorOperator
{
    static void read_item(const vector<T>& src, vector<T>& dst, int pos, int size)
    {
        dst.clear();
        if (pos < 0 || size <= 0 || static_cast<size_t>(pos) >= src.size())
        {
            return;
        }
        const size_t actualSize = std::min(static_cast<size_t>(size), src.size() - pos);
        dst.assign(src.begin() + pos, src.begin() + pos + actualSize);
    }
};

typedef struct ST_AlarmMsgParam
{
    int almID;
    string strModule;
    vector<string> locationValueVec;

    ST_AlarmMsgParam() : almID(0) {}

    ST_AlarmMsgParam(const string& module, int alarmId, vector<string>& locationValues)
        : almID(alarmId), strModule(module), locationValueVec(locationValues)
    {
    }
} AlarmMsgParam;

typedef struct _ST_ErrorMetaData
{
    int robotId;
    int _err_code;
    string _err_str;

    _ST_ErrorMetaData()
    {
        _init();
    }

    explicit _ST_ErrorMetaData(int robotIdValue)
        : robotId(robotIdValue), _err_code(0)
    {
    }

    int error_code() const
    {
        return _err_code;
    }

    string error_str() const
    {
        return _err_str;
    }

    void _init()
    {
        robotId = 0;
        _err_code = 0;
        _err_str.clear();
    }

    void _copy(const _ST_ErrorMetaData& src)
    {
        robotId = src.robotId;
        _err_code = src._err_code;
        _err_str = src._err_str;
    }

    static bool IsRobotIdValid(int robotIdValue)
    {
        return robotIdValue >= 0 && robotIdValue <= 5;
    }

    bool IsOverrideValid(double overrideValue)
    {
        return overrideValue >= 0.01 && overrideValue <= 1.0;
    }

    static int PickRobotId(const vector<string>& params, bool fromXml, int& robotIdValue)
    {
        if ((fromXml && params.size() < 2) || (!fromXml && params.empty()))
        {
            return REC_ParamError;
        }
        const string& value = fromXml ? params.at(1) : params.at(0);
        return !value.empty() && LL_Str2Int(value, robotIdValue) ? REC_Successed : REC_ParamError;
    }
} ST_ErrorMetaData;

typedef ST_ErrorMetaData ST_MsgCheck;

struct ST_AxisErrorCodes
{
    int data[LLPC_MaxAxis] = {};
};

typedef struct _ST_ControllerErrorInfo
{
    int errorAxisID;
    int errorCode;
    int axisGroupErrorCode;
    ST_AxisErrorCodes axisErrorCode;

    _ST_ControllerErrorInfo()
        : errorAxisID(0), errorCode(0), axisGroupErrorCode(0)
    {
    }
} ST_ControllerErrorInfo;

#endif
