@echo off
setlocal enabledelayedexpansion

REM Entelechy Build Script
REM Usage: build.bat [ --config <json> ] [ --debug | --release ]

set CONFIG=configs/full_build.json
set BUILD_TYPE=Release

:parse_args
if "%~1"=="" goto :done_parsing
if "%~1"=="--config" (
    set CONFIG=%~2
    shift
    shift
    goto :parse_args
)
if "%~1"=="--debug" (
    set BUILD_TYPE=Debug
    shift
    goto :parse_args
)
if "%~1"=="--release" (
    set BUILD_TYPE=Release
    shift
    goto :parse_args
)
shift
goto :parse_args
:done_parsing

echo [Build] Config: %CONFIG%
echo [Build] Type: %BUILD_TYPE%

REM Step 1: Generate Source Forest
echo [Build] Step 1: Generating Source Forest...
python launch/generator.py --config %CONFIG%
if errorlevel 1 (
    echo [Build] Generator failed.
    exit /b 1
)

REM Step 2: Run CMake
echo [Build] Step 2: Running CMake...
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_CONFIGURATION_TYPES="Debug;Release"
if errorlevel 1 (
    echo [Build] CMake configuration failed.
    exit /b 1
)

REM Step 3: Build
echo [Build] Step 3: Building %BUILD_TYPE%...
cmake --build build --config %BUILD_TYPE% --parallel
if errorlevel 1 (
    echo [Build] Build failed.
    exit /b 1
)

echo [Build] Success! Output: build/bin/
