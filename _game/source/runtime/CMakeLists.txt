cmake_minimum_required(VERSION 3.20)

add_library(RuntimeLib STATIC
    ${CMAKE_CURRENT_LIST_DIR}/game_runtime.cpp
    ${CMAKE_CURRENT_LIST_DIR}/game_runtime.h
)

target_include_directories(RuntimeLib PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
)

target_compile_features(RuntimeLib PUBLIC cxx_std_17)

target_link_libraries(RuntimeLib PUBLIC
    CoreLib
    SystemLib
    BridgeLib
)
