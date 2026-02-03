cmake_minimum_required(VERSION 3.10)
project(HFT_Exchange_Engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -march=native -pthread -Wall")

include_directories(
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/exchange
)

file(GLOB_RECURSE EXCHANGE_SOURCES 
    "exchange/matcher/*.cpp"
    "exchange/market_data/*.cpp" 
    "exchange/order_server/*.cpp"
)

add_executable(exchange_main 
    exchange/exchange_main.cpp 
    ${EXCHANGE_SOURCES}
)

find_package(Threads REQUIRED)
target_link_libraries(exchange_main PRIVATE Threads::Threads)