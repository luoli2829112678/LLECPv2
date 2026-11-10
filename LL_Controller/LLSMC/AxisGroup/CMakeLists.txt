# 创建轴组动态库
add_library(AxisGroup SHARED
    AxisGroup.cpp
)
target_link_libraries(AxisGroup PUBLIC CIA402Axis SMCAlgorithm)

# 公开轴组头文件目录
target_include_directories(AxisGroup PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

# 设置C++17标准
set_target_properties(AxisGroup PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# 设置轴组功能块扫描目录
set(AXISGROUP_FRIEND_SCAN_DIR "${CMAKE_CURRENT_LIST_DIR}/../SMbasic/AxisGroup")

# 递归搜索所有子目录中的功能块头文件
file(GLOB_RECURSE AXISGROUP_FRIEND_HEADERS CONFIGURE_DEPENDS
    "${AXISGROUP_FRIEND_SCAN_DIR}/*.h"
    "${AXISGROUP_FRIEND_SCAN_DIR}/*.hpp"
    "${AXISGROUP_FRIEND_SCAN_DIR}/*.hxx"
)

# 头文件内容变化时重新运行CMake，更新类名列表
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    ${AXISGROUP_FRIEND_HEADERS}
)

# 设置生成文件路径，保存到AxisGroup源码目录
set(AXISGROUP_FRIEND_FILE "${CMAKE_CURRENT_LIST_DIR}/AxisGroupFriendLists.h")

# 初始化友元类名列表
set(AXISGROUP_FRIEND_CLASSES "")

# 遍历功能块头文件
foreach(AXISGROUP_HEADER IN LISTS AXISGROUP_FRIEND_HEADERS)
    # 读取头文件内容
    file(READ "${AXISGROUP_HEADER}" AXISGROUP_HEADER_CONTENT)

    # 匹配普通类定义，支持继承类和final类
    string(REGEX MATCHALL
        "class[ \t\r\n]+([A-Za-z_][A-Za-z0-9_]*)[ \t\r\n]*(\\{|:|final[ \t\r\n])"
        AXISGROUP_CLASS_MATCHES
        "${AXISGROUP_HEADER_CONTENT}"
    )

    # 提取类名
    foreach(AXISGROUP_MATCH IN LISTS AXISGROUP_CLASS_MATCHES)
        string(REGEX REPLACE
            "class[ \t\r\n]+([A-Za-z_][A-Za-z0-9_]*).*"
            "\\1"
            AXISGROUP_CLASS_NAME
            "${AXISGROUP_MATCH}"
        )

        # 将类名添加到列表
        list(APPEND AXISGROUP_FRIEND_CLASSES "${AXISGROUP_CLASS_NAME}")
    endforeach()
endforeach()

# 去除重复类名
list(REMOVE_DUPLICATES AXISGROUP_FRIEND_CLASSES)

# 排序，保证生成内容稳定
list(SORT AXISGROUP_FRIEND_CLASSES)

# 初始化生成文件内容
set(AXISGROUP_FRIEND_CONTENT "/* 该文件自动生成，不需要手动修改 */\n#pragma once\n\n")

# 生成全局类前置声明，避免包含功能块头文件造成循环依赖
foreach(AXISGROUP_CLASS_NAME IN LISTS AXISGROUP_FRIEND_CLASSES)
    string(APPEND AXISGROUP_FRIEND_CONTENT "class ${AXISGROUP_CLASS_NAME};\n")
endforeach()

# 定义轴组友元宏
string(APPEND AXISGROUP_FRIEND_CONTENT "\n#define AXISGROUP_FRIEND_CLASSES")

# 追加友元声明，最后一个声明不带续行符
foreach(AXISGROUP_CLASS_NAME IN LISTS AXISGROUP_FRIEND_CLASSES)
    string(APPEND AXISGROUP_FRIEND_CONTENT " \\\n    friend class ::${AXISGROUP_CLASS_NAME};")
endforeach()

# 添加文件结尾换行
string(APPEND AXISGROUP_FRIEND_CONTENT "\n")

# 默认需要写入生成文件
set(AXISGROUP_FRIEND_NEED_WRITE TRUE)

# 对比已有内容，避免重复写入导致不必要的重新编译
if(EXISTS "${AXISGROUP_FRIEND_FILE}")
    file(READ "${AXISGROUP_FRIEND_FILE}" AXISGROUP_FRIEND_OLD_CONTENT)

    if("${AXISGROUP_FRIEND_OLD_CONTENT}" STREQUAL "${AXISGROUP_FRIEND_CONTENT}")
        set(AXISGROUP_FRIEND_NEED_WRITE FALSE)
    endif()
endif()

# 内容变化时写入文件
if(AXISGROUP_FRIEND_NEED_WRITE)
    file(WRITE "${AXISGROUP_FRIEND_FILE}" "${AXISGROUP_FRIEND_CONTENT}")
endif()

# 显示识别到的友元类数量
list(LENGTH AXISGROUP_FRIEND_CLASSES AXISGROUP_FRIEND_COUNT)
message(STATUS "[AxisGroup] ${AXISGROUP_FRIEND_FILE}，共${AXISGROUP_FRIEND_COUNT}个友元类")
