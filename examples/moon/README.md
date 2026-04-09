# Moon Example

*This example is a work in progress!*

This example shows you how to render a star field with Jupiter and it's moons.

- [Get the Moon Model](#get-the-moon-model)
- [Python Example](#python-example)
- [C++ Example](#c-example)

## Get the Moon Model

*Coming Soon*

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
python moon.py /your/output/path/moon.glb
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
./moon /your/output/path/moon.glb
```
