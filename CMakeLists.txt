# clang -std=c++17 -lstdc++ -pthread -g dispatcher.cpp

cmake_minimum_required(VERSION 3.13)
project(GC VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED true)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_library(EventMachine src/Scheduler.cpp)
include_directories(src)

add_executable(EventMachineTest src/Scheduler.cpp)

target_link_libraries(EventMachineTest PRIVATE Threads::Threads)
