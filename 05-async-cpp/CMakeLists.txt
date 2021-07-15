cmake_minimum_required (VERSION 3.2.0)
project (asynctest)

set(CMAKE_C_COMPILER "gcc-10")
set(CMAKE_CXX_COMPILER "/usr/bin/g++-10")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcoroutines")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

find_package(Threads REQUIRED)

add_executable(asynctest main.cpp threadpool.cpp sleeper.cpp)

target_link_libraries(asynctest PRIVATE Threads::Threads)
