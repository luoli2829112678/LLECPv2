#ifndef HSTR_MODULE_H
#define HSTR_MODULE_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <HSTR/generated/interface_signature.h>
/* HSTR.so 与生成的 PLC 模块之间的内部 C ABI；更改布局时必须更新版本。 */
/* 硬件接口增删由生成的签名校验；只改这里的固定结构时才需要更新版本。 */
#define HSTR_MODULE_ABI 2

typedef struct
{
    const char* name; /* 完整 IEC 变量路径。 */
    const char* type; /* IEC 类型描述字符串。 */
    void* address;    /* 实际变量存储地址，仅库内部读写。 */
    size_t size;      /* 原生变量大小，单位字节。 */
    int read_only;    /* 非零代表常量，只允许读取。 */
} HSTR_ModuleVariable;

typedef struct
{
    void* user; /* 宿主 Runtime 上下文，不由生成模块管理生命周期。 */
    /* 每个硬件函数的强类型指针由接口清单生成。 */
#include <HSTR/generated/host_fields.inc>
    /* 语句/循环检测：上下文、源文件、行号、调用深度、是否可进入；空文件表示轮询点。 */
    void (*trace)(void*, const char*, uint32_t, int, int);
} HSTR_ModuleHost;
#endif
