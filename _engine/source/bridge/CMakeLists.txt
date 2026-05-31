cmake_minimum_required(VERSION 3.20)

entelechy_module(
    NAME BridgeLib
    PUBLIC_DEPS
        EcsLib
        MotorLib
        LogLib
    INIT_FUNCTION initBridge
    NAMESPACE Entelechy
)

