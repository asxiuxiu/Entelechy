cmake_minimum_required(VERSION 3.20)

project(Entelechy)

# 自动检测 Python 解释器
find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(SOURCETREE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/build/sourcetree")
set(CONFIG_FILE "${CMAKE_CURRENT_SOURCE_DIR}/configs/full_build.json")

# 如果源码树不存在，自动生成
if(NOT EXISTS "${SOURCETREE_DIR}/CMakeLists.txt")
    message(STATUS "Source forest not generated yet. Running generator...")
    execute_process(
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/launch/generator.py" --config "${CONFIG_FILE}"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        RESULT_VARIABLE GENERATOR_RESULT
        OUTPUT_VARIABLE GENERATOR_OUTPUT
        ERROR_VARIABLE GENERATOR_ERROR
    )
    if(NOT GENERATOR_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to generate source forest:\n${GENERATOR_ERROR}")
    endif()
    message(STATUS "Source forest generated successfully.")
endif()

# 将生成的源码树作为子目录加入构建
add_subdirectory("${SOURCETREE_DIR}" "${CMAKE_BINARY_DIR}/sourcetree_build")
