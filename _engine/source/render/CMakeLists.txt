cmake_minimum_required(VERSION 3.20)

find_package(glad REQUIRED)
find_package(stb REQUIRED)
find_package(DXC REQUIRED)

entelechy_module(
    NAME RenderCoreLib
    PUBLIC_DEPS
        glad::glad
        WindowLib
        LogLib
        CoreLib
    PRIVATE_DEPS
        ThreadPoolLib
        stb::stb
        # DXC: runtime DXIL container reflection (dxcapi.h) for the D3D12 backend
        DXC::DXC
        # D3D12 backend system libraries (Windows SDK)
        d3d12
        dxgi
        dxguid
)

