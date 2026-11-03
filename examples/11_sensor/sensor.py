"""
11 - Sensor

Demonstrates:
    - The sensor's geometry: resolution, pixel pitch and principal point
    - Turning photons into digital numbers (DN): quantum efficiency, full well capacity,
      conversion gain (or gain in dB), bias level and bit depth
    - Noise: shot noise and read noise (on by default), row and column noise, dark current, and
      the noise seed; and fixed patterns made from the seed: PRNU and hot pixels
    - The physical signal chain (huira.visible8): each pixel collects one value from every
      spectral bin, as a real photosite does, where huira.rgb fills three color channels
    - Saving the raw DN: 16-bit FITS with header metadata and a world coordinate system (WCS),
      and 16-bit TIFF; reading the FITS back
    - The same frame without noise, for comparison
    - Calibration maps: each pixel's own dark current, from a master dark and a master bias,
      and its own bias
    - A filter wheel: a filter's transmission per spectral bin, and its flat from a flat field

Builds on: 10 Optics
Data: tycho2.hrsc, the Tycho-2 star catalog (see README.md)

Run:
    python sensor.py tycho2.hrsc

Writes:
    output/11_sensor.png             the readout, as a picture
    output/11_sensor.fits            ...as DN, with a header
    output/11_sensor.tif             ...as DN
    output/11_sensor_noiseless.png   the same frame without noise
    output/11_sensor_calibrated.png  the frame with each pixel's own dark current and bias
    output/11_sensor_red.png         the frame through a red filter, with its flat
"""

import math
import sys
from pathlib import Path

import numpy as np

import huira
from huira.visible8 import Scene, SceneView, Renderer

from huira.units import Millimeter as mm
from huira.units import Micrometer as um
from huira.units import Second as sec


def direction(ra_deg, dec_deg):
    """The unit vector toward a right ascension and declination, in the J2000 frame."""
    ra = math.radians(ra_deg)
    dec = math.radians(dec_deg)
    return huira.Vec3(math.cos(dec) * math.cos(ra), math.cos(dec) * math.sin(ra), math.sin(dec))


def main():
    if len(sys.argv) != 2:
        sys.exit("Usage: python sensor.py <tycho2.hrsc>\nREADME.md says where to get it.")
    catalog = Path(sys.argv[1])
    output_dir = Path("output")
    output_dir.mkdir(exist_ok=True)

    # This example renders with huira.visible8 rather than huira.rgb. huira.rgb is a special case
    # for making pictures: it fills the sensor response's red, green and blue channels
    # separately, as renderers usually do, though no real photosite works that way. The other
    # spectral types (huira.visible8, huira.swir8) run the real signal chain: each pixel collects
    # one pile of electrons, from every spectral bin, weighted by the quantum efficiency, so the
    # sensor response is one value per pixel, as from a monochrome sensor. Their eight bins are
    # the spectral sampling, not color channels; color from them takes filters, with an
    # exposure through each, as on a real instrument.
    scene = Scene()
    time = huira.Time("2026-01-01T00:00:00")
    scene.load_stars(catalog, time, 12.0)

    camera_model = scene.new_camera_model()
    camera_model.set_focal_length(mm(100))
    camera_model.set_fstop(4)

    # The sensor's geometry: 1024 x 768 pixels, 6.5 um square, with the principal point (where
    # the optical axis meets the sensor) given in pixels. Here it is the center, which is also
    # the default; a calibrated camera would give its own.
    width, height = 1024, 768
    cx, cy = (width - 1) / 2, (height - 1) / 2
    camera_model.configure_sensor_from_pitch((width, height), um(6.5), um(6.5), cx, cy)

    # How light becomes a digital number (DN), in order:
    #   - quantum efficiency: the fraction of photons that free an electron (one number here, or
    #     a value for each spectral bin: SpectralBins.integrate_over_data averages a measured QE
    #     curve over each bin)
    #   - full well capacity: the most electrons a pixel holds before it saturates
    #   - dark current: electrons per second that build up with no light at all
    #   - conversion gain: electrons per DN. (Gain can also be set in decibels with
    #     set_sensor_gain_db, relative to the level set_sensor_unity_db marks as unity.)
    #   - bias level: a constant offset in DN, so noise below it is not clipped at 0
    #   - bit depth: the range of DN, from 0 to 2^bits - 1
    camera_model.set_sensor_quantum_efficiency(0.7)
    camera_model.set_sensor_full_well_capacity(30000)
    camera_model.set_sensor_dark_current(5)
    camera_model.set_sensor_conversion_gain(2.0)
    camera_model.set_sensor_bias_level(100)
    camera_model.set_sensor_bit_depth(14)

    # Noise: shot noise (the randomness of the photons and dark current's electrons) and read
    # noise (electrons RMS, added at readout) are on by default. A CMOS sensor's rows, and to a
    # lesser degree its columns, also shift together by a different amount in every frame, as
    # faint banding: row and column noise, in electrons RMS, 0 unless set. The seed makes the
    # noise repeatable: the same seed gives the same frames, in the same order.
    camera_model.set_sensor_read_noise(8)
    camera_model.set_sensor_row_noise(1.5)
    camera_model.set_sensor_column_noise(0.5)
    camera_model.enable_sensor_noise(True)
    camera_model.set_sensor_noise_seed(42)

    # Fixed patterns, the same in every frame, made from the seed: each pixel's own gain (PRNU, a
    # standard deviation of 1% here) and hot pixels (0.1% of the pixels, with a dark current of
    # about 200 e-/s each, spread about 1.6x either way). A seed is a detector: another seed has
    # other patterns. Calibration frames from a real camera hold its own (see below).
    camera_model.set_sensor_prnu(0.01)
    camera_model.set_sensor_hot_pixels(0.001, 200, spread=0.5)

    pointing_ra, pointing_dec = 56.75, 24.12  # the Pleiades
    camera = scene.root.new_instance(camera_model)
    camera.look_at(direction(pointing_ra, pointing_dec), huira.Vec3(0, 0, 1))

    exposure_seconds = 4.0
    exposure = huira.Interval.from_centered(time, sec(exposure_seconds))
    scene_view = SceneView(scene, exposure, camera)
    renderer = Renderer()

    frame_buffer = camera_model.make_frame_buffer()
    frame_buffer.enable_sensor_response()
    renderer.render(scene_view, frame_buffer)

    # The sensor response holds DN / (2^bits - 1), from 0 to 1. As a picture:
    huira.write_png(
        output_dir / "11_sensor.png", huira.linear_to_srgb(frame_buffer.sensor_response)
    )

    # FITS stores the DN themselves (16-bit unsigned here), with a header. The WCS keywords say
    # where the image points: the pointing at the reference pixel, in FITS's pixel convention
    # (1-based, y up), and degrees per pixel (negative in x, since east is left with north up).
    degrees_per_pixel = math.degrees(math.atan(6.5e-3 / 100))
    metadata = huira.FitsMetadata()
    metadata.object = "Pleiades"
    metadata.telescop = "Huira example 11"
    metadata.instrume = "100 mm f/4, 14-bit"
    metadata.date_obs = time.to_iso_8601()
    metadata.exptime = exposure_seconds
    metadata.ctype1 = "RA---TAN"
    metadata.ctype2 = "DEC--TAN"
    metadata.crval1 = pointing_ra
    metadata.crval2 = pointing_dec
    metadata.crpix1 = cx + 1
    metadata.crpix2 = height - cy
    metadata.cdelt1 = -degrees_per_pixel
    metadata.cdelt2 = degrees_per_pixel
    metadata.custom_keywords = [
        huira.FitsKeyword("GAIN", camera_model.sensor_conversion_gain(), "e-/DN"),
        huira.FitsKeyword("RDNOISE", camera_model.sensor_read_noise(), "e- RMS"),
    ]
    metadata.comments = ["Rendered by Huira's sensor example"]
    huira.write_fits(output_dir / "11_sensor.fits", frame_buffer.sensor_response, 16, metadata)

    # TIFF stores the DN too, at the bundle's bit depth:
    bundle = huira.ImageBundle_f32(frame_buffer.sensor_response)
    bundle.bit_depth = 16
    huira.write_tiff(output_dir / "11_sensor.tif", bundle)

    # read_fits gives back the image (as DN / (2^bits - 1) again) and its header:
    image, header = huira.read_fits(output_dir / "11_sensor.fits")
    full_scale = 2**image.sensor_bit_depth - 1
    brightest = image.to_numpy(origin=huira.PixelOrigin.TopLeft).max() * full_scale
    print(f"Read back {header.object}, {header.exptime} s: "
          f"brightest pixel {brightest:.0f} DN of {full_scale}")

    # The same frame without noise (shot, read, row or column). The dark current and bias are
    # still added: they are not noise.
    camera_model.enable_sensor_noise(False)
    frame_buffer = camera_model.make_frame_buffer()
    frame_buffer.enable_sensor_response()
    renderer.render(scene_view, frame_buffer)
    huira.write_png(
        output_dir / "11_sensor_noiseless.png", huira.linear_to_srgb(frame_buffer.sensor_response)
    )

    # Calibration maps. A real sensor's pixels differ: each has its own dark current, some far
    # above the rest (hot pixels), and the bias has fixed patterns. Calibration frames measure
    # them: a master bias is the mean of many frames taken with no exposure, so each pixel reads
    # its bias, and a master dark the mean of many taken with the shutter closed, so each pixel
    # reads its bias and its dark current's charge. Here they are made up with numpy, in DN,
    # standing in for a real camera's, which huira.read_fits() would read.
    rng = np.random.default_rng(7)
    rows, columns = np.mgrid[0:height, 0:width]
    gain = camera_model.sensor_conversion_gain()
    bias_dn = 100 + np.tile(rng.normal(0, 1.5, width), (height, 1))  # a pattern by column
    rates = 5 + 60 * np.exp(-np.hypot(width - columns, height - rows) / 150)  # glow in a corner
    rates[rng.integers(0, height, 300), rng.integers(0, width, 300)] = 300  # hot pixels
    dark_exposure = 30.0
    dark_dn = bias_dn + rates * dark_exposure / gain

    # An array doesn't record which of its rows is the top of the image, so origin says: here,
    # the first. Each pixel's dark current is (dark - bias) * gain / exposure, with the gain of
    # the camera that took the frames:
    top_left = huira.PixelOrigin.TopLeft
    master_bias = huira.Image_f32(bias_dn, origin=top_left)
    master_dark = huira.Image_f32(dark_dn, origin=top_left)
    dark_map = huira.dark_current_from_master_dark(
        master_dark, master_bias, gain, sec(dark_exposure)
    )
    print(f"Dark current: {np.mean(dark_map):.1f} e-/s on average, {np.max(dark_map):.0f} at most")
    camera_model.set_sensor_dark_current_map(dark_map)

    # The master bias is each pixel's bias, in place of the bias level (BiasMapKind.Absolute, the
    # default); a pattern of offsets from the bias level would be BiasMapKind.Relative. Like the
    # single values, the maps are not noise, and apply with noise off too.
    camera_model.set_sensor_bias_map(master_bias)

    # The master dark holds the camera's hot pixels, so the made-up ones go:
    camera_model.set_sensor_hot_pixels(0, 0)

    camera_model.enable_sensor_noise(True)
    frame_buffer = camera_model.make_frame_buffer()
    frame_buffer.enable_sensor_response()
    renderer.render(scene_view, frame_buffer)
    huira.write_png(
        output_dir / "11_sensor_calibrated.png", huira.linear_to_srgb(frame_buffer.sensor_response)
    )

    # A filter wheel. A filter passes a fraction of the light in each spectral bin (visible8's
    # eight bins run from blue to red), and may have a flat: each pixel's response through it,
    # relative to the principal point's, from a flat field (a frame of a uniformly lit scene,
    # bias and dark subtracted). Here the flat field is made up: the falloff Huira renders
    # itself, times extra vignetting toward the corners and a dust shadow. flat_from_measured
    # divides the rendered falloff out, so the flat keeps only the rest, and normalizes it to the
    # median of a central patch, so the quantum efficiency is the response at the principal
    # point and pixels that respond more are above 1. An empty wheel is a clear filter.
    rendered_falloff = np.array(
        [[camera_model.pixel_radiance_to_power(x, y) for x in range(width)] for y in range(height)]
    )
    vignetting = 1 - 0.4 * ((columns - cx) ** 2 + (rows - cy) ** 2) / (cx**2 + cy**2)
    dust = 1 - 0.3 * np.exp(-np.hypot(columns - 300, rows - 200) / 12)
    flat_field = huira.Image_f32(rendered_falloff * vignetting * dust, origin=top_left)
    red_flat = camera_model.flat_from_measured(flat_field)
    camera_model.add_filter("clear", 1.0)
    camera_model.add_filter("red", [0, 0, 0, 0, 0.2, 0.8, 0.9, 0.8], red_flat)
    camera_model.select_filter("red")
    print(f"Filter {camera_model.selected_filter_name()!r} of {camera_model.filter_count()}: "
          f"flat from {np.min(red_flat):.2f} to {np.max(red_flat):.2f}")

    frame_buffer = camera_model.make_frame_buffer()
    frame_buffer.enable_sensor_response()
    renderer.render(scene_view, frame_buffer)
    huira.write_png(
        output_dir / "11_sensor_red.png", huira.linear_to_srgb(frame_buffer.sensor_response)
    )


if __name__ == "__main__":
    main()
