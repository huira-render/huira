# Python Bindings

## Table of Contents
- [System Requirements](#system-requirements)
- [Building and Installing Python Bindings](#building-and-installing-python-bindings)

***

## System Requirements

- **C++ Compiler:** C++20 compatible
  - GCC 10+, Clang 10+, or MSVC 2019+
- **Build System:** CMake 3.16+
- **Configuration Tool:** pkg-config (if on Linux/macOS)

## Building and Installing Python Bindings
Make sure you have `conda` (miniforge) installed and setup on your system.  Please refer to the [Windows](windows.md) [Linux/macOS](linux.md) quickstart guides for getting `conda Installation` before proceeding with the instructions below.

Once you have `conda` available, building and installing the python bindings is trivial.  From within the root directory of the project, simply run:

```
conda env create -f packaging/environment.yml
conda activate huira_env
cd bindings/python
pip install .
```

This creates the `huira_env` conda environment, activates it, and then installs the python bindings into that environment.  You can verify the installation by running:

```
cd ~
python -c "import huira; print(huira.__version__)"
```

NOTE: Do not try to run `import huira` while inside of `bindings/python` as this will cause an import error.
