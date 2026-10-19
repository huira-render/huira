"""
10 - Optics

Demonstrates:
    - Diffraction: the aperture's pattern, set by its f-number (or diameter), as every star's
      point spread function (PSF)
    - Focus: set_focus_distance (or set_focus_diopters, set_focus_sensor_offset), and
      defocus_blur_radius
    - Scattered light (set_scatter) and veiling glare (set_veiling_glare)
    - A measured PSF in place of diffraction (set_measured_psf), and going back (clear_psf)
    - Viewing a PSF: psf_image, diffraction_image, scatter_image and defocus_image
    - Lens distortion: Brown-Conrady, OpenCV (from OpenCV's own coefficients) and Owen models,
      and project_point to see where they move a star
    - describe(), which summarizes a camera model

Builds on: 09 Stars
Data: tycho2.hrsc, the Tycho-2 star catalog (see README.md)

Run:
    python optics.py tycho2.hrsc

Writes:
    output/10_optics.png                 the Pleiades, diffraction-limited at f/16
    output/10_optics_defocus.png         focused at 50 m instead of infinity
    output/10_optics_scatter.png         with scattered light and veiling glare
    output/10_optics_psf.png             that PSF, as an image
    output/10_optics_measured_psf.png    with a measured (here, made-up Gaussian) PSF
    output/10_optics_distortion.png      a wide lens with barrel distortion
"""

import math
import sys
from pathlib import Path

import numpy as np

import huira
from huira.rgb import Scene, SceneView, Renderer, Image

from huira.units import Meter as m
from huira.units import Millimeter as mm
from huira.units import Micrometer as um
from huira.units import Degree as deg
from huira.units import Diopter as dpt
from huira.units import Second as sec


def direction(ra_deg, dec_deg):
    """The unit vector toward a right ascension and declination, in the J2000 frame."""
    ra = math.radians(ra_deg)
    dec = math.radians(dec_deg)
    return huira.Vec3(math.cos(dec) * math.cos(ra), math.cos(dec) * math.sin(ra), math.sin(dec))


def normalized(image):
    """An image scaled so its brightest value is 1."""
    array = image.to_numpy()
    return Image((array / array.max()).astype(np.float32))


def gaussian_psf(samples_per_pixel, sigma_pixels, radius_pixels):
    """A made-up measured PSF: a round Gaussian, sampled finely, the same in every channel."""
    n = 2 * radius_pixels * samples_per_pixel + 1
    offsets = (np.arange(n) - n // 2) / samples_per_pixel
    xx, yy = np.meshgrid(offsets, offsets)
    psf = np.exp(-(xx**2 + yy**2) / (2 * sigma_pixels**2))
    psf /= psf.sum()
    return Image(np.repeat(psf[:, :, None], 3, axis=2).astype(np.float32))


def main():
    if len(sys.argv) != 2:
        sys.exit("Usage: python optics.py <tycho2.hrsc>\nREADME.md says where to get it.")
    catalog = Path(sys.argv[1])
    output_dir = Path("output")
    output_dir.mkdir(exist_ok=True)

    scene = Scene()
    time = huira.Time("2026-01-01T00:00:00")
    scene.load_stars(catalog, time, 12.0)
    exposure = huira.Interval.from_centered(time, sec(2))
    renderer = Renderer()

    def render(camera, camera_model, filename):
        # The camera's optics are rebuilt after any change; doing it here keeps that out of the
        # render.
        camera_model.precompute()
        scene_view = SceneView(scene, exposure, camera)
        frame_buffer = camera_model.make_frame_buffer()
        frame_buffer.enable_sensor_response()
        renderer.render(scene_view, frame_buffer)
        huira.write_png(output_dir / filename, huira.linear_to_srgb(frame_buffer.sensor_response))

    # A 200 mm lens on 5.5 um pixels, aimed at the Pleiades: about 1.6 degrees across.
    camera_model = scene.new_camera_model()
    camera_model.set_focal_length(mm(200))
    camera_model.configure_sensor_from_pitch((1024, 1024), um(5.5))
    camera = scene.root.new_instance(camera_model)
    camera.look_at(direction(56.75, 24.12), huira.Vec3(0, 0, 1))

    # Diffraction: by default every star is drawn with the diffraction pattern of the camera's
    # circular aperture, averaged over each spectral bin's wavelengths (so red stars' patterns
    # are larger). At f/16 its first dark ring is about two pixels out. The aperture can be set by
    # its diameter instead (set_aperture_diameter), and diffraction turned off
    # (enable_diffraction(False)), which puts each star's light in the pixel it falls in.
    camera_model.set_fstop(16)
    print(camera_model.describe())
    render(camera, camera_model, "10_optics.png")

    # Focus: the camera focuses at infinity by default, where the stars are. Focused at 50 m
    # instead, every star blurs into a disc, the aperture's image out of focus. The same focus
    # can be given in diopters (set_focus_diopters(dpt(0.02))), or as how far the sensor sits
    # from the focal plane (set_focus_sensor_offset).
    camera_model.set_focus_distance(m(50))
    print(f"Defocus blur radius: {camera_model.defocus_blur_radius():.2f} px")
    render(camera, camera_model, "10_optics_defocus.png")
    camera_model.set_focus_diopters(dpt(0))  # back to infinity

    # Scattered light: a fraction of each star's light spreads into wide wings, flat within the
    # shoulder angle and falling off beyond it as angle^-slope (and faster beyond an outer
    # angle). Veiling glare spreads a fraction of all the light evenly over the image.
    camera_model.set_scatter(0.05, 2.5, deg(0.005), deg(2.0))
    camera_model.set_veiling_glare(0.01)
    render(camera, camera_model, "10_optics_scatter.png")

    # psf_image draws the PSF as the renderer does, centered in an image 2 * radius + 1 pixels
    # wide; diffraction_image, scatter_image and defocus_image draw each part alone.
    huira.write_png(
        output_dir / "10_optics_psf.png",
        huira.linear_to_srgb(normalized(camera_model.psf_image(32))),
    )
    camera_model.disable_scatter()
    camera_model.disable_veiling_glare()

    # A measured PSF replaces the diffraction pattern: an image of the PSF, centered, with how
    # many samples it has per sensor pixel. PointSampled says the values are the PSF's intensity
    # at points (from optical design software, or measured finely); PixelIntegrated would say
    # they are what pixels of this sensor collected, as in a star image. InFocus says the camera's
    # focus should still blur it. Here, a Gaussian 1.5 pixels wide, sampled 4 times per pixel:
    samples_per_pixel = 4
    psf = gaussian_psf(samples_per_pixel, sigma_pixels=1.5, radius_pixels=8)
    camera_model.set_measured_psf(
        psf, samples_per_pixel, huira.PSFSampling.PointSampled, huira.PSFFocus.InFocus
    )
    render(camera, camera_model, "10_optics_measured_psf.png")
    camera_model.clear_psf()  # back to the aperture's diffraction pattern

    # Distortion moves where things land on the image, and matters most far from the center. A
    # wide 16 mm lens shows it, with its own camera model in the same scene:
    wide_model = scene.new_camera_model()
    wide_model.set_focal_length(mm(16))
    wide_model.set_fstop(4)
    wide_model.configure_sensor_from_pitch((1024, 1024), um(10))
    wide_camera = scene.root.new_instance(wide_model)
    wide_camera.look_at(direction(62.0, 20.0), huira.Vec3(0, 0, 1))

    # project_point gives where a point in the camera's frame lands on the image. A star near the
    # corner, with each distortion model in turn:
    corner = huira.Vec3(0.25, 0.25, 1.0)
    print(f"No distortion:  {wide_model.project_point(corner)}")

    # Brown-Conrady: radial (k1, k2, k3) and tangential (p1, p2) terms, in normalized image
    # coordinates. A negative k1 is barrel distortion.
    wide_model.set_brown_conrady_distortion(huira.BrownCoefficients(-0.2, 0.02, 0.0, 0.0, 0.0))
    print(f"Brown-Conrady:  {wide_model.project_point(corner)}")

    # OpenCV's model, built from the distortion vector cv2.calibrateCamera returns
    # (k1, k2, p1, p2, k3, ...):
    wide_model.set_opencv_distortion(
        huira.OpenCVCoefficients.from_opencv(np.array([-0.2, 0.02, 0.0, 0.0, 0.0]))
    )
    print(f"OpenCV:         {wide_model.project_point(corner)}")

    # Owen's model: radial terms (e2 times r^2, e4 times r^4), rotational terms (e1 and e3), and
    # terms linear in x and y (e6 and e5):
    wide_model.set_owen_distortion(huira.OwenCoefficients(0.0, -0.2, 0.0, 0.02, 0.0, 0.0))
    print(f"Owen:           {wide_model.project_point(corner)}")

    wide_model.set_brown_conrady_distortion(huira.BrownCoefficients(-0.2, 0.02, 0.0, 0.0, 0.0))
    render(wide_camera, wide_model, "10_optics_distortion.png")

    # delete_distortion() removes it again.
    wide_model.delete_distortion()


if __name__ == "__main__":
    main()
