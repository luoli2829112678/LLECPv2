cmake_minimum_required(VERSION 3.16) # 指定最低 CMake 版本
project(LLECP LANGUAGES C CXX) # 创建工程，同时启用 C 和 C++ 编译器

set(CMAKE_CXX_STANDARD 17) # 使用 C++17
set(CMAKE_CXX_STANDARD_REQUIRED ON) # 编译器必须支持指定标准
set(CMAKE_CXX_EXTENSIONS OFF) # 禁用编译器特有的 C++ 扩展
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # 生成代码补全所需的编译信息

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE) # 默认启用优化并保留调试信息
endif()

add_subdirectory(EtherCAT_master) # 添加主站库工程，SOEM 源码由该工程直接编入主站库

add_executable(LLECP main.cpp) # 将 main.cpp 编译为 LLECP 可执行程序
target_link_libraries(LLECP PRIVATE EtherCAT_Master) # 链接主站库，同时继承它公开的头文件目录

set_target_properties(LLECP PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin") # 指定可执行程序输出目录
set_target_properties(LLECP PROPERTIES RUNTIME_OUTPUT_DIRECTORY "/usr/local/bin/HRPC") # 将可执行程序生成到 Ubuntu 本地 HRPC 目录