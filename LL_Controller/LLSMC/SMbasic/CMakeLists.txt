# 设置单轴和轴组模块的根目录
set(SM_MODULE_DIRS
    "${CMAKE_CURRENT_SOURCE_DIR}/Axis"
    "${CMAKE_CURRENT_SOURCE_DIR}/AxisGroup"
)

# 初始化模块源码列表
set(SM_MODULE_SOURCES "")

# 初始化模块头文件列表
set(SM_MODULE_HEADERS "")

# 遍历单轴和轴组模块
foreach(SM_MODULE_DIR IN LISTS SM_MODULE_DIRS)
    # 递归搜索模块中的.cpp文件，新增或删除文件后自动触发重新配置
    file(GLOB_RECURSE SM_CURRENT_SOURCES CONFIGURE_DEPENDS
        "${SM_MODULE_DIR}/*.cpp"
    )

    # 递归搜索模块中的头文件，使用相对于SMbasic目录的路径
    file(GLOB_RECURSE SM_CURRENT_HEADERS CONFIGURE_DEPENDS
        RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
        "${SM_MODULE_DIR}/*.h"
        "${SM_MODULE_DIR}/*.hpp"
        "${SM_MODULE_DIR}/*.hxx"
    )

    # 追加当前模块的源码文件
    list(APPEND SM_MODULE_SOURCES ${SM_CURRENT_SOURCES})

    # 追加当前模块的头文件
    list(APPEND SM_MODULE_HEADERS ${SM_CURRENT_HEADERS})
endforeach()

# 去除重复源码文件并排序
list(REMOVE_DUPLICATES SM_MODULE_SOURCES)
list(SORT SM_MODULE_SOURCES)

# 去除重复头文件并排序
list(REMOVE_DUPLICATES SM_MODULE_HEADERS)
list(SORT SM_MODULE_HEADERS)

# 设置汇总头文件路径，与当前CMakeLists.txt同级
set(SM_GENERATED_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/SMbasic_stdafx.h")

# 初始化汇总头文件内容
set(SM_HEADER_CONTENT "#pragma once\n\n/* 该文件自动生成，不需要手动修改 */\n\n")

# 为每个模块头文件追加包含指令，保留完整相对路径
foreach(SM_HEADER IN LISTS SM_MODULE_HEADERS)
    string(APPEND SM_HEADER_CONTENT "#include \"${SM_HEADER}\"\n")
endforeach()

# 生成汇总头文件，内容不变时不更新时间戳
file(GENERATE
    OUTPUT "${SM_GENERATED_HEADER}"
    CONTENT "${SM_HEADER_CONTENT}"
)

# 编译SMbasic自身及全部单轴、轴组模块源码
add_library(SMbasic SHARED
    "${CMAKE_CURRENT_SOURCE_DIR}/SMbasic.cpp"
    ${SM_MODULE_SOURCES}
)

# 公开头文件搜索目录
target_include_directories(SMbasic PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

# 设置C++17标准并禁止编译器专有语言扩展
set_target_properties(SMbasic PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# 链接单轴控制库，并向使用SMbasic的目标传递该依赖
target_link_libraries(SMbasic PUBLIC
    CIA402Axis
)

# 配置时显示自动生成文件的位置
message(STATUS "SMbasic汇总头文件：${SM_GENERATED_HEADER}")