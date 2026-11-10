# 搜索当前目录及子目录中的全部.cpp文件
file(GLOB_RECURSE SMC_ALGORITHM_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

# 搜索全部头文件，使用相对于当前源码目录的路径
file(GLOB_RECURSE SMC_ALGORITHM_HEADERS CONFIGURE_DEPENDS
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hxx"
)

# 排除自动生成的汇总头文件，避免包含自身
list(FILTER SMC_ALGORITHM_HEADERS EXCLUDE REGEX "(^|/)AxisGroupAlgorithStdafx\\.h$")

# 对源码文件排序
list(SORT SMC_ALGORITHM_SOURCES)

# 对头文件排序
list(SORT SMC_ALGORITHM_HEADERS)

# 设置生成目录，与当前CMakeLists.txt同级
set(SMC_ALGORITHM_GENERATED_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

# 初始化汇总头文件内容，cstdio提供std::printf
set(SMC_ALGORITHM_STDAFX_CONTENT "// 此文件由CMake自动生成，请勿手动修改。\n#pragma once\n#include <cstdio>\n\n")

# 追加全部头文件的包含指令
foreach(SMC_ALGORITHM_HEADER IN LISTS SMC_ALGORITHM_HEADERS)
    string(APPEND SMC_ALGORITHM_STDAFX_CONTENT "#include \"${SMC_ALGORITHM_HEADER}\"\n")
endforeach()

# 追加算法工厂函数，inline避免头文件被多处包含时发生重复定义
string(APPEND SMC_ALGORITHM_STDAFX_CONTENT [=[

// 根据轴组模型创建算法对象，调用方负责释放返回的对象
inline AxisGroupAlgorithmBase* GenerativeModelAlgorithm(ENAxisGroupModel enGroupModel, ST_AxisGroupDH stDH)
{
    // 初始化算法对象指针
    AxisGroupAlgorithmBase* pAxisGroupAlgorithm = nullptr;

    // 暂未接入DH配置,显式标记参数已处理,避免未使用参数警告
    (void)stDH;

    // 根据轴组模型选择算法
    switch (enGroupModel)
    {
    case AxisGroupModel_null:
        // 未配置有效模型
        std::printf("无效的轴组模型\n");
        break;

    case AxisGroupModel_xy_Cartesian:
        pAxisGroupAlgorithm = new AxisGroupAlgorithm_XY_Cartesian();
        break;

    default:
        // 当前尚未支持的模型
        std::printf("不支持的轴组模型\n");
        break;
    }

    // 返回创建的算法对象,未创建成功时返回nullptr
    return pAxisGroupAlgorithm;
}
]=])

# 生成汇总头文件，内容不变时不更新时间戳
file(GENERATE
    OUTPUT "${SMC_ALGORITHM_GENERATED_DIR}/AxisGroupAlgorithStdafx.h"
    CONTENT "${SMC_ALGORITHM_STDAFX_CONTENT}"
)

# 将自动搜索到的全部.cpp文件编译为动态库
add_library(SMCAlgorithm SHARED
    ${SMC_ALGORITHM_SOURCES}
)

# 公开头文件搜索目录，生成目录与源码目录相同，只需添加一次
target_include_directories(SMCAlgorithm PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

# 设置C++17标准，并禁用编译器专有语言扩展
set_target_properties(SMCAlgorithm PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# 配置时输出生成文件位置，方便确认
message(STATUS "轴组汇总头文件：${SMC_ALGORITHM_GENERATED_DIR}/AxisGroupAlgorithStdafx.h")