import sys
import os
from pathlib import Path
import numpy as np

import huira
from huira.rgb import Scene, SceneView, Renderer, McEwenBSDF
from huira.rgb import RGB as spec

from huira.units import Meter as m
from huira.units import Kilometer as km
from huira.units import Millimeter as mm
from huira.units import Degree as deg
from huira.units import Watt as W
from huira.units import Second as sec
from huira.units import AstronomicalUnit as au

def parse_input_paths():
    if len(sys.argv) != 2:
        print("Usage: python earth.py <assets_path>")
        sys.exit(1)
    assets_path = Path(sys.argv[1])
    return assets_path

def main():
    # Parse input paths
    assets_path = parse_input_paths()
    kernels_path = assets_path / "kernels/"
    textures_path = assets_path / "textures/"

    # Enable logger output
    huira.set_log_level(huira.LogLevel.Debug)
    huira.enable_console_debug(True)

    # Load the require SPICE kernels
    huira.spice.furnsh(kernels_path / "spk/de440s.bsp")
    huira.spice.furnsh(kernels_path / "pck/earth_latest_high_prec.bpc")
    huira.spice.furnsh(kernels_path / "pck/earth_fixed.tf")

    # Create the scene
    scene = Scene()

    eci = scene.root.new_spice_subframe("EARTH", "J2000")
    ecef = scene.root.new_spice_subframe("EARTH", "ITRF93")

    # Set the observation time
    time = huira.Time("2019-02-06T10:27:00")
    exposure = huira.Interval(time, time + sec(0.00005))

    # Configure a camera model
    camera_model = scene.new_camera_model()
    camera_model.set_focal_length(mm(25))
    camera_model.configure_sensor_from_size((1080, 1080), mm(6))

    # Set camera exposure settings
    camera_model.set_fstop(5)
    camera_model.set_sensor_gain(1)
    camera_model.set_sensor_bit_depth(12)
    camera_model.set_sensor_quantum_efficiency(0.8)
    camera_model.set_sensor_full_well_capacity(20000)
    camera_model.set_sensor_simulate_noise(False)

    # Huira uses the OpenCV convention by default, which is
    # +z forward, +y down.  Blender uses -z forward, +y up.
    # This flag allows you to match Blender's for easier
    # comparison with blender generated images.
    camera_model.use_blender_convention()

    # Create the Earth material
    ct_bsdf = scene.new_bsdf_cook_torrance()
    earth_material = scene.new_material(ct_bsdf)

    earth_albedo_rgb = huira.read_image(textures_path / "8k_earth_daymap.jpg")
    earth_albedo_spec = huira.rgb_to_spectral(earth_albedo_rgb.image)
    earth_albedo_tex = scene.add_texture(earth_albedo_spec)
    earth_material.set_albedo_image(earth_albedo_tex)

    earth_roughness = huira.read_image_mono(textures_path / "8k_earth_roughness_map.ti")
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

    # Create the Earth's atmosphere
    null_bsdf = scene.new_bsdf_null()
    atmosphere_material = scene.new_material(null_bsdf)
    atmosphere_material.set_transmission_factor(spec(1))

    constant_density_field = scene.new_constant_density_field(spec(0), spec(2e-6))
    isotropic_phase_function = scene.new_isotropic_phase_function()
    atmosphere_medium = scene.new_medium(constant_density_field, isotropic_phase_function)
    
    R_a = R_e + km(100)
    atmosphere_ellipsoid = scene.add_ellipsoid(R_a, R_a, R_a)
    atmosphere = scene.add_primitive(atmosphere_ellipsoid, atmosphere_material, atmosphere_medium)
    ecef.new_instance(atmosphere)


    # Create an instance of the camera
    navcam = eci.new_instance(camera_model)
    navcam.set_position(km(100000), m(0), m(0))
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
    renderer.set_max_bounces(3);
    renderer.set_samples_per_pixel(100);
    
    # Create a scene view over the exposure interval
    num_blur_samples = 1
    scene_view = SceneView(scene, exposure, navcam, huira.ObservationMode.GEOMETRIC_STATE, num_blur_samples)
    
    # Render the current scene view
    renderer.render(scene_view, frame_buffer)
    
    # Save the results to PNGs
    huira.write_png("output/earth.png", huira.linear_to_srgb(frame_buffer.sensor_response))

if __name__ == "__main__":
    main()
