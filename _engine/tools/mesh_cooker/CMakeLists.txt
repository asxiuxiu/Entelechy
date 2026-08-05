cmake_minimum_required(VERSION 3.20)

find_package(cgltf REQUIRED)

entelechy_module(
    NAME MeshCooker
    TYPE EXECUTABLE
    NO_TESTS
    PRIVATE_DEPS
        AssetLib
        cgltf::cgltf
)
