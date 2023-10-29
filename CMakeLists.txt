cmake_minimum_required(VERSION 3.15)
PROJECT(STUDY VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

SET(CMAKE_C_COMPILER "D:/CppQt/Tools/mingw1120_64/bin/gcc.exe")
SET(CMAKE_CXX_COMPILER "D:/CppQt/Tools/mingw1120_64/bin/g++.exe")
SET(EXECUTABLE_OUTPUT_PATH "${PROJECT_SOURCE_DIR}/bin")
SET(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin")
SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin")
SET(CMAKE_BUILD_TYPE "Debug") # Debug模式 选项: Release Debug MinSizeRel RelWithDebInfo
set(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g -ggdb")
set(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall")

file(GLOB src_files ${PROJECT_SOURCE_DIR}/*.cpp)
file(GLOB inc_files ${PROJECT_SOURCE_DIR}/*.h)
file(GLOB hpp_files ${PROJECT_SOURCE_DIR}/*.hpp)

add_executable(
        ${PROJECT_NAME}
        ${src_files}
        ${inc_files}
        ${hpp_files}
)

