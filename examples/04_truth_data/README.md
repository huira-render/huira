# 04 - Truth data

The images a render can make besides the picture: depth, normals, albedo, geometry IDs and received power. And ray queries, which ask the scene what lies along a ray.

| Rendered image | Depth | Normals |
|---|---|---|
| ![Rendered image](https://media.huira.space/examples/04_truth_data.png) | ![Depth](https://media.huira.space/examples/04_truth_data_depth.png) | ![Normals](https://media.huira.space/examples/04_truth_data_normals.png) |
| **Albedo** | **Geometry IDs** | **Indirect light** |
| ![Albedo](https://media.huira.space/examples/04_truth_data_albedo.png) | ![Geometry IDs](https://media.huira.space/examples/04_truth_data_ids.png) | ![Indirect light](https://media.huira.space/examples/04_truth_data_indirect.png) |

## Data

| File | What it is | From |
|---|---|---|
| `gateway.glb` | NASA's model of the Gateway | [media.huira.space](https://media.huira.space/data/models/gateway/) |

Download it into this folder:

```bash
curl -fLO https://media.huira.space/data/models/gateway/gateway.glb
```

Licenses, and how to credit the data: [Gateway model](../../docs/getting_started/data.md#gateway-model).

## Run

From this folder, in Python:

```bash
python truth_data.py gateway.glb
```

or in C++, built as in [Running the examples](../README.md#c):

```bash
../../build/04_truth_data gateway.glb
```

Each writes its images to `output/`, in the folder it runs from.
