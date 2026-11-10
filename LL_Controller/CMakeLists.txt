add_subdirectory(EtherCAT_master)
add_subdirectory(LLSMC)
add_subdirectory(HSTR)
# Nested library projects may reinitialize Linux linker flags.
if(CMAKE_CXX_COMPILER MATCHES "zig")
    set(CMAKE_SHARED_LIBRARY_RPATH_LINK_C_FLAG "-L")
    set(CMAKE_SHARED_LIBRARY_RPATH_LINK_CXX_FLAG "-L")
    set(CMAKE_EXECUTABLE_RPATH_LINK_C_FLAG "-L")
    set(CMAKE_EXECUTABLE_RPATH_LINK_CXX_FLAG "-L")
endif()
file(GLOB_RECURSE CONTROLLER_CORE_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Core/*.cpp")
file(GLOB CONTROLLER_SUPPORT_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Support/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/Bus/*.cpp")
add_library(LL_Controller_Core SHARED ${CONTROLLER_CORE_SOURCES} ${CONTROLLER_SUPPORT_SOURCES} slaves/LLC_SlaveBase.cpp)
target_include_directories(LL_Controller_Core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/Core"
    "${CMAKE_CURRENT_SOURCE_DIR}/Core/status" "${CMAKE_CURRENT_SOURCE_DIR}/Core/trans"
    "${CMAKE_CURRENT_SOURCE_DIR}/slaves" "${CMAKE_CURRENT_SOURCE_DIR}/Support" "${CMAKE_CURRENT_SOURCE_DIR}/Bus"
    PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/EtherCAT_master/SOEM/include"
    "${CMAKE_CURRENT_BINARY_DIR}/EtherCAT_master/generated/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/EtherCAT_master/SOEM/osal"
    "${CMAKE_CURRENT_SOURCE_DIR}/EtherCAT_master/SOEM/osal/linux"
    "${CMAKE_CURRENT_SOURCE_DIR}/EtherCAT_master/SOEM/oshw/linux"
)
target_link_libraries(LL_Controller_Core PUBLIC LL_Public HSTR::HSTR
    PRIVATE EtherCAT_Master CIA402Axis AxisGroup SMbasic SoftMotion SMCAlgorithm Threads::Threads ${CMAKE_DL_LIBS} rt)
add_dependencies(LL_Controller_Core hstr_interfaces)
target_link_options(LL_Controller_Core PRIVATE "-Wl,--no-undefined")
add_executable(LL_Controller main.cpp)
target_link_libraries(LL_Controller PRIVATE LL_Controller_Core Threads::Threads)

if(BUILD_TESTING)
    add_executable(axis_enable_tests tests/axis_enable_tests.cpp)
    target_include_directories(axis_enable_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/LLSMC/SMbasic")
    target_link_libraries(axis_enable_tests PRIVATE SMbasic CIA402Axis)
    add_test(NAME axis_enable_pdo_sequence COMMAND axis_enable_tests)
    add_executable(cps_network_tests tests/cps_network_tests.cpp)
    target_link_libraries(cps_network_tests PRIVATE LL_Controller_Core Threads::Threads)
    add_test(NAME cps_controller_integration COMMAND cps_network_tests)
    set_tests_properties(cps_controller_integration PROPERTIES TIMEOUT 90 RUN_SERIAL TRUE)
    add_executable(protocol_registry_tests tests/protocol_registry_tests.cpp Bus/LL_MasterRegistry.cpp)
    target_include_directories(protocol_registry_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Bus")
    target_link_libraries(protocol_registry_tests PRIVATE LL_Protocol Threads::Threads)
    add_test(NAME protocol_and_registry COMMAND protocol_registry_tests)
    add_executable(controller_network_tests tests/controller_network_tests.cpp)
    target_link_libraries(controller_network_tests PRIVATE LL_Controller_Core Threads::Threads)
    add_test(NAME controller_tcp_and_hstr COMMAND controller_network_tests)
    set_tests_properties(controller_tcp_and_hstr PROPERTIES TIMEOUT 30 RUN_SERIAL TRUE)
endif()
