@echo off
setlocal enabledelayedexpansion

REM Check if directory argument is provided
if "%~1"=="" (
    echo Error: No directory specified
    echo Usage: %~nx0 ^<output_directory^>
    echo Example: %~nx0 C:\Users\chris\huira_data\kernels
    exit /b 1
)

REM Get the output directory from the first argument
set "OUTPUT_DIR=%~1"

REM Remove trailing backslash if present
if "%OUTPUT_DIR:~-1%"=="\" set "OUTPUT_DIR=%OUTPUT_DIR:~0,-1%"
if "%OUTPUT_DIR:~-1%"=="/" set "OUTPUT_DIR=%OUTPUT_DIR:~0,-1%"

REM Create the base directory and subdirectories
mkdir "%OUTPUT_DIR%" 2>nul
mkdir "%OUTPUT_DIR%\spk" 2>nul

echo Downloading kernels to %OUTPUT_DIR%...
echo.

REM Get the SPK kernels
curl -o "%OUTPUT_DIR%\spk\de440s.bsp" https://naif.jpl.nasa.gov/pub/naif/generic_kernels/spk/planets/de440s.bsp
curl -o "%OUTPUT_DIR%\spk\jup365.bsp" https://naif.jpl.nasa.gov/pub/naif/generic_kernels/spk/satellites/jup365.bsp

echo.
echo Download complete! Files saved to: %OUTPUT_DIR%

endlocal
