# JSON 解析模块
# 负责读取 Source Forest 配置并产出 ENABLED_MODULES

set(ENABLED_MODULES "")

function(ParseSourceForest config_path)
    message(STATUS "Parsing Source Forest config: ${config_path}")
    
    set(modules_file "${CMAKE_CURRENT_SOURCE_DIR}/generated/modules.cmake")
    message(STATUS "Including modules file: ${modules_file}")
    include("${modules_file}")
    
    # 关键：将变量传递到父作用域
    set(ENABLED_MODULES ${ENABLED_MODULES} PARENT_SCOPE)
endfunction()
