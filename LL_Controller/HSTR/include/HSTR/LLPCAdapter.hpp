#pragma once
// 使用前先包含 llpc.json 中 sdk 对应的函数声明；LLC 使用 LLC_Rbt_Bridge.h。
// SDK 调用在执行 ST 的线程中同步完成，必须有界、非阻塞，且不得重入 Runtime。
#include <HSTR/HSTR.hpp>

namespace HSTR
{
/// 根据接口清单构造全部 SDK 回调；在停止状态交给 SetHardwareCallbacks 注册。
inline HardwareCallbacks MakeLLPCHardwareCallbacks()
{
    HardwareCallbacks h;
#include <HSTR/generated/llpc_adapter.inc>
    return h;
}
} // namespace HSTR
