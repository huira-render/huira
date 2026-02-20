# Linux C++ Build Guide

## Table of Contents
- [System Requirements](#system-requirements)
- [Method 1: conda Environment Setup (Preferred)](#method-1-conda-environment-setup-preferred)
- [Method 2: vcpkg Package Manager](#method-2-vcpkg-package-manager)

***

## System Requirements

- **C++ Compiler:** C++20 compatible
  - GCC 10+ or Clang 10+
- **Version Control:** Git (recent version)
- **Build System:** CMake 3.16+
- **Configuration Tool:** pkg-config

### Installing Base Dependencies

For systems lacking these tools:
```bash
# On Ubuntu/Debian systems:
sudo apt update && sudo apt install build-essential cmake git pkg-config

# On RHEL/CentOS systems:
sudo yum groupinstall "Development Tools"
sudo yum install cmake git pkgconfig
```

### Alternative for conda Users

When using the conda approach below, you may install prerequisites within your conda environment instead of system-wide:

```bash
conda install -c conda-forge cmake git pkg-config cxx-compiler make
```

This eliminates the need for `sudo` privileges while still providing the necessary build tools.

***

## Method 1: conda Environment Setup (Preferred)

This approach leverages [conda](https://github.com/conda-forge/miniforge) for streamlined dependency management and is our top recommendation. Ensure your [system requirements](#system-requirements) are met first.

### Step 1: conda Installation
If conda isn't already available:
```bash
cd ~
curl -L -O "https://github.com/conda-forge/miniforge/releases/latest/download/Miniforge3-Linux-x86_64.sh"

chmod +x Miniforge3-Linux-x86_64.sh
./Miniforge3-Linux-x86_64.sh
```
Complete the installation prompts and verify conda activation by checking for `(base)` in your terminal prompt.  If it is not activated, you may need to run `source ~/miniforge3/bin/activate`.

### Step 2: Environment Creation
```bash
conda env create -f packaging/environment.yml
conda activate huira_env
```

### Step 3: Compilation Process
```bash
mkdir build && cd build
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/conda-toolchain.cmake -DCMAKE_BUILD_TYPE=Release ../
cmake --build . -j
```

### Step 4: Environment Installation
After successful compilation, integrate *Huira* into your conda environment:

```bash
cmake --install . --prefix "$CONDA_PREFIX"
```

***

## Method 2: vcpkg Package Manager

[vcpkg](https://github.com/microsoft/vcpkg) provides cross-platform package management through source compilation. Initial builds may take considerable time due to source-based dependency building. Verify [system requirements](#system-requirements) before proceeding.

### vcpkg Configuration:
```bash
git clone https://github.com/microsoft/vcpkg.git ~/vcpkg
~/vcpkg/bootstrap-vcpkg.sh
~/vcpkg/vcpkg integrate install
```

### Compilation Steps:
```bash
mkdir build && cd build
cmake -DCMAKE_TOOLCHAIN_FILE=~/vcpkg/scripts/buildsystems/vcpkg.cmake -DCMAKE_BUILD_TYPE=Release ../
cmake --build . -j
```
