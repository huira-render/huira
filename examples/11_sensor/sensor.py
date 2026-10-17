"""
11 - Sensor

Demonstrates:
    - The sensor's geometry: resolution, pixel pitch and principal point
    - Turning photons into digital numbers (DN): quantum efficiency, full well capacity,
      conversion gain (or gain in dB), bias level and bit depth
    - Noise: shot noise and read noise (on by default), dark current, and the noise seed
    - The physical signal chain (huira.visible8): each pixel collects one value from every
      spectral bin, as a real photosite does, where huira.rgb fills three color channels
    - Saving the raw DN: 16-bit FITS with header metadata and a world coordinate system (WCS),
      and 16-bit TIFF; reading the FITS back
    - The same frame without noise, for comparison

Builds on: 10 Optics
Data: tycho2.hrsc, the Tycho-2 star catalog (see README.md)

Run:
    python sensor.py tycho2.hrsc

Writes:
    output/11_sensor.png             the readout, as a picture
    output/11_sensor.fits            ...as DN, with a header
    output/11_sensor.tif             ...as DN
    output/11_sensor_noiseless.png   the same frame without shot or read noise
"""

import math
import sys
from pathlib import Path

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
    # noise (electrons RMS, added at readout) are on by default. The seed makes the noise
    # repeatable: the same seed gives the same frames, in the same order.
    camera_model.set_sensor_read_noise(8)
    camera_model.enable_sensor_noise(True)
    camera_model.set_sensor_noise_seed(42)

    pointing_ra, pointing_dec = 56.75, 24.12  # the Pleiades
    camera = scene.root.new_instance(camera_model)
    camera.look_at(direction(pointing_ra, pointing_dec), huira.Vec3(0, 0, 1))

    exposure_seconds = 4.0
    exposure = huira.Interval.from_centered(time, sec(exposure_seconds))
    scene_view = SceneView(scene, exposure, camera, huira.ObservationMode.GEOMETRIC_STATE)
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
    print(f"Read back {header.object}, {header.exptime} s: "
          f"brightest pixel {image.to_numpy().max() * full_scale:.0f} DN of {full_scale}")

    # The same frame without shot and read noise. The dark current and bias are still added: they
    # are not noise.
    camera_model.enable_sensor_noise(False)
    frame_buffer = camera_model.make_frame_buffer()
    frame_buffer.enable_sensor_response()
    renderer.render(scene_view, frame_buffer)
    huira.write_png(
        output_dir / "11_sensor_noiseless.png", huira.linear_to_srgb(frame_buffer.sensor_response)
    )


if __name__ == "__main__":
    main()
