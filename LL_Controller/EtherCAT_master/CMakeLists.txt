cmake_minimum_required(VERSION 3.16) # 指定最低 CMake 版本
project(EtherCAT_Master LANGUAGES C CXX) # 同时启用 C 和 C++ 编译器
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "This project supports Linux only")
endif()
set(SOEM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/SOEM") # SOEM 源码目录
find_package(Threads REQUIRED) # 查找线程库
# Debug enable
option(EC_DEBUG "Enable debug output")

# Configurable sizes
set(EC_BUFSIZE EC_MAXECATFRAME CACHE STRING "standard frame buffer size in bytes")
set(EC_MAXBUF 16 CACHE STRING "number of frame buffers per channel (tx, rx1 rx2)")
set(EC_MAXEEPBITMAP 128 CACHE STRING "size of EEPROM bitmap cache")
set(EC_MAXEEPBUF "EC_MAXEEPBITMAP << 5" CACHE STRING "size of EEPROM cache buffer")
set(EC_LOGGROUPOFFSET 16 CACHE STRING "default group size in 2^x")
set(EC_MAXELIST 64 CACHE STRING "max. entries in EtherCAT error list")
set(EC_MAXNAME 40 CACHE STRING "max. length of readable name in slavelist and Object Description List")
set(EC_MAXSLAVE 200 CACHE STRING "max. number of slaves in array")
set(EC_MAXGROUP 2 CACHE STRING "max. number of groups")
set(EC_MAXIOSEGMENTS 64 CACHE STRING "max. number of IO segments per group")
set(EC_MAXMBX 1486 CACHE STRING "max. mailbox size")
set(EC_MBXPOOLSIZE 32 CACHE STRING "number of mailboxes in pool")
set(EC_MAXEEPDO 0x200 CACHE STRING "max. eeprom PDO entries")
set(EC_MAXSM 8 CACHE STRING "max. SM used")
set(EC_MAXFMMU 4 CACHE STRING "max. FMMU used")
set(EC_MAXLEN_ADAPTERNAME 128 CACHE STRING "max. adapter name length")
set(EC_MAX_MAPT 1 CACHE STRING " define maximum number of concurrent threads in mapping")
set(EC_MAXODLIST 1024 CACHE STRING "max entries in Object Description list")
set(EC_MAXOELIST 256 CACHE STRING "max entries in Object Entry list")
set(EC_SOE_MAXNAME 60 CACHE STRING "max. length of readable SoE name")
set(EC_SOE_MAXMAPPING 64 CACHE STRING "max. number of SoE mappings")

# Configurable timeouts and retries
set(EC_TIMEOUTRET 2000 CACHE STRING "timeout value in us for tx frame to return to rx")
set(EC_TIMEOUTRET3 "EC_TIMEOUTRET * 3" CACHE STRING "timeout value in us for safe data transfer, max. triple retry")
set(EC_TIMEOUTSAFE 20000 CACHE STRING "timeout value in us for return \"safe\" variant (f.e. wireless)")
set(EC_TIMEOUTEEP 20000 CACHE STRING "timeout value in us for EEPROM access")
set(EC_TIMEOUTTXM 20000 CACHE STRING "timeout value in us for tx mailbox cycle")
set(EC_TIMEOUTRXM 700000 CACHE STRING "timeout value in us for rx mailbox cycle")
set(EC_TIMEOUTSTATE 2000000 CACHE STRING "timeout value in us for check statechange")
set(EC_DEFAULTRETRIES 3 CACHE STRING "default number of retries if wkc <= 0")

# MAC addresses
set(EC_PRIMARY_MAC "01:01:01:01:01:01" CACHE STRING "Primary MAC address")
set(EC_SECONDARY_MAC "04:04:04:04:04:04" CACHE STRING "Secondary MAC address")

# Convert mac address to word arrays for use in options file
macro(convert_mac address array)
  set(RE_BYTE "([0-9A-Fa-f][0-9A-Fa-f])")
  string(REGEX REPLACE
    "^${RE_BYTE}:${RE_BYTE}:${RE_BYTE}:${RE_BYTE}:${RE_BYTE}:${RE_BYTE}$"
    "{0x\\1\\2, 0x\\3\\4, 0x\\5\\6}"
    ${array}
    ${address})
endmacro()

convert_mac(${EC_PRIMARY_MAC} EC_PRIMARY_MAC_ARRAY)
convert_mac(${EC_SECONDARY_MAC} EC_SECONDARY_MAC_ARRAY)

configure_file(
  ${SOEM_DIR}/include/soem/ec_options.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/generated/include/soem/ec_options.h
)


file(GLOB SOEM_SOURCES CONFIGURE_DEPENDS "${SOEM_DIR}/src/*.c") # 收集 SOEM 核心源码
add_library(EtherCAT_Master SHARED EtherCAT_Master.cpp ${SOEM_SOURCES} "${SOEM_DIR}/osal/linux/osal.c" "${SOEM_DIR}/oshw/linux/oshw.c" "${SOEM_DIR}/oshw/linux/nicdrv.c") # 所有源码直接编入同一个动态库
target_compile_features(EtherCAT_Master PUBLIC cxx_std_17) # 类实现使用 C++17
target_include_directories(EtherCAT_Master PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}") # 对外提供类头文件
target_include_directories(EtherCAT_Master PRIVATE "${SOEM_DIR}/include" "${CMAKE_CURRENT_BINARY_DIR}/generated/include" "${SOEM_DIR}/osal" "${SOEM_DIR}/osal/linux" "${SOEM_DIR}/oshw/linux") # SOEM 头文件仅供内部使用
target_compile_definitions(EtherCAT_Master PRIVATE $<$<BOOL:${EC_DEBUG}>:EC_DEBUG>)
include(TestBigEndian)
test_big_endian(IS_BIG_ENDIAN)
if(IS_BIG_ENDIAN)
    target_compile_definitions(EtherCAT_Master PRIVATE EC_BIG_ENDIAN)
endif()
target_link_libraries(EtherCAT_Master PRIVATE Threads::Threads rt) # 链接 Linux 系统库
target_link_options(EtherCAT_Master PRIVATE "-Wl,--no-undefined") # 链接时检查符号是否完整

# 无需网卡或 root 权限：测试程序用模拟后端替换 SOEM 的网络函数。
if(BUILD_TESTING)
    add_executable(master_tests EtherCAT_Master.cpp tests/master_tests.cpp)
    target_compile_features(master_tests PRIVATE cxx_std_17)
    target_include_directories(master_tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${SOEM_DIR}/include"
        "${CMAKE_CURRENT_BINARY_DIR}/generated/include"
        "${SOEM_DIR}/osal" "${SOEM_DIR}/osal/linux" "${SOEM_DIR}/oshw/linux"
    )
    target_link_libraries(master_tests PRIVATE Threads::Threads)
    if(IS_BIG_ENDIAN)
        target_compile_definitions(master_tests PRIVATE EC_BIG_ENDIAN)
    endif()
    add_test(NAME master_isolation_and_foe COMMAND master_tests)
    set_tests_properties(master_isolation_and_foe PROPERTIES TIMEOUT 30)
endif()