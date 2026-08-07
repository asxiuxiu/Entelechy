cmake_minimum_required(VERSION 3.20)

# Find DXC (prebuilt in third_party/dxc/)
find_package(DXC REQUIRED)

entelechy_module(
    NAME ShaderCompiler
    TYPE EXECUTABLE
    NO_TESTS
    PRIVATE_DEPS
        CoreLib
        DXC::DXC
        spirv-cross::spirv-cross
)

# Windows: ensure Win32 API headers are available for DXC COM types
if(WIN32)
    target_compile_definitions(ShaderCompiler PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

# Copy dxcompiler.dll and dxil.dll to output directory so the tool can find them at runtime
add_custom_command(TARGET ShaderCompiler POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${DXC_DLL}" "$<TARGET_FILE_DIR:ShaderCompiler>/"
    COMMENT "Copying dxcompiler.dll to ShaderCompiler output directory"
)

# Also copy dxil.dll (required by dxcompiler.dll for validation)
get_filename_component(DXC_BIN_DIR "${DXC_DLL}" DIRECTORY)
add_custom_command(TARGET ShaderCompiler POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${DXC_BIN_DIR}/dxil.dll" "$<TARGET_FILE_DIR:ShaderCompiler>/"
    COMMENT "Copying dxil.dll to ShaderCompiler output directory"
)
