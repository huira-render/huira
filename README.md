# <img src="./docs/_static/logo/full/adaptive.svg" width="400">

*Huira* is a ray-tracing library for rendering large scenes, star fields, and simulating solar radiation pressure.


[![Linux CI/CD](https://github.com/huira-render/huira/actions/workflows/linux-ci-cd.yml/badge.svg?branch=main)](https://github.com/huira-render/huira/actions/workflows/linux-ci-cd.yml?query=branch%3Amain)
[![Windows CI/CD](https://github.com/huira-render/huira/actions/workflows/windows-ci-cd.yml/badge.svg?branch=main)](https://github.com/huira-render/huira/actions/workflows/windows-ci-cd.yml?query=branch%3Amain)
[![macOS CI/CD](https://github.com/huira-render/huira/actions/workflows/macos-ci-cd.yml/badge.svg?branch=main)](https://github.com/huira-render/huira/actions/workflows/macos-ci-cd.yml?query=branch%3Amain)
[![Coverage](https://codecov.io/gh/huira-render/huira/branch/main/graph/badge.svg)](https://app.codecov.io/gh/huira-render/huira/tree/main)

[![Conda Build](https://github.com/huira-render/huira/actions/workflows/conda-build.yml/badge.svg?branch=main)](https://github.com/huira-render/huira/actions/workflows/conda-build.yml?query=branch%3Amain)
[![Python](https://github.com/huira-render/huira/actions/workflows/python.yml/badge.svg?branch=main)](https://github.com/huira-render/huira/actions/workflows/python.yml?query=branch%3Amain)

***
# Features
Initial work on Huira has been on the basic architecture as well as distribution/cross-platform compatibility.  As much of that work is now completed, new features are expected to be released in relatively short order.

If there are features you wish to see, that you don't see listed here, please feel free to submit a [Feature Request](https://github.com/huira-render/huira/issues/new?template=feature_request.md)

## Currently Stable Features (as of v0.8.1)
- Radiometrically accurate unresolved rendering with calibrated camera distortion models and common camera controls
- SPICE toolkit integration for spacecraft ephemeris and reference frames
- Star field rendering with accurate celestial coordinates
- Python Bindings
- Logging and crash report generation
- API Reference Documentation (NOTE: Some docs may appear incomplete or poorly formatted)

## Features Coming Soon
- 3D mesh and material support
- Motion blur
- Camera Depth-of-Field
- Digital Elevation Maps
- Level-of-detail support
- Solar Radiation Pressure simulation
- LIDAR simulation
- TLE support
- Improved API Reference Documentation and Quick-start guides

## Long Term Plans
- Vulkan based GPU Acceleration
- Desktop application (GUI)

***

# Installing Huira

## Python
Huira's python bindings are available on [PyPI](https://pypi.org/project/huira/), and can be installed with:

```bash
pip install huira
```

For building from source or more details, see the **[Python Bindings](docs/getting_started/build_instructions/python-bindings.md)** guide.

## C++ (Building From Source)
Please see the platform specific build guides:
- **[Linux](docs/getting_started/build_instructions/linux.md)**
- **[macOS](docs/getting_started/build_instructions/macos.md)**
- **[Windows (PowerShell)](docs/getting_started/build_instructions/windows.md)**
- **[Windows (Visual Studio)](docs/getting_started/build_instructions/visual-studio.md)**
- **[Build Options](docs/getting_started/build_instructions/options.md)**

***

# Examples
Example programs demonstrating common usage patterns are available in the [`examples/`](examples/) directory.

*** 

# Background
Huira is a complete rewrite providing similar functionality to the [vira](https://github.com/nasa/vira) project originally developed by the same author while at NASA's Goddard Space Flight Center. While inspired by vira, huira is built from scratch with new code and is released under the MIT license.

Vira is still maintained on a [personal fork by the original author](https://github.com/crgnam/vira).  However it is recommended to use this project moving forward.


***

# License
Huira is licensed under the [MIT License](./LICENSE)
