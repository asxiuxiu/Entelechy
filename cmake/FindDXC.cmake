# FindDXC.cmake — Locate Microsoft DirectX Shader Compiler (prebuilt)
#
# Expects prebuilt DXC in third_party/dxc/ or path specified by DXC_ROOT.
# Provides imported targets:
#   DXC::DXC   — dxcompiler library (import lib + DLL)
#   DXC::Tool  — dxc.exe command-line compiler

find_path(DXC_INCLUDE_DIR
    NAMES dxcapi.h
    PATHS
        "${PROJECT_SOURCE_DIR}/third_party/dxc/inc"
        "$ENV{DXC_ROOT}/inc"
    NO_DEFAULT_PATH
)

find_library(DXC_LIBRARY
    NAMES dxcompiler
    PATHS
        "${PROJECT_SOURCE_DIR}/third_party/dxc/lib/x64"
        "$ENV{DXC_ROOT}/lib/x64"
    NO_DEFAULT_PATH
)

find_file(DXC_DLL
    NAMES dxcompiler.dll
    PATHS
        "${PROJECT_SOURCE_DIR}/third_party/dxc/bin/x64"
        "$ENV{DXC_ROOT}/bin/x64"
    NO_DEFAULT_PATH
)

find_program(DXC_TOOL
    NAMES dxc
    PATHS
        "${PROJECT_SOURCE_DIR}/third_party/dxc/bin/x64"
        "$ENV{DXC_ROOT}/bin/x64"
    NO_DEFAULT_PATH
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(DXC
    REQUIRED_VARS DXC_INCLUDE_DIR DXC_LIBRARY DXC_DLL DXC_TOOL
)

if(DXC_FOUND AND NOT TARGET DXC::DXC)
    add_library(DXC::DXC SHARED IMPORTED)
    set_target_properties(DXC::DXC PROPERTIES
        IMPORTED_LOCATION "${DXC_DLL}"
        IMPORTED_IMPLIB "${DXC_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${DXC_INCLUDE_DIR}"
    )
endif()

if(DXC_FOUND AND NOT TARGET DXC::Tool)
    add_executable(DXC::Tool IMPORTED)
    set_target_properties(DXC::Tool PROPERTIES
        IMPORTED_LOCATION "${DXC_TOOL}"
    )
endif()
