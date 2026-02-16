# Jupiter Long Range Example

This example shows you how to render a star field with Jupiter and it's moons.

## Get the Tycho2 Star Catalog and SPICE Kernels:

1. Fetch the Tycho-2 Star Catalog Data

To run this example, we must first use the `huira` CLI to fetch the relevant data.  If you are building huira from source, make sure you have built the CLI binary by configuring cmake with `HUIRA_APPS=ON`.  Once you have the CLI binary, run the following command:

```bash
huira fetch-tycho2 /your/output/path/ --process --clean
```
This tells `huira` to download and process the Tycho-2 star catalog so that we can render it.  This needs only be run once.  It will produce a `tycho2.hrsc` file (`hrsc` = "Huira Star Catalog") in your designated path, which we can use moving forward.

2. Downlaod the required OSIRIS-REx kernels:

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
└── spk
    ├── de440s.bsp
    └── jup365.bsp
```


## C++ Example Code
Simply build the project using the cmake option `HUIRA_EXAMPLES=ON`.  This will build all the examples, including this one.  For example, if you are on linux/macos using a conda environment:
```bash
mkdir build
cd build
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/conda-toolchain.cmake -CMAKE_BUILD_TYPE=Release -DHUIRA_EXAMPLES=ON ..
cmake --build . -j
```

The example code allows you to pass in the location of the Tycho-2 star catalog file and the SPICE kernels via command line arguments.  For example:
```bash
./jupiter_range_example /your/output/path/tycho2.hrsc /path/to/your/kernels/
```

## Python Example
Make sure you've installed the `huira` python package.  If you are installing from source, simply navigate to the `bindings/python/` and run `pip install .`

Once you have `huira` installed, you can run the python example as follows:
```bash
python jupiter_long_range.py /your/output/path/tycho2.hrsc /path/to/your/kernels/
```
