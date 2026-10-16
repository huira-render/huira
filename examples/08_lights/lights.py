"""
08 - Lights

Demonstrates:
    - Sphere lights, given by their power in each spectral bin (or their total power, in W)
    - Light adds up: three colored lights, and the colors of the shadows where some are blocked
    - Several lights in one scene. Huira samples each light directly at every bounce (next event
      estimation), and weighs that against the BSDF's own samples (multiple importance
      sampling), so several lights converge as quickly as one
    - Indirect sources: marking a sunlit body as a reflector to sample, for planetshine
    - Focusing the camera at a distance (example 11 covers focus)

Builds on: 05 Planetary BRDFs
Data: none

Run:
    python lights.py

Writes:
    output/08_lights.png               three colored lights over three spheres
    output/08_lights_planetshine.png   a satellite's night side, lit by the planet below it
"""

from pathlib import Path

import huira
from huira.rgb import Scene, SceneView, Renderer, SpectralBins, SpectralWatts

from huira.units import Meter as m
from huira.units import Kilometer as km
from huira.units import Millimeter as mm
from huira.units import AstronomicalUnit as au
from huira.units import Millisecond as ms


def render(scene, camera, camera_model, exposure, filename, samples):
    scene_view = SceneView(scene, exposure, camera, huira.ObservationMode.GEOMETRIC_STATE)
    frame_buffer = camera_model.make_frame_buffer()
    frame_buffer.enable_sensor_response()

    renderer = Renderer()
    renderer.set_samples_per_pixel(samples)
    renderer.set_max_bounces(4)
    camera_model.precompute()
    renderer.render(scene_view, frame_buffer)

    output_dir = Path("output")
    output_dir.mkdir(exist_ok=True)
    huira.write_png(output_dir / filename, huira.linear_to_srgb(frame_buffer.sensor_response))


def colored_lights():
    scene = Scene()
    lambertian = scene.new_bsdf_lambertian()

    # A floor: an ellipsoid flattened into a disk 20 m across and 10 cm thick.
    floor_material = scene.new_material(lambertian)
    floor_material.set_albedo_factor(SpectralBins(0.5))
    floor = scene.add_ellipsoid(m(10), m(10), m(0.05))
    scene.root.new_instance(scene.add_primitive(floor, floor_material))

    # Three white spheres resting on it:
    ball_material = scene.new_material(lambertian)
    ball_material.set_albedo_factor(SpectralBins(0.6))
    ball = scene.add_primitive(scene.add_ellipsoid(m(0.5), m(0.5), m(0.5)), ball_material)
    for x in (-1.5, 0.0, 1.5):
        scene.root.new_instance(ball).set_position(m(x), m(0), m(0.5))

    # A sphere light is a glowing sphere: give it a radius and the power it emits. Here the
    # power is given in each spectral bin (red, green, blue), so each light emits in one bin
    # only: one red, one green and one blue light, 300 W each, in a row behind the spheres and
    # above the row of spheres. (A total power, huira.units.Watt(300), is spread evenly over
    # the bins: a white light.) They are small, 5 cm, so their shadows are sharp; a larger light
    # casts softer ones.
    colors = [(-2.5, [300.0, 0.0, 0.0]), (0.0, [0.0, 300.0, 0.0]), (2.5, [0.0, 0.0, 300.0])]
    for x, power in colors:
        light = scene.new_sphere_light(m(0.05), SpectralWatts(SpectralBins(power)))
        scene.root.new_instance(light).set_position(m(x), m(4), m(1.75))

    # Light adds up. Where all three lights reach the floor, it is white. Each sphere casts a
    # shadow from each light, toward the camera, and only the other two lights reach into it:
    # the red light's shadow is cyan (green and blue), the green light's magenta, and the blue
    # light's yellow. Where two shadows overlap, one light is left, and where all three overlap,
    # none: black.

    camera_model = scene.new_camera_model()
    camera_model.use_blender_convention()
    camera_model.set_focal_length(mm(35))
    camera_model.configure_sensor_from_size((1280, 800), mm(36))
    camera_model.set_fstop(4)

    # At f/4 the depth of field is shallow, and the camera focuses at infinity by default:
    # focus on the spheres instead.
    camera_model.set_focus_distance(m(8))

    camera = scene.root.new_instance(camera_model)
    camera.set_position(m(0), m(-7), m(4))
    camera.look_at(huira.Vec3(0, 0, 0.5), huira.Vec3(0, 0, 1))

    time = huira.Time("2026-01-01T00:00:00")
    render(scene, camera, camera_model, huira.Interval.from_start(time, ms(5)),
           "08_lights.png", samples=256)


def planetshine():
    scene = Scene()
    lambertian = scene.new_bsdf_lambertian()

    # A planet 6000 km in radius, and a satellite 1 m in radius, 500 km above its north pole:
    planet_material = scene.new_material(lambertian)
    planet_material.set_albedo_factor(SpectralBins(0.3))
    planet_shape = scene.add_ellipsoid(km(6000), km(6000), km(6000))
    planet = scene.root.new_instance(scene.add_primitive(planet_shape, planet_material))

    satellite_material = scene.new_material(lambertian)
    satellite_material.set_albedo_factor(SpectralBins(0.8))
    satellite_shape = scene.add_ellipsoid(m(1), m(1), m(1))
    satellite = scene.root.new_instance(scene.add_primitive(satellite_shape, satellite_material))
    satellite.set_position(m(0), m(0), km(6500))

    # The Sun lights the planet's +x half, and the satellite's +x side.
    sun = scene.root.new_instance(scene.new_sun_light())
    sun.set_position(au(1), m(0), m(0))

    # The satellite's other side is lit only by sunlight reflected off the planet. Marking the
    # planet as an indirect source makes the renderer sample it like a light (it emits nothing
    # itself), as it does the Sun. That matters most when the reflector is small in the sky,
    # such as the Moon seen from the Earth, where paths would rarely find it by chance.
    planet.set_indirect_source(True)

    # The camera looks up at the satellite from below its night side:
    camera_model = scene.new_camera_model()
    camera_model.use_blender_convention()
    camera_model.set_focal_length(mm(50))
    camera_model.configure_sensor_from_size((1024, 1024), mm(36))
    camera_model.set_fstop(16)

    camera = scene.root.new_instance(camera_model)
    camera.set_position(m(-4), m(-4), m(6500e3 - 3))
    camera.look_at(satellite, huira.Vec3(0, 0, 1))

    time = huira.Time("2026-01-01T00:00:00")
    render(scene, camera, camera_model, huira.Interval.from_start(time, ms(1)),
           "08_lights_planetshine.png", samples=256)


def main():
    colored_lights()
    planetshine()


if __name__ == "__main__":
    main()
