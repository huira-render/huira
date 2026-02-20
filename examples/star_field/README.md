# Star Field Example

This example shows you how to render a star field using OSIRIS-REx ephemerides.

- [Get the SPICE Kernels](#get-the-spice-kernels)
- [Python Example](#python-example)
- [C++ Example](#c-example)

## Get the SPICE Kernels

First, you'll need to download the required OSIRIS-REx kernels:

On windows:

```powershell
fetch_kernels.bat C:\path\to\your\kernels\
```

On linux/macos:
```bash
./fetch_kernels.sh /path/to/your/kernels/
```

Inside of the provided `kernels/` it will download the following:
```
├── ck
│   ├── orx_sc_rel_160919_160925_v01.bc
│   └── orx_struct_mapcam_v01.bc
├── fk
│   └── orx_v14.tf
├── sclk
│   └── orx_sclkscet_00093.tsc
└── spk
    ├── de424.bsp
    ├── orx_160909_171201_170830_od023_v1.bsp
    └── orx_struct_v04.bsp
```

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

### Fetch the Tycho-2 Star Catalog Data
Once installed, the `huira` command line tool will be available in your environment.  We can use this to download and construct a Tycho2 star catalog:
```bash
huira fetch-tycho2 /your/output/path/ --process --clean
```

This will create a file at `/your/output/path/tycho2.hrsc`.  While we'll be using this in our example, this file contains the entire Tycho2 catalog so you can use it for any future projects as well.  You only need to run this command once to get the star catalog data.

### Run the Example
Once you have the star catalog and kernels downloaded, you can run the example as follows:

```bash
python star_field_example.py /your/output/path/tycho2.hrsc /path/to/your/kernels/
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
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/conda-toolchain.cmake -CMAKE_BUILD_TYPE=Release -DHUIRA_EXAMPLES=ON -DHUIRA_TOOLS=ON ..
cmake --build . -j
```

while on windows you'll need to specify the release configuration when building:

```bash
mkdir build
cd build
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/conda-toolchain.cmake -CMAKE_BUILD_TYPE=Release -DHUIRA_EXAMPLES=ON -DHUIRA_TOOLS=ON ..
cmake --build . --config Release -j
```

This will create a `huira` executable in your build directory.  (On windows, it may be located in `build/Release/`)

### Fetch the Tycho-2 Star Catalog Data
We can now fetch the tycho2 star catalog data using the `huira` CLI tool, which we will need to run the example:
```bash
./huira fetch-tycho2 /your/output/path/ --process --clean
```

This will create a file at `/your/output/path/tycho2.hrsc`.  While we'll be using this in our example, this file contains the entire Tycho2 catalog so you can use it for any future projects as well.  You only need to run this command once to get the star catalog data.

### Run the Example
The example code allows you to pass in the location of the Tycho-2 star catalog file and the SPICE kernels via command line arguments.  For example:
```bash
./star_field_example /your/output/path/tycho2.hrsc /path/to/your/kernels/
```
