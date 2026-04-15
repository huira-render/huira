# <img src="./docs/_static/logo/full/adaptive.svg" width="400">

*Huira* is a library for space rendering, LiDAR simulation, and solar radiation pressure modeling.

[![Linux Build](https://github.com/huira-render/huira/actions/workflows/linux-ci-cd.yml/badge.svg?branch=main)](https://github.com/huira-render/huira/actions/workflows/linux-ci-cd.yml?query=branch%3Amain)
[![Windows Build](https://github.com/huira-render/huira/actions/workflows/windows-ci-cd.yml/badge.svg?branch=main)](https://github.com/huira-render/huira/actions/workflows/windows-ci-cd.yml?query=branch%3Amain)
[![macOS Build](https://github.com/huira-render/huira/actions/workflows/macos-ci-cd.yml/badge.svg?branch=main)](https://github.com/huira-render/huira/actions/workflows/macos-ci-cd.yml?query=branch%3Amain)

[![Conda Build](https://github.com/huira-render/huira/actions/workflows/conda-build.yml/badge.svg?branch=main)](https://github.com/huira-render/huira/actions/workflows/conda-build.yml?query=branch%3Amain)
[![vcpkg Build](https://github.com/huira-render/huira/actions/workflows/vcpkg-build.yml/badge.svg?branch=main)](https://github.com/huira-render/huira/actions/workflows/vcpkg-build.yml?query=branch%3Amain)
[![Python (PyPI)](https://github.com/huira-render/huira/actions/workflows/python.yml/badge.svg?branch=main)](https://github.com/huira-render/huira/actions/workflows/python.yml?query=branch%3Amain)

[![Coverage](https://codecov.io/gh/huira-render/huira/branch/main/graph/badge.svg)](https://app.codecov.io/gh/huira-render/huira/tree/main)

***

# Features
Initial work on Huira has been on the basic architecture as well as distribution/cross-platform compatibility.  As much of that work is now completed, new features are expected to be released in relatively short order.

If there are features you wish to see, that you don't see listed here, please feel free to submit a [Feature Request](https://github.com/huira-render/huira/issues/new?template=feature_request.md)

## Currently Stable Features (as of v0.9.5)
- Radiometrically accurate rendering
- Planetary BRDFS including Lambertian, Oren-Nayar, Cook-Torrance, McEwen, Lommel-Seeliger
- Basic volumetric rendering of planetary atmospheres
- Calibrated camera distortion models and common camera controls
- Motion blur
- Depth-of-Field
- SPICE toolkit integration for spacecraft ephemeris and reference frames
- Star field and unresolved rendering
- Spherical light sources with Next Event Estimation and Multiple Importance Sampling
- Python Bindings
- Logging and crash report generation
- API Reference Documentation (NOTE: Some docs may appear incomplete or poorly formatted)

## Features Coming Soon (Order of Priority)
| Feature | Status | Expected by | Version |
| --- | --- | --- | --- |
| Digital Elevation Maps | In-Progress | 5/1/26 | v0.9.7 |
| Level-of-Detail | Designed | 5/1/26 | v0.9.8 |
| Solar Radiation Pressure | Designed | 6/1/26 | v1.0.X |
| LIDAR simulation | Planned | 6/1/26 | v1.0.X |
| Comprehensive Tutorials | In-Progress | 6/1/26 | v1.0.X |
| TLE support | Licensing | - | - |

## Long Term Plans
- Vulkan based GPU Acceleration
- Desktop application (GUI)

## Known Bugs and Limitations
- Severe lack of formal testing
- Star fields potentially too dim (validation with real-world images is currently in-progress)

***

# Installing Huira

## Python
Huira's python bindings are available on [PyPI](https://pypi.org/project/huira/) and can be installed with:

```bash
pip install huira
```

For building from source or more details, see the **[Python Bindings](docs/getting_started/build_instructions/python-bindings.md)** guide.

## C++ (Package Managers)
Huira's C++ library is available on [vcpkg](https://vcpkg.io/en/package/huira), and can be installed with:

### vcpkg:
```bash
vcpkg install huira[tools]
```

*Note:* vcpkg is not intended to be an application distribution system, so installed applications are not immediately accessible.  To use the `huira` command line program, you need to run:

```bash
.\path\to\vcpkg\installed\x64-windows\tools\huira\huira
```

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

*Comprehensive tutorials are coming soon!*

*** 

# Background
Huira is a complete rewrite providing similar functionality to the [vira](https://github.com/nasa/vira) project originally developed by the same author while at NASA's Goddard Space Flight Center. While inspired by vira, huira is built from scratch with new code and is released under the MIT license.

Vira is still maintained on a [personal fork by the original author](https://github.com/crgnam/vira).  However it is recommended to use this project moving forward.

***
# Contributing to Huira

We welcome contributions! To keep the codebase clean, highly readable, and consistent, we enforce a strict style guide using `clang-format`. 

## Code Formatting
Huira uses a custom `.clang-format` file located in the root of the repository. Before submitting a pull request, please ensure your code is formatted.

*NOTE: Most modern IDEs (Visual Studio, VS Code, CLion) will automatically detect and apply this file when you format your document.*

If you prefer the command line, you can format your changes using:

```bash
clang-format -i path/to/your/file.cpp
```

### Key Rules
* **Line Length:** Maximum 100 characters per line.
* **Indentation:** 4 spaces (no tabs).
* **Brace Style:** Modified Stroustrup/K&R style. 
  * Classes, structs, and control statements keep their opening brace on the same line.
  * Function definitions drop their opening brace to the next line to separate complex signatures from logic.
* **Control Statements:** Single-statement `if` or `while` blocks must always be wrapped in braces. 
* **Includes:** Includes are automatically sorted alphabetically into three groups: System headers, Third-party headers, and huira headers.

### Naming Conventions
* **Classes and Structs:** `PascalCase`
* **Functions:** `snake_case`
* **Variables:** `snake_case`
* **Private Members:** `snake_case` with a trailing underscore

***

# License
Huira is licensed under the [MIT License](./LICENSE)
