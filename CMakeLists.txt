cmake_minimum_required(VERSION 3.10)
project(UringFiber LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(LIB_SOURCES
    context/context.cpp
    context/stack.cpp
    hook/fd_desc.cpp
    hook/hook.cpp
    io_uring/io_uring_engine.cpp
    processer/processer.cpp
    processer/task.cpp
    scheduler/scheduler.cpp
)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/context
    ${CMAKE_CURRENT_SOURCE_DIR}/hook
    ${CMAKE_CURRENT_SOURCE_DIR}/io_uring
    ${CMAKE_CURRENT_SOURCE_DIR}/processer
    ${CMAKE_CURRENT_SOURCE_DIR}/scheduler
    ${CMAKE_CURRENT_SOURCE_DIR}/sync
    ${CMAKE_CURRENT_SOURCE_DIR}/thread_safe
)

add_library(UringFiber STATIC ${LIB_SOURCES})
