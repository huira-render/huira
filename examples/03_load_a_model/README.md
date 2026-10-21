# 03 - Load a model

NASA's model of the Gateway, loaded from a glTF file and path traced.

![03 - Load a model](https://media.huira.space/examples/03_load_a_model.png)

## Data

| File | What it is | From |
|---|---|---|
| `gateway.glb` | NASA's model of the Gateway | [media.huira.space](https://media.huira.space/data/models/nasa/) |

Download it into this folder:

```bash
curl -fLO https://media.huira.space/data/models/nasa/gateway.glb
```

Licenses, and how to credit the data: [Gateway model](../../docs/getting_started/data.md#nasa-spacecraft-models).

## Run

From this folder, in Python:

```bash
python load_a_model.py gateway.glb
```

or in C++, built as in [Running the examples](../README.md#c):

```bash
../../build/03_load_a_model gateway.glb
```

Each writes its images to `output/`, in the folder it runs from.
