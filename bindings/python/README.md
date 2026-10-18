![Huira](https://raw.githubusercontent.com/huira-render/huira/main/docs/_static/logo/full/blue_on_transparent.svg)

*Huira* is a library for space rendering, LiDAR simulation, and solar radiation pressure modeling.

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

## Quickstart Examples
The [examples](https://github.com/huira-render/huira/tree/main/examples) walk through Huira one feature at a time, in both Python and C++. Each lists the data it needs, with links to download it, and their README has the full list and how to run them.

The data the examples use is not part of Huira: it comes from other providers, under their own licenses. See [Data Sources and Licenses](https://docs.huira.space/getting_started/data.html). In particular, the Tycho-2 star catalog is licensed for non-commercial use only.

## Features
To see what features are currently being worked on, see the [open issues](https://github.com/huira-render/huira/issues?q=is%3Aissue+state%3Aopen+label%3Aenhancement).  If there's a feature you'd like that isn't among them, please submit a [Feature Request](https://github.com/huira-render/huira/issues/new?template=feature_request.md).

## Background
Huira is a complete rewrite providing similar functionality to the [vira](https://github.com/nasa/vira) project originally developed by the same author while at NASA's Goddard Space Flight Center. While inspired by vira, huira is built from scratch with new code and is released under the MIT license.

Vira is also available on a [personal fork by the original author](https://github.com/crgnam/vira).  However it is no longer maintained, and it is recommended to use this project moving forward.

## Citing Huira
If you use Huira in published work, please cite it. On the [Huira GitHub page](https://github.com/huira-render/huira), click **Cite this repository** in the sidebar for APA and BibTeX, or use:

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

## License
Huira is licensed under the [MIT License](https://github.com/huira-render/huira/blob/main/LICENSE).

The MIT License covers Huira's code. It does not cover the data that the examples use, such as star catalogs, SPICE kernels, models and textures: each has its own license, listed in [Data Sources and Licenses](https://docs.huira.space/getting_started/data.html).
