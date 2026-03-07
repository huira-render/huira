import sys
import os
from pathlib import Path

import huira
from huira.rgb import Scene, SceneView, Renderer

from huira.units import Meter as m
from huira.units import Millimeter as mm
from huira.units import Degree as deg
from huira.units import Watt as W
from huira.units import Second as sec

def parse_input_paths():
    if len(sys.argv) != 2:
        print("Usage: python lunar_gateway.py <gateway.glb_path> <kernel_path>")
        sys.exit(1)
    gateway_path = Path(sys.argv[1])
    return gateway_path

def main():
    # Parse input paths
    gateway_path = parse_input_paths()

    # Create the scene
    scene = Scene()

    # Set the observation time
    time = huira.Time("2019-02-06T10:27:00")
    exposure = huira.Interval(time, time + sec(0.05))

    # Configure a camera model
    camera_model = scene.new_camera_model()
    camera_model.use_blender_convention()
    camera_model.set_focal_length(mm(50))
    camera_model.set_sensor_resolution(1920, 1080)
    camera_model.set_sensor_size(mm(36))

    # Create an instance of the camera
    navcam = scene.root.new_instance(camera_model)
    navcam.set_position(m(40), m(-40), m(0))
    navcam.set_euler_angles(deg(95), deg(0), deg(45))
    
    # Load stars
    gateway_model = scene.load_model(gateway_path)
    gateway = scene.root.new_instance(gateway_model)
    
    # Create a local light source
    point_light = scene.new_point_light(W(5000))
    light = scene.root.new_instance(point_light)
    light.set_position(m(0), m(-200), m(50))

    # Print the scene contents
    scene.print_contents()


    # Configure the render buffers
    frame_buffer = camera_model.make_frame_buffer()
    frame_buffer.enable_sensor_response();
    
    # Create the renderer
    renderer = Renderer()
    
    # Create a scene view over the exposure interval
    scene_view = SceneView(scene, exposure, navcam, huira.ObservationMode.ABERRATED_STATE)
    
    # Render the current scene view
    renderer.render(scene_view, frame_buffer)
    
    # Save the results
    huira.write_png("output/gateway_render.png", frame_buffer.sensor_response, 8)

if __name__ == "__main__":
    main()
