"""
07 - Clouds and atmosphere

Demonstrates:
    - Alpha: a texture that makes parts of a surface transparent (the clouds)
    - Layered shells: one geometry inside another, each with its own material
    - Participating media: a constant density field (absorption and scattering), an isotropic
      phase function, and a medium made of the two
    - A primitive with both a material and a medium; the null BSDF and a transmission factor,
      for a boundary that light crosses unchanged

Builds on: 06 Materials and textures
Data: the Earth's daymap and cloud map (see README.md)

Run:
    python clouds_and_atmosphere.py 8k_earth_daymap.jpg 8k_earth_clouds.jpg

Writes:
    output/07_clouds_and_atmosphere.png

Note: volumetric rendering is still noisy; improvements are coming.
"""

import sys
from pathlib import Path

import huira
from huira.rgb import Scene, SceneView, Renderer, SpectralBins, rgb_to_spectral

from huira.units import Meter as m
from huira.units import Kilometer as km
from huira.units import Millimeter as mm
from huira.units import Degree as deg
from huira.units import AstronomicalUnit as au
from huira.units import Microsecond as us

EQUATORIAL_RADIUS_KM = 6378.137
POLAR_RADIUS_KM = 6356.752


def earth_shell(scene, altitude_km):
    """An ellipsoid the Earth's shape, altitude_km above its surface."""
    a = km(EQUATORIAL_RADIUS_KM + altitude_km)
    b = km(POLAR_RADIUS_KM + altitude_km)
    return scene.add_ellipsoid(a, a, b)


def main():
    if len(sys.argv) != 3:
        sys.exit(
            "Usage: python clouds_and_atmosphere.py <daymap> <cloud_map>\n"
            "README.md says where to get them."
        )
    daymap_path, cloud_map_path = Path(sys.argv[1]), Path(sys.argv[2])

    scene = Scene()

    # The surface: the daymap, on a Lambertian BSDF. (Example 06 builds the full material.)
    surface = scene.new_material(scene.new_bsdf_lambertian())
    daymap_srgb = huira.read_image(daymap_path)
    daymap = huira.srgb_to_linear(daymap_srgb)  # JPEG textures are in sRGB color space
    surface.set_albedo_image(scene.add_texture(rgb_to_spectral(daymap.image)))
    scene.root.new_instance(scene.add_primitive(earth_shell(scene, 0), surface))

    # The clouds are a second shell, 6 km up, with a white Lambertian material. Its alpha image
    # (the cloud map, read as one channel) says how opaque each point is: where it is 0, light
    # passes through to the surface below.
    cloud_material = scene.new_material(scene.new_bsdf_lambertian())
    cloud_map = huira.read_image_mono(cloud_map_path)
    cloud_material.set_alpha_image(scene.add_texture(cloud_map.image))
    scene.root.new_instance(scene.add_primitive(earth_shell(scene, 6), cloud_material))

    # The atmosphere is a medium: a volume that absorbs and scatters light along the paths
    # through it. A medium is a density field (how much it absorbs and scatters, per meter, in
    # each spectral bin) with a phase function (which way scattered light goes). This one is
    # uniform, scattering blue more than red, and scatters equally in all directions.
    absorption = SpectralBins(0.0)  # per meter
    scattering = SpectralBins([0.5e-6, 1.0e-6, 2.0e-6])  # per meter (red, green, blue)
    density = scene.new_constant_density_field(absorption, scattering)
    phase_function = scene.new_isotropic_phase_function()
    air = scene.new_medium(density, phase_function)

    # The medium fills a shell 100 km up. Its surface has the null BSDF, which reflects nothing,
    # and a transmission of 1, so light crosses the boundary unchanged and only the medium
    # inside acts on it.
    boundary = scene.new_material(scene.new_bsdf_null())
    boundary.set_transmission_factor(SpectralBins(1.0))
    scene.root.new_instance(scene.add_primitive(earth_shell(scene, 100), boundary, air))

    sun = scene.root.new_instance(scene.new_sun_light())
    sun.set_position(au(0.866), au(0.5), m(0))

    camera_model = scene.new_camera_model()
    camera_model.use_blender_convention()
    camera_model.set_focal_length(mm(25))
    camera_model.configure_sensor_from_size((1080, 1080), mm(6))
    camera_model.set_fstop(5)
    camera_model.set_sensor_bit_depth(12)
    camera_model.set_sensor_conversion_gain(1)
    camera_model.set_sensor_quantum_efficiency(0.8)
    camera_model.set_sensor_full_well_capacity(20000)
    camera_model.enable_sensor_noise(False)

    camera = scene.root.new_instance(camera_model)
    camera.set_position(km(100000), m(0), m(0))
    camera.set_euler_angles(deg(90), deg(0), deg(90))

    time = huira.Time("2026-01-01T00:00:00")
    exposure = huira.Interval.from_start(time, us(25))
    scene_view = SceneView(scene, exposure, camera, huira.ObservationMode.GEOMETRIC_STATE)

    frame_buffer = camera_model.make_frame_buffer()
    frame_buffer.enable_sensor_response()

    # Light that scatters in the atmosphere needs bounces to reach the camera, and volumes need
    # more samples than surfaces to settle.
    renderer = Renderer()
    renderer.set_samples_per_pixel(256)
    renderer.set_max_bounces(4)

    camera_model.precompute()
    renderer.render(scene_view, frame_buffer)

    output_dir = Path("output")
    output_dir.mkdir(exist_ok=True)
    huira.write_png(
        output_dir / "07_clouds_and_atmosphere.png",
        huira.linear_to_srgb(frame_buffer.sensor_response),
    )


if __name__ == "__main__":
    main()
