import sys
import os
from pathlib import Path
import numpy as np

import huira
from huira.visible8 import Scene, SceneView, Renderer, McEwenBSDF

from huira.units import Meter as m
from huira.units import Kilometer as km
from huira.units import Millimeter as mm
from huira.units import Degree as deg
from huira.units import Watt as W
from huira.units import Second as sec
from huira.units import AstronomicalUnit as au

def parse_input_paths():
    if len(sys.argv) != 2:
        print("Usage: python moon.py <moon.glb_path>")
        sys.exit(1)
    moon_path = Path(sys.argv[1])
    return moon_path

def main():
    # Parse input paths
    moon_path = parse_input_paths()

    # Create the scene
    scene = Scene()

    # Set the observation time
    time = huira.Time("2019-02-06T10:27:00")
    exposure = huira.Interval(time, time + sec(0.00025))

    # Configure a camera model
    camera_model = scene.new_camera_model()
    camera_model.set_focal_length(mm(25))
    camera_model.set_sensor_resolution(1080, 1080)
    camera_model.set_sensor_size(mm(6))

    # Set camera exposure settings
    camera_model.set_fstop(18)
    camera_model.set_sensor_gain(0.8)
    camera_model.set_sensor_bit_depth(12)
    camera_model.set_sensor_quantum_efficiency(0.6)
    camera_model.set_sensor_full_well_capacity(20000)

    # Huira uses the OpenCV convention by default, which is
    # +z forward, +y down.  Blender uses -z forward, +y up.
    # This flag allows you to match Blender's for easier
    # comparison with blender generated images.
    camera_model.use_blender_convention()

    # Disable noise
    camera_model.set_sensor_simulate_noise(False);
    camera_model.use_aperture_psf(False);

    # Create an instance of the camera
    navcam = scene.root.new_instance(camera_model)
    navcam.set_position(m(0), m(10), m(0))
    navcam.set_euler_angles(deg(90), deg(0), deg(180))
    
    # Load the moon model
    moon_model = scene.load_model(moon_path)
    moon_model.set_all_bsdfs(McEwenBSDF())

    # Add moon model to the scene
    moon = scene.root.new_instance(moon_model)
    
    # Create the sun
    sun_light = scene.new_sun_light()
    sun = scene.root.new_instance(sun_light)
    sun.set_position(au(1), m(0), m(0))


    # Print the scene contents
    scene.print_contents()


    # Configure the render buffers
    frame_buffer = camera_model.make_frame_buffer()
    frame_buffer.enable_sensor_response()
    frame_buffer.enable_albedo() # This produces a flat, unshaded image
    
    # Create the renderer
    renderer = Renderer()
    renderer.set_max_bounces(1);
    renderer.set_samples_per_pixel(100);
    
    # Create a scene view over the exposure interval
    scene_view = SceneView(scene, exposure, navcam, huira.ObservationMode.GEOMETRIC_STATE)
    
    # Render the current scene view
    renderer.render(scene_view, frame_buffer)
    
    # Save the results to PNGs
    huira.write_png("output/moon_render.png", frame_buffer.sensor_response, 8)
    huira.write_png("output/moon_albedo.png", frame_buffer.albedo.get_channel(0), 8)

    # Save the results to CSVs
    np.savetxt("output/moon_render.csv", frame_buffer.sensor_response.get_channel(0), delimiter=",")
    np.savetxt("output/moon_albedo.csv", frame_buffer.albedo.get_channel(0), delimiter=",")

if __name__ == "__main__":
    main()
