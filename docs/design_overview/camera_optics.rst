==========================
Precomputing Camera Optics
==========================

TLDR: a camera builds the tables its optics need (the PSF's tables or grid, and the
convolution of resolved bodies with it) and the tables of its geometry (each pixel's ray direction and solid angle) when they
are first needed, not when it is configured, so setters are cheap. Call ``precompute()`` once the
camera is set up, so that the first render is not slower than the rest. If you don't, the
render does it for you and says so in the log; ``enable_auto_precompute(false)`` makes it throw
instead.

What is precomputed
-------------------

Some of a camera's optics are applied with tables derived from its settings (see
:doc:`psf_model` for the PSF they hold). Building them can take a second or two. Each is built
from the settings it depends on, and rebuilt only when one of those changes:

.. list-table::
   :header-rows: 1
   :widths: 24 32 44

   * - Table
     - Used for
     - Depends on
   * - The PSF's tables
     - Unresolved sources, still and moving, with the aperture's diffraction pattern (the
       default) and scattered light; with a PSF set in its place, or neither, the scattered
       light alone
     - The focal length, pixel pitch and aperture (f-stop), the scattered light
       (``set_scatter()``), whether diffraction is on (``enable_diffraction()``), and the blur
       a star gets from the focus and depth of field. A larger frame needs them to reach
       further.
   * - A set PSF's grid
     - Unresolved sources, with a PSF set with ``set_psf()`` or ``set_measured_psf()``, or,
       without diffraction or a set PSF, the defocus blur alone
     - The PSF, the pixels' shape, and a star's blur
   * - Tables and grids for other blurs
     - Unresolved objects at a range whose defocus differs from a star's: built by the render
       that first needs them, and kept while used
     - As above, and the object's depth
   * - The convolution of resolved bodies
     - The path-traced image (unless ``enable_psf_convolution(false)``): the whole PSF in focus,
       across the frame
     - The PSF's tables in focus, a set PSF's grid, and the resolution
   * - Pixel geometry: each pixel corner's ray direction (with distortion), and each pixel's
       solid angle
     - Tracing rays through the pixels, and converting radiance to the power each pixel
       receives
     - The focal length, sensor (resolution and pitch), intrinsics, distortion, pixel
       convention and axis convention
   * - View frustum
     - Culling unresolved sources and geometry outside the view
     - As for the pixel geometry, along the image's boundary only

Veiling glare needs nothing precomputed.

Only what the current settings use is built: with a PSF set in place of diffraction, for
example, no diffraction tables are built, and changing the f-stop does not rebuild them until
they are needed again. A refocus that leaves a star's blur under :math:`10^{-4}` px, where it
counts as in focus, rebuilds nothing. Settings nothing depends on, such as veiling glare or the sensor's noise,
never make the camera out of date, and neither does setting a value to what it already is.

Getters never build the per-pixel tables, and give the same values whether they have been
built or not: ``cast_ray()`` computes its ray directly, and ``pixel_radiance_to_power()``
computes the pixel the way the table does until the table is built. ``in_fov()``,
``try_project_point()`` and ``view_frustum()`` build the frustum, which takes only the image's
boundary, when first called after a change.

Usage
-----

C++:

.. code-block:: cpp

    camera_model.set_focal_length(50_mm);
    camera_model.set_fstop(8.f);
    camera_model.precompute(); // the diffraction tables (the default PSF) are built here

    renderer.render(scene_view, frame_buffer); // no building here

Python:

.. code-block:: python

    camera_model.set_focal_length(mm(50))
    camera_model.set_fstop(8)
    camera_model.precompute()  # the diffraction tables (the default PSF) are built here

``is_precomputed()`` says whether anything is out of date.

What a render does
------------------

Before it starts, a render checks that the camera's tables are up to date. If they are, it goes
ahead. If they are not:

- By default it builds them, as ``precompute()`` would, and logs how long that took. That is a
  warning the first time after ``precompute()`` was called, since the camera has then changed
  since it was last precomputed, and information otherwise, so that a camera changed on every
  frame (a focus pull, say) does not warn on every frame.
- With ``enable_auto_precompute(false)`` it throws. Use this when no render may include that time,
  such as a timed or hardware-in-the-loop run, and call ``precompute()`` after every change.

The image is the same whichever way the tables were built, and whatever order the settings
were made in.

Setters check their own values straight away: ``set_scatter()`` with a slope of 2 or less and
no outer angle, for example, throws then rather than at the render. Settings that make sense
alone but not together, such as a lens distortion that folds over (or, for the OpenCV model,
reaches the pole of its rational radial factor) inside the image at the current focal length
and sensor, are reported by ``precompute()`` or the render.

Renders on several threads may share a camera: the first to find it out of date builds the
tables while the others wait. Changing a camera while it is rendering is not supported.

Changes in v0.9.10
------------------

Before v0.9.10, setters built kernels as they went: ``use_aperture_psf()`` built the Airy
stamps, every later change to the focal length or f-stop built them again, and every focus
change rebuilt the defocus stamps. Convolution kernels were built during the first render that
used them. Now no setter builds anything, and the time is spent once, in ``precompute()`` or
the first render. The stamps and kernels themselves are gone: see :doc:`psf_model` for what
replaced them, and for the calls that were removed with them.

The geometry tables were rebuilt by every setter that changes the camera's geometry: setting the
focal length of a 4096 x 4096 camera with distortion took about 8 seconds, every time, and
constructing a camera built tables for its default 1024 x 1024 sensor. Now setters take
microseconds, and the tables are built once, in one pass over the pixel corners, in about a third
of the time (each corner's direction was found five times). With distortion, ``cast_ray()``
computes its ray exactly rather than interpolating it, and the tables are computed in double
precision; rays and each pixel's radiometry change by less than a millionth.

``Aperture`` now only describes the aperture's shape (``rasterize_shape()``) and its size:
``build_defocus_kernel()``, ``get_defocus_kernel()``, the other ``get_defocus_*()`` functions
and ``make_psf()`` are gone. The camera's ``defocus_blur_radius()`` gives the blur radius.
``CameraModel::psf_kernel_version()`` is gone too, since the renderer keeps no copies of the
camera's tables.

Three defaults change what is rendered in v0.9.10:

- The aperture's diffraction pattern is on (``enable_diffraction()``). Before, stars without
  ``use_aperture_psf()`` put all their light in one pixel; ``enable_diffraction(false)`` does
  that now.
- The PSF and scattered light blur resolved bodies (``enable_psf_convolution(false)`` turns that
  off), across the whole frame, and always apply to unresolved sources.
- Depth of field is on. With the focus at infinity, the default, only bodies near the camera
  blur (for 50 mm at f/2.8 and 8.5 um pixels, closer than about 100 m), and they need more
  samples per pixel to look clean. Unresolved sources' defocus now follows
  ``enable_depth_of_field()`` too.
