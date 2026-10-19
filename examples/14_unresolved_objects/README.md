# 14 - Unresolved objects

Jupiter and its four largest moons, too far away to cover a pixel: placed by ephemeris and drawn like stars, as seen from low Earth orbit on 2016-09-19. And how light time and aberration move them.

![14 - Unresolved objects](https://media.huira.space/examples/14_unresolved_objects.png)

## Data

| File | What it is | From |
|---|---|---|
| `de440s.bsp` | JPL's planetary ephemeris DE440 (short version), an SPK | [NAIF](https://naif.jpl.nasa.gov/pub/naif/generic_kernels/spk/planets/) |
| `jup365.bsp` | JPL's ephemeris of Jupiter and its moons, an SPK | [NAIF](https://naif.jpl.nasa.gov/pub/naif/generic_kernels/spk/satellites/) |
| `tycho2.hrsc` | The Tycho-2 star catalog, in Huira's format | [media.huira.space](https://media.huira.space/data/stars/tycho2/) |

`de440s.bsp` is the kernel from [13 - SPICE](../13_spice/): if you have it already, pass its path instead. Otherwise, download the kernels into this folder:

```bash
curl -fLO https://naif.jpl.nasa.gov/pub/naif/generic_kernels/spk/planets/de440s.bsp
curl -fLO https://naif.jpl.nasa.gov/pub/naif/generic_kernels/spk/satellites/jup365.bsp
```

`jup365.bsp` is over 1 GB.

`tycho2.hrsc` is the star catalog from [09 - Stars](../09_stars/). It is about 120 MB, so keep one copy somewhere central on your machine rather than in this folder, and pass its path. If you don't have it yet, download it from the folder you choose with:

```bash
curl -fLO https://media.huira.space/data/stars/tycho2/tycho2.hrsc
```

The Tycho-2 catalog is licensed for **non-commercial use only**.

Licenses, and how to credit the data: [SPICE kernels](../../docs/getting_started/data.md#spice-kernels), [Tycho-2 star catalog](../../docs/getting_started/data.md#tycho-2-star-catalog).

## Run

From this folder, in Python:

```bash
python unresolved_objects.py de440s.bsp jup365.bsp /path/to/tycho2.hrsc
```

or in C++, built as in [Running the examples](../README.md#c):

```bash
../../build/14_unresolved_objects de440s.bsp jup365.bsp /path/to/tycho2.hrsc
```

Each writes its images to `output/`, in the folder it runs from.
