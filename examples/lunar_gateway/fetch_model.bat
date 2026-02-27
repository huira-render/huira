@echo off
setlocal enabledelayedexpansion

REM Check if directory argument is provided
if "%~1"=="" (
    echo Error: No directory specified
    echo Usage: %~nx0 ^<output_directory^>
    echo Example: %~nx0 C:\Users\chris\huira_data\models
    exit /b 1
)

REM Get the output directory from the first argument
set "OUTPUT_DIR=%~1"

REM Remove trailing backslash if present
if "%OUTPUT_DIR:~-1%"=="\" set "OUTPUT_DIR=%OUTPUT_DIR:~0,-1%"
if "%OUTPUT_DIR:~-1%"=="/" set "OUTPUT_DIR=%OUTPUT_DIR:~0,-1%"

REM Create the directory
mkdir "%OUTPUT_DIR%" 2>nul

echo Downloading Gateway model to %OUTPUT_DIR%...
echo.

REM Download the Gateway model
curl -o "%OUTPUT_DIR%\gateway.glb" "https://assets.science.nasa.gov/content/dam/science/cds/3d/resources/model/gateway/Gateway%%20Core.glb"

echo.
echo Download complete! File saved to: %OUTPUT_DIR%\gateway.glb

endlocal
