import sys
from pathlib import Path

import huira
from huira.visible8 import SpectralBins, Scene, SceneView, Renderer, add_qld_tiles

from huira.units import Kilometer as km
from huira.units import Millimeter as mm
from huira.units import Degree as deg
from huira.units import Second as sec


def parse_input_paths():
    if len(sys.argv) != 2:
        print("Usage: python moon.py <qpu_dir>")
        sys.exit(1)
    qpu_path = Path(sys.argv[1])
    return qpu_path


def main():
    qpu_path = parse_input_paths()

    huira.set_log_level(huira.LogLevel.Debug)
    huira.enable_console_debug(True)

    scene = Scene()

    bsdf = scene.new_bsdf_mcewen()
    moon_material = scene.new_material(bsdf)
    moon_material.set_albedo_factor(SpectralBins(1.0))

    moon = scene.root.new_subframe()
    add_qld_tiles(scene, moon, qpu_path, moon_material, 0, 1737400.0, "moon", 2000.0)

    camera_model = scene.new_camera_model()
    camera_model.set_focal_length(mm(35))
    camera_model.configure_sensor_from_size((1024, 1024), mm(12))
    camera_model.set_fstop(5.6)
    camera_model.set_sensor_gain(1.0)
    camera_model.set_sensor_bit_depth(12)
    camera_model.set_sensor_quantum_efficiency(0.8)
    camera_model.set_sensor_full_well_capacity(20000)
    camera_model.set_sensor_simulate_noise(False)
    camera_model.use_blender_convention()

    camera = scene.root.new_instance(camera_model)
    camera.set_position(km(12000), km(0), km(0))
    camera.set_euler_angles(deg(90), deg(0), deg(90))

    sun_light = scene.new_sun_light()
    sun = scene.root.new_instance(sun_light)
    sun.set_position(km(150000000), km(0), km(0))

    frame_buffer = camera_model.make_frame_buffer()
    frame_buffer.enable_sensor_response()

    renderer = Renderer()
    renderer.set_max_bounces(2)
    renderer.set_samples_per_pixel(32)

    time = huira.Time("2025-06-07T12:00:00")
    exposure = huira.Interval(time, time + sec(0.00005))
    scene_view = SceneView(scene, exposure, camera, huira.ObservationMode.GEOMETRIC_STATE, 1)

    renderer.render(scene_view, frame_buffer)

    Path("output").mkdir(exist_ok=True)
    huira.write_png("output/moon.png", huira.linear_to_srgb(frame_buffer.sensor_response))


if __name__ == "__main__":
    main()
