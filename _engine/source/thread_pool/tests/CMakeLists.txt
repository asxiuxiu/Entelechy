cmake_minimum_required(VERSION 3.20)

add_library(ThreadPoolTests OBJECT
    ${CMAKE_CURRENT_LIST_DIR}/test_thread_pool.cpp
)

target_link_libraries(ThreadPoolTests PUBLIC ThreadPoolLib TestFrameworkLib)

target_include_directories(ThreadPoolTests PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../private)

target_compile_features(ThreadPoolTests PUBLIC cxx_std_20)

set_property(GLOBAL APPEND PROPERTY ENTELECHY_ENABLED_MODULES ThreadPoolTests)
