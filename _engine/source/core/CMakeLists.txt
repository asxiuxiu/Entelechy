cmake_minimum_required(VERSION 3.20)

add_library(CoreLib STATIC
    ${CMAKE_CURRENT_LIST_DIR}/world.cpp
    ${CMAKE_CURRENT_LIST_DIR}/world.h
    ${CMAKE_CURRENT_LIST_DIR}/types.h
    ${CMAKE_CURRENT_LIST_DIR}/type_registry.cpp
    ${CMAKE_CURRENT_LIST_DIR}/type_registry.h
    ${CMAKE_CURRENT_LIST_DIR}/component_array.h
    ${CMAKE_CURRENT_LIST_DIR}/string_id.h
    ${CMAKE_CURRENT_LIST_DIR}/small_string.h
    ${CMAKE_CURRENT_LIST_DIR}/string_format.h
    ${CMAKE_CURRENT_LIST_DIR}/string_intern_pool.h
    ${CMAKE_CURRENT_LIST_DIR}/string_intern_pool.cpp
)

target_include_directories(CoreLib PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(CoreLib PUBLIC MemoryLib)

target_compile_features(CoreLib PUBLIC cxx_std_20)
