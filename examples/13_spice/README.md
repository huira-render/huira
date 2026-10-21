# 13 - SPICE

The Earth placed and turned by SPICE, with its orientation from NAIF's kernels and the Sun where it really was, seen from a satellite on a Keplerian orbit; and six hours later, with the Earth turned and the satellite moved on.

| 2019-02-06 10:27 UTC | Six hours later |
|:--------------------:|:---------------:|
| ![13 - SPICE](https://media.huira.space/examples/13_spice.png) | ![13 - SPICE, six hours later](https://media.huira.space/examples/13_spice_6h_later.png) |

## Data

| File | What it is | From |
|---|---|---|
| `de440s.bsp` | JPL's planetary ephemeris DE440 (short version), an SPK | [NAIF](https://naif.jpl.nasa.gov/pub/naif/generic_kernels/spk/planets/) |
| `earth_latest_high_prec.bpc` | The Earth's precise orientation, a binary PCK | [NAIF](https://naif.jpl.nasa.gov/pub/naif/generic_kernels/pck/) |
| `8k_earth_daymap.jpg` | The Earth's surface by day | [media.huira.space](https://media.huira.space/data/textures/earth/) |

`8k_earth_daymap.jpg` is the texture from [06 - Materials and textures](../06_materials_and_textures/): if you have it already, pass its path instead. Otherwise, download them into this folder:

```bash
curl -fLO https://naif.jpl.nasa.gov/pub/naif/generic_kernels/spk/planets/de440s.bsp
curl -fLO https://naif.jpl.nasa.gov/pub/naif/generic_kernels/pck/earth_latest_high_prec.bpc
curl -fLO https://media.huira.space/data/textures/earth/8k_earth_daymap.jpg
```

Licenses, and how to credit the data: [SPICE kernels](../../docs/getting_started/data.md#spice-kernels), [Earth textures](../../docs/getting_started/data.md#solar-system-textures).

## Run

From this folder, in Python:

```bash
python spice.py de440s.bsp earth_latest_high_prec.bpc 8k_earth_daymap.jpg
```

or in C++, built as in [Running the examples](../README.md#c):

```bash
../../build/13_spice de440s.bsp earth_latest_high_prec.bpc 8k_earth_daymap.jpg
```

Each writes its images to `output/`, in the folder it runs from.
