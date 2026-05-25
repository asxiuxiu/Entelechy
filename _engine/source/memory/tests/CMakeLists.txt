cmake_minimum_required(VERSION 3.20)

add_library(MemoryTests OBJECT
    ${CMAKE_CURRENT_LIST_DIR}/test_memory.cpp
)

target_link_libraries(MemoryTests PUBLIC MemoryLib TestFrameworkLib)

target_compile_features(MemoryTests PUBLIC cxx_std_20)
