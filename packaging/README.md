# Dependencies Directory

This directory contains the dependencies required for building huira.

# conda
- `enviornment.yml`: used to set up a conda environment with the necessary packages.
- `recipe/`: contains the conda build recipe for creating a conda package of huira.  This is used in CI and for distribution via conda-forge.

# vcpkg
- `vcpkg-port/vcpkg.json`: manifest file to be used with [vcpkg](https://vcpkg.io/en/)
- `vcpkg-port/portfile.cmake`: portfile used in distribution
- `vcpkg-port/usage`: usage file used in distribution
