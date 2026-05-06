cmake_minimum_required(VERSION 3.20)

add_library(LogLib STATIC
    ${CMAKE_CURRENT_LIST_DIR}/log_level.h
    ${CMAKE_CURRENT_LIST_DIR}/log_entry.h
    ${CMAKE_CURRENT_LIST_DIR}/queued_log_entry.h
    ${CMAKE_CURRENT_LIST_DIR}/log_category.h
    ${CMAKE_CURRENT_LIST_DIR}/log_output_device.h
    ${CMAKE_CURRENT_LIST_DIR}/log_utils.h
    ${CMAKE_CURRENT_LIST_DIR}/console_output.h
    ${CMAKE_CURRENT_LIST_DIR}/console_output.cpp
    ${CMAKE_CURRENT_LIST_DIR}/file_output.h
    ${CMAKE_CURRENT_LIST_DIR}/file_output.cpp
    ${CMAKE_CURRENT_LIST_DIR}/json_file_output.h
    ${CMAKE_CURRENT_LIST_DIR}/json_file_output.cpp
    ${CMAKE_CURRENT_LIST_DIR}/logger.h
    ${CMAKE_CURRENT_LIST_DIR}/logger.cpp
    ${CMAKE_CURRENT_LIST_DIR}/log_macros.h
)

target_include_directories(LogLib PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/..
)

target_link_libraries(LogLib PUBLIC CoreLib)

if(MSVC)
    target_compile_definitions(LogLib PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

target_compile_features(LogLib PUBLIC cxx_std_17)
