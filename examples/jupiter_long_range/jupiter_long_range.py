import sys
import os
from pathlib import Path

import huira
from huira.rgb import Scene, SceneView, RasterRenderer

from huira.units import Millimeter as mm
from huira.units import Micrometer as um
from huira.units import Degree as deg

def parse_input_paths():
    if len(sys.argv) != 3:
        print("Usage: star_field.py <tycho2.hrsc_path> <kernel_path>")
        sys.exit(1)
    star_catalog_path = Path(sys.argv[1])
    kernel_path = Path(sys.argv[2])
    return star_catalog_path, kernel_path

def main():
    # Parse input paths
    star_catalog_path, kernel_path = parse_input_paths()

    # Load the require SPICE kernels:
    huira.spice.furnsh(kernel_path / "spk/de440s.bsp");
    huira.spice.furnsh(kernel_path / "spk/jup365.bsp");

    # Create the scene
    scene = Scene()

    # Configure a camera model
    camera_model = scene.new_camera_model()
    camera_model.set_focal_length(mm(125))
    camera_model.set_fstop(3.30)
    camera_model.set_sensor_pixel_pitch(um(8.5), um(8.5))
    camera_model.set_sensor_resolution(1920, 1080)
    camera_model.set_sensor_bit_depth(14)
    camera_model.use_aperture_psf(32, 16)
    
    # Set the observation time
    time = huira.Time("2016-09-19T16:22:05.728")
    exposure_time = 1
    
    # Load stars
    scene.load_stars(star_catalog_path, time)
    scene.print_contents()

    # Create the sun
    sun_light = scene.new_sun_light()
    sun = scene.root.new_instance(sun_light)
    sun.set_spice_origin("SUN")

    # Create unresolved objects for Jupiter and it's moons
    #jupiter_model = scene.new_unresolved_sphere(m(69911000), sun, 0.5);
    jupiter_model = scene.new_unresolved_object_from_magnitude(-1.44, "Jupiter");
    #jupiter_model = scene.new_unresolved_object(W(1e-8));
    io_model = scene.new_unresolved_object_from_magnitude(5.02);
    europa_model = scene.new_unresolved_object_from_magnitude(5.29);
    ganymede_model = scene.new_unresolved_object_from_magnitude(4.61);
    callisto_model = scene.new_unresolved_object_from_magnitude(5.65);

    # Create new instances of the unresolved objects:
    jupiter = scene.root.new_instance(jupiter_model);
    jupiter.set_spice_origin("JUPITER");

    io = scene.root.new_instance(io_model);
    io.set_spice_origin("IO");

    europa = scene.root.new_instance(europa_model);
    europa.set_spice_origin("EUROPA");

    ganymede = scene.root.new_instance(ganymede_model);
    ganymede.set_spice_origin("GANYMEDE");

    callisto = scene.root.new_instance(callisto_model);
    callisto.set_spice_origin("CALLISTO");

    
    # Create an instance of the camera
    navcam = scene.root.new_instance(camera_model)
    navcam.set_spice_origin("EARTH_BARYCENTER")
    navcam.set_euler_angles(deg(90), deg(0), deg(272))
    
    # Configure the render buffers
    frame_buffer = camera_model.make_frame_buffer()
    frame_buffer.enable_received_power()
    frame_buffer.enable_sensor_response()
    
    # Create the renderer
    renderer = RasterRenderer()
    
    # Create a scene view at the observation time
    scene_view = SceneView(scene, time, navcam, huira.ObservationMode.ABERRATED_STATE)
    
    # Render the current scene view
    renderer.render(scene_view, frame_buffer, exposure_time)
    
    # Save the results
    huira.write_png("output/jupiter_long_range.png", frame_buffer.sensor_response, 8)

    print("DONE")

if __name__ == "__main__":
    main()
