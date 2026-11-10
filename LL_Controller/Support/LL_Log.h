#pragma once
#include <cstdio>
#define LL_LOG_Error(...) do { std::fprintf(stderr, __VA_ARGS__); std::fputc('\n', stderr); } while (false)
#define LL_LOG_Info(...) do { std::fprintf(stdout, __VA_ARGS__); std::fputc('\n', stdout); } while (false)
#define LL_LOG_Warning(...) LL_LOG_Error(__VA_ARGS__)
