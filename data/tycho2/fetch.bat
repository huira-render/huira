@echo off
setlocal enabledelayedexpansion

:: Download Tycho-2 star catalog data files from CDS Strasbourg

if "%~1"=="" (
    echo Usage: %~nx0 ^<output_directory^>
    echo Example: %~nx0 C:\Users\you\huira_data
    exit /b 1
)

set "OUTPUT_DIR=%~1\tycho2"
set "BASE_URL=https://cdsarc.cds.unistra.fr/viz-bin/nph-Cat/txt?I/259"

if not exist "%OUTPUT_DIR%" mkdir "%OUTPUT_DIR%"

for /L %%i in (0,1,19) do (
    set "num=0%%i"
    set "num=!num:~-2!"
    set "file=tyc2.dat.!num!"
    echo Downloading !file!...
    curl -fsSL "%BASE_URL%/!file!.gz" -o "%OUTPUT_DIR%\!file!"
)

echo Downloaded to: %OUTPUT_DIR%
