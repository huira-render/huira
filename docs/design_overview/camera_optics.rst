==========================
Precomputing Camera Optics
==========================

TLDR: a camera builds the kernels its optics need (PSF stamps, defocus stamps, convolution
kernels) when they are first needed, not when it is configured. Call ``precompute()`` once the
camera is set up, so that the first render is not slower than the rest. If you don't, the
render does it for you and says so in the log; ``set_auto_precompute(false)`` makes it throw
instead.

What is precomputed
-------------------

Some of a camera's optics are applied with kernels derived from its settings. Building them
can take a while: a large Airy pattern or a frame-sized convolution kernel takes seconds. Each
kernel is built from the settings it depends on, and rebuilt only when one of those changes:

.. list-table::
   :header-rows: 1
   :widths: 24 32 44

   * - Kernel
     - Used for
     - Depends on
   * - PSF stamps
     - Unresolved sources in focus, when the camera has a PSF
     - For ``use_aperture_psf()``: the focal length, pixel pitch and aperture (f-stop), and the
       stamp size. For ``set_psf()`` or ``set_measured_psf()``: the PSF given.
   * - Defocus stamps
     - Unresolved sources out of focus (a blur of half a pixel or more)
     - The focal length, pixel pitch, aperture and focus
   * - Convolution kernel, and its spectrum
     - ``enable_psf_convolution()``: the path-traced image, and unresolved sources out of
       focus
     - Whatever the PSF depends on, ``set_psf_convolution_radius()`` and
       ``set_harvey_shack_scatter()``. The spectrum also depends on the resolution.
   * - Scattered-light wings, and their spectrum
     - ``enable_psf_convolution()`` with Harvey-Shack scatter: unresolved sources in focus
     - The PSF's stamp size, ``set_psf_convolution_radius()`` and
       ``set_harvey_shack_scatter()``. The spectrum also depends on the resolution.

Veiling glare needs nothing precomputed.

Only what the current settings use is built: while unresolved sources are out of focus, for
example, the PSF stamps are not built (the defocus stamps replace them), and changing the
f-stop does not rebuild them until they are needed again. Settings nothing depends on, such as
the pixel convention, distortion or veiling glare, never make the camera out of date, and
neither does setting a value to what it already is.

Usage
-----

C++:

.. code-block:: cpp

    camera_model.set_focal_length(50_mm);
    camera_model.set_fstop(8.f);
    camera_model.use_aperture_psf();
    camera_model.precompute(); // the Airy stamps are built here

    renderer.render(scene_view, frame_buffer); // no building here

Python:

.. code-block:: python

    camera_model.set_focal_length(mm(50))
    camera_model.set_fstop(8)
    camera_model.use_aperture_psf()
    camera_model.precompute()

``is_precomputed()`` says whether anything is out of date.

What a render does
------------------

Before it starts, a render checks that the camera's kernels are up to date. If they are, it goes
ahead. If they are not:

- By default it builds them, as ``precompute()`` would, and logs how long that took. That is a
  warning the first time after ``precompute()`` was called, since the camera has then changed
  since it was last precomputed, and information otherwise, so that a camera changed on every
  frame (a focus pull, say) does not warn on every frame.
- With ``set_auto_precompute(false)`` it throws. Use this when no render may include that time,
  such as a timed or hardware-in-the-loop run, and call ``precompute()`` after every change.

The image is the same whichever way the kernels were built, and whatever order the settings
were made in.

Setters check their own values straight away: ``use_aperture_psf(0)``, for example, throws
then rather than at the render. Settings that make sense alone but not together, such as PSF
convolution with neither a PSF nor scattering, are reported by ``precompute()`` or the render.

Renders on several threads may share a camera: the first to find it out of date builds the
kernels while the others wait. Changing a camera while it is rendering is not supported.

Changes in v0.9.10
------------------

Before v0.9.10, setters built kernels as they went: ``use_aperture_psf()`` built the Airy
stamps, every later change to the focal length or f-stop built them again, and every focus
change rebuilt the defocus stamps. Convolution kernels were built during the first render that
used them. Now no setter builds anything, and the time is spent once, in ``precompute()`` or
the first render.

Building kernels lazily changes no rendered image, except that the defocus blur radius is
computed in double precision, which moves defocused stamps by around a billionth of their
value. (Other changes in v0.9.10 do change images; see :doc:`pixel_conventions` and
:doc:`aperture`.)

The defocus stamps have moved from the aperture to the camera: ``Aperture`` now only describes
the aperture's shape (``rasterize_shape()``), and ``build_defocus_kernel()``,
``get_defocus_kernel()`` and the other ``get_defocus_*()`` functions are gone. The camera's
``defocus_blur_radius()`` gives the blur radius as before. ``CameraModel::psf_kernel_version()``
is gone too, since the renderer no longer keeps its own copies of the kernels.
