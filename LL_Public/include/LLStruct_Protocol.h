#ifndef _LL_PROTOCOL_STRUCT_H_
#define _LL_PROTOCOL_STRUCT_H_

#pragma pack(push, 1)

#define DataBufferSize 10485760 // 10M
typedef struct _ST_DataBuffer
{
    int unDataBufferTotalSize;
    int unWritePos;
    int unReadPos;
    int unCurDataSize;
    char* data;
} ST_DataBuffer;

#define MagicNUM 0x5242544C
#define MagicStr "LTBR"

typedef struct _ST_LLPackHeaderSend
{
    unsigned int dwMagicNum;
    unsigned int dwTotalSize;
    unsigned int dwDataSize;
} ST_LLPackHeaderSend;

typedef struct _ST_LLPackHeader
{
    unsigned int dwMagicNum;
    unsigned int dwTotalSize;
    unsigned int dwDataSize;
    unsigned char data[1];
} ST_LLPackHeader;

#define CMD_LEN 64
enum EN_Cmd_Type
{
    enCmdType_Req = 0,
    enCmdType_Res,
};

typedef struct _ST_JsonCmd
{
    char szCmd[CMD_LEN];
    char cType;
    unsigned char szJson[1];
} ST_JsonCmd;

typedef struct _ST_ProReq
{
    char szCmd[100];
    unsigned char data[1];
} ST_ProReq;

typedef struct _ST_ProResponse
{
    char szCmd[100];
    int nErrCode;
} ST_ProResponse;

typedef struct _ST_ProResJson
{
    char szCmd[100];
    unsigned char json[1];
} ST_ProResJson;

typedef struct _ST_ScriptFile
{
    char szFileName[100];
    unsigned char filedata[1];
} ST_ScriptFile;

#pragma pack(pop)

#endif
