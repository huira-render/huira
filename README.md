# Huira Renderer
*Huira* is a ray-tracing library for rendering large scenes, star fields, and simulating solar radiation pressure.


**Supported Platforms:** Linux, macOS (ARM), Windows

[![CI/CD Pipeline](https://github.com/huira-render/huira/actions/workflows/ci-cd.yml/badge.svg?branch=main)](https://github.com/huira-render/huira/actions/workflows/ci-cd.yml?query=branch%3Amain)
[![Coverage](https://codecov.io/gh/huira-render/huira/branch/main/graph/badge.svg)](https://app.codecov.io/gh/huira-render/huira/tree/main)

***

# Key Features
- Large-scale scene support (large datasets and large celestial distances)
- Radiometrically accurate rendering with calibrated camera distortion models, depth-of-field, and common camera controls

# Building Huira
Please see the platform specific quickstart guides:
- **[Linux](docs/build/linux.md)**
- **[MacOS](docs/build/macos.md)**
- **[Windows (Powershell)](docs/build/windows.md)**
- **[Windows (Visual Studio)](docs/build/visual-studio.md)**

To see additional optionals for building *Huira*, please refer to:
- **[Huira Build Options](docs/build/options.md)**

# Examples
Example programs demonstrating common usage patterns are available in the [`examples/`](examples/) directory.

<!--
# Background
Huira is a complete rewrite providing similar functionality to the [vira](https://github.com/crgnam/vira) project originally developed by the same author while at NASA's Goddard Space Flight Center. While inspired by vira, huira is built from scratch with new code and is released under the MIT license.

Vira is still maintained on a [personal fork by the original author](https://github.com/crgnam/vira).  However it is recommended to use this project moving forward.
-->

***

# License
Huira is licensed under the [MIT License](./LICENSE)