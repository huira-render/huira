============================
Pixel Coordinate Conventions
============================

TLDR: Huira numbers pixels like OpenCV by default: the center of the top-left pixel is
:math:`(0, 0)`, :math:`x` runs right and :math:`y` runs down. If your calibration or analysis
tools number pixels differently, tell the camera once with ``set_pixel_convention()`` and pass
your numbers through unchanged.

Why it matters
--------------

A pixel is an area, not a point, so "pixel :math:`(3, 7)`" leaves two choices open: which point
of the pixel the integer coordinates name, and which corner of the image the count starts from.
Tools disagree on both, and the disagreement is silent. A principal point calibrated in one
convention and used in another moves the whole image by half a pixel, or a whole pixel, or
mirrors :math:`y` - and every image still looks plausible.

Pixel coordinates are exchanged with a camera in three places:

- the principal point :math:`(c_x, c_y)` passed to ``configure_sensor_from_pitch()``,
  ``configure_sensor_from_size()``, ``set_intrinsics()`` and ``set_intrinsic_matrix()``;
- the positions ``project_point()`` returns;
- the positions ``cast_ray()`` takes.

The camera's pixel convention says what all of them mean.

The conventions
---------------

A convention has two parts: the coordinate of the center of the first pixel
(``PixelCenter``), and the corner the count starts from (``PixelOrigin``). Presets cover common
tools:

.. list-table::
   :header-rows: 1
   :widths: 14 22 10 22 32

   * - Preset
     - First pixel's center
     - :math:`y` runs
     - Centered principal point
     - Used by
   * - ``opencv()``
     - :math:`(0, 0)`
     - down
     - :math:`\left(\frac{W-1}{2}, \frac{H-1}{2}\right)`
     - OpenCV, Kornia, most calibration tools. **The default.**
   * - ``colmap()``
     - :math:`(0.5, 0.5)`
     - down
     - :math:`\left(\frac{W}{2}, \frac{H}{2}\right)`
     - COLMAP, graphics APIs; Huira before v0.9.10
   * - ``matlab()``
     - :math:`(1, 1)`
     - down
     - :math:`\left(\frac{W+1}{2}, \frac{H+1}{2}\right)`
     - MATLAB
   * - ``fits()``
     - :math:`(1, 1)`, bottom-left pixel
     - up
     - :math:`\left(\frac{W+1}{2}, \frac{H+1}{2}\right)`
     - FITS WCS keywords such as ``CRPIX``, DS9

Any other combination of ``PixelCenter`` and ``PixelOrigin`` can be built directly, e.g.
``PixelConvention{PixelCenter::Integer, PixelOrigin::BottomLeft}``.

Usage
-----

C++:

.. code-block:: cpp

    camera_model.set_pixel_convention(huira::PixelConvention::matlab());
    camera_model.set_intrinsics(fx, fy, cx, cy, {1024, 1024}, 50_mm); // cx, cy straight from MATLAB

Python:

.. code-block:: python

    camera_model.set_pixel_convention(huira.PixelConvention.matlab())
    camera_model.set_intrinsics(fx, fy, cx, cy, (1024, 1024), mm(50))

The convention can be set before or after the principal point: a principal point is kept as it
was given and read in whichever convention is current when rendering. It is a per-camera
setting.

What it does not change
-----------------------

The rendered image. If the principal point is left at its default, the center of the sensor, the
image is identical in every convention: only the numbers used to describe positions in it
change. If a principal point is given, the convention decides where on the sensor it is - which
is the point.

The image layout. Images are always stored with their top row first, indexed ``image(x, y)``
with :math:`y` down, and arrive in NumPy as ``(height, width, channels)`` arrays with row 0 at
the top. File formats whose rows run bottom up (FITS, BMP, TGA) are flipped as they are read and
written, so a FITS file written by Huira has its first row at the bottom, as FITS viewers
expect. That is why the ``fits()`` convention counts :math:`y` up from the bottom row.

Internally, Huira works in *sensor coordinates*: :math:`x` right and :math:`y` down from the
sensor's top-left corner, with pixel :math:`i` covering :math:`[i, i+1)`. The renderer, the
star stamps and the ray tracer all use them, and the convention converts only at the three
places above. ``PixelConvention::to_sensor()`` and ``from_sensor()`` do the conversion.

Changes in v0.9.10
------------------

Before v0.9.10 Huira had no setting, and read and wrote pixel coordinates as ``colmap()`` does.
The default is now ``opencv()``. Renders that leave the principal point at its default are
unchanged. Code that passes an explicit principal point, or uses ``project_point()`` or
``cast_ray()``, should either convert its numbers or call
``set_pixel_convention(PixelConvention::colmap())`` to keep the old behaviour.
