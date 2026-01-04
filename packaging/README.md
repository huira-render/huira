# Dependencies Directory

This directory contains the dependencies required for building huira.

# conda
- `enviornment.yml`: used to set up a conda environment with the necessary packages.
- `recipe/`: contains the conda build recipe for creating a conda package of huira.  This is used in CI and for distribution via conda-forge.

# vcpkg
- `vcpkg.json`: vcpkg manifest file to be used with [vcpkg](https://vcpkg.io/en/)
