# Earth Example

This example shows you how to create a model of the Earth with clouds, and an atmosphere, using
Huira's Ellipsoid primitive, and manually constructing materials.

- [Get the Data](#get-the-data)
- [Python Example](#python-example)
- [C++ Example](#c-example)

![Example Render](https://www.huira.space/assets/examples/earth.png)

*NOTE: Volumetric rendering is still very noisy though an update is coming soon to fix that!*

## Get the Data

First, you'll need to clone a repository which has all of the required data for this example:

```bash
git clone https://github.com/huira-render/huira-assets.git
```

Inside of the cloned repository will be a `kernels/` and `textures/` directory.  Point the example
program to the cloned repository so it can access both the textures and kernels.


***

## Python Example

### Install the `huira` Python Package
You can install the `huira` python package either from PyPI:
```bash
pip install huira
```

or you can build it from source by running the following command from the root of the huira repository:
```bash
conda env create -f packaging/environment.yml
conda activate huira_env
pip install bindings/python/
```

### Run the Example
Once you have the star catalog and kernels downloaded, you can run the example as follows:

```bash
python earth.py /your/cloned/huira-assets/
```

***

## C++ Example


### Build the Example
For the C++ examples, we'll need to build the project from source.  Please refer to the build instructions:

- **[Linux](../../docs/getting_started/build_instructions/linux.md)**
- **[macOS](../../docs/getting_started/build_instructions/macos.md)**
- **[Windows (Powershell)](../../docs/getting_started/build_instructions/windows.md)**
- **[Windows (Visual Studio)](../../docs/getting_started/build_instructions/visual-studio.md)**

for detailed steps on how to do this.  The important thing is to enable `HUIRA_EXAMPLES=ON` and `HUIRA_TOOLS=ON` when configuring cmake so that the example binaries will be built.  For example, on linux/macos using a conda environment:
```bash
mkdir build
cd build
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/conda-toolchain.cmake -DCMAKE_BUILD_TYPE=Release -DHUIRA_EXAMPLES=ON -DHUIRA_TOOLS=ON ..
cmake --build . -j
```

while on windows you'll need to specify the release configuration when building:

```bash
mkdir build
cd build
cmake -DCMAKE_TOOLCHAIN_FILE="../cmake/conda-toolchain.cmake" -DCMAKE_BUILD_TYPE=Release -DHUIRA_EXAMPLES=ON -DHUIRA_TOOLS=ON ..
cmake --build . --config Release -j
```

This will create a `huira` executable in your build directory.  (On windows, it may be located in `build/Release/`)

### Run the Example
The example code allows you to pass in the location of the Tycho-2 star catalog file and the SPICE kernels via command line arguments.  For example:
```bash
./earth /your/cloned/huira-assets/
```
