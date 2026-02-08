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
mkdir "%OUTPUT_DIR%\fk" 2>nul
mkdir "%OUTPUT_DIR%\sclk" 2>nul
mkdir "%OUTPUT_DIR%\ck" 2>nul
mkdir "%OUTPUT_DIR%\spk" 2>nul

echo Downloading kernels to %OUTPUT_DIR%...
echo.

REM Get the frame kernel
curl -o "%OUTPUT_DIR%\fk\orx_v14.tf" https://naif.jpl.nasa.gov/pub/naif/pds/pds4/orex/orex_spice/spice_kernels/fk/orx_v14.tf

REM Get the Spacecraft Clock kernel
curl -o "%OUTPUT_DIR%\sclk\orx_sclkscet_00093.tsc" https://naif.jpl.nasa.gov/pub/naif/pds/pds4/orex/orex_spice/spice_kernels/sclk/orx_sclkscet_00093.tsc

REM Get the CK kernels
curl -o "%OUTPUT_DIR%\ck\orx_struct_mapcam_v01.bc" https://naif.jpl.nasa.gov/pub/naif/pds/pds4/orex/orex_spice/spice_kernels/ck/orx_struct_mapcam_v01.bc
curl -o "%OUTPUT_DIR%\ck\orx_sc_rel_160919_160925_v01.bc" https://naif.jpl.nasa.gov/pub/naif/pds/pds4/orex/orex_spice/spice_kernels/ck/orx_sc_rel_160919_160925_v01.bc

REM Get the SPK kernels
curl -o "%OUTPUT_DIR%\spk\orx_160909_171201_170830_od023_v1.bsp" https://naif.jpl.nasa.gov/pub/naif/pds/pds4/orex/orex_spice/spice_kernels/spk/orx_160909_171201_170830_od023_v1.bsp
curl -p "%OUTPUT_DIR%\spk\orx_struct_v04.bsp" https://naif.jpl.nasa.gov/pub/naif/pds/pds4/orex/orex_spice/spice_kernels/spk/orx_struct_v04.bsp
curl -p "%OUTPUT_DIR%\spk\de424.bsp" https://naif.jpl.nasa.gov/pub/naif/ORX/kernels/spk/de424.bsp

echo.
echo Download complete! Files saved to: %OUTPUT_DIR%

endlocal
