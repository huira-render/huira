# 15 - Satellites

The sky ahead of the ISS, from a camera riding it: every active satellite in view, placed by SGP4 from a catalog of NORAD element sets and drawn as a sunlit point, streaking across a one-second exposure; with the Earth's limb below.

![15 - Satellites](https://media.huira.space/examples/15_satellites.png)

## Data

| File | What it is | From |
|---|---|---|
| `de440s.bsp` | JPL's planetary ephemeris DE440 (short version), an SPK | [NAIF](https://naif.jpl.nasa.gov/pub/naif/generic_kernels/spk/planets/) |
| `active.csv` | Element sets of every active satellite, as OMMs | [CelesTrak](https://celestrak.org/NORAD/elements/) |
| `8k_earth_daymap.jpg` | The Earth's surface by day | [media.huira.space](https://media.huira.space/data/textures/solar_system/) |

`de440s.bsp` is the kernel from [13 - SPICE](../13_spice/), and `8k_earth_daymap.jpg` the texture from [06 - Materials and textures](../06_materials_and_textures/): if you have them already, pass their paths instead. Otherwise, download them into this folder:

```bash
curl -fLO https://naif.jpl.nasa.gov/pub/naif/generic_kernels/spk/planets/de440s.bsp
curl -fLO https://media.huira.space/data/textures/solar_system/8k_earth_daymap.jpg
```

The catalog comes from CelesTrak, which publishes the current element sets of every object in orbit, updated a few times a day. Download the active satellites, in any of its formats: as OMMs in CSV (its default, and the format CelesTrak recommends),

```bash
curl -fL "https://celestrak.org/NORAD/elements/gp.php?GROUP=active&FORMAT=csv" -o active.csv
```

or as TLEs (`FORMAT=tle`), or OMMs in JSON (`FORMAT=json`) or KVN (`FORMAT=kvn`); Huira reads them all. CelesTrak asks that you not download a catalog more than once an hour: see its [usage policy](https://www.celestrak.org/usage-policy.php). The example renders the scene at the epoch of the ISS's element set, so the image is of the day you download the catalog.

Licenses, and how to credit the data: [SPICE kernels](../../docs/getting_started/data.md#spice-kernels), [NORAD element sets](../../docs/getting_started/data.md#norad-element-sets), [Earth textures](../../docs/getting_started/data.md#solar-system-textures).

## Run

From this folder, in Python:

```bash
python satellites.py de440s.bsp active.csv /path/to/8k_earth_daymap.jpg
```

or in C++, built as in [Running the examples](../README.md#c):

```bash
../../build/15_satellites de440s.bsp active.csv /path/to/8k_earth_daymap.jpg
```

Each writes its image to `output/`, in the folder it runs from, with `15_satellites.csv` beside it: each satellite in the field of view, with its catalog number, name and pixel.
