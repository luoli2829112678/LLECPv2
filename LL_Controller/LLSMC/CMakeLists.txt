cmake_minimum_required(VERSION 3.10)
project(llsmc LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

add_subdirectory(CIA402Axis)
add_subdirectory(AxisGroup)
add_subdirectory(SMCAlgorithm)
add_subdirectory(SMbasic)
add_subdirectory(SoftMotion)