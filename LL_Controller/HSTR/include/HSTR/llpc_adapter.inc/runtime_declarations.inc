// 自动生成自 interfaces/llpc.json，请修改清单后重新构建。
/// 读取 PDO，返回 32 位值。
int32_t SLAVE_PDO_READ(int32_t hstr_arg_0, int32_t hstr_arg_1, int32_t hstr_arg_2);
/// 写入 32 位 PDO 值。
void SLAVE_PDO_WRITE(int32_t hstr_arg_0, int32_t hstr_arg_1, int32_t hstr_arg_2, int32_t hstr_arg_3);
/// 使能指定轴。
void AXIS_ENABLE(int32_t hstr_arg_0);
/// 读取轴实际位置，单位由 SDK 定义。
double AXIS_GETACTPOS(int32_t hstr_arg_0);
/// 历史别名：实际读取位置，不修改轴位置。
double AXIS_SETAXISACTPOS(int32_t hstr_arg_0);
