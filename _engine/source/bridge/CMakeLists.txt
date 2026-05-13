cmake_minimum_required(VERSION 3.20)

add_library(BridgeLib STATIC
    ${CMAKE_CURRENT_LIST_DIR}/agent_bridge.cpp
    ${CMAKE_CURRENT_LIST_DIR}/agent_bridge.h
    ${CMAKE_CURRENT_LIST_DIR}/tool_registry.cpp
    ${CMAKE_CURRENT_LIST_DIR}/tool_registry.h
)

target_include_directories(BridgeLib PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
)

target_compile_features(BridgeLib PUBLIC cxx_std_20)

target_link_libraries(BridgeLib PUBLIC
    CoreLib
    MotorLib
    LogLib
)
