"""
13 - SPICE

Demonstrates:
    - Loading SPICE kernels (huira.spice.furnsh): an ephemeris (SPK) and a binary PCK for the
      Earth's orientation. Huira loads its own leap seconds kernel when it first needs one.
    - SPICE frames in the scene graph (new_spice_subframe): an Earth-centered inertial frame
      (J2000) and an Earth-fixed one (ITRF93) that turns with the Earth
    - Placing an instance by ephemeris (set_spice_origin): the Sun, where it really was
    - Placing a node on a Keplerian orbit (set_keplerian_orbit), with no kernel for it: the
      camera, on a satellite orbiting the Earth, kept pointed at the Earth with look_at
    - Time driving the scene: the same scene rendered at two times, six hours apart

Builds on: 06 Materials and textures
Data: two SPICE kernels and the Earth's daymap (see README.md)

Run:
    python spice.py de440s.bsp earth_latest_high_prec.bpc 8k_earth_daymap.jpg

Writes:
    output/13_spice.png             2019-02-06 10:27 UTC
    output/13_spice_6h_later.png    six hours later
"""

import sys
from pathlib import Path

import huira
from huira.rgb import Scene, SceneView, Renderer, rgb_to_spectral

from huira.units import Kilometer as km
from huira.units import Millimeter as mm
from huira.units import Degree as deg
from huira.units import Hour as hr
from huira.units import Microsecond as us


def main():
    if len(sys.argv) != 4:
        sys.exit(
            "Usage: python spice.py <de440s.bsp> <earth_latest_high_prec.bpc> <daymap>\n"
            "README.md says where to get them."
        )
    planets_kernel, earth_kernel, daymap_path = (Path(a) for a in sys.argv[1:])

    # SPICE kernels hold ephemerides, orientations, frames and clocks. de440s gives the positions
    # of the Sun, Moon and planets; earth_latest_high_prec gives the Earth's precise orientation,
    # for the ITRF93 frame. (huira.spice.load_default_pck() loads NAIF's generic PCK, which ships
    # with Huira: its IAU_EARTH frame needs no download, but is less precise.)
    huira.spice.furnsh(planets_kernel)
    huira.spice.furnsh(earth_kernel)

    scene = Scene()

    # A SPICE subframe takes its origin and orientation from SPICE, at whatever time the scene is
    # viewed. Both of these are centered on the Earth: eci keeps the J2000 orientation, and ecef
    # turns with the Earth.
    eci = scene.root.new_spice_subframe("EARTH", "J2000")
    ecef = scene.root.new_spice_subframe("EARTH", "ITRF93")

    # The Earth is built in the Earth-fixed frame, so its surface turns with it. (Example 06
    # builds the full material; the daymap alone is enough here. Its colors are sRGB-encoded, so
    # they are made linear first, as in example 06.)
    surface = scene.new_material(scene.new_bsdf_lambertian())
    daymap = huira.srgb_to_linear(huira.read_image(daymap_path).image)
    surface.set_albedo_image(scene.add_texture(rgb_to_spectral(daymap.image)))
    earth_shape = scene.add_ellipsoid(km(6378.137), km(6378.137), km(6356.752))
    earth = ecef.new_instance(scene.add_primitive(earth_shape, surface))

    # The Sun's position comes from the ephemeris, rather than set by hand:
    sun = scene.root.new_instance(scene.new_sun_light())
    sun.set_spice_origin("SUN")

    camera_model = scene.new_camera_model()
    camera_model.use_blender_convention()
    camera_model.set_focal_length(mm(25))
    camera_model.configure_sensor_from_size((1080, 1080), mm(6))
    camera_model.set_fstop(5)
    camera_model.set_sensor_bit_depth(12)
    camera_model.set_sensor_conversion_gain(1.0)
    camera_model.set_sensor_quantum_efficiency(0.8)
    camera_model.set_sensor_full_well_capacity(20000)
    camera_model.enable_sensor_noise(False)

    # The camera rides a satellite on a circular orbit 100,000 km from the Earth's center,
    # inclined 30 degrees to the equator. set_keplerian_orbit places a node by its orbital
    # elements about its parent, here the inertial frame: no kernel is needed. The elements are
    # the semi-major axis, eccentricity, inclination, right ascension of the ascending node,
    # argument of periapsis and mean anomaly at an epoch, and the central body's gravitational
    # parameter, in km^3/s^2. The orbit takes about 87 hours, so in six hours the satellite
    # moves about 25 degrees along it.
    time = huira.Time("2019-02-06T10:27:00")
    camera = eci.new_instance(camera_model)
    camera.set_keplerian_orbit(km(100000), 0.0, deg(30), deg(0), deg(0), deg(0), time, 398600.4418)

    # A node can also be placed by a table of times and positions (set_position_table), as from
    # a trajectory file, or by any function of time (set_position_function), and turned the
    # same ways (set_rotation_table, set_rotation_function).

    # look_at keeps the camera pointed at the Earth as both move, with celestial north up:
    camera.look_at(earth, huira.Vec3(0, 0, 1))

    renderer = Renderer()
    renderer.set_samples_per_pixel(64)
    camera_model.precompute()

    output_dir = Path("output")
    output_dir.mkdir(exist_ok=True)

    # Every SPICE-driven frame and instance, and every orbit, is evaluated at the scene view's
    # time. Six hours later, the Earth has turned a quarter of the way around, the satellite has
    # moved along its orbit, and the Sun has hardly moved.
    renders = [
        (time, "13_spice.png"),
        (time + hr(6), "13_spice_6h_later.png"),
    ]
    for when, filename in renders:
        exposure = huira.Interval.from_start(when, us(25))
        scene_view = SceneView(scene, exposure, camera)
        frame_buffer = camera_model.make_frame_buffer()
        frame_buffer.enable_sensor_response()
        renderer.render(scene_view, frame_buffer)
        huira.write_png(output_dir / filename, huira.linear_to_srgb(frame_buffer.sensor_response))


if __name__ == "__main__":
    main()
