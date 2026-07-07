cmake_minimum_required(VERSION 3.20)

entelechy_module(
    NAME EcsLib
    PUBLIC_DEPS
        CoreLib
    PRIVATE_DEPS
        LogLib
        ThreadPoolLib
    INIT_FUNCTION initEcs
    NAMESPACE Entelechy
)

