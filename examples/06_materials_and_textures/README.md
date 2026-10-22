# 06 - Materials and textures

The Earth, textured: a color map for its albedo, a roughness map that makes the oceans glint, a normal map for relief, and city lights that glow on its night side.

| Day Side | Night Side |
|:--------:|:----------:|
| ![06 - Materials and textures](https://media.huira.space/examples/06_materials_and_textures.png) | ![06 - Materials and textures](https://media.huira.space/examples/06_materials_and_textures_night.png) |


## Data

| File | What it is | From |
|---|---|---|
| `8k_earth_city_emission.jpg` | The Earth's city lights | [media.huira.space](https://media.huira.space/data/textures/solar_system/) |
| `8k_earth_daymap.jpg` | The Earth's surface by day | [media.huira.space](https://media.huira.space/data/textures/solar_system/) |
| `8k_earth_roughness_map.tif` | The Earth's roughness: low over water | [media.huira.space](https://media.huira.space/data/textures/solar_system/) |
| `8k_earth_normal_map.tif` | The Earth's relief, as a normal map | [media.huira.space](https://media.huira.space/data/textures/solar_system/) |

Download them into this folder:

```bash
curl -fLO https://media.huira.space/data/textures/solar_system/8k_earth_city_emission.jpg
curl -fLO https://media.huira.space/data/textures/solar_system/8k_earth_daymap.jpg
curl -fLO https://media.huira.space/data/textures/solar_system/8k_earth_roughness_map.tif
curl -fLO https://media.huira.space/data/textures/solar_system/8k_earth_normal_map.tif
```

Licenses, and how to credit the data: [Earth textures](../../docs/getting_started/data.md#solar-system-textures).

## Run

From this folder, in Python:

```bash
python materials_and_textures.py 8k_earth_daymap.jpg 8k_earth_city_emission.jpg 8k_earth_roughness_map.tif 8k_earth_normal_map.tif
```

or in C++, built as in [Running the examples](../README.md#c):

```bash
../../build/06_materials_and_textures 8k_earth_daymap.jpg 8k_earth_city_emission.jpg 8k_earth_roughness_map.tif 8k_earth_normal_map.tif
```

Each writes its images to `output/`, in the folder it runs from.
