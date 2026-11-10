cmake_minimum_required(VERSION 3.20)

project(HSTR VERSION 0.2.1 LANGUAGES C CXX)

# 当前运行库使用 Linux/POSIX 接口。
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "HSTR supports Linux/POSIX only")
endif()

find_package(Threads REQUIRED)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

# ==================== 自动生成接口 ====================

set(HSTR_GENERATED_INCLUDE "${CMAKE_CURRENT_BINARY_DIR}/generated/include")
set(HSTR_GENERATED "${HSTR_GENERATED_INCLUDE}/HSTR/generated")
set(HSTR_INTERFACE_STAMP "${CMAKE_CURRENT_BINARY_DIR}/interfaces.stamp")

set(HSTR_INTERFACE_OUTPUTS)

foreach(name
    callbacks.inc
    host_fields.inc
    host_bindings.inc
    runtime_declarations.inc
    runtime_definitions.inc
    llpc_adapter.inc
    interface_signature.h
    hardware_st.hpp
    hardware.st
    INTERFACES.md
)
    list(APPEND HSTR_INTERFACE_OUTPUTS "${HSTR_GENERATED}/${name}")
endforeach()

# 修改接口清单或生成脚本后，自动重新生成接口。
add_custom_command(
    OUTPUT
        "${HSTR_INTERFACE_STAMP}"
        ${HSTR_INTERFACE_OUTPUTS}

    COMMAND
        "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_interfaces.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/interfaces/llpc.json"
        "${HSTR_GENERATED}"

    COMMAND
        "${CMAKE_COMMAND}" -E touch "${HSTR_INTERFACE_STAMP}"

    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/interfaces/llpc.json"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_interfaces.py"

    COMMENT "Generating HSTR interfaces"
    VERBATIM
)

add_custom_target(hstr_interfaces
    DEPENDS
        "${HSTR_INTERFACE_STAMP}"
        ${HSTR_INTERFACE_OUTPUTS}
)

# ==================== 内嵌 ST 编译器 ====================

set(M "${CMAKE_CURRENT_SOURCE_DIR}/third_party/matiec")

# 已包含生成文件时，不需要安装 Bison/Flex。
if(NOT EXISTS "${M}/stage1_2/iec_bison.cc"
   OR NOT EXISTS "${M}/stage1_2/iec_bison.hh"
   OR NOT EXISTS "${M}/stage1_2/iec_flex.cc")

    find_package(BISON 3.0 REQUIRED)
    find_package(FLEX REQUIRED)

    execute_process(
        COMMAND
            "${BISON_EXECUTABLE}"
            -d -Wno-yacc
            --defines=iec_bison.hh
            -o iec_bison.cc
            iec_bison.yy

        WORKING_DIRECTORY "${M}/stage1_2"
        COMMAND_ERROR_IS_FATAL ANY
    )

    execute_process(
        COMMAND
            "${FLEX_EXECUTABLE}"
            -o iec_flex.cc
            iec_flex.ll

        WORKING_DIRECTORY "${M}/stage1_2"
        COMMAND_ERROR_IS_FATAL ANY
    )
endif()

file(GLOB MATIEC_SOURCES CONFIGURE_DEPENDS
    "${M}/absyntax/*.cc"
    "${M}/absyntax_utils/*.cc"
    "${M}/stage3/*.cc"
)

list(APPEND MATIEC_SOURCES
    "${M}/stage1_2/iec_bison.cc"
    "${M}/stage1_2/iec_flex.cc"
    "${M}/stage1_2/stage1_2.cc"
    "${M}/stage1_2/create_enumtype_conversion_functions.cc"
    "${M}/stage4/stage4.cc"
    "${M}/stage4/generate_c/generate_c.cc"
    "${M}/main.cc"
)

# 重命名编译器入口，避免与宿主程序的 main 冲突。
set_source_files_properties(
    "${M}/main.cc"
    PROPERTIES COMPILE_DEFINITIONS "main=hstr_matiec_main"
)

# 只生成对象文件，最终合并进 HSTR.so，不生成编译器可执行文件。
add_library(hstr_matiec OBJECT ${MATIEC_SOURCES})

set_target_properties(hstr_matiec PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

target_compile_features(hstr_matiec PRIVATE cxx_std_17)

target_compile_options(hstr_matiec PRIVATE
    -w
    -fpermissive
)

target_compile_definitions(hstr_matiec PRIVATE
    DEFAULT_LIBDIR="lib"
    YY_BUF_SIZE=65536
    HGVERSION="7680ed8e7ffc1a76fa9f9620d6c6e7a3e75c088d+hstr"
)

target_include_directories(hstr_matiec PRIVATE
    "${M}"
    "${M}/stage1_2"
)

# ==================== 内嵌 IEC 资源 ====================

file(GLOB_RECURSE HSTR_IEC_RESOURCES CONFIGURE_DEPENDS
    "${M}/lib/*"
)

add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/embedded.hpp"

    COMMAND
        "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/embed.py"
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_BINARY_DIR}/embedded.hpp"
        "${HSTR_GENERATED}"

    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/embed.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/HSTR/module.h"
        "${HSTR_INTERFACE_STAMP}"
        ${HSTR_INTERFACE_OUTPUTS}
        ${HSTR_IEC_RESOURCES}

    COMMENT "Embedding IEC library and HSTR ABI"
    VERBATIM
)

# ==================== HSTR 动态库 ====================

add_library(HSTR SHARED
    src/hstr.cpp
    src/compiler.cpp
    "${CMAKE_CURRENT_BINARY_DIR}/embedded.hpp"
    $<TARGET_OBJECTS:hstr_matiec>
)

add_library(HSTR::HSTR ALIAS HSTR)

add_dependencies(HSTR hstr_interfaces)

# PUBLIC：链接 HSTR 的宿主目标也会获得 C++17 要求。
target_compile_features(HSTR PUBLIC cxx_std_17)

set_target_properties(HSTR PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    PREFIX ""
    OUTPUT_NAME "HSTR"
)

# PUBLIC 路径自动传递给链接 HSTR 的目标。
target_include_directories(HSTR
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${HSTR_GENERATED_INCLUDE}>
        $<INSTALL_INTERFACE:include>

    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party"
        "${CMAKE_CURRENT_BINARY_DIR}"
)

target_link_libraries(HSTR PRIVATE
    Threads::Threads
    ${CMAKE_DL_LIBS}
)

target_compile_options(HSTR PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

# ==================== 安装运行库和头文件 ====================

install(TARGETS HSTR
    LIBRARY DESTINATION lib
)

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/"
    DESTINATION include
)

install(DIRECTORY "${HSTR_GENERATED}/"
    DESTINATION include/HSTR/generated
    FILES_MATCHING
        PATTERN "*.h"
        PATTERN "*.inc"
)