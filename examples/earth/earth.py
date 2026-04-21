import sys
import os
from pathlib import Path
import numpy as np

import huira
from huira.rgb import Scene, SceneView, Renderer, McEwenBSDF

from huira.units import Meter as m
from huira.units import Kilometer as km
from huira.units import Millimeter as mm
from huira.units import Degree as deg
from huira.units import Watt as W
from huira.units import Second as sec
from huira.units import AstronomicalUnit as au

def parse_input_paths():
    if len(sys.argv) != 3:
        print("Usage: python earth.py <earth.glb_path> <kernel_path>")
        sys.exit(1)
    earth_path = Path(sys.argv[1])
    kernel_path = Path(sys.argv[2])
    return earth_path, kernel_path

def main():
    # Parse input paths
    earth_path, kernel_path = parse_input_paths()

    # Enable logger output
    huira.set_log_level(huira.LogLevel.Debug)
    huira.enable_console_debug(True)

    # Load the require SPICE kernels
    huira.spice.furnsh(kernel_path / "spk/de440s.bsp")
    huira.spice.furnsh(kernel_path / "pck/earth_latest_high_prec.bpc")
    huira.spice.furnsh(kernel_path / "pck/earth_fixed.tf")

    # Create the scene
    scene = Scene()

    # Set the observation time
    time = huira.Time("2019-02-06T10:27:00")
    exposure = huira.Interval(time, time + sec(0.00025))

    # Configure a camera model
    camera_model = scene.new_camera_model()
    camera_model.set_focal_length(mm(25))
    camera_model.configure_sensor_from_size((1080, 1080), mm(6))

    # Set camera exposure settings
    camera_model.set_fstop(10)
    camera_model.set_sensor_gain(1)
    camera_model.set_sensor_bit_depth(12)
    camera_model.set_sensor_quantum_efficiency(0.8)
    camera_model.set_sensor_full_well_capacity(20000)

    # Huira uses the OpenCV convention by default, which is
    # +z forward, +y down.  Blender uses -z forward, +y up.
    # This flag allows you to match Blender's for easier
    # comparison with blender generated images.
    camera_model.use_blender_convention()

    # Disable noise
    camera_model.set_sensor_simulate_noise(True)
    #camera_model.use_aperture_psf(True)
    #camera_model.enable_psf_convolution(True)

    # Create the reference frames
    eci = scene.root.new_spice_subframe("EARTH", "J2000")
    ecef = scene.root.new_spice_subframe("EARTH", "ITRF93")

    # Create an instance of the camera
    navcam = eci.new_instance(camera_model)
    navcam.set_position(km(100000), m(0), m(0))
    navcam.set_euler_angles(deg(90), deg(0), deg(90))
    
    # Load the Earth model
    earth_model = scene.load_model(earth_path)
    earth = ecef.new_instance(earth_model)
    earth.set_scale(6371*1000)

    
    # Create the sun
    sun_light = scene.new_sun_light()
    sun = scene.root.new_instance(sun_light)
    sun.set_spice_origin("SUN")


    # Configure the render buffers
    frame_buffer = camera_model.make_frame_buffer()
    frame_buffer.enable_sensor_response()
    frame_buffer.enable_camera_normals()
    frame_buffer.enable_albedo()
    
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
    huira.write_png("output/earth_render.png", huira.linear_to_srgb(frame_buffer.sensor_response))
    huira.write_png("output/earth_albedo.png", huira.linear_to_srgb(frame_buffer.albedo.get_channel(0)))
    huira.write_png("output/earth_normals.png", huira.normal_map(frame_buffer.camera_normals))

if __name__ == "__main__":
    main()
