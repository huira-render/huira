=========================
The Point Spread Function
=========================

TLDR: every source is blurred by the camera's point spread function (PSF), made of named parts:
the aperture's **diffraction** pattern (on by default) or a **PSF you set** in its place,
**scattered light** (off by default), the **defocus** at the source's own depth (from the
focus, with depth of field on by default) and **veiling glare**. Unresolved sources, such as
stars, are drawn with the whole PSF at their exact position, as far as their light reaches, and
moving ones as line integrals along their path. Resolved bodies are convolved with the PSF in
focus, across the whole frame; path tracing gives them their defocus. ``psf_image()`` shows the
PSF as it is drawn, and ``describe()`` lists its parts.

The parts
---------

.. list-table::
   :header-rows: 1
   :widths: 18 40 42

   * - Part
     - What it is
     - Calls
   * - Diffraction
     - The circular aperture's Airy pattern, averaged evenly over each spectral bin's
       wavelengths. On by default; off, a point in focus puts this light in the pixel it falls
       in.
     - ``enable_diffraction()``, ``diffraction_enabled()``; follows ``set_fstop()``,
       ``set_aperture_diameter()``, the focal length and the pixel pitch
   * - A set PSF
     - A measured PSF, or a PSF class of your own, in place of diffraction until it is cleared,
       whichever way diffraction is switched. It is taken as complete within its extent.
     - ``set_measured_psf()``, ``set_psf<T>()`` (C++), ``clear_psf()``, ``has_set_psf()``
   * - Scattered light
     - Harvey-Shack wings, a fraction :math:`f` of each source's light, in angles (below).
       Added to diffraction or to a set PSF.
     - ``set_scatter()``, ``disable_scatter()``, ``scatter_enabled()``
   * - Defocus
     - A uniform disc, the aperture's image out of focus, of radius :math:`R f |v|` over the
       pixel pitch, for aperture radius :math:`R`, focal length :math:`f` and :math:`v` the
       difference between the source's vergence (one over its depth) and the focus's.
     - The focus setters, ``enable_depth_of_field()``, ``defocus_blur_radius(range)``
   * - Veiling glare
     - A fraction of all the light on the sensor, spread evenly over the frame.
     - ``set_veiling_glare()``, ``disable_veiling_glare()``

Whether resolved bodies are blurred is chosen with ``enable_psf_convolution()`` (on by
default). How far unresolved sources are drawn is the renderer's ``set_unresolved_taper()``.

Scattered light
~~~~~~~~~~~~~~~

A fraction :math:`f` of each source's light leaves the diffraction pattern, or the set PSF, and
spreads as

.. math::

   S(\theta) = K \left(1 + \frac{\theta^2}{\theta_0^2}\right)^{-s/2}
               \left(1 + \frac{\theta^2}{\theta_1^2}\right)^{-1}

for an angle :math:`\theta` from the source: flat within the shoulder angle :math:`\theta_0`,
falling as :math:`\theta^{-s}` beyond it, and as :math:`\theta^{-(s+2)}` beyond the optional
outer angle :math:`\theta_1`. :math:`K` makes it total :math:`f`. Without an outer angle the light
has a finite total only for a slope :math:`s` over 2, so ``set_scatter()`` throws for a slope of
2 or less without one. Being set in angles, it keeps its size on the sky when the focal length
changes. Measured stray light around stars often falls with a slope of 2 to 3.

Set PSFs
~~~~~~~~

``set_measured_psf(data, samples_per_pixel, sampling, focus)`` takes an image of samples,
centered on the source, ``samples_per_pixel`` to a pixel. ``sampling`` says what the samples are
(``PSFSampling``):

- ``PointSampled``: the pattern's intensity at points, as optical design software gives it.
  Huira integrates it over each pixel. Sample it finely, 4 or more to a pixel.
- ``PixelIntegrated``: what a pixel of the simulated sensor collects with its center at each
  sample, such as a star cut from a frame the sensor took. Huira uses the values as they are.

``focus`` (``PSFFocus``) says whether the samples are a point's in focus, which the camera's
focus then blurs as it blurs diffraction (``InFocus``, the default), or already hold the defocus
(``IncludesDefocus``). With the latter, no defocus is added for any source, and resolved bodies,
whose depth of field comes from path tracing, are blurred by it again: their defocus is then
approximate at best, and the camera warns.

The measurement is taken whole: it holds all of a source's light, out to its farthest sample,
and none beyond. Crop it to use less. Measurements rarely capture faint far wings; scattered
light can be added to stand in for them.

``set_psf<T>(args...)`` sets a PSF class of your own, derived from ``PSF``. Its ``evaluate()``
gives its value at an offset in pixels, and it is sampled every 1/16 px out to its
``radius()``, ``DEFAULT_PSF_RADIUS`` (64 px) unless it sets one.

How unresolved sources are drawn
--------------------------------

Every pixel's value is the PSF integrated over that pixel, for the source at its exact position:
nothing is rounded to a grid of subpixel positions, so a source's centroid in the image is where
the source is.

**Diffraction and scattered light** come from tables of the radial profile, built per channel:

- Near the source (out to about 8 to 20 px, following the optics and the blur), a table of the pixel-integrated
  PSF against the offset from the source, read by cubic B-spline interpolation, sampled finely
  enough to follow the finest ring: 1/56 px at f/3.3 with 8.5 µm pixels.
- Further out, the profile integrated over the pixel's projection on the line from the source,
  from two tabulated antiderivatives: four lookups a pixel.
- Beyond, where the rings are washed out by the pixel, the far field averaged over them.

Out of focus, the profile is the in-focus one blurred by the disc, and tabulated the same way.
Scattered light alone, as with a set PSF or none, has no rings, and its tables follow its shoulder
instead: they build in about a tenth of a second and take about a megabyte.

**A set PSF** is put on a grid: the cubic B-spline through its samples, integrated over each
pixel exactly (or, for pixel-integrated samples, taken at each pixel's center). Out of focus,
the grid is blurred by the disc. Without diffraction or a set PSF, the defocus disc is drawn
alone, and in focus the source's light goes into one pixel.

**How far.** Each source is drawn out to where the light it puts in a pixel, as the sensor reads
it out, falls to the taper :math:`\tau`: ``set_unresolved_taper()``, a fraction of the sensor's
read noise (0.001 by default), or of an electron without read noise. From that radius to twice
it the source fades smoothly to nothing, and the light it still has on the sensor is added
evenly over the frame, so that no light is lost and no edge forms. No pixel is off by more than
about :math:`2\tau`. A source is always drawn at least out to where it holds 90% of its light. A
faint star is drawn over a few pixels; Jupiter in the example over hundreds; a taper of 0 draws
every source over the whole frame. Rings are followed only as far as they change a pixel by more
than :math:`\tau`. RGB sensors read out each channel; others, the sum of their channels.

**Moving sources** are drawn as the line integral of their pixels' light along each straight
piece of their path, rather than as a row of stamps, so streaks do not bead. For diffraction and
scattered light the integral comes from the profile's line spread; for a set PSF, from tables
of the grid's line integrals for the piece's direction (4096 directions around the turn), or
directly when too few pieces share a direction to pay for them. With neither diffraction nor a
set PSF, and in focus, there is nothing to integrate: the light is put in the pixel each of the
path's samples falls in.

**Unresolved objects at a finite range** are blurred by the defocus at their own depth. Those
whose blur matches a star's within :math:`2.5 \times 10^{-6}` px share the stars' tables; others
get tables of their own, built by the render that first needs them (even with
``enable_auto_precompute(false)``) and kept while they are used. For a 125 mm f/3.3 lens with
8.5 µm pixels (278.5 px of blur per diopter), each takes about 2 s and 10 MB; focused at
infinity, objects beyond about 2,800 km count as at infinity, and nearer ones that move in depth
can need new tables every frame.

How resolved bodies are blurred
-------------------------------

The path-traced image is convolved with the whole PSF in focus (diffraction or the set PSF, with
scattered light), across the whole frame, so that a bright body's glare reaches every pixel;
path tracing gives bodies their depth of field. The PSF is split into a near part, convolved
through FFTs at full resolution out to at least 128 px, and its smooth far field, convolved on a
coarse grid and interpolated back; the two are blended so that no seam forms.

A uniform sky is kept out of the convolution: it continues beyond the frame, where the
convolution cannot see it, and a sky convolved with a PSF that holds all of its light is the sky
again. Pixels that saw only sky keep it as traced, and only the light geometry adds to the sky,
or takes from it, is spread.

Each FFT spreads its rounding error over the whole frame in proportion to the frame's brightest
pixel. Bodies are convolved in single precision unless that error would reach
``set_fft_rounding_tolerance()`` of the read noise (0.1 by default), and in double precision
otherwise; 0 always uses double. ``set_fft_planning(FftPlanEffort.Measure)`` finds faster
transforms, at a few seconds the first time each size is used in a process.

Inspecting the PSF
------------------

``psf_image(radius, x_offset, y_offset, range)`` gives the light a source puts in each pixel
around it, per channel, exactly as the renderer draws it: the source offset from the center
pixel's center, at a range along the camera's axis (a star without one). It sums to the part of
the light that falls within the image. Resolved bodies are convolved with it, in focus.

Each part can be seen alone, normalized to hold all of its light:

- ``diffraction_image(radius, x_offset, y_offset)``: the aperture's pattern, in focus, whether
  or not it is in use.
- ``scatter_image(radius, x_offset, y_offset)``: the scattered light, in focus.
- ``defocus_image(radius, x_offset, y_offset, range)``: the disc for a source at a range.

In focus, ``psf_image()`` is :math:`(1 - f)` of ``diffraction_image()`` plus :math:`f` of
``scatter_image()``. ``describe()`` lists the parts with their values, the stars' blur and the
tables' memory.

C++:

.. code-block:: cpp

    camera_model.set_focal_length(125_mm);
    camera_model.set_fstop(3.3f);
    camera_model.set_scatter(0.02f, 2.5f, units::Arcsecond(20.0));
    camera_model.set_focus_diopters(0.0025_dpt);

    const auto psf = camera_model.psf_image(16, 0.2f, -0.3f);         // as drawn, for a star
    const auto near = camera_model.psf_image(16, 0.f, 0.f, 1000_m);    // for an object at 1 km
    const auto pattern = camera_model.diffraction_image(16);
    std::cout << camera_model.describe() << "\n";

Python:

.. code-block:: python

    from huira.units import Arcsecond, Meter

    camera_model.set_scatter(0.02, 2.5, Arcsecond(20))
    psf = camera_model.psf_image(16, 0.2, -0.3)
    blur = camera_model.defocus_blur_radius(Meter(1000))  # in pixels, for an object at 1 km
    print(camera_model.describe())

Accuracy
--------

Measured against independent references in the tests:

.. list-table::
   :header-rows: 1
   :widths: 45 55

   * - What
     - Within
   * - Diffraction, pixel by pixel, against exact pixel integrals of the band-averaged Airy
       pattern (f/2 to f/16, 5 to 10 µm pixels, square and rectangular)
     - 0.03% of each pixel near the source, 0.07% further out, 0.1% in the far field
   * - Defocused diffraction, against a Hankel-transform reference
     - :math:`10^{-5}` of the peak
   * - Scattered light alone, in focus or blurred
     - :math:`2 \times 10^{-5}` of the peak, :math:`10^{-4}` at the edge of a blur over 6 px
   * - A set PSF: a Gaussian sampled 4 times a pixel, against its exact pixel integrals
     - :math:`10^{-4}` of the source's light, in any pixel
   * - Streaks with diffraction or a set PSF, against the still source's pixels integrated
       along the path
     - :math:`10^{-4}` of the peak
   * - The taper
     - About :math:`2\tau` in any pixel, with no light lost
   * - Resolved bodies' far glare, from the coarse grid
     - :math:`10^{-4}` of the light where it takes over

Cost
----

For the Jupiter example (125 mm at f/3.3, 1920 x 1080 pixels of 8.5 µm, Visible8, about 1,500
stars), on 2 cores:

.. list-table::
   :header-rows: 1
   :widths: 50 25 25

   * - Settings
     - ``precompute()``
     - Each frame
   * - The defaults: diffraction, in focus, Jupiter convolved
     - 0.8 s
     - 0.2 s
   * - Diffraction and scattered light, a 5.6 px blur, stars streaking 40 px
     - 2.5 s
     - 1.9 s
   * - A measured PSF and scattered light, a 5.6 px blur
     - 1.0 s
     - 0.4 s

The diffraction tables take about 8 to 15 MB, more for a larger blur; scattered light's own about
1 MB.

Limitations
-----------

- Circular apertures only: the diffraction pattern is that of a circle of the aperture's
  bounding radius. Central obstructions and spiders are not modelled; a measured PSF can stand
  in for them.
- Within a spectral bin, wavelengths weigh evenly: a source's spectrum and the quantum
  efficiency inside a bin are not modelled.
- The PSF is the same across the field (no field-dependent aberrations), and defocus is the same
  in every channel.
- Light a source would put in pixels below the taper lands evenly over the frame rather than
  where it falls. No pixel is off by more than about :math:`2\tau`, but a heavily stretched
  frame, or a stack of many, shows halos ending early.
- A set PSF holds no light beyond its extent; wings it does not measure need scattered light.
- Resolved bodies: each path-traced pixel's light is imaged as if it came from the pixel's
  center, which for a PSF wider than a pixel blurs fine detail by up to a pixel more than the
  optics would.
- Unresolved objects at different depths need tables of their own, about 2 s each with the
  Jupiter camera, and those near the camera that move in depth may need new ones every frame.
- Sources are drawn out to twice the frame's diagonal: light from far outside the field (stray
  light) is not.

Changes in v0.9.10
------------------

Before v0.9.10, a PSF had to be asked for (``use_aperture_psf()``); unresolved sources were
then stamped from kernels made for 16 subpixel positions per axis, out to a fixed radius;
defocus under half a pixel was ignored; streaks were rows of stamps; and bodies were convolved
with a kernel of ``set_psf_convolution_radius()``. A bright source showed its stamp's square
edge, streaks beaded, and the light beyond the stamps was lost. Now every part is modelled as
above, and diffraction and depth of field are on by default.

Calls that were removed fail to compile in C++, and raise ``AttributeError`` in Python, with a
message naming what to use instead:

.. list-table::
   :header-rows: 1
   :widths: 45 55

   * - Removed
     - Instead
   * - ``use_aperture_psf(true)``, ``use_aperture_psf(radius, banks)``
     - Diffraction is on by default: ``enable_diffraction()``, and ``clear_psf()`` after a set
       PSF. Its extent is automatic.
   * - ``use_aperture_psf(false)``, ``delete_psf()``
     - ``enable_diffraction(false)``; ``clear_psf()`` for a set PSF
   * - ``has_psf()``
     - ``diffraction_enabled()``, ``has_set_psf()``
   * - ``set_measured_psf(data, samples_per_pixel, radius, banks)``
     - ``set_measured_psf(data, samples_per_pixel, sampling, focus)``: the measurement is taken
       whole
   * - ``set_harvey_shack_scatter(fraction, exponent, r0, radius)``
     - ``set_scatter(fraction, slope, shoulder_angle, outer_angle)``, in angles: an angle
       :math:`\theta` is :math:`f\theta` on the sensor
   * - ``disable_harvey_shack_scatter()``
     - ``disable_scatter()``
   * - ``set_psf_convolution_radius()``
     - Nothing: bodies are convolved across the whole frame
   * - ``get_psf_radius()``, ``get_psf_kernel()``, ``get_psf_convolution_kernel()``,
       ``get_psf_wings_kernel()``
     - ``psf_image()``, ``diffraction_image()``, ``scatter_image()``
   * - ``set_diopters()``, ``get_diopters()``, ``get_focus_distance()``
     - ``set_focus_diopters()``, ``focus_diopters()``, ``focus_distance()``

``defocus_blur_radius()`` now gives the blur sources are drawn with, in pixel widths, from
:math:`10^{-4}` px, where it was 0 under half a pixel, and takes an optional range. Classes
derived from ``PSF`` no longer have stamps: ``set_polyphase_size()``, ``get_kernel()`` and
``generate_convolution_kernel()`` are gone, and ``radius()`` and ``set_radius()`` remain.
``AiryDisk`` no longer takes ``banks``, and ``Aperture::make_psf()`` is gone.
