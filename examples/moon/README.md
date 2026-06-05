# Moon Example

This example shows how to render the Moon from QLD terrain/albedo tiles using Huira.

- [Get the Data](#get-the-data)
- [Python Example](#python-example)
- [C++ Example](#c-example)

## Get the Data

Download the LDEM16/WAC dataset with:

```bash
build/huira fetch-moon-data /path/to/data
```

Convert the rasters to QLD tiles:

```bash
build/huira dem2qld dems/LDEM_16.LBL qpu -r /path/to/data --moon-ldem16
```

This creates:

```text
/path/to/data/qpu/*.qld
```

## Python Example

The Python example uses the QLD loader binding added to Huira's `visible8`
module. If you have a PyPI copy of `huira` installed, reinstall the bindings
from this checkout so Python imports the local build:

```bash
conda activate huira_env
pip install --force-reinstall --no-deps bindings/python/
```

Run the example from the repository root:

```bash
python examples/moon/moon.py /path/to/data/qpu
```

The render is written to:

```text
output/moon.png
```

## C++ Example

### Build the Example

For the C++ examples, build with `HUIRA_EXAMPLES=ON` and `HUIRA_TOOLS=ON`:

```bash
mkdir build
cd build
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/conda-toolchain.cmake -DCMAKE_BUILD_TYPE=Release -DHUIRA_EXAMPLES=ON -DHUIRA_TOOLS=ON ..
cmake --build . -j
```

### Run the Example

From the build directory:

```bash
./moon /path/to/data/qpu
```

The render is written to:

```text
output/moon.png
```
