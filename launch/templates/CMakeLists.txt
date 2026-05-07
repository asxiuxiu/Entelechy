cmake_minimum_required(VERSION 3.20)
project(@SOLUTION_NAME@ LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Shipping build: strip debug/info logs for minimal binary size
if(SHIPPING_BUILD)
    add_compile_definitions(SHIPPING_BUILD)
endif()

# Unified output directories (must be set before add_subdirectory)
if(CMAKE_CONFIGURATION_TYPES)
    # Multi-config generators (Visual Studio, Xcode): per-config subdirectories
    # so Debug and Release libraries do not overwrite each other.
    foreach(CONFIG_TYPE ${CMAKE_CONFIGURATION_TYPES})
        string(TOUPPER ${CONFIG_TYPE} CONFIG_TYPE_UPPER)
        set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CONFIG_TYPE_UPPER} ${CMAKE_BINARY_DIR}/lib/${CONFIG_TYPE})
        set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CONFIG_TYPE_UPPER} ${CMAKE_BINARY_DIR}/lib/${CONFIG_TYPE})
        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONFIG_TYPE_UPPER} ${CMAKE_BINARY_DIR}/bin/${CONFIG_TYPE})
    endforeach()
else()
    # Single-config generators (Ninja, Makefiles)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
endif()

# Include JSON parser module
include(@CMAKE_CURRENT_SOURCE_DIR@/cmake/json_parser.cmake)

# Parse Source Forest config
ParseSourceForest("@CONFIG_PATH@")

# Add all enabled modules
foreach(module ${ENABLED_MODULES})
    message(STATUS "Adding module: ${module}")
    add_subdirectory(${module})
endforeach()

# Create final executable
add_executable(${PROJECT_NAME}
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)

# Windows: hide console window while keeping main() as entry point
if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)
    target_link_options(${PROJECT_NAME} PRIVATE "/ENTRY:mainCRTStartup")
endif()

target_link_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_BINARY_DIR}/lib/$<CONFIG>
)

target_link_libraries(${PROJECT_NAME} PRIVATE
@LINK_LIBS@
)
