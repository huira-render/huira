"""
03 - Load a model

Demonstrates:
    - Loading a 3D model from a file (glTF/GLB here; any format Assimp reads), with its
      meshes, materials and textures
    - Instancing a model, and printing its internal graph
    - The camera's intrinsic matrix
    - A background radiance, for the sky behind the model
    - Path tracing settings: samples per pixel, bounces, and clamping indirect light
    - Dynamic sampling: stopping early on pixels that have converged

Builds on: 02 Scene graph
Data: gateway.glb, NASA's model of the Gateway (see README.md)

Run:
    python load_a_model.py gateway.glb

Writes:
    output/03_load_a_model.png
"""

import sys
from pathlib import Path

import huira
from huira.rgb import Scene, SceneView, Renderer

from huira.units import Meter as m
from huira.units import Millimeter as mm
from huira.units import Degree as deg
from huira.units import AstronomicalUnit as au
from huira.units import Millisecond as ms


def main():
    if len(sys.argv) != 2:
        sys.exit("Usage: python load_a_model.py <gateway.glb>\nREADME.md says where to get it.")
    model_path = Path(sys.argv[1])

    huira.enable_console_info()

    scene = Scene()

    # load_model reads a model file into the scene as one asset: its meshes, materials and
    # textures, and the graph of nodes that places them. Huira reads models with Assimp, so
    # besides glTF it reads OBJ, FBX, PLY, STL and many other formats.
    gateway_model = scene.load_model(model_path, "gateway")
    gateway_model.print_graph()

    # Like any asset, a model is placed with an instance. Here, at the origin:
    gateway = scene.root.new_instance(gateway_model)

    sun = scene.root.new_instance(scene.new_sun_light())
    sun.set_position(m(0), au(-1), m(0))

    # Rays that hit nothing see the background radiance, given here as one value for every
    # spectral bin: a faint glow, rather than black. It can also be an image (an environment).
    scene.set_background_radiance(1e-7)

    camera_model = scene.new_camera_model()
    camera_model.use_blender_convention()
    camera_model.set_focal_length(mm(50))
    camera_model.configure_sensor_from_size((1920, 1080), mm(36))
    camera_model.set_fstop(16)

    # The camera matrix, in pixels, as set_intrinsic_matrix() would take it:
    print("Camera matrix (pixels):")
    print(camera_model.intrinsic_matrix())

    camera = scene.root.new_instance(camera_model)
    camera.set_position(m(40), m(-40), m(0))
    camera.set_euler_angles(deg(95), deg(0), deg(45))

    scene.print_contents()

    time = huira.Time("2026-01-01T00:00:00")
    exposure = huira.Interval.from_start(time, ms(1))
    scene_view = SceneView(scene, exposure, camera, huira.ObservationMode.GEOMETRIC_STATE)

    frame_buffer = camera_model.make_frame_buffer()
    frame_buffer.enable_sensor_response()

    # Path tracing follows each camera ray as it bounces from surface to surface:
    #   - samples per pixel: rays traced through each pixel (more is less noisy)
    #   - max bounces: how many surfaces a path may hit (1 is direct light only)
    #   - indirect clamp: a cap on the light any one bounce adds, which trades a little bias
    #     for much less "firefly" noise
    renderer = Renderer()
    renderer.set_samples_per_pixel(256)
    renderer.set_max_bounces(3)
    renderer.set_indirect_clamp(10.0)

    # Dynamic sampling stops tracing a pixel once its noise (the uncertainty in its brightness,
    # relative to it) and its neighbours' is below a threshold (0.01, about 1%, by default),
    # after a minimum number of samples: flat and empty parts of the image finish early, and
    # the samples go where the noise is.
    renderer.set_dynamic_sampling(True)
    renderer.set_min_samples(16)
    renderer.set_noise_threshold(0.01)

    # The camera's optics are precomputed before the first render. Doing it here keeps that time
    # out of the render (and is when any problem with the settings is reported):
    camera_model.precompute()

    renderer.render(scene_view, frame_buffer)

    output_dir = Path("output")
    output_dir.mkdir(exist_ok=True)
    huira.write_png(
        output_dir / "03_load_a_model.png", huira.linear_to_srgb(frame_buffer.sensor_response)
    )


if __name__ == "__main__":
    main()
