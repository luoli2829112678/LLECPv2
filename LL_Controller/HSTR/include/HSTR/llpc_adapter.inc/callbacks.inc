// 自动生成自 interfaces/llpc.json，请修改清单后重新构建。
/// 读取 PDO，返回 32 位值。
int32_t (*pdoRead)(void*, int32_t, int32_t, int32_t) = nullptr;
/// 写入 32 位 PDO 值。
void (*pdoWrite)(void*, int32_t, int32_t, int32_t, int32_t) = nullptr;
/// 使能指定轴。
void (*axisEnable)(void*, int32_t) = nullptr;
/// 读取轴实际位置，单位由 SDK 定义。
double (*axisGetActPos)(void*, int32_t) = nullptr;
