// 自动生成自 interfaces/llpc.json，请修改清单后重新构建。
h.pdoRead = [](void*, int32_t hstr_arg_0, int32_t hstr_arg_1, int32_t hstr_arg_2) -> int32_t
{
    return SLAVE_PDORead(hstr_arg_0, hstr_arg_1, hstr_arg_2);
};
h.pdoWrite = [](void*, int32_t hstr_arg_0, int32_t hstr_arg_1, int32_t hstr_arg_2, int32_t hstr_arg_3) -> void
{
    SLAVE_PDOWrite(hstr_arg_0, hstr_arg_1, hstr_arg_2, hstr_arg_3);
};
h.axisEnable = [](void*, int32_t hstr_arg_0) -> void
{
    AXIS_Enable(hstr_arg_0);
};
h.axisGetActPos = [](void*, int32_t hstr_arg_0) -> double
{
    return AXIS_GetActPos(hstr_arg_0);
};
