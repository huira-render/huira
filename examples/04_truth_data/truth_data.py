"""
04 - Truth data

Demonstrates:
    - The frame buffer's other images: depth, camera and world normals, albedo, geometry IDs,
      and the power each pixel receives (in total, and directly and indirectly)
    - Viewing them: depth_map and normal_map, and normalizing with numpy
    - Images as numpy arrays, top row first, and back (Image(array, origin=...))
    - Saving float data losslessly as FITS
    - Ray queries: casting a camera ray through a pixel, intersecting it with the scene, and
      resolving the hit into the surface's position and normal
    - Projecting a point onto the image, and pixel coordinate conventions (OpenCV and FITS)

Builds on: 03 Load a model
Data: gateway.glb, NASA's model of the Gateway (see README.md)

Run:
    python truth_data.py gateway.glb

Writes:
    output/04_truth_data.png             the rendered image
    output/04_truth_data_depth.png       the distance to each pixel's surface, as a picture...
    output/04_truth_data_depth.fits      ...and in meters
    output/04_truth_data_normals.png     the surface normals, in the camera's frame
    output/04_truth_data_albedo.png      the surface albedo
    output/04_truth_data_ids.png         which geometry each pixel sees, one color each
    output/04_truth_data_indirect.png    the light each pixel receives after bouncing
"""

import sys
from pathlib import Path

import numpy as np

import huira
from huira.rgb import Scene, SceneView, Renderer, Image

from huira.units import Meter as m
from huira.units import Millimeter as mm
from huira.units import Degree as deg
from huira.units import AstronomicalUnit as au
from huira.units import Millisecond as ms


def normalized(image):
    """An image scaled so its brightest value is 1.

    np.asarray(image) is the image as an array, without copying, top row first. Image(array,
    origin=...) copies an array back, and needs to be told which row of it is the top: here the
    first, as np.asarray gave it.
    """
    array = np.asarray(image)
    peak = np.nanmax(array)
    scaled = (array / peak if peak > 0 else array).astype(np.float32)
    return Image(scaled, origin=huira.PixelOrigin.TopLeft)


def id_colors(geometry_ids):
    """A color for each geometry ID, and black where a pixel saw nothing."""
    ids = np.asarray(geometry_ids)
    missed = ids == np.iinfo(np.uint64).max
    rng = np.random.default_rng(0)
    palette = rng.uniform(0.2, 1.0, size=(256, 3)).astype(np.float32)
    colors = palette[(ids % 256).astype(np.int64)]
    colors[missed] = 0.0
    return Image(colors, origin=huira.PixelOrigin.TopLeft)


def main():
    if len(sys.argv) != 2:
        sys.exit("Usage: python truth_data.py <gateway.glb>\nREADME.md says where to get it.")
    model_path = Path(sys.argv[1])

    # The scene of example 03:
    scene = Scene()
    gateway_model = scene.load_model(model_path)
    gateway = scene.root.new_instance(gateway_model)

    sun = scene.root.new_instance(scene.new_sun_light())
    sun.set_position(m(0), au(-1), m(0))

    camera_model = scene.new_camera_model()
    camera_model.use_blender_convention()
    camera_model.set_focal_length(mm(50))
    camera_model.configure_sensor_from_size((1920, 1080), mm(36))
    camera_model.set_fstop(16)

    camera = scene.root.new_instance(camera_model)
    camera.set_position(m(40), m(-40), m(0))
    camera.set_euler_angles(deg(95), deg(0), deg(45))

    time = huira.Time("2026-01-01T00:00:00")
    exposure = huira.Interval.from_start(time, ms(1))
    scene_view = SceneView(scene, exposure, camera)

    # Besides the sensor response, a frame buffer can hold images of what each pixel sees. Each
    # is filled only if enabled, since each costs memory and time:
    #   - depth: the distance along the ray to the nearest surface, in meters
    #   - camera_normals / world_normals: the surface normal, in the camera's or world's frame
    #   - albedo: the surface albedo
    #   - geometry_ids: which geometry the pixel sees
    #   - received_power: the power the pixel collects (W, in each spectral bin), split into
    #     received_direct_power (straight from the lights) and received_indirect_power (after
    #     bouncing off other surfaces)
    frame_buffer = camera_model.make_frame_buffer()
    frame_buffer.enable_sensor_response()
    frame_buffer.enable_depth()
    frame_buffer.enable_camera_normals()
    frame_buffer.enable_world_normals()
    frame_buffer.enable_albedo()
    frame_buffer.enable_geometry_ids()
    frame_buffer.enable_received_direct_power()
    frame_buffer.enable_received_indirect_power()

    renderer = Renderer()
    renderer.set_samples_per_pixel(64)
    renderer.set_max_bounces(3)
    renderer.render(scene_view, frame_buffer)

    output_dir = Path("output")
    output_dir.mkdir(exist_ok=True)
    huira.write_png(
        output_dir / "04_truth_data.png", huira.linear_to_srgb(frame_buffer.sensor_response)
    )

    # depth_map and normal_map turn depth and normals into pictures. FITS keeps the depth itself,
    # as 32-bit floats:
    huira.write_png(output_dir / "04_truth_data_depth.png", huira.depth_map(frame_buffer.depth))
    huira.write_fits(output_dir / "04_truth_data_depth.fits", frame_buffer.depth)
    huira.write_png(
        output_dir / "04_truth_data_normals.png", huira.normal_map(frame_buffer.camera_normals)
    )

    # The other images are arrays like any other (np.asarray), to scale or color as needed:
    huira.write_png(
        output_dir / "04_truth_data_albedo.png", huira.linear_to_srgb(frame_buffer.albedo)
    )
    huira.write_png(output_dir / "04_truth_data_ids.png", id_colors(frame_buffer.geometry_ids))
    huira.write_png(
        output_dir / "04_truth_data_indirect.png",
        huira.linear_to_srgb(normalized(frame_buffer.received_indirect_power)),
    )

    # Ray queries ask the scene what is along a ray, without rendering. Take the pixel nearest the
    # image's center that sees something (its depth is finite; where a pixel sees nothing, the
    # depth is infinite). Its rows run down from the top, as pixel y does:
    depth = np.asarray(frame_buffer.depth)
    rows, cols = np.nonzero(np.isfinite(depth))
    if rows.size == 0:
        print("The camera sees nothing")
        return
    height, width = depth.shape
    nearest = np.argmin((cols - width / 2) ** 2 + (rows - height / 2) ** 2)
    x, y = int(cols[nearest]), int(rows[nearest])

    # cast_ray gives the ray through the pixel's center; SceneView.intersect traces it. Both work
    # in the camera's frame, so the hit point is relative to the camera.
    ray = camera_model.cast_ray(x, y)
    hit = scene_view.intersect(ray)
    if not hit.hit():
        print(f"The ray through the center of pixel ({x}, {y}) misses")
        return

    point = ray.at(hit.t)
    print(f"Pixel ({x}, {y}) sees geometry {hit.geom_id} at {hit.t:.3f} m")
    print(f"  The depth image has {depth[y, x]:.3f} m there (its nearest hit over the pixel)")

    # resolve_hit fills in the surface at the hit: its position, normals and texture coordinates.
    surface = scene_view.resolve_hit(ray, hit)
    print(f"  Surface normal: {surface.normal_s}")

    # project_point goes the other way, from a point in the camera's frame to the image. Pixel
    # coordinates follow the camera's pixel convention: OpenCV's by default (the first pixel's
    # center at (0, 0), y down), or others such as FITS's (the first pixel's center at (1, 1),
    # y up):
    print(f"  It projects to {camera_model.project_point(point)} (OpenCV convention)")
    camera_model.set_pixel_convention(huira.PixelConvention.fits())
    print(f"  It projects to {camera_model.project_point(point)} (FITS convention)")
    camera_model.set_pixel_convention(huira.PixelConvention.opencv())


if __name__ == "__main__":
    main()
