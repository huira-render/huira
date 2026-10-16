# 07 - Clouds and atmosphere

Clouds as a second, partly transparent shell over the Earth, and an atmosphere as a medium that scatters light.

![07 - Clouds and atmosphere](https://media.huira.space/examples/07_clouds_and_atmosphere.png)

## Data

| File | What it is | From |
|---|---|---|
| `8k_earth_daymap.jpg` | The Earth's surface by day | [media.huira.space](https://media.huira.space/data/textures/earth/) |
| `8k_earth_clouds.jpg` | The Earth's clouds | [media.huira.space](https://media.huira.space/data/textures/earth/) |

Download them into this folder:

```bash
curl -fLO https://media.huira.space/data/textures/earth/8k_earth_daymap.jpg
curl -fLO https://media.huira.space/data/textures/earth/8k_earth_clouds.jpg
```

Licenses, and how to credit the data: [Earth textures](../../docs/getting_started/data.md#earth-textures).

## Run

From this folder, in Python:

```bash
python clouds_and_atmosphere.py 8k_earth_daymap.jpg 8k_earth_clouds.jpg
```

or in C++, built as in [Running the examples](../README.md#c):

```bash
../../build/07_clouds_and_atmosphere 8k_earth_daymap.jpg 8k_earth_clouds.jpg
```

Each writes its images to `output/`, in the folder it runs from.
