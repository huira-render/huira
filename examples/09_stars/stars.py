"""
09 - Stars

Demonstrates:
    - Loading stars, down to a faintest magnitude, with their positions at an epoch
    - The scene's axes: x, y and z of the J2000 (ICRF) frame, so a right ascension and
      declination give a direction
    - Pointing a camera at a direction with look_at, with celestial north up
    - Exposing for stars: seconds rather than milliseconds, and a fast lens
    - Dynamic stars: moving the catalog to another epoch with update_star_epoch, without
      reloading it (Barnard's star, which moves fastest across the sky, over a century)

Builds on: 01 First render
Data: tycho2.hrsc, the Tycho-2 star catalog (see README.md)

Run:
    python stars.py /path/to/tycho2.hrsc

Writes:
    output/09_stars.png                 Orion
    output/09_stars_barnard_2000.png    Barnard's star in 2000...
    output/09_stars_barnard_2100.png    ...and in 2100
"""

import math
import sys
from pathlib import Path

import huira
from huira.rgb import Scene, SceneView, Renderer

from huira.units import Millimeter as mm
from huira.units import Micrometer as um
from huira.units import Second as sec


def direction(ra_deg, dec_deg):
    """The unit vector toward a right ascension and declination, in the J2000 frame."""
    ra = math.radians(ra_deg)
    dec = math.radians(dec_deg)
    return huira.Vec3(math.cos(dec) * math.cos(ra), math.cos(dec) * math.sin(ra), math.sin(dec))


def render(scene, camera, camera_model, exposure, filename):
    scene_view = SceneView(scene, exposure, camera, huira.ObservationMode.GEOMETRIC_STATE)
    frame_buffer = camera_model.make_frame_buffer()
    frame_buffer.enable_sensor_response()
    Renderer().render(scene_view, frame_buffer)

    output_dir = Path("output")
    output_dir.mkdir(exist_ok=True)
    huira.write_png(output_dir / filename, huira.linear_to_srgb(frame_buffer.sensor_response))


def orion(catalog):
    scene = Scene()

    # load_stars reads the catalog, keeping the stars down to the faintest magnitude given, and
    # moves each by its proper motion to the time given. Each star's spectrum comes from its
    # temperature, so stars have their colors.
    time = huira.Time("2026-01-01T00:00:00")
    scene.load_stars(catalog, time, 11.0)

    # A 35 mm lens at f/2.8 on a sensor of 10 um pixels: about 25 x 16 degrees.
    camera_model = scene.new_camera_model()
    camera_model.set_focal_length(mm(35))
    camera_model.set_fstop(2.8)
    camera_model.configure_sensor_from_pitch((1536, 1024), um(10))

    # The scene's axes are those of J2000, so a direction from right ascension and declination
    # can be looked at directly (look_at takes a position; a unit vector is a point 1 m away).
    # Up is the celestial north pole, +z. Orion's belt is at about RA 84, Dec -1 degrees:
    camera = scene.root.new_instance(camera_model)
    camera.look_at(direction(84.0, -1.0), huira.Vec3(0, 0, 1))

    # Stars are faint: expose for seconds.
    render(scene, camera, camera_model, huira.Interval.from_centered(time, sec(2)),
           "09_stars.png")


def barnards_star(catalog):
    scene = Scene()

    # load_dynamic_stars keeps the catalog's proper motions, so update_star_epoch can move every
    # star to another time later, without reading the catalog again.
    year_2000 = huira.Time("2000-01-01T12:00:00")
    year_2100 = huira.Time("2100-01-01T12:00:00")
    scene.load_dynamic_stars(catalog, year_2000, 12.0)

    # A 300 mm lens: about 1 degree across. Barnard's star (RA 269.45, Dec 4.69 in 2000) moves
    # about 10 arcseconds a year, so a century carries it 17 arcminutes north: about 150 pixels.
    # The camera points halfway along that path.
    camera_model = scene.new_camera_model()
    camera_model.set_focal_length(mm(300))
    camera_model.set_fstop(4)
    camera_model.configure_sensor_from_pitch((512, 512), um(10))

    camera = scene.root.new_instance(camera_model)
    camera.look_at(direction(269.45, 4.83), huira.Vec3(0, 0, 1))

    render(scene, camera, camera_model, huira.Interval.from_centered(year_2000, sec(5)),
           "09_stars_barnard_2000.png")

    scene.update_star_epoch(year_2100)
    render(scene, camera, camera_model, huira.Interval.from_centered(year_2100, sec(5)),
           "09_stars_barnard_2100.png")


def main():
    if len(sys.argv) != 2:
        sys.exit("Usage: python stars.py <tycho2.hrsc>\nREADME.md says where to get it.")
    catalog = Path(sys.argv[1])

    orion(catalog)
    barnards_star(catalog)


if __name__ == "__main__":
    main()
