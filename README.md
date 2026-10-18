# <img src="./docs/_static/logo/full/adaptive.svg" width="400">

*Huira* is a library for space rendering, LiDAR simulation, and solar radiation pressure modeling.

If you use Huira in published work, please [cite it](#citing-huira).

[![Linux Build](https://github.com/huira-render/huira/actions/workflows/linux-ci-cd.yml/badge.svg?branch=main)](https://github.com/huira-render/huira/actions/workflows/linux-ci-cd.yml?query=branch%3Amain)
[![Windows Build](https://github.com/huira-render/huira/actions/workflows/windows-ci-cd.yml/badge.svg?branch=main)](https://github.com/huira-render/huira/actions/workflows/windows-ci-cd.yml?query=branch%3Amain)
[![macOS Build](https://github.com/huira-render/huira/actions/workflows/macos-ci-cd.yml/badge.svg?branch=main)](https://github.com/huira-render/huira/actions/workflows/macos-ci-cd.yml?query=branch%3Amain)

[![Conda Build](https://github.com/huira-render/huira/actions/workflows/conda-build.yml/badge.svg?branch=main)](https://github.com/huira-render/huira/actions/workflows/conda-build.yml?query=branch%3Amain)
[![vcpkg Build](https://github.com/huira-render/huira/actions/workflows/vcpkg-build.yml/badge.svg?branch=main)](https://github.com/huira-render/huira/actions/workflows/vcpkg-build.yml?query=branch%3Amain)
[![Python (PyPI)](https://github.com/huira-render/huira/actions/workflows/python.yml/badge.svg?branch=main)](https://github.com/huira-render/huira/actions/workflows/python.yml?query=branch%3Amain)

[![Coverage](https://codecov.io/gh/huira-render/huira/branch/main/graph/badge.svg)](https://app.codecov.io/gh/huira-render/huira/tree/main)

[![DOI](https://zenodo.org/badge/DOI/10.5281/zenodo.23333475.svg)](https://doi.org/10.5281/zenodo.23333475)

| Gateway | Earth |
|:--:|:--|
| ![Gateway Render](https://media.huira.space/examples/03_load_a_model.png) | ![Earth Render](https://media.huira.space/examples/07_clouds_and_atmosphere.png) |

## Installing Huira

### PyPI (Python only)
Huira's python bindings are available on [PyPI](https://pypi.org/project/huira/) and can be installed with:

```bash
pip install huira
```

For building from source or more details on python bindings, see the **[Python Bindings](docs/getting_started/build_instructions/python-bindings.md)** guide.

### Conda (Python and C++)
Huira's python bindings and C++ headers are available on [conda-forge](https://anaconda.org/channels/conda-forge/packages/huira/overview) and can be installed with:

```bash
conda install conda-forge::huira
```

For building from source or more details on python bindings, see the **[Python Bindings](docs/getting_started/build_instructions/python-bindings.md)** guide.

### vcpkg (C++ only)
Huira's C++ headers are available on [vcpkg](https://vcpkg.io/en/package/huira), and can be installed with:

```bash
vcpkg install huira[tools]
```

*Note:* vcpkg is not intended to be an application distribution system, so installed applications are not immediately accessible.  To use the `huira` command line program, you need to run:

```bash
.\path\to\vcpkg\installed\x64-windows\tools\huira\huira
```

### Building from Source
Please see the platform specific build guides:
- **[Linux](docs/getting_started/build_instructions/linux.md)**
- **[macOS](docs/getting_started/build_instructions/macos.md)**
- **[Windows (PowerShell)](docs/getting_started/build_instructions/windows.md)**
- **[Windows (Visual Studio)](docs/getting_started/build_instructions/visual-studio.md)**
- **[Build Options](docs/getting_started/build_instructions/options.md)**

If you wish to build the python bindings from source (perhaps for local edits or development), see the **[Python Bindings](docs/getting_started/build_instructions/python-bindings.md)** guide.

## Quickstart Examples
The [`examples/`](examples/) walk through Huira one feature at a time, in both Python and C++. Each lists the data it needs, with links to download it. See [`examples/README.md`](examples/README.md) for the full list and how to run them.

The data the examples use is not part of Huira: it comes from other providers, under their own licenses. See [Data Sources and Licenses](docs/getting_started/data.md). In particular, the Tycho-2 star catalog is licensed for non-commercial use only.

## Features
To see what features are currently being worked on, see the [open issues](https://github.com/huira-render/huira/issues?q=is%3Aissue+state%3Aopen+label%3Aenhancement).  If there's a feature you'd like that isn't among them, please submit a [Feature Request](https://github.com/huira-render/huira/issues/new?template=feature_request.md).

## Background
Huira is a complete rewrite providing similar functionality to the [vira](https://github.com/nasa/vira) project originally developed by the same author while at NASA's Goddard Space Flight Center. While inspired by vira, huira is built from scratch with new code and is released under the MIT license.

Vira is also available on a [personal fork by the original author](https://github.com/crgnam/vira).  However it is no longer maintained, and it is recommended to use this project moving forward.

## Citing Huira
If you use Huira in published work, please cite it. On GitHub, click
**Cite this repository** in the sidebar for APA and BibTeX, or use:

```bibtex
@software{gnam_huira,
  author    = {Gnam, Christopher},
  title     = {Huira},
  year      = {2026},
  doi       = {10.5281/zenodo.23333475},
  url       = {https://github.com/huira-render/huira}
}
```

Please also note which version of Huira you used.

## Use of AI Tools

As the physical correctness of this library is critical, I feel it's important to acknowledge that AI coding assistants have been used during Huira's development. This has primarily been for:
- Boilerplate
- CI configuration and packaging
- Documentation

AI has also been involved in the development of several algorithms, and used as a "sounding board" during architectural brainstorming.

With that said, every architecture and design decision in Huira is my own (and the core architecture was established before any AI usage). Every line of AI-generated code has been reviewed by me before being merged.

## Contributing to Huira

Contributions are welcome! Please read [CONTRIBUTING.md](CONTRIBUTING.md) before
submitting a pull request. It covers code style, commit conventions, and the
requirements for AI-assisted contributions.

## License
Huira is licensed under the [MIT License](./LICENSE).

The MIT License covers Huira's code. It does not cover the data that the examples use such as star catalogs, SPICE kernels, models and textures: each has its own license, listed in [Data Sources and Licenses](docs/getting_started/data.md).