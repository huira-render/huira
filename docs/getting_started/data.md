# Data Sources and Licenses

Huira's code is released under the [MIT License](https://github.com/huira-render/huira/blob/main/LICENSE).
The data that Huira's [examples](https://github.com/huira-render/huira/tree/main/examples) use
is not part of Huira. Some of it is hosted at
[media.huira.space](https://media.huira.space): 3D models, textures and star catalogs. The rest
comes straight from its provider, such as SPICE kernels from NASA's NAIF. Each dataset has its
own license, listed on this page, and each example's README lists the files it uses.

Please credit the data's providers when you publish work made with it, including rendered
images.

> **The Tycho-2 star catalog is licensed for non-commercial use only.** Huira's MIT license does
> not change that. If you render stars for commercial work, check that your use is allowed: see
> [Tycho-2 star catalog](#tycho-2-star-catalog).

| Data | Where to get it | License |
|---|---|---|
| [Tycho-2 star catalog](#tycho-2-star-catalog) | media.huira.space | CC BY-NC 3.0 IGO (non-commercial) |
| [SPICE kernels](#spice-kernels) | NASA's NAIF | Redistributable unmodified |
| [Gateway model](#gateway-model) | media.huira.space | No copyright; NASA's media guidelines |
| [Earth textures](#earth-textures) | media.huira.space | CC BY 4.0 |
| [Kernels that ship with Huira](#kernels-that-ship-with-huira) | Included with Huira | Redistributable unmodified |

Each folder on media.huira.space holds an `ATTRIBUTION.txt`, with its data's source, license and
how to credit it, and a `SHA256SUMS`, to check downloads with `sha256sum -c SHA256SUMS`.

## Tycho-2 star catalog

The Tycho-2 Catalogue of the 2.5 million brightest stars, from ESA's Hipparcos mission, as
distributed by CDS, Strasbourg (VizieR catalog I/259).

- **Get it:** download the pre-built catalog, in Huira's star catalog format, from
  [media.huira.space/data/stars/tycho2/tycho2.hrsc](https://media.huira.space/data/stars/tycho2/tycho2.hrsc):

  ```sh
  curl -fLO https://media.huira.space/data/stars/tycho2/tycho2.hrsc
  ```

  `tycho2.hrsc` is always the latest build. Earlier builds stay at
  [media.huira.space/data/stars/tycho2/archive/](https://media.huira.space/data/stars/tycho2/archive/)
  as `tycho2-d<N>.hrsc`, where `N` is the build's data version (below), next to the catalog's
  original files from CDS.
- **Source:** [cdsarc.cds.unistra.fr/viz-bin/cat/I/259](https://cdsarc.cds.unistra.fr/viz-bin/cat/I/259)
- **License:** [CC BY-NC 3.0 IGO](https://creativecommons.org/licenses/by-nc/3.0/igo/). You may
  share and adapt the catalog, with credit, for non-commercial purposes only.
- **Huira's star catalog:** `tycho2.hrsc` is an adaptation of Tycho-2, under the same license.
  The conversion keeps each star's identifier, position (brought to epoch J2000.0) and proper
  motion; computes an approximate Johnson V magnitude, an effective temperature and a solid angle
  from the catalog's BT and VT magnitudes; drops the stars without magnitudes and the catalog's
  other columns; and sorts the stars by V magnitude.
- **Versions:** an `.hrsc` file carries two version numbers in its header, which Huira logs when
  it loads one. The *format version* is the file's layout; Huira refuses a format it cannot
  read and says whether Huira or the catalog is the older side. The *data version* is how the
  values were computed, and goes up whenever a new build changes them (a better temperature
  from B-V, say); Huira loads any data version. If your stars differ from someone else's,
  compare data versions. The format is documented in
  [`star_catalog.hpp`](https://github.com/huira-render/huira/blob/main/include/huira/stars/io/star_catalog.hpp).
- **Building it yourself:** the catalog is built from the CDS files by `build_star_catalog`,
  in Huira's [`tools/`](https://github.com/huira-render/huira/tree/main/tools) directory,
  built with `-DHUIRA_BUILD_TOOLS=ON`. You only need this to change how the values are
  computed.
- **Cite:**
  - Høg, E., et al. 2000, "The Tycho-2 Catalogue of the 2.5 Million Brightest Stars",
    *Astronomy & Astrophysics*, 355, L27
  - Høg, E., et al. 2000, "Construction and verification of the Tycho-2 Catalogue",
    *Astronomy & Astrophysics*, 357, 367
  - and acknowledge CDS: "This research has made use of the VizieR catalogue access tool, CDS,
    Strasbourg, France (DOI: 10.26093/cds/vizier)."

## SPICE kernels

SPICE kernels hold ephemerides, orientations, frames and clocks. Huira does not host them: they
come straight from NASA's Navigation and Ancillary Information Facility (NAIF), which keeps them
at permanent addresses. The examples that use SPICE list the kernels they load, with links.

- **Get them:** NAIF's [generic kernels](https://naif.jpl.nasa.gov/pub/naif/generic_kernels/)
  (planetary and satellite ephemerides, planetary constants, the Earth's orientation), and
  missions' kernels in NAIF's [Planetary Data System archives](https://naif.jpl.nasa.gov/pub/naif/pds/).
- **License:** NAIF permits redistributing the kernels it distributes, as long as they are not
  modified ([NAIF's rules](https://naif.jpl.nasa.gov/naif/rules.html)).
- **Credit:** NAIF asks that SPICE, NAIF, and the teams that produced the data be acknowledged
  ([how to cite](https://naif.jpl.nasa.gov/naif/credit.html)). A kernel's comments name its
  producer, and often the papers to cite; JPL's planetary ephemeris DE440, for example, is
  Park, R. S., et al. 2021, "The JPL Planetary and Lunar Ephemerides DE440 and DE441",
  *The Astronomical Journal*, 161, 105. For SPICE and NAIF:
  - Acton, C. H. 1996, "Ancillary Data Services of NASA's Navigation and Ancillary Information
    Facility", *Planetary and Space Science*, 44(1), 65-70,
    [doi:10.1016/0032-0633(95)00107-7](https://doi.org/10.1016/0032-0633(95)00107-7)
  - Acton, C., Bachman, N., Semenov, B., and Wright, E., "A look toward the future in the
    handling of space science mission geometry", *Planetary and Space Science*,
    [doi:10.1016/j.pss.2017.02.013](https://doi.org/10.1016/j.pss.2017.02.013)
  - A mission's archive is cited by its DOI, which the PDS Engineering Node's DOI search finds
    from the archive's identifier (for example, `urn:nasa:pds:orex.spice` for OSIRIS-REx).

## Gateway model

NASA's 3D model of the Gateway core module.

- **Get it:** [media.huira.space/data/models/gateway/](https://media.huira.space/data/models/gateway/)
- **Source:** [NASA's 3D Resources](https://science.nasa.gov/3d-resources/), published there as
  `Gateway Core.glb`. Huira's copy is renamed `gateway.glb`, and otherwise unchanged.
- **License:** NASA's 3D resources are free to use and not copyrighted. NASA's
  [media usage guidelines](https://www.nasa.gov/nasa-brand-center/images-and-media/) apply: credit
  NASA, do not use the model to imply that NASA endorses anything, and do not use NASA's insignia.
- **Credit:** "Gateway model: NASA."

## Earth textures

8k textures of the Earth: its surface by day and by night, its clouds, a normal map and a
roughness map.

- **Get them:** [media.huira.space/data/textures/earth/](https://media.huira.space/data/textures/earth/)
- **Source:** the day, night, cloud and normal maps are from
  [Solar System Scope](https://www.solarsystemscope.com/textures/), based on NASA imagery. The
  roughness map (`8k_earth_roughness_map.tif`) was made for Huira by Christopher Gnam, by
  modifying Solar System Scope's specular map.  The city emission map (`8k_earth_ctiy_emission.jpg`) 
  was made for Huira by Christopher Gnam, by modifying Solar System Scope's Night map.
- **License:** [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/). The roughness map, an
  adaptation, is distributed under the same license.
- **Credit:** "Earth textures: Solar System Scope (solarsystemscope.com), CC BY 4.0. Roughness map
  adapted from its specular map." Images rendered with the textures should carry this credit too.

## Kernels that ship with Huira

Huira includes two of NAIF's generic kernels, in its `data/kernels/` folder:

- `lsk/naif0012.tls`: leap seconds, which Huira loads when it first converts a time.
- `pck/pck00011.tpc`: planetary constants (sizes and orientations), which
  `huira.spice.load_default_pck()` loads.

Both are unmodified, and redistributed under [NAIF's rules](https://naif.jpl.nasa.gov/naif/rules.html).
