cmake_minimum_required(VERSION 3.0)

project(uarch-toolkit VERSION 2.0)


if(NOT ${CMAKE_BUILD_TYPE} STREQUAL "Debug")
    message(FATAL_ERROR "Unsupportted build type ${CMAKE_BUILD_TYPE}, "
                        "only \"Debug\" is allowed")
endif()

if(NOT ${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    message(FATAL_ERROR "Unsupported platform ${CMAKE_SYSTEM_NAME}, "
                        "only \"Linux\" is supported")
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS true) # always export compile db
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_EXTERNAL_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/extern")

set(CMAKE_C_FLAGS "-std=gnu99 -g -Wall -O1 -Wno-unknown-pragmas -Wno-unused-function")
add_definitions(-D_GNU_SOURCE) # make our life easier with Linux

include_directories(../LLCFeasible)
include_directories(../LLCFeasible/include)

add_subdirectory(../LLCFeasible/libs libs)
add_subdirectory(../LLCFeasible/src src)
add_subdirectory(../LLCFeasible/tests tests)

# add_executable(build_ev build_ev.c)
file(GLOB SOURCES "build_ev.c")
add_library("BUILD_EV" ${SOURCES})
target_link_libraries("BUILD_EV" PUBLIC "CACHE" m pthread PMU)

add_executable(evict_reload evict_reload.c)
target_link_libraries(evict_reload PUBLIC "CACHE" m pthread PMU "BUILD_EV")

add_executable(force_exclusive force_exclusive.c)
target_link_libraries(force_exclusive PUBLIC "CACHE" m pthread PMU "BUILD_EV")