cmake_minimum_required(VERSION 3.20)

add_library(BaseLib STATIC
    ${CMAKE_CURRENT_LIST_DIR}/foundation_types.h
    ${CMAKE_CURRENT_LIST_DIR}/string_id.h
    ${CMAKE_CURRENT_LIST_DIR}/small_string.h
    ${CMAKE_CURRENT_LIST_DIR}/string_format.h
    ${CMAKE_CURRENT_LIST_DIR}/string_intern_pool.h
    ${CMAKE_CURRENT_LIST_DIR}/string_intern_pool.cpp
    ${CMAKE_CURRENT_LIST_DIR}/dynamic_array.h
    ${CMAKE_CURRENT_LIST_DIR}/hash_map.h
    ${CMAKE_CURRENT_LIST_DIR}/inline_array.h
    ${CMAKE_CURRENT_LIST_DIR}/ring_buffer.h
    ${CMAKE_CURRENT_LIST_DIR}/path.h
)

target_include_directories(BaseLib PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/../memory
)

target_compile_features(BaseLib PUBLIC cxx_std_20)
