# Examples

These examples walk through Huira one feature at a time, each building on the ones before it.
Every example comes in Python and C++, which take the same steps and write the same images.

Each example's folder has a README with a preview of what it renders, the data it needs, and how
to run it. Each source file opens with the features it demonstrates.

| # | Example | What it demonstrates |
|---|---|---|
| 01 | [First render](01_first_render/) | A scene, a sphere, the Sun and a camera; rendering, and saving the image |
| 02 | [Scene graph](02_scene_graph/) | Sharing assets, frames, rotations and scale, a mesh built in code, `look_at`, and Blender's camera convention |
| 03 | [Load a model](03_load_a_model/) | Loading a glTF model; path tracing settings, and dynamic sampling |

## Running the examples

### Python

Install Huira from PyPI or conda-forge:

```bash
pip install huira
```

or see the [Python Bindings](../docs/getting_started/build_instructions/python-bindings.md) guide
to build it from source. Then run an example from its folder, for example:

```bash
cd examples/01_first_render
python first_render.py
```

Images are written to `output/`, in the folder you run from. Examples that need data take the
paths to it as arguments: each example's README shows how.

### C++

The C++ examples are built with Huira from source. Follow the build guide for your platform:

- **[Linux](../docs/getting_started/build_instructions/linux.md)**
- **[macOS](../docs/getting_started/build_instructions/macos.md)**
- **[Windows (PowerShell)](../docs/getting_started/build_instructions/windows.md)**
- **[Windows (Visual Studio)](../docs/getting_started/build_instructions/visual-studio.md)**

and turn on `HUIRA_EXAMPLES` when configuring. For example, on Linux or macOS with the conda
environment:

```bash
cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/conda-toolchain.cmake -DCMAKE_BUILD_TYPE=Release \
      -DHUIRA_EXAMPLES=ON
cmake --build build -j
```

On Windows, add `--config Release` to the build command.

Each example builds to an executable named after its folder, in the build folder (in
`build/Release/` on Windows):

```bash
./build/01_first_render
```

## Data

Some examples use data that is not part of Huira: 3D models, textures, star catalogs and SPICE
kernels. Each example's README lists the files it needs, with links to download them, either
from [media.huira.space](https://media.huira.space) or straight from their provider (SPICE
kernels, for example, come from NASA's NAIF), and the commands to download them with `curl`,
which comes with Windows 10 and later, macOS and Linux. For example:

```bash
curl -fLO https://media.huira.space/data/models/gateway/gateway.glb
```

In Windows PowerShell 5, type `curl.exe` rather than `curl`, which there is another command.

The data comes from NASA, NAIF, ESA and CDS, and Solar System Scope, each under its own terms:
Huira's MIT license does not cover it. In particular, the Tycho-2 star catalog is licensed for
**non-commercial use only**. See [Data Sources and Licenses](../docs/getting_started/data.md) for
each dataset's license and how to credit it.
