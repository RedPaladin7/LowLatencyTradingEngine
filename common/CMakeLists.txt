cmake_minimum_required(VERSION 3.10)
project(LowLatencyApp)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -Wall -Wextra -Wpedantic")

set(LIB_SOURCES 
    tcp_socket.cpp 
    tcp_server.cpp 
    mcast_socket.cpp
)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_library(libcommon STATIC ${LIB_SOURCES})

add_executable(socket_example socket_example.cpp)
target_link_libraries(socket_example libcommon pthread)