"""
15 - Satellites

Demonstrates:
    - Reading a catalog of NORAD element sets, as TLEs or OMMs, from CelesTrak or Space-Track
      (ElementSetCatalog), and what it leaves out
    - Placing one satellite by its element set, with SGP4 (set_tle_position): the camera rides
      the ISS, in the scene's TEME frame
    - A constellation: every satellite in the catalog at once, each seen as a sunlit sphere,
      with the element sets it holds brought up to date from the catalog (new_constellation)
    - A satellite is placed once: the constellation leaves out the ISS, which the camera's node
      holds, and says so
    - Truth data for a point source: where each satellite in view is, from the scene view
    - A satellite SGP4 can't place during the exposure (decayed, most often) is left out of
      the view with a warning, rather than stopping it

Builds on: 13 SPICE, 14 Unresolved objects
Data: an Earth ephemeris (de440s.bsp), a catalog of element sets from CelesTrak, and the Earth's
daymap (see README.md)

Run:
    python satellites.py de440s.bsp active.csv /path/to/8k_earth_daymap.jpg

Writes:
    output/15_satellites.png   the sky ahead of the ISS, with the satellites in view as streaks
    output/15_satellites.csv   each satellite in view: its catalog number, name and pixel
"""

import csv
import math
import sys
from pathlib import Path

import numpy as np

import huira
from huira.rgb import Scene, SceneView, Renderer, rgb_to_spectral

from huira.units import Day as day
from huira.units import Kilometer as km
from huira.units import Meter as m
from huira.units import Millimeter as mm
from huira.units import Micrometer as um
from huira.units import Second as sec


def main():
    if len(sys.argv) != 4:
        sys.exit(
            "Usage: python satellites.py <de440s.bsp> <catalog> <8k_earth_daymap.jpg>\n"
            "README.md says where to get them."
        )
    planets_kernel, catalog_path, daymap_path = (Path(a) for a in sys.argv[1:])

    huira.enable_console_info()

    # SGP4 gives positions relative to the Earth, in the TEME frame, so rendering anything it
    # places needs the Earth's ephemeris: de440s, as in example 13. NAIF's generic PCK, which
    # ships with Huira, orients the Earth itself (its IAU_EARTH frame).
    huira.spice.furnsh(planets_kernel)
    huira.spice.load_default_pck()

    # A catalog of element sets, in any of the formats CelesTrak and Space-Track publish: TLEs,
    # with or without name lines, or OMMs as CSV, KVN or JSON (read() works the format out from
    # the text). Each entry is checked as SGP4 needs it. An entry that isn't an element set is
    # left out and reported, with why, rather than failing the read; and a satellite listed more
    # than once is kept once, with its newest element set. (One element set alone is read with
    # SatRec.from_tle(line1, line2) or SatRec.from_omm({...}).)
    catalog = huira.ElementSetCatalog.read(catalog_path)
    print(f"Read {len(catalog)} element sets, leaving out {len(catalog.skipped)} entries")
    for skipped in catalog.skipped[:5]:
        print(f"  entry {skipped.entry}, line {skipped.line}: {skipped.message}")

    # The ISS, by its NORAD catalog number
    iss = next((s for s in catalog.element_sets if s.catalog_number() == 25544), None)
    if iss is None:
        sys.exit("The catalog has no element set for the ISS (25544)")

    # The scene is viewed at the ISS's element set's epoch, when SGP4 places it best: an element
    # set is good to about a kilometer at its epoch, and drifts from there. The rest of the
    # catalog's element sets are hours to days from theirs.
    time = iss.epoch()
    print(f"The ISS's element set: {iss.name()}, epoch {time.to_utc_string()}")

    scene = Scene()

    # The Earth, placed and turned by SPICE as in example 13, lit by the Sun where it really is.
    # It hides the satellites behind it, and shades those in its shadow.
    surface = scene.new_material(scene.new_bsdf_lambertian())
    daymap = huira.srgb_to_linear(huira.read_image(daymap_path).image)
    surface.set_albedo_image(scene.add_texture(rgb_to_spectral(daymap.image)))
    earth_shape = scene.add_ellipsoid(km(6378.137), km(6378.137), km(6356.752))
    ecef = scene.root.new_spice_subframe("EARTH", "IAU_EARTH")
    ecef.new_instance(scene.add_primitive(earth_shape, surface))

    sun = scene.root.new_instance(scene.new_sun_light())
    sun.set_spice_origin("SUN")

    # One satellite, placed by its element set. SGP4 places things in the TEME frame, so a node
    # it places is a child of the scene's TEME frame: centered on the Earth, with the axes SGP4
    # uses. Whatever the node holds moves with it: here, the camera.
    teme = scene.teme_frame()
    iss_node = teme.new_subframe()
    iss_node.set_tle_position(iss)

    # Every other satellite, as a constellation. Each is placed by SGP4 as a node would be, but
    # without a node: a scene view places them all at once, in parallel. Each is seen as an
    # instance of one unresolved object; here, a sunlit Lambertian sphere 3 m in radius with an
    # albedo of 0.3, bright or faint by its phase and dark in the Earth's shadow (see example
    # 14 for the others, such as a fixed magnitude).
    body = scene.new_unresolved_sphere(m(3), sun, 0.3)
    constellation = scene.new_constellation(body, "catalog")

    # load() reads the catalog again, with options: here, only element sets within a month of
    # the time viewed, which leaves out stale ones for satellites that have since decayed. It
    # adds the satellites new to the constellation, and brings up to date those it holds, so
    # that a newer catalog can be loaded over it. A satellite is placed once: the ISS, which the
    # camera's node holds, is left out and reported as CLAIMED. (add() takes SatRecs directly.)
    report = constellation.load(catalog_path, reference_time=time, max_age=day(30))
    print(
        f"The constellation holds {len(constellation)} satellites: {report.added} added, "
        f"{len(report.skipped)} left out"
    )
    for skipped in report.skipped:
        if skipped.reason == huira.SkipReason.CLAIMED:
            print(f"  {skipped.catalog_number}: {skipped.message}")

    # The camera looks ahead along the ISS's orbit, tilted 12 degrees down, so that the Earth's
    # limb, 20 degrees below the horizontal at the ISS's height, is in the lower part of the
    # frame. The TLE node keeps the TEME frame's axes, so the camera's rotation is built there,
    # from the ISS's position and velocity at the time viewed: the camera's +z axis looks out,
    # and its +y axis points down the image (see example 01).
    position, velocity = (v.to_numpy() for v in iss.evaluate(time))
    forward = velocity / np.linalg.norm(velocity)
    down = -position / np.linalg.norm(position)
    tilt = math.radians(12)
    look = forward * math.cos(tilt) + down * math.sin(tilt)
    image_down = down * math.cos(tilt) - forward * math.sin(tilt)
    camera_model = scene.new_camera_model()
    camera_model.set_focal_length(mm(35))
    camera_model.configure_sensor_from_pitch((1920, 1200), um(10))
    camera_model.set_fstop(1.4)
    camera_model.set_sensor_bit_depth(12)
    camera_model.enable_sensor_noise(False)
    camera_model.precompute()
    camera = iss_node.new_instance(camera_model)
    camera.set_rotation(
        huira.Rotation.from_basis_vectors(np.cross(image_down, look), image_down, look)
    )

    # A second's exposure, at f/1.4: satellites a few hundred kilometers away, faint as they are,
    # cross a few pixels, and are drawn as streaks, each with the temporal samples its own motion
    # needs, while the Earth below smears. A satellite SGP4 can't place during the exposure, one
    # that has decayed since its element set most often, is left out of the view with a warning,
    # and listed by skipped_satellites(); a node placed by a TLE, asked for by name,
    # throws instead.
    exposure = huira.Interval.from_centered(time, sec(1))
    scene_view = SceneView(scene, exposure, camera)
    for skipped in scene_view.skipped_satellites():
        print(f"  left out of the view, {skipped.catalog_number}: {skipped.message}")

    renderer = Renderer()
    renderer.set_samples_per_pixel(64)
    frame_buffer = camera_model.make_frame_buffer()
    frame_buffer.enable_sensor_response()
    renderer.render(scene_view, frame_buffer)

    output_dir = Path("output")
    output_dir.mkdir(exist_ok=True)
    huira.write_png(
        output_dir / "15_satellites.png", huira.linear_to_srgb(frame_buffer.sensor_response)
    )

    # Truth data: the scene view holds every satellite in front of the camera, with where it is
    # in the camera's frame at each of its temporal samples. Projected through the camera model,
    # that is its pixel; those outside the field of view project to nan. Each satellite's
    # irradiance at the camera is there too, as the view resolved it.
    in_view = 0
    with open(output_dir / "15_satellites.csv", "w", newline="") as file:
        writer = csv.writer(file)
        writer.writerow(["catalog_number", "name", "x", "y"])
        for instance in scene_view.unresolved_instances():
            if instance.constellation_id != constellation.id():
                continue
            middle = instance.positions[len(instance.positions) // 2]
            x, y = camera_model.try_project_point(middle)
            if math.isnan(x):
                continue
            name = constellation.element_set(instance.catalog_number).name()
            writer.writerow([instance.catalog_number, name, f"{x:.2f}", f"{y:.2f}"])
            in_view += 1
    print(f"{in_view} satellites in the field of view; see output/15_satellites.csv")


if __name__ == "__main__":
    main()
