cmake_minimum_required(VERSION 3.20)

add_library(EcsTests OBJECT
    ${CMAKE_CURRENT_LIST_DIR}/test_world.cpp
    ${CMAKE_CURRENT_LIST_DIR}/test_archetype_world.cpp
)

target_link_libraries(EcsTests PUBLIC EcsLib TestFrameworkLib)

target_include_directories(EcsTests PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../private)

target_compile_features(EcsTests PUBLIC cxx_std_20)

set_property(GLOBAL APPEND PROPERTY ENTELECHY_ENABLED_MODULES EcsTests)
