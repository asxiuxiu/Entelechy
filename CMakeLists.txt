cmake_minimum_required(VERSION 3.20)

project(Entelechy LANGUAGES CXX)

# ---- Basic settings ----
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# MSVC: force UTF-8 source/execution charset to prevent C4819 warnings
if(MSVC)
    add_compile_options(/utf-8)
endif()

# Shipping build: strip debug/info logs for minimal binary size
if(SHIPPING_BUILD)
    add_compile_definitions(SHIPPING_BUILD)
endif()

# Unified output directories (must be set before add_subdirectory)
if(CMAKE_CONFIGURATION_TYPES)
    foreach(CONFIG_TYPE ${CMAKE_CONFIGURATION_TYPES})
        string(TOUPPER ${CONFIG_TYPE} CONFIG_TYPE_UPPER)
        set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CONFIG_TYPE_UPPER} ${CMAKE_BINARY_DIR}/lib/${CONFIG_TYPE})
        set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CONFIG_TYPE_UPPER} ${CMAKE_BINARY_DIR}/lib/${CONFIG_TYPE})
        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONFIG_TYPE_UPPER} ${CMAKE_BINARY_DIR}/bin/${CONFIG_TYPE})
    endforeach()
else()
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
endif()

# ---- Include module macros ----
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(EntelechyModule)

# ---- Conan dependencies (top-level to avoid duplication) ----
find_package(glad REQUIRED)
find_package(glfw3 REQUIRED)
find_package(imgui REQUIRED)

# ---- Parse config and add modules directly ----
set(CONFIG_FILE "${CMAKE_CURRENT_SOURCE_DIR}/configs/full_build.json")
file(READ "${CONFIG_FILE}" CONFIG_JSON)

string(JSON SOLUTION_NAME GET "${CONFIG_JSON}" "solution_name")
string(JSON SOURCE_TREE_LEN LENGTH "${CONFIG_JSON}" "source_tree")

math(EXPR SOURCE_TREE_LAST "${SOURCE_TREE_LEN} - 1")

set(FORWARD_DECLS "")
set(INIT_CALLS "")

foreach(I RANGE 0 ${SOURCE_TREE_LAST})
    string(JSON REPO_CONFIG_PATH GET "${CONFIG_JSON}" "source_tree" ${I})
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/${REPO_CONFIG_PATH}" REPO_JSON)

    string(JSON REPO_ROOT GET "${REPO_JSON}" "root_path")
    string(JSON MODULES_LEN LENGTH "${REPO_JSON}" "modules")
    math(EXPR MODULES_LAST "${MODULES_LEN} - 1")

    foreach(J RANGE 0 ${MODULES_LAST})
        string(JSON MODULE_ENABLED GET "${REPO_JSON}" "modules" ${J} "enabled")
        if(MODULE_ENABLED STREQUAL "false")
            continue()
        endif()

        string(JSON MODULE_PATH GET "${REPO_JSON}" "modules" ${J} "path")
        set(REAL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/${REPO_ROOT}/${MODULE_PATH}")

        message(STATUS "[Entelechy] Adding module: ${REPO_ROOT}/${MODULE_PATH}")
        add_subdirectory("${REAL_PATH}")
    endforeach()
endforeach()

# ---- Collect init functions from global property ----
entelechy_get_init_functions(ALL_INIT_FUNCTIONS)
set(_INIT_KEYS "")
set(FORWARD_DECLS "")
set(INIT_CALLS "")
foreach(FUNC ${ALL_INIT_FUNCTIONS})
    string(FIND "${FUNC}" "::" SEP_POS)
    if(SEP_POS EQUAL -1)
        message(WARNING "Invalid init function format: ${FUNC}")
        continue()
    endif()
    string(SUBSTRING "${FUNC}" 0 ${SEP_POS} NS)
    math(EXPR NAME_START "${SEP_POS} + 2")
    string(SUBSTRING "${FUNC}" ${NAME_START} -1 FUNC_NAME)
    set(_KEY "${NS}::${FUNC_NAME}")
    if(NOT _KEY IN_LIST _INIT_KEYS)
        list(APPEND _INIT_KEYS ${_KEY})
        string(APPEND FORWARD_DECLS "namespace ${NS} { void ${FUNC_NAME}(); }\n")
    endif()
    string(APPEND INIT_CALLS "    ${NS}::${FUNC_NAME}();\n")
endforeach()

# ---- Generate main.cpp ----
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/launch/templates/main.cpp.in"
    "${CMAKE_BINARY_DIR}/generated/main.cpp"
    @ONLY
)

# ---- Create final executable ----
add_executable(${PROJECT_NAME} "${CMAKE_BINARY_DIR}/generated/main.cpp")

# Windows: hide console window while keeping main() as entry point
if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)
    target_link_options(${PROJECT_NAME} PRIVATE "/ENTRY:mainCRTStartup")
endif()

# ---- Auto-link all non-test modules ----
entelechy_get_enabled_modules(ALL_MODULES)
set(LINK_MODULES "")
foreach(TARGET_NAME ${ALL_MODULES})
    if(NOT TARGET_NAME MATCHES "Tests$" AND NOT TARGET_NAME STREQUAL "${PROJECT_NAME}")
        list(APPEND LINK_MODULES ${TARGET_NAME})
    endif()
endforeach()

target_link_libraries(${PROJECT_NAME} PRIVATE ${LINK_MODULES})
