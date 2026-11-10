#ifndef _LLC_ACTION_General_H
#define _LLC_ACTION_General_H

#include "LLC_SMBase.h"

#ifdef __cplusplus
extern "C"
{
#endif//extern "C"

// 读取SDO
bool action_LLCReadSDO(actionParam);

// 写入SDO
bool action_LLCWriteSDO(actionParam);
// 写入PDO
bool action_LLCWritePDO(actionParam);
//升级XML
bool action_UpdateESI(actionParam);
//升级固件
bool action_FOEwrite(actionParam);
//升级固件
bool action_FOEread(actionParam);
//获取写入进度
bool action_GetFOEwriteProgress(actionParam);
//获取下载进度
bool action_GetFOEreadProgress(actionParam);

bool action_ReadAxisInformation(actionParam);

bool action_SetIO(actionParam);

#ifdef __cplusplus
}
#endif //extern "C"

#endif 