# Windows (Powershell) C++ Build Guide

## Table of Contents
- [System Requirements](#system-requirements)
- [Method 1: conda Environment Setup (Preferred)](#method-1-conda-environment-setup-preferred)
- [Method 2: vcpkg Package Manager](#method-2-vcpkg-package-manager)

***

## System Requirements

- **C++ Compiler:** C++20 compatible
  - Visual Studio 2019 16.8+ or Visual Studio 2022
- **Version Control:** Git (recent version)
- **Build System:** CMake 3.16+

### Installing Base Dependencies

For systems lacking these tools, you can install using the Visual Studio Installer:

- Download and install [Visual Studio Community](https://visualstudio.microsoft.com/downloads/) (free)
- While installing, select `Desktop development with C++` workload
- You can optionally select `C++ Clang Compiler for Windows`


## Method 1: conda Environment Setup (Preferred)

This approach leverages [conda](https://github.com/conda-forge/miniforge) for streamlined dependency management and is our top recommendation. Ensure your [system requirements](#system-requirements) are met first.

### Step 1: conda Installation
If conda isn't already available:
- Go to: https://github.com/conda-forge/miniforge/releases/latest
- Download the `Miniforge3-Windows-x86_64.exe` installer
- Run the installer and follow the prompts
- Launch "Miniforge Prompt" and run `conda init powershell` so that the conda environment can be accessed from powershell.

### Step 2: Environment Creation
```powershell
conda env create -f packaging/environment.yml
conda activate huira_env
```

### Step 3: Compilation Process
```powershell
mkdir build && cd build
cmake -D CMAKE_TOOLCHAIN_FILE="../cmake/conda-toolchain.cmake" ../
cmake --build . --config Release -j
```
NOTE: If you are using bash shell, you may not need the space between `-D` and `CMAKE_TOOLCHAIN_FILE`.

### Step 4: Environment Installation
After successful compilation, integrate *Huira* into your conda environment:

If you're in `powershell`:
```powershell
cmake --install . --config Release --prefix "$env:CONDA_PREFIX"
```

If you're in cmd prompt or miniforge prompt:
```cmd
cmake --install . --config Release --prefix "%CONDA_PREFIX%"
```

***

## Method 2: vcpkg Package Manager

[vcpkg](https://github.com/microsoft/vcpkg) provides cross-platform package management through source compilation. Initial builds may take considerable time due to source-based dependency building. Verify [system requirements](#system-requirements) before proceeding.

### vcpkg Configuration:
```powershell
git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
cd C:\vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg integrate install
```

### Compilation Steps:
```powershell
mkdir build && cd build
cmake -D CMAKE_TOOLCHAIN_FILE="C:/vcpkg/scripts/buildsystems/vcpkg.cmake" ../
cmake --build . --config Release -j
```

NOTE: If you are using bash shell, you may not need the space between `-D` and `CMAKE_TOOLCHAIN_FILE`.
