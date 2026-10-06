=============
Saving Images
=============

TLDR: a sensor response is saved as the sensor's digital numbers (DN), in every format that can
hold them, following each format's own standard, and reads back exactly as it was. Anything else
is saved as it is, or stretched over the format's range. Nothing is converted between color
encodings unless you call ``linear_to_srgb()``.

Data and pictures
-----------------

A camera's sensor response (``FrameBuffer::sensor_response()``) holds each pixel's DN divided by
the largest DN the sensor can output, :math:`2^{\text{bits}} - 1`, a value in [0, 1], and the image
records the sensor's bit depth (``Image::sensor_bit_depth()``; see :doc:`sensor_modeling`). The
writers use it to save the DN themselves, which is what a camera produces and what analysis
software expects: a 12-bit sensor's pixels are 0 to 4095.

Any other image (received power, a depth map, a picture made with ``linear_to_srgb()``) has no
sensor bit depth, and is saved as before: its values as they are, in a float format, or
stretched over an integer format's range (0 to 1 becomes 0 to 255, or 0 to 65535).

To save a sensor response stretched over the format's range instead, for viewing, set
``ImageBundle::scaling`` (or ``write_image_fits()``'s ``scaling``) to ``PixelScaling::FullRange``.

How each format holds a sensor's DN
-----------------------------------

The DN are kept exactly when the file has at least as many bits per sample as the sensor. When it
has fewer, the lowest bits are dropped, as by a camera whose output is narrower than its ADC: a
12-bit sensor's 4095 is 255 in an 8-bit file.

.. list-table::
   :header-rows: 1
   :widths: 18 52 30

   * - Format
     - A 12-bit sensor's DN
     - Standard
   * - FITS, integer
     - As they are, 0 to 4095, in a 16- or 32-bit file. In an 8-bit file, the lowest 4 bits are
       dropped and ``BSCALE = 16`` scales the stored values back to DN, so every FITS reader still
       sees DN (in steps of 16).
     - FITS ``BZERO`` / ``BSCALE``
   * - FITS, float
     - As they are, as floating-point values.
     - FITS
   * - TIFF
     - As they are, 0 to 4095 in 16 bits, with the ``MaxSampleValue`` tag set to 4095.
     - Baseline TIFF tag; usual scientific practice
   * - PNG
     - Scaled up to 16 bits by left-bit replication (each 12-bit value repeated into the lower
       bits: 4095 is 65535), with an ``sBIT`` chunk of 12, so that DN = value >> 4 exactly.
     - The PNG specification requires samples to use the whole range, and ``sBIT`` records the
       original bit depth
   * - JPEG
     - Not kept: JPEG is lossy and 8-bit, so it is for pictures only. Values are stretched over
       0 to 255.
     -

Huira's readers undo each of these: ``read_image_fits()``, ``read_image_png()`` and
``read_image_tiff_rgb()`` (and the mono versions) return the sensor response as it was, with its
sensor bit depth, so it reads back exactly.

FITS keywords
-------------

The FITS writer sets the keywords that describe the data from the image it writes. Those given in
the ``FitsMetadata`` (which may have been read from another file) are ignored:

- ``BZERO`` and ``BSCALE``: stored values to physical values (DN for a sensor response).
  Unsigned 16- and 32-bit images are stored signed, offset by ``BZERO``, as the standard
  prescribes.
- ``SATURATE``: the ADC's largest DN, :math:`2^{\text{bits}} - 1`; for another integer image, the
  type's largest value. A float image that is not a sensor response keeps ``FitsMetadata``'s.
- ``ADCBITS``: the sensor's bit depth (not a standard keyword). ``read_image_fits()`` divides by
  :math:`2^{\text{ADCBITS}} - 1` to return the sensor response, whatever the BITPIX.
- ``DATAMIN`` and ``DATAMAX``: the range of the defined pixels, in physical values. Not written
  if every pixel is undefined.
- ``BLANK``: see below.
- ``BUNIT`` is ``'adu'`` for a sensor response, unless ``FitsMetadata`` gives a unit.

Missing data
------------

A pixel that is NaN or infinite has no value to save.

- **FITS, float:** written as it is; NaN is the standard's mark for an undefined value.
- **FITS, integer:** marked by ``BLANK``, a value the data never take. A 12-bit sensor in a
  16-bit file leaves 4096 to 65535 unused, so ``BLANK`` is the type's largest value and no pixel
  changes. When the data use every value (a 16-bit sensor in a 16-bit file, or an image that is
  not a sensor response), ``BLANK`` is 0, and valid pixels are written as at least one step above
  it. ``read_image_fits()`` returns undefined pixels as NaN.
- **PNG, TIFF, JPEG:** these formats have no way to mark missing data, so such pixels are written
  as 0, and a warning gives how many there were of each kind.

Color encoding
--------------

Huira renders linear values, and nothing in it converts between color encodings by itself:
readers return the values a file holds, and writers write the values they are given.
``ImageBundle::color_space`` records the encoding the values have, and writers label the file
with it (a PNG's ``gAMA`` or ``sRGB`` chunk). It is ``Linear`` by default.

For a picture to view, convert explicitly: ``linear_to_srgb()`` returns the sRGB values, labelled
sRGB. Its result is no longer a sensor's DN, so it has no sensor bit depth, and is stretched over
the format's range when saved. Readers label what they read: sRGB for JPEG, BMP and TGA, and for
TIFFs of integers that are not a sensor's DN; linear for float TIFFs, Radiance HDR and sensor
data; and as tagged for PNG.

Usage
-----

A monochrome sensor (any spectral type but RGB) gives an ``Image<float>``; an RGB sensor gives
an ``Image<RGB>``, whose channels ``get_channel()`` returns, with the sensor bit depth, for
formats that take one channel such as FITS.

C++:

.. code-block:: cpp

    renderer.render(scene_view, frame_buffer);
    const auto& response = frame_buffer.sensor_response(); // RGB here

    // The sensor's DN, for analysis:
    huira::write_image_fits("frame_red.fits", response.get_channel(0), 16);
    huira::ImageBundle<huira::RGB> data(response);
    data.bit_depth = 16;
    huira::write_image_tiff("frame.tif", data); // 0 to 4095 for a 12-bit sensor

    // A picture to view:
    huira::write_image_png("frame.png", huira::linear_to_srgb(response));

Python:

.. code-block:: python

    response = frame_buffer.sensor_response
    huira.write_fits("frame_red.fits", response.get_channel(0), bit_depth=16)
    huira.write_png("frame.png", huira.linear_to_srgb(response))

    image, metadata = huira.read_fits("frame_red.fits")  # the sensor response, as it was

Changes in v0.9.10
------------------

- A sensor response is saved as DN in FITS (integer and float), TIFF and PNG, as above. Before,
  TIFF and PNG stretched it over the format's range, a float FITS held the values in [0, 1], and
  an integer FITS with fewer bits than the sensor clipped every DN above its range to the largest
  value (an 8-bit file of a 12-bit sensor saturated at 6% of full scale) while ``SATURATE`` gave
  the sensor's range.
- The FITS writer sets ``BZERO``, ``BSCALE``, ``BLANK``, ``SATURATE``, ``DATAMIN`` and
  ``DATAMAX`` from the image. Before, metadata read from another file could carry them over: a
  ``BLANK`` from a file with undefined pixels made every 0 DN pixel of the next file read back as
  undefined, and an old ``SATURATE`` normalized the next one wrongly.
- Integer FITS images with undefined pixels keep 0 DN when the data leave a value spare, and an
  image with no defined pixels no longer writes a ``DATAMIN`` that cannot be read back.
- PNG, TIFF and JPEG write NaN and infinities as 0, with a warning. Converting them to an integer
  was undefined behaviour.
- TIFF rounds values stretched over its range, as PNG and JPEG do; it truncated them.
- A gray image is written to PNG as gray, not as three equal channels.
- ``ImageBundle`` is labelled ``Linear`` by default; it was labelled sRGB, so linear values
  written without ``linear_to_srgb()`` were labelled as sRGB. Radiance HDR files are read as
  linear; they were labelled sRGB, so their textures were converted from sRGB on loading.
- ``write_image_fits()`` takes an optional ``scaling``, and ``read_fits()`` and ``write_fits()``
  are new in Python.
