# Windows (Powershell) Quickstart Guide

## Table of Contents
- [System Requirements](#system-requirements)
- [Method 1: conda Environment Setup (Preferred)](#method-1-conda-environment-setup-preferred)
- [Method 2: vcpkg Package Manager](#method-2-vcpkg-package-manager)
- [Method 3: Manual Dependency Management (Advanced Users)](#method-3-manual-dependency-management-advanced-users)

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
cmake -DCMAKE_TOOLCHAIN_FILE="../cmake/conda-toolchain.cmake" ../
cmake --build . --config Release -j
```

### Step 4: Environment Installation (Optional)
After successful compilation, integrate *Huira* into your conda environment:

```powershell
cmake --install . --config Release
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
cmake -DCMAKE_TOOLCHAIN_FILE="C:/vcpkg/scripts/buildsystems/vcpkg.cmake" ../
cmake --build . --config Release -j
```

***

## Method 3: Manual Dependency Management (Advanced Users)

This approach requires manual installation of all dependencies through system package managers or source compilation. **Warning:** This method is unsupported and may encounter compatibility issues with system-available library versions.

### Required Dependencies

| Package | Version Requirement | Purpose |
|---------|:-------------------:|---------|
| assimp | >=5.2,<6.0 | 3D asset importing |
| catch2 | >=3.8.0 | Unit testing suite (when `HUIRA_TESTS=ON`) |
| cfitsio | >=3.49 | FITS file handling |
| cspice | =67 | NASA SPICE toolkit |
| embree3 | >=3.13,<4.0 | Ray intersection kernels |
| fftw | >=3.3.10,<4.0 | Fourier transform operations |
| gdal | >=3.10,<4.0 | Geographic data processing |
| glm | >=1.0.1 | Mathematical operations |
| libtiff | >=4.7.0 | TIFF image processing |
| tbb-devel | >=2021.0 | Threading Building Blocks |

### Build Process

Once dependencies are satisfied, execute from the repository root:

```bash
mkdir build
cd build
cmake ../
cmake --build . --config Release -j
```