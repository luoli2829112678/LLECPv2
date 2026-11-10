// 自动生成自 interfaces/llpc.json，请修改清单后重新构建。
host.pdoRead = [](void* hstr_context, int32_t hstr_arg_0, int32_t hstr_arg_1, int32_t hstr_arg_2) -> int32_t
{
    auto& h = static_cast<Impl*>(hstr_context)->hardware;
    if (!h.pdoRead)
    {
        throw Error(ErrorCode::HardwareUnavailable, "SLAVE_PDO_READ: callback pdoRead missing");
    }
    return h.pdoRead(h.user, hstr_arg_0, hstr_arg_1, hstr_arg_2);
};
host.pdoWrite = [](void* hstr_context, int32_t hstr_arg_0, int32_t hstr_arg_1, int32_t hstr_arg_2, int32_t hstr_arg_3) -> void
{
    auto& h = static_cast<Impl*>(hstr_context)->hardware;
    if (!h.pdoWrite)
    {
        throw Error(ErrorCode::HardwareUnavailable, "SLAVE_PDO_WRITE: callback pdoWrite missing");
    }
    return h.pdoWrite(h.user, hstr_arg_0, hstr_arg_1, hstr_arg_2, hstr_arg_3);
};
host.axisEnable = [](void* hstr_context, int32_t hstr_arg_0) -> void
{
    auto& h = static_cast<Impl*>(hstr_context)->hardware;
    if (!h.axisEnable)
    {
        throw Error(ErrorCode::HardwareUnavailable, "AXIS_ENABLE: callback axisEnable missing");
    }
    return h.axisEnable(h.user, hstr_arg_0);
};
host.axisGetActPos = [](void* hstr_context, int32_t hstr_arg_0) -> double
{
    auto& h = static_cast<Impl*>(hstr_context)->hardware;
    if (!h.axisGetActPos)
    {
        throw Error(ErrorCode::HardwareUnavailable, "AXIS_GETACTPOS: callback axisGetActPos missing");
    }
    return h.axisGetActPos(h.user, hstr_arg_0);
};
