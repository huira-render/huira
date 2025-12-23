# Huira Renderer

*Huira* is a ray-tracing library for rendering large scenes, star fields, and simulating solar radiation pressure.

> [!WARNING]
> Active Development: Huira is currently in an alpha state and is not yet feature-complete. For production environments requiring immediate stability, please continue to use the [Vira maintenance fork](https://github.com/crgnam/vira). This notice will be removed once Huira reaches architectural parity and stable release status.

[![Linux Build](https://github.com/huira-render/huira/actions/workflows/linux-build.yml/badge.svg?branch=main)](https://github.com/huira-render/huira/actions/workflows/linux-build.yml)
[![MacOS (x86) Build](https://github.com/huira-render/huira/actions/workflows/macos-x86-build.yml/badge.svg?branch=main)](https://github.com/huira-render/huira/actions/workflows/macos-x86-build.yml)
[![MacOS (ARM) Build](https://github.com/huira-render/huira/actions/workflows/macos-arm-build.yml/badge.svg?branch=main)](https://github.com/huira-render/huira/actions/workflows/macos-arm-build.yml)
[![Windows Build](https://github.com/huira-render/huira/actions/workflows/windows-build.yml/badge.svg?branch=main)](https://github.com/huira-render/huira/actions/workflows/windows-build.yml)

[![Documentation](https://github.com/huira-render/huira/actions/workflows/documentation.yml/badge.svg?branch=main)](https://github.com/huira-render/huira/actions/workflows/documentation.yml)

***

# Key Features
- Large-scale scene support (large datasets and large celestial distances)
- Radiometrically accurate rendering with calibrated camera distortion models, depth-of-field, and common camera controls

# Building Huira

Please see the platform specific build instructions:
- **[Linux](docs/build/linux.md)**
- **[MacOS](docs/build/macos.md)**
- **[Windows](docs/build/windows.md)** (Includes both *Powershell* and *Visual Studio*)

To see additional options for building *Huira*, please refer to the [Options](docs/build/options.md) guide.

# License
Huira is licensed under the [MIT License](./LICENSE)

***

# Project History
Huira is a from-scratch rewrite of [Vira](https://github.com/nasa/vira), which I originally authored during my time at NASA. This rewrite was undertaken for two key reasons:

- **More permissive licensing:** Huira uses the MIT License instead of NOSA, making it easier to integrate into a wider range of projects
- **Improved architecture:** Addresses architectural limitations and technical debt from the original implementation

If you wish to use the original Vira, it is recommended to use my [Vira maintenance fork](https://github.com/crgnam/vira), as the original NASA release is no longer maintained.

## Naming
Both projects are named after the Inca creator deity of the sun, moon, and stars. While Viracocha is the traditional Hispanicized spelling, Huiracocha represents a more modern phonetic transcription of the same Quechua root. This naming convention signifies that while Huira is a complete technical reimagining, it remains fundamentally rooted in the original's purpose and vision.

## Acknowledgements
The original Vira project was developed in large part to support the LuNaMaps project, led by Carolina Restrepo. Her encouragement and support were instrumental in bringing these capabilities to fruition, and that foundational vision continues in Huira's development.
