cmake_minimum_required(VERSION 3.15)
PROJECT(ForkJoinPool VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


if (WIN32)
    find_package(fmt REQUIRED PATHS ${PROJECT_SOURCE_DIR}/Module/Windows NO_DEFAULT_PATH)
    find_package(spdlog REQUIRED PATHS ${PROJECT_SOURCE_DIR}/Module/Windows NO_DEFAULT_PATH)

elseif (UNIX)
    find_package(fmt REQUIRED PATHS ${PROJECT_SOURCE_DIR}/Module/Linux NO_DEFAULT_PATH)
    find_package(spdlog REQUIRED PATHS ${PROJECT_SOURCE_DIR}/Module/Linux NO_DEFAULT_PATH)
endif ()

find_package(Threads REQUIRED)

if(WIN32)
    SET(CMAKE_C_COMPILER "D:/CppQt/Tools/mingw1120_64/bin/gcc.exe")
    SET(CMAKE_CXX_COMPILER "D:/CppQt/Tools/mingw1120_64/bin/g++.exe")
endif ()
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

target_link_libraries(${PROJECT_NAME} PUBLIC spdlog::spdlog fmt::fmt)
target_compile_definitions(${PROJECT_NAME} PUBLIC SPDLOG_FMT_EXTERNAL)

target_link_libraries(${PROJECT_NAME} PUBLIC Threads::Threads)