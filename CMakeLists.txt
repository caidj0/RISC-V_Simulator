cmake_minimum_required(VERSION 3.28.3)

project(RISC-V_Simulator VERSION 1.0)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(code simulator.cpp CPU.cpp)