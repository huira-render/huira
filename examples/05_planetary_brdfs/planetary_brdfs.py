"""
05 - Planetary BRDFs

Demonstrates:
    - The BSDFs made for planetary surfaces, side by side on spheres of the same albedo:
      Lambertian, Oren-Nayar (with a roughness), Lommel-Seeliger, McEwen and Hapke
    - Hapke's parameters: opposition surge width and amplitude, and the phase function's
      asymmetry and forward/backward fraction
    - How the phase angle (Sun-target-camera) changes each one: rendered near opposition, where
      regolith brightens sharply, and at 90 degrees, where limb darkening differs most
    - Moving a light between renders of the same scene

Builds on: 02 Scene graph
Data: none

Run:
    python planetary_brdfs.py

Writes:
    output/05_planetary_brdfs.png              at a 90 degree phase angle
    output/05_planetary_brdfs_opposition.png   at a 5 degree phase angle
"""

import math
from pathlib import Path

import huira
from huira.rgb import Scene, SceneView, Renderer, SpectralBins

from huira.units import Meter as m
from huira.units import Millimeter as mm
from huira.units import AstronomicalUnit as au
from huira.units import Microsecond as us


def main():
    scene = Scene()
    sphere = scene.add_ellipsoid(m(1), m(1), m(1))

    # The planetary BSDFs, left to right. Each takes the albedo from its material:
    #   - Lambertian: ideal diffuse; brightness depends only on the incidence angle
    #   - Oren-Nayar: rough diffuse, with the roughness from the material (0 is Lambertian)
    #   - Lommel-Seeliger: single scattering in a dark, dusty medium; no limb darkening
    #   - McEwen: blends Lommel-Seeliger and Lambert with phase angle, as fits the Moon
    #   - Hapke: the five-parameter regolith model. h and B0 are the width and amplitude of the
    #     opposition surge; b and c the asymmetry and forward/backward fraction of its
    #     double Henyey-Greenstein phase function. (These values are illustrative, of the order
    #     reported for lunar regolith.) Its albedo is the single scattering albedo.
    bsdfs = [
        ("lambertian", scene.new_bsdf_lambertian()),
        ("oren_nayar", scene.new_bsdf_oren_nayar()),
        ("lommel_seeliger", scene.new_bsdf_lommel_seeliger()),
        ("mcewen", scene.new_bsdf_mcewen()),
        ("hapke", scene.new_bsdf_hapke(h=0.06, B0=1.0, b=0.25, c=0.6)),
    ]

    spacing = 2.5
    for i, (name, bsdf) in enumerate(bsdfs):
        material = scene.new_material(bsdf, name)
        material.set_albedo_factor(SpectralBins(0.3))
        if name == "oren_nayar":
            material.set_roughness_factor(0.5)

        primitive = scene.add_primitive(sphere, material, name)
        instance = scene.root.new_instance(primitive)
        instance.set_position(m((i - 2) * spacing), m(0), m(0))

    # A long lens from far away, so every sphere is seen from nearly the same direction:
    camera_model = scene.new_camera_model()
    camera_model.set_focal_length(mm(100))
    camera_model.configure_sensor_from_size((1600, 400), mm(36))
    camera_model.set_fstop(16)

    camera = scene.root.new_instance(camera_model)
    camera.set_position(m(0), m(0), m(-40))

    sun = scene.root.new_instance(scene.new_sun_light())

    time = huira.Time("2026-01-01T00:00:00")
    exposure = huira.Interval.from_start(time, us(400))

    renderer = Renderer()
    renderer.set_samples_per_pixel(64)

    output_dir = Path("output")
    output_dir.mkdir(exist_ok=True)

    # The camera looks along +z, with +y down the image. The Sun, 1 AU away in the y-z plane and
    # phase_angle away from straight behind the camera, lights the spheres from above at that
    # phase angle (from the side, along the row, each sphere would shadow the next). A scene can
    # be rendered again after changes: each SceneView takes the scene as it is when it is made.
    for phase_angle_deg, filename in [(90, "05_planetary_brdfs.png"),
                                      (5, "05_planetary_brdfs_opposition.png")]:
        phase = math.radians(phase_angle_deg)
        sun.set_position(m(0), au(-math.sin(phase)), au(-math.cos(phase)))

        scene_view = SceneView(scene, exposure, camera, huira.ObservationMode.GEOMETRIC_STATE)
        frame_buffer = camera_model.make_frame_buffer()
        frame_buffer.enable_sensor_response()
        renderer.render(scene_view, frame_buffer)

        huira.write_png(output_dir / filename, huira.linear_to_srgb(frame_buffer.sensor_response))


if __name__ == "__main__":
    main()
