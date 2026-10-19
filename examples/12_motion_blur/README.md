# 12 - Motion blur

NASA's model of the Gateway, spinning once a second. A short exposure freezes the spin; a long one smears its outer parts into arcs around the spin axis, while the parts near it stay sharp.

| Short exposure (0.25 ms) | Long exposure (16 ms) |
|:------------------------:|:---------------------:|
| ![12 - Short exposure](https://media.huira.space/examples/12_motion_blur_short.png) | ![12 - Long exposure](https://media.huira.space/examples/12_motion_blur_long.png) |

Then the Gateway passes the camera at 60 m/s. A camera that stares sees it smear along its path; one that tracks it with `look_at` turns to follow it, and holds it sharp. With motion blur off, the camera renders a still frame from the middle of the exposure.

| Staring (16 ms) | Tracking (16 ms) | Motion blur off |
|:---------------:|:----------------:|:---------------:|
| ![12 - Staring](https://media.huira.space/examples/12_motion_blur_passing.png) | ![12 - Tracking](https://media.huira.space/examples/12_motion_blur_tracking.png) | ![12 - Motion blur off](https://media.huira.space/examples/12_motion_blur_still.png) |

## Data

| File | What it is | From |
|---|---|---|
| `gateway.glb` | NASA's model of the Gateway | [media.huira.space](https://media.huira.space/data/models/gateway/) |

This is the model from [03 - Load a model](../03_load_a_model/): if you have it already, pass its path instead. Otherwise, download it into this folder:

```bash
curl -fLO https://media.huira.space/data/models/gateway/gateway.glb
```

Licenses, and how to credit the data: [Gateway model](../../docs/getting_started/data.md#gateway-model).

## Run

From this folder, in Python:

```bash
python motion_blur.py gateway.glb
```

or in C++, built as in [Running the examples](../README.md#c):

```bash
../../build/12_motion_blur gateway.glb
```

Each writes its images to `output/`, in the folder it runs from.
