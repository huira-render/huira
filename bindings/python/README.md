![Huira](https://raw.githubusercontent.com/huira-render/huira/main/docs/_static/logo/full/blue_on_transparent.svg)

*Huira* is a ray-tracing library for rendering large scenes, star fields, and simulating solar radiation pressure.


[![Linux CI/CD](https://github.com/huira-render/huira/actions/workflows/linux-ci-cd.yml/badge.svg?branch=main)](https://github.com/huira-render/huira/actions/workflows/linux-ci-cd.yml?query=branch%3Amain)
[![Windows CI/CD](https://github.com/huira-render/huira/actions/workflows/windows-ci-cd.yml/badge.svg?branch=main)](https://github.com/huira-render/huira/actions/workflows/windows-ci-cd.yml?query=branch%3Amain)
[![macOS CI/CD](https://github.com/huira-render/huira/actions/workflows/macos-ci-cd.yml/badge.svg?branch=main)](https://github.com/huira-render/huira/actions/workflows/macos-ci-cd.yml?query=branch%3Amain)
[![Coverage](https://codecov.io/gh/huira-render/huira/branch/main/graph/badge.svg)](https://app.codecov.io/gh/huira-render/huira/tree/main)

[![Conda Build](https://github.com/huira-render/huira/actions/workflows/conda-build.yml/badge.svg?branch=main)](https://github.com/huira-render/huira/actions/workflows/conda-build.yml?query=branch%3Amain)
[![Python](https://github.com/huira-render/huira/actions/workflows/python.yml/badge.svg?branch=main)](https://github.com/huira-render/huira/actions/workflows/python.yml?query=branch%3Amain)

***
# Features
Initial work on Huira has been on the basic architecture as well as distribution/cross-platform compatability.  As much of that work is now completed, new features are expected to be released in relatively short order.

If there are features you wish to see, that you don't see listed here, please feel free to submit a [Feature Request](https://github.com/huira-render/huira/issues/new?template=feature_request.md)

## Currently Stable Features (as of v0.8.0)
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
