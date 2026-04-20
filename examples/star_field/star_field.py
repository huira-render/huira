import sys
import os
from pathlib import Path

import huira
from huira.rgb import Scene, SceneView, Renderer

from huira.units import Millimeter as mm
from huira.units import Micrometer as um
from huira.units import Degree as deg
from huira.units import Second as sec

def parse_input_paths():
    if len(sys.argv) != 3:
        print("Usage: python star_field.py <tycho2.hrsc_path> <kernel_path>")
        sys.exit(1)
    star_catalog_path = Path(sys.argv[1])
    kernel_path = Path(sys.argv[2])
    return star_catalog_path, kernel_path

def main():
    # Parse input paths
    star_catalog_path, kernel_path = parse_input_paths()

    # Load the required SPICE kernels
    huira.spice.furnsh(kernel_path / "fk/orx_v14.tf")
    huira.spice.furnsh(kernel_path / "sclk/orx_sclkscet_00093.tsc")
    huira.spice.furnsh(kernel_path / "ck/orx_struct_mapcam_v01.bc")
    huira.spice.furnsh(kernel_path / "ck/orx_sc_rel_160919_160925_v01.bc")
    huira.spice.furnsh(kernel_path / "spk/orx_struct_v04.bsp")
    huira.spice.furnsh(kernel_path / "spk/orx_160909_171201_170830_od023_v1.bsp")
    huira.spice.furnsh(kernel_path / "spk/de424.bsp")

    # Create the scene
    scene = Scene()
    scene.set_background_radiance(1e-5)

    # Configure a camera model
    camera_model = scene.new_camera_model()
    camera_model.set_focal_length(mm(125))
    camera_model.set_fstop(3.30)
    camera_model.set_sensor_rotation(deg(90))
    camera_model.configure_sensor_from_pitch((1920, 1080), um(8.5))
    camera_model.use_aperture_psf(32, 16)
    camera_model.set_sensor_bit_depth(14)
    
    # Set the observation time
    time = huira.Time("2016-09-19T16:22:05.728")
    exposure = huira.Interval.from_centered(time, sec(9.984285275))
    
    # Load stars
    scene.load_stars(star_catalog_path, time)
    
    # Create an instance of the camera using SPICE configuration
    mapcam = scene.root.new_instance(camera_model)
    mapcam.set_spice("ORX_OCAMS_MAPCAM", "ORX_OCAMS_MAPCAM")
    
    # Configure the render buffers
    frame_buffer = camera_model.make_frame_buffer()
    frame_buffer.enable_sensor_response()
    
    # Create the renderer
    renderer = Renderer()
    
    # Create a scene view over the exposure interval
    scene_view = SceneView(scene, exposure, mapcam, huira.ObservationMode.ABERRATED_STATE)
    
    # Render the current scene view
    renderer.render(scene_view, frame_buffer)
    
    # Save the results
    huira.write_png("output/starfield.png", huira.linear_to_srgb(frame_buffer.sensor_response))

if __name__ == "__main__":
    main()
