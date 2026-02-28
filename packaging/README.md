# Dependencies Directory

This directory contains lists of dependencies for various package managers required for building/distributing huira.

- `enviornment.yml`: Used to set up the `huira_env` conda environment for building the project locally.

- `recipe/`: The conda build recipe for creating a conda package of huira distributed via conda-forge.  This is also used in the conda-build CI workflow.

- `vcpkg-port/`: Contains the `vcpkg.json` manifest file, `portfile.cmake`, and `usage` file distributed via [vcpkg](https://vcpkg.io/en/package/huira)
  - When building locally with vcpkg as a package manager, this `vcpkg.json` manifest file is used to install all dependencies.  This is also used in the vcpkg-build CI workflow.
