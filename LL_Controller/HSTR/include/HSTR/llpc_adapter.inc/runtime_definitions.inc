// 自动生成自 interfaces/llpc.json，请修改清单后重新构建。
int32_t Runtime::SLAVE_PDO_READ(int32_t hstr_arg_0, int32_t hstr_arg_1, int32_t hstr_arg_2)
{
    std::lock_guard<std::mutex> lock(p_->mutex);
    return p_->host.pdoRead(p_.get(), hstr_arg_0, hstr_arg_1, hstr_arg_2);
}
void Runtime::SLAVE_PDO_WRITE(int32_t hstr_arg_0, int32_t hstr_arg_1, int32_t hstr_arg_2, int32_t hstr_arg_3)
{
    std::lock_guard<std::mutex> lock(p_->mutex);
    return p_->host.pdoWrite(p_.get(), hstr_arg_0, hstr_arg_1, hstr_arg_2, hstr_arg_3);
}
void Runtime::AXIS_ENABLE(int32_t hstr_arg_0)
{
    std::lock_guard<std::mutex> lock(p_->mutex);
    return p_->host.axisEnable(p_.get(), hstr_arg_0);
}
double Runtime::AXIS_GETACTPOS(int32_t hstr_arg_0)
{
    std::lock_guard<std::mutex> lock(p_->mutex);
    return p_->host.axisGetActPos(p_.get(), hstr_arg_0);
}
double Runtime::AXIS_SETAXISACTPOS(int32_t hstr_arg_0)
{
    std::lock_guard<std::mutex> lock(p_->mutex);
    return p_->host.axisGetActPos(p_.get(), hstr_arg_0);
}
