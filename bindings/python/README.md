![Huira](https://raw.githubusercontent.com/huira-render/huira/main/docs/_static/logo/full/blue_on_transparent.svg)

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
Initial work on Huira has been on the basic architecture as well as distribution/cross-platform compatability.  As much of that work is now completed, new features are expected to be released in relatively short order.

If there are features you wish to see, that you don't see listed here, please feel free to submit a [Feature Request](https://github.com/huira-render/huira/issues/new?template=feature_request.md)

## Currently Stable Features (as of v0.9.3)
- Radiometrically accurate rendering
- Planetary BRDFS including Lambertian, Oren-Nayar, Cook-Torrance, McEwen, Lommel-Seeliger.
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
| Planetary Atmospheres | In-Progress | 5/1/26 | v0.9.4 |
| Digital Elevation Maps | In-Progress | 5/1/26 | v0.9.5 |
| Level-of-Detail | Designed | 5/1/26 | v0.9.6 |
| Solar Radiation Pressure | Designed | 6/1/26 | v1.0.X |
| LIDAR simulation | Planned | 6/1/26 | v1.0.X |
| TLE support | Licensing | - | - |

## Long Term Plans
- Vulkan based GPU Acceleration
- Desktop application (GUI)

## Known Bugs and Limitations
- Normal maps are not currently supported (see [issue](https://github.com/huira-render/huira/issues/32))
- PSF is not applied to extended objects, only point sources
- Severe lack of formal testing


***

# Examples
Example programs demonstrating common usage patterns are available in https://github.com/huira-render/huira/tree/main/examples

*** 

# Background
Huira is a complete rewrite providing similar functionality to the [vira](https://github.com/nasa/vira) project originally developed by the same author while at NASA's Goddard Space Flight Center. While inspired by vira, huira is built from scratch with new code and is released under the MIT license.

Vira is still maintained on a [personal fork by the original author](https://github.com/crgnam/vira).  However it is recommended to use this project moving forward.


***

# License
Huira is licensed under the [MIT License](https://github.com/huira-render/huira/blob/main/LICENSE)
