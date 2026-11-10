cmake_minimum_required(VERSION 3.10)
project(llsmc LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

add_subdirectory(CIA402Axis)
add_subdirectory(AxisGroup)
add_subdirectory(SMCAlgorithm)
add_subdirectory(SMbasic)
add_subdirectory(SoftMotion)

# Each shared library must declare its own implementation dependencies.
# Fail here rather than leaving unresolved symbols for downstream executables.
if(UNIX AND NOT APPLE)
    foreach(motion_target CIA402Axis AxisGroup SMCAlgorithm SMbasic SoftMotion)
        target_link_options(${motion_target} PRIVATE "-Wl,--no-undefined")
    endforeach()
endif()
