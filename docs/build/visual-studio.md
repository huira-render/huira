# Windows (Visual Studio) Quickstart Guide

## Table of Contents
- [Installing Visual Studio](#installing-visual-studio)
- [Method 1: vcpkg Package Manager (Preferred)](#method-1-vcpkg-package-manager-preferred)
- [Method 2: conda Environment Setup](#method-2-conda-environment-setup)

***

## Installing Visual Studio

- Download and install [Visual Studio Community](https://visualstudio.microsoft.com/downloads/) (free)
- While installing, select `Desktop development with C++` workload
- You can optionally select `C++ Clang Compiler for Windows`


## Method 2: vcpkg Package Manager (Preferred)

[vcpkg](https://github.com/microsoft/vcpkg) provides cross-platform package management through source compilation. Initial builds may take considerable time due to source-based dependency building. For developers of *Huira* using Visual Studio, we recommend using vcpkg.

### vcpkg Configuration:
```powershell
git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
cd C:\vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg integrate install
```

### Compilation Steps:
Once vcpkg is installed, Visual Studio can use it natively.  To Build *Huira* simply:
- Open the `huira` folder in Visual Studio
- Wait for CMake configuration to complete
- From the top tool bar, select `Build -> Build All`

***

## Method 2: conda Environment Setup

This approach leverages [conda](https://github.com/conda-forge/miniforge) for streamlined dependency management.

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

### Step 3: Launch Visual Studio with conda Environment Enabled
Once the `huira_env` conda environment has been activated, we can have Visual Studio inherit all of the relevant PATH variables by launching it from within this environment using `devenv.exe`:
```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\devenv.exe"
```
**NOTE: The exact path to `devenv.exe` may vary slightly.**
