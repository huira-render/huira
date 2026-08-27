import sys
import os
from pathlib import Path
from time import perf_counter
import numpy as np

import huira
from huira.rgb import SpectralBins, Scene, SceneView, Renderer, rgb_to_spectral

from huira.units import Meter as m
from huira.units import Kilometer as km
from huira.units import Millimeter as mm
from huira.units import Degree as deg
from huira.units import Watt as W
from huira.units import Second as sec
from huira.units import AstronomicalUnit as au

from huira.units import Diopter as dpt


def parse_input_paths():
    if len(sys.argv) != 3:
        print("Usage: python earth_star_field.py <assets_path> <tycho2.hrsc_path>")
        sys.exit(1)
    assets_path = Path(sys.argv[1])
    star_catalog_path = Path(sys.argv[2])
    return assets_path, star_catalog_path


def main():
    # Parse input paths
    assets_path, star_catalog_path = parse_input_paths()
    kernels_path = assets_path / "kernels/"
    textures_path = assets_path / "textures/"

    # Enable logger output
    huira.set_log_level(huira.LogLevel.Debug)
    huira.enable_console_debug(True)

    # Load the required SPICE kernels
    huira.spice.furnsh(kernels_path / "spk/de440s.bsp")
    huira.spice.furnsh(kernels_path / "pck/earth_latest_high_prec.bpc")
    huira.spice.furnsh(kernels_path / "pck/earth_fixed.tf")

    # Create the scene
    scene = Scene()

    # Faint non-zero background so the sky isn't perfectly black (from star_field.py)
    scene.set_background_radiance(1e-5)

    eci = scene.root.new_spice_subframe("EARTH", "J2000")
    ecef = scene.root.new_spice_subframe("EARTH", "ITRF93")

    # Set the observation time.  Named obs_time rather than time so it doesn't
    # shadow anything from the timing import.
    obs_time = huira.Time("2019-02-06T10:27:00")

    # NOTE: this exposure is tuned for a sunlit Earth.  Stars will collect
    # essentially no signal at 50 us -- raise this (star_field.py used ~10 s)
    # if you want them visible, and expect the Earth to blow out.
    exposure_duration = sec(0.1)
    exposure = huira.Interval(obs_time, obs_time + exposure_duration)

    # Load the star catalog.  Proper motion is propagated to the epoch, so this
    # has to come after the observation time is defined.
    scene.load_stars(star_catalog_path, obs_time)

    # Configure a camera model
    camera_model = scene.new_camera_model()
    camera_model.set_focal_length(mm(25))
    camera_model.configure_sensor_from_size((1080, 1080), mm(6))

    # Set camera exposure settings
    camera_model.set_fstop(2)
    camera_model.set_sensor_conversion_gain(0.3)
    camera_model.set_sensor_bit_depth(12)
    camera_model.set_sensor_quantum_efficiency(0.8)
    camera_model.set_sensor_full_well_capacity(20000)
    camera_model.set_sensor_simulate_noise(False)

    camera_model.use_aperture_psf(16, 16)
    camera_model.set_psf_convolution_radius(1024)
    camera_model.enable_psf_convolution()
    camera_model.set_veiling_glare(0.01)
    camera_model.set_harvey_shack_scatter(0.01, 2.5, 0.5)
    camera_model.set_diopters(dpt(0.005))

    # Stars are point sources, so an aperture PSF spreads them over more than a
    # single pixel.  Uncomment to enable (also adds diffraction to Earth's limb).
    # camera_model.use_aperture_psf(32, 16)

    # Huira uses the OpenCV convention by default, which is
    # +z forward, +y down.  Blender uses -z forward, +y up.
    # This flag allows you to match Blender's for easier
    # comparison with blender generated images.
    camera_model.use_blender_convention()

    # Create the Earth material
    ct_bsdf = scene.new_bsdf_cook_torrance()
    earth_material = scene.new_material(ct_bsdf)

    earth_albedo_rgb = huira.read_image(textures_path / "8k_earth_daymap.jpg")
    earth_albedo_spec = rgb_to_spectral(earth_albedo_rgb.image)
    earth_albedo_tex = scene.add_texture(earth_albedo_spec)
    earth_material.set_albedo_image(earth_albedo_tex)

    earth_roughness = huira.read_image_mono(textures_path / "8k_earth_roughness_map.tif")
    earth_roughness_tex = scene.add_texture(earth_roughness.image)
    earth_material.set_roughness_image(earth_roughness_tex)
    earth_material.set_metallic_factor(0)

    earth_normal = huira.read_image(textures_path / "8k_earth_normal_map.tif")
    earth_normal_tex = scene.add_normal_texture(earth_normal.image)
    earth_material.set_normal_image(earth_normal_tex)

    # Create the Earth's surface
    R_e = km(6378.137)
    earth_ellipsoid = scene.add_ellipsoid(R_e, R_e, R_e)
    earth_primitive = scene.add_primitive(earth_ellipsoid, earth_material)
    ecef.new_instance(earth_primitive)

    # Create the Earth's clouds
    lam_bsdf = scene.new_bsdf_lambertian()
    earth_clouds_material = scene.new_material(lam_bsdf)
    earth_clouds_alpha = huira.read_image_mono(textures_path / "8k_earth_clouds.jpg")
    earth_clouds_tex = scene.add_texture(earth_clouds_alpha.image)
    earth_clouds_material.set_alpha_image(earth_clouds_tex)

    R_c = R_e + km(6)
    earth_clouds_ellipsoid = scene.add_ellipsoid(R_c, R_c, R_c)
    earth_clouds = scene.add_primitive(earth_clouds_ellipsoid, earth_clouds_material)
    ecef.new_instance(earth_clouds)

    ecef.set_visible(False)

    # # Create the Earth's atmosphere
    # null_bsdf = scene.new_bsdf_null()
    # atmosphere_material = scene.new_material(null_bsdf)
    # atmosphere_material.set_transmission_factor(SpectralBins(1))

    # constant_density_field = scene.new_constant_density_field(SpectralBins(0), SpectralBins(2e-6))
    # isotropic_phase_function = scene.new_isotropic_phase_function()
    # atmosphere_medium = scene.new_medium(constant_density_field, isotropic_phase_function)

    # R_a = R_e + km(100)
    # atmosphere_ellipsoid = scene.add_ellipsoid(R_a, R_a, R_a)
    # atmosphere = scene.add_primitive(atmosphere_ellipsoid, atmosphere_material, atmosphere_medium)
    # ecef.new_instance(atmosphere)

    # Create an instance of the camera
    navcam = eci.new_instance(camera_model)
    navcam.set_position(km(500000), m(0), m(0))
    navcam.set_euler_angles(deg(90), deg(0), deg(90))

    # Create the sun
    sun_light = scene.new_sun_light()
    sun = scene.root.new_instance(sun_light)
    sun.set_spice_origin("SUN")

    # Configure the render buffers
    frame_buffer = camera_model.make_frame_buffer()
    frame_buffer.enable_sensor_response()

    # Create the renderer
    renderer = Renderer()
    renderer.set_max_bounces(1)
    renderer.set_samples_per_pixel(100)

    # Create a scene view over the exposure interval.  Switch to
    # ABERRATED_STATE if you want stellar aberration applied to the star
    # positions (~20 arcsec for an Earth-orbiting observer).
    num_blur_samples = 1
    scene_view = SceneView(scene, exposure, navcam, huira.ObservationMode.GEOMETRIC_STATE, num_blur_samples)

    # Render the current scene view
    for i in range(0, 3):
        render_start = perf_counter()
        renderer.render(scene_view, frame_buffer)
        render_elapsed = perf_counter() - render_start
        print(f"Render took {render_elapsed:.3f} s ({render_elapsed / 60:.2f} min)")

    # Save the results to PNGs
    os.makedirs("output", exist_ok=True)
    huira.write_png("output/earth_star_field.png", huira.linear_to_srgb(frame_buffer.sensor_response))


if __name__ == "__main__":
    main()