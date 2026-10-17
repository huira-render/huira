# 09 - Stars

Stars from the Tycho-2 catalog: Orion, and Barnard's star moving over a century.

| Orion | Barnard's star 100 year motion |
|---|---|
| ![09 - Stars](https://media.huira.space/examples/09_stars.png) | ![09 - Stars](https://media.huira.space/examples/09_stars_barnard.gif) 

## Data

| File | What it is | From |
|---|---|---|
| `tycho2.hrsc` | The Tycho-2 star catalog, in Huira's format | [media.huira.space](https://media.huira.space/data/stars/tycho2/) |

You will need this catalog whenever you render stars with Huira: in the later examples, and in your own scenes, which for a space scene is almost always. It is about 120 MB, so keep one copy somewhere central on your machine rather than in this folder, and pass its path wherever you load stars. From the folder you choose, download it with:

```bash
curl -fLO https://media.huira.space/data/stars/tycho2/tycho2.hrsc
```

The Tycho-2 catalog is licensed for **non-commercial use only**.

Licenses, and how to credit the data: [Tycho-2 star catalog](../../docs/getting_started/data.md#tycho-2-star-catalog).

## Run

From this folder, in Python:

```bash
python stars.py /path/to/tycho2.hrsc
```

or in C++, built as in [Running the examples](../README.md#c):

```bash
../../build/09_stars /path/to/tycho2.hrsc
```

Each writes its images to `output/`, in the folder it runs from.
