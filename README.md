# Huira Renderer

*Huira* is a ray-tracing library for rendering large scenes, star fields, and simulating solar radiation pressure.

### Main Branch Status

[![Docs](https://github.com/huira-render/huira/actions/workflows/documentation.yml/badge.svg?branch=main)](https://github.com/huira-render/huira/actions/workflows/documentation.yml?query=branch%3Amain)

[![C++ Tests](https://github.com/huira-render/huira/actions/workflows/cpp-tests.yml/badge.svg?branch=main)](https://github.com/huira-render/huira/actions/workflows/cpp-tests.yml?query=branch%3Amain])
[![Coverage](https://codecov.io/gh/huira-render/huira/branch/main/graph/badge.svg)](https://app.codecov.io/gh/huira-render/huira/tree/main)

| Platform | C++ Build | Python Bindings |
|:--------:|:---------:|:---------------:|
| Linux       | [![Linux][ref-nix-badge]]([ref-nix-yml])     |  |
| MacOS (x86) | [![MacOS x86][ref-osi-badge]](ref-osi-yml)   |  |
| MacOS (ARM) | [![MacOS ARM][ref-osa-badge]]([ref-osa-yml]) |  |
| Windows     | [![Windows][ref-win-badge]]([ref-win-yml])   |  |

### Develop Branch Status (Latest)

[![C++ Tests](https://github.com/huira-render/huira/actions/workflows/cpp-tests.yml/badge.svg?branch=develop)](https://github.com/huira-render/huira/actions/workflows/cpp-tests.yml?query=branch%3Adevelop)
[![Coverage](https://codecov.io/gh/huira-render/huira/branch/develop/graph/badge.svg)](https://app.codecov.io/gh/huira-render/huira/tree/develop)

| Branch | Builds | Quality |
|:-------|:------:|:-------:|
| **Main (Stable)** | [![Linux](https://github.com/huira-render/huira/actions/workflows/linux-build.yml/badge.svg?branch=main)](https://github.com/huira-render/huira/actions/workflows/linux-build.yml?query=branch%3Amain) [![macOS x86](https://github.com/huira-render/huira/actions/workflows/macos-x86-build.yml/badge.svg?branch=main)](https://github.com/huira-render/huira/actions/workflows/macos-x86-build.yml?query=branch%3Amain) [![macOS ARM](https://github.com/huira-render/huira/actions/workflows/macos-arm-build.yml/badge.svg?branch=main)](https://github.com/huira-render/huira/actions/workflows/macos-arm-build.yml?query=branch%3Amain) [![Windows](https://github.com/huira-render/huira/actions/workflows/windows-build.yml/badge.svg?branch=main)](https://github.com/huira-render/huira/actions/workflows/windows-build.yml?query=branch%3Amain) | [![Tests](https://github.com/huira-render/huira/actions/workflows/cpp-tests.yml/badge.svg?branch=main)](https://github.com/huira-render/huira/actions/workflows/cpp-tests.yml?query=branch%3Amain) [![Coverage](https://codecov.io/gh/huira-render/huira/branch/main/graph/badge.svg)](https://app.codecov.io/gh/huira-render/huira/tree/main) [![Docs](https://github.com/huira-render/huira/actions/workflows/documentation.yml/badge.svg?branch=main)](https://github.com/huira-render/huira/actions/workflows/documentation.yml?query=branch%3Amain) |
| **Develop (Latest)** | [![Linux](https://github.com/huira-render/huira/actions/workflows/linux-build.yml/badge.svg?branch=develop)](https://github.com/huira-render/huira/actions/workflows/linux-build.yml?query=branch%3Adevelop) | [![Tests](https://github.com/huira-render/huira/actions/workflows/cpp-tests.yml/badge.svg?branch=develop)](https://github.com/huira-render/huira/actions/workflows/cpp-tests.yml?query=branch%3Adevelop) [![Coverage](https://codecov.io/gh/huira-render/huira/branch/develop/graph/badge.svg)](https://app.codecov.io/gh/huira-render/huira/tree/develop) |

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

***

***

# License
Huira is licensed under the [MIT License](./LICENSE)


<!--- Linux Badges and Workflows --->
[ref-nix-badge]: https://github.com/huira-render/huira/actions/workflows/linux-build.yml/badge.svg?branch=main
[ref-nix-yml]: https://github.com/huira-render/huira/actions/workflows/linux-build.yml?query=branch%3Amain

[ref-nix-dev-badge]: https://github.com/huira-render/huira/actions/workflows/linux-build.yml/badge.svg?branch=develop
[ref-nix-dev-yml]: https://github.com/huira-render/huira/actions/workflows/linux-build.yml?query=branch%3Adevelop

<!--- MacOS x86 Badges and Workflows --->
[ref-osi-badge]: https://github.com/huira-render/huira/actions/workflows/macos-x86-build.yml/badge.svg?branch=main
[ref-osi-yml]: https://github.com/huira-render/huira/actions/workflows/macos-x86-build.yml?query=branch%3Amain

<!--- MacOS ARM Badges and Workflows --->
[ref-osa-badge]: https://github.com/huira-render/huira/actions/workflows/macos-arm-build.yml/badge.svg?branch=main
[ref-osa-yml]: https://github.com/huira-render/huira/actions/workflows/macos-arm-build.yml?query=branch%3Amain

<!--- Windows Badges and Workflows --->
[ref-win-badge]: https://github.com/huira-render/huira/actions/workflows/windows-build.yml/badge.svg?branch=main
[ref-win-yml]: https://github.com/huira-render/huira/actions/workflows/windows-build.yml?query=branch%3Amain