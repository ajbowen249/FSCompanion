cmake_minimum_required(VERSION 3.7)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(SimConnectBridge)

add_executable(SimConnectBridge
    src/main.cpp
    src/yhs.c
)

target_include_directories(SimConnectBridge PUBLIC "${CMAKE_SOURCE_DIR}/include")

# IMPROVE: Allow a way to specify this path
target_include_directories(SimConnectBridge PUBLIC "C:\\MSFS SDK\\SimConnect SDK\\include")
target_link_libraries(SimConnectBridge "C:\\MSFS SDK\\SimConnect SDK\\lib\\static\\SimConnect.lib")

target_link_libraries(SimConnectBridge
    wsock32
    ws2_32
    Shlwapi
)
