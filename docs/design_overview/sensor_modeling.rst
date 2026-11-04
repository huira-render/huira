======================================================
Sensor Modeling: From Spectral Flux to Digital Numbers
======================================================

This document outlines the steps to model a digital image sensor's response to incoming spectral flux, converting it into Digital Numbers (DN) that represent pixel values in an image. The process involves several key stages, each accounting for different physical and electronic phenomena.

This is not a comprehensive sensor model but captures the essential physics and assumptions commonly used in image sensor simulations, and which are implemented in the simplest included sensor model in Huira.


1. Photon Conversion (Input):
-----------------------------

Starting with a given pixel's receieved spectral energy (joules per wavelength bin), convert to *Photon Counts* using the photon energy :math:`E = hc/\lambda`.

.. math::

    N_{photons}(\lambda) = \frac{E_{received}(\lambda) \cdot \lambda}{hc}



2. Quantum Efficiency (QE):
---------------------------

A sensor will have a *Quantum Efficiency* curve that describes the probability of a photon at wavelength :math:`\lambda` generating an electron. Multiply the photon counts (per wavelength) by the QE curve to get the expected number of signal electrons.

.. math::

    N_{electrons\_signal} = \sum_{\lambda} (N_{photons}(\lambda) \times QE(\lambda))

Through a filter, the photons are also multiplied by the filter's transmission in each bin,
:math:`T(\lambda)`, and, if the filter has a flat, by the pixel's value in it, :math:`V(x, y)`
(see Filters and flats, below):

.. math::

    N_{electrons\_signal}(x, y) = V(x, y) \sum_{\lambda} (N_{photons}(\lambda) \times QE(\lambda) \times T(\lambda))



3. Dark Current (Thermal Noise):
--------------------------------

Due to thermal energy, the sensor generates electrons even in the absence of light. This is characterized by a *Dark Current* (electrons per second) that depends on the sensor temperature :math:`T`. Multiply the Dark Rate by the exposure time to get the expected number of dark electrons.


.. math::

    N_{electrons\_dark} = \text{DarkRate}(T) \times t_{exposure}

The dark rate is the same for every pixel, or each pixel's own, from a dark current map (see
Calibration maps, below).


4. Shot Noise (The randomness of light/matter):
-----------------------------------------------

Photon arrival and dark current generation are Poisson processes. You must "perturb" the ideal counts using a random number generator.  This is particularly important for low-light scenarios.

.. math::

    N_{electrons\_total} \sim \text{Poisson}(N_{electrons\_signal} + N_{electrons\_dark})

Huira draws this exactly for means below 20 electrons, where the Poisson distribution's
discreteness and skew matter (faint stars, dark sky), and from a normal distribution of the same
mean and variance, rounded to a whole number of electrons, above it.


5. Photosite Well Capacity (Saturation):
----------------------------------------

Each pixel has a maximum number of electrons it can hold, known as the *Well Capacity*. If the total number of electrons exceeds this capacity, it saturates at the maximum value.  In reality, this often means blooming into adjacent pixels, but for simplicity we just clamp the value here.

.. math::
    N_{electrons\_total} = \min(N_{electrons\_total}, \text{WellCapacity})


6. Read Noise (Electronics):
----------------------------

When reading out the pixel values, the sensor electronics introduce additional noise, typically modeled as Gaussian (Normal) noise with a standard deviation :math:`\sigma_{read}` (in electrons).

.. math::

    N_{electrons\_read} = N_{electrons\_total} + \text{Normal}(0, \sigma_{read})

It is added after the full well has clamped the charge, so saturated pixels have read noise too.

**Row and column noise.** Read noise is each pixel's own. Some of the readout's noise is
shared instead, by the pixels read through the same circuits at the same time, and shows as
banding that changes from frame to frame:

- *Row noise* (``set_sensor_row_noise()``): a CMOS sensor converts a whole row at once, through
  ADCs that share a ramp, references and supplies, whose fluctuations shift the whole row. A CCD
  reads each row out through its output amplifier, whose slow drift shifts the row too. It is
  the larger of the two in most CMOS sensors.
- *Column noise* (``set_sensor_column_noise()``): each column of a CMOS sensor is read through
  its own amplifier and ADC, whose slow (1/f) noise shifts the whole column. It is usually
  smaller; a CCD, which has nothing of its own for each column, has none.

Both are standard deviations in electrons, 0 by default, and independent of each other: a CMOS
sensor has both. In each readout, every row's pixels shift together by one draw from a normal
distribution of the row noise's standard deviation, and every column's by one of the column
noise's, added with the read noise:

.. math::

    N_{electrons\_read}(x, y) = N_{electrons\_total} + \text{Normal}(0, \sigma_{read})
        + \text{Normal}_y(0, \sigma_{row}) + \text{Normal}_x(0, \sigma_{column})

For RGB, a pixel's three channels shift together. Rows run along the sensor's x axis, the
image's rows, and turn with it (``set_sensor_roll()``); for a sensor read out along y, set the
column noise instead. These are temporal: a fixed pattern of rows or columns, the same in every
frame, is part of the bias, and is given by a bias map (see Calibration maps). Each has random
numbers of its own, so turning one on changes nothing else's noise (see Random numbers).


7. Gain & ADC (Quantization):
-----------------------------

An Analog-to-Digital Converter (ADC) converts the number of electrons to Digital Numbers (DN). This involves applying a *System Gain* (:math:`e^-/ADU`, where ADU is the Analog-to-Digital Unit) and adding a *Bias* level to avoid negative values. The result is quantized to a whole number of DN, and clamped to the range the ADC can output, from 0 to :math:`\text{MaxDN} = 2^{\text{bit depth}} - 1`:

.. math::

    DN = \text{clamp}\left( \text{round}\left( \frac{N_{electrons\_read}}{\text{Gain}} + \text{Bias} \right), 0, \text{MaxDN} \right)

An ideal ADC rounds to the nearest DN, as here. Many real ADCs round down instead, which differs only by a constant half a DN that bias calibration absorbs. The bias is the same for every pixel, or each pixel's own, from a bias map (see Calibration maps, below).

The sensor response (``FrameBuffer::sensor_response()``) holds :math:`DN / \text{MaxDN}`, a value in [0, 1], and records the bit depth (``Image::sensor_bit_depth()``), so the image writers can write the DN themselves: see :doc:`image_output`. Every DN is recovered exactly from the stored value, as :math:`\text{round}(\text{value} \times \text{MaxDN})`, for bit depths up to 24, the most a sensor accepts (``SensorModel::MAX_BIT_DEPTH``). This is a limit of how the response is stored, as a single-precision float; it is far beyond real ADCs, which reach 16 to 18 bits.


Saturation
----------

A pixel saturates at whichever limit it reaches first: the full well, at :math:`\text{WellCapacity} / \text{Gain} + \text{Bias}` DN, or the ADC, at MaxDN. Both happen in real cameras. The default gain is matched to the full well: :math:`\text{WellCapacity} / (\text{MaxDN} - \text{Bias})` = 20000 / (4095 - 10) = 4.896 e⁻/DN would make a full well reach exactly the ADC's largest DN. As in many cameras, it is set slightly lower, at 4.88 e⁻/DN, so that the ADC saturates just before the well is full (at 19935 e⁻): nearly the whole well is used, saturated pixels read exactly MaxDN, and the response near full well, which is not quite linear in a real sensor, is cut off. The conversion gain in e⁻/DN is the inverse of the "overall system gain" *K*, in DN/e⁻, of the EMVA 1288 standard.


Calibration maps
----------------

A real sensor's pixels differ: each has its own dark current, and the bias varies across the
sensor in fixed patterns (columns, gradients, the corners near an amplifier). Calibration frames
measure this. A *master bias* is the mean, or median, of many frames taken with no exposure, so
each pixel reads its bias; a *master dark* is the same for frames taken with the shutter closed,
so each pixel reads its bias and its dark current's charge. A simulated sensor can be given both,
as maps:

- ``set_sensor_dark_current_map()`` sets each pixel's dark current, in e⁻/s, in place of the one
  for all (``set_sensor_dark_current()``). ``dark_current_from_master_dark()`` makes the map from a
  master dark and a master bias (an image, or one value for every pixel):

  .. math::

      \text{DarkRate}(x, y) = \frac{(\text{Dark}(x, y) - \text{Bias}(x, y)) \cdot \text{Gain}}{t_{exposure}}

  with the conversion gain and the master dark's exposure time of the camera that took them. A
  pixel below the master bias, which noise in either does near zero dark current, has a dark
  current of 0; a warning gives the number of them.
- ``set_sensor_bias_map()`` sets each pixel's bias, in DN, in one of two forms
  (``BiasMapKind``):

  - ``Absolute`` (the default): each pixel's bias, as a master bias gives it. It replaces the bias
    level (``set_sensor_bias_level()``), which is not used while the map is set.
  - ``Relative``: each pixel's offset from the bias level, the bias's structure alone, averaging
    about 0. The bias level still applies, and changing it moves the whole map. A warning points
    out offsets that average more than half a DN from 0, which look like a map of the bias
    itself.

Each map is at the sensor's resolution, in the sensor response's layout: top row first, as every
image is stored (from numpy, ``huira.Image_f32(array, origin=...)`` says which row of the array is
the top; see :doc:`pixel_conventions`). A pixel's dark current is added to its signal electrons
before shot noise, which draws the two together, and its bias is added by the ADC, as for the
single values. For RGB, each channel of a pixel gathers its dark current and has its bias. The
maps are not noise, so they apply with noise off too; they are among the sensor's imperfections,
and are off with them (see Noise and imperfections). The camera keeps them until they are
cleared (``clear_sensor_dark_current_map()``, ``clear_sensor_bias_map()``) or replaced, and a
copy of a sensor shares them. ``sensor_dark_current_map()`` and ``sensor_bias_map()`` give each
pixel's value, from the map or the single value; the camera's ``describe()`` summarizes them.

Values are checked when set: a dark current must be non-negative and finite, and each pixel's
bias (a relative map's added to the bias level) finite and in :math:`[0, \text{MaxDN})`, as for
the bias level; a later bias level or bit depth that would take a pixel out of that range is
refused. A map that is no longer at the sensor's resolution, after the camera's sensor is
configured again, stays set, with a warning, and a readout throws until it is replaced or
cleared.

Calibration frames are in DN, and how a file holds them depends on what wrote it. Huira's
readers return values normalized to about [0, 1], and record the bit depth they were normalized
by where it is known (``Image::sensor_bit_depth()``; ``sensor_bit_depth`` in Python): the values
are then :math:`DN / (2^{\text{bits}} - 1)`, and are turned back into DN exactly. So:

- **Integer FITS files**, from any software, just work: FITS integers are counts, and the reader
  records their bit depth (see :doc:`image_output`). So do Huira's own sensor responses, and
  frames it saved.
- **16- and 32-bit integer TIFFs** just work too, for the same reason. An 8-bit TIFF is a
  picture, and too coarse to be a calibration frame.
- **Float FITS and TIFF files** are read as they are, so their values must be DN. Many hold DN
  (astropy, MaxIm DL and professional pipelines). Siril and PixInsight save normalized floats
  instead, :math:`DN / 65535`: multiply them by 65535 (``np.asarray(image) * 65535`` into a new
  ``Image_f32``).
- **PNG, JPEG and other pictures** hold no DN at all: they are display images, often
  sRGB-encoded.
- **Cameras that scale their DN to fill 16 bits** (many 12- and 14-bit cameras' software
  multiplies by 16 or 4) give files whose DN are the scaled ones: give
  ``dark_current_from_master_dark()`` the conversion gain per file DN, not per ADC DN.

``dark_current_from_master_dark()`` and an absolute bias map refuse an image without a bit depth
whose values are all at most 1 DN: no real master dark or bias in DN is, as the bias alone is
many DN, so it is normalized, and taking it as DN would give a dark current and bias of about 0.
An image of zeros is accepted. A relative bias map is offsets in DN, so one with a sensor bit
depth is refused. Nothing can tell a float file in DN from one in some other unit: that, and the
conversion gain's DN, are for the user to know.

From C++, the maps are on the sensor (``SensorModel::set_dark_current_map()``,
``set_bias_map()``), and through the camera's handle as above. In Python::

    import numpy as np
    import huira

    master_dark, _ = huira.read_fits("master_dark.fits")  # 30 s exposures
    master_bias, _ = huira.read_fits("master_bias.fits")
    rates = huira.dark_current_from_master_dark(
        master_dark, master_bias, conversion_gain=1.5, exposure_time=huira.units.Second(30))
    camera_model.set_sensor_dark_current_map(rates)
    camera_model.set_sensor_bias_map(master_bias)

    # A float master from Siril or PixInsight holds DN / 65535. np.asarray() is top row first:
    siril_bias, _ = huira.read_fits("master_bias_siril.fits")
    siril_bias = huira.Image_f32(np.asarray(siril_bias) * 65535, origin=huira.PixelOrigin.TopLeft)

    # Or a bias pattern made in numpy, as offsets from the bias level:
    columns = np.tile(np.random.default_rng(1).normal(0, 2, 1024), (1024, 1))
    camera_model.set_sensor_bias_map(
        huira.Image_f32(columns, origin=huira.PixelOrigin.TopLeft), huira.BiasMapKind.Relative)


PRNU and hot pixels
-------------------

Without calibration frames, a sensor's two most visible fixed patterns can be made up instead,
from parameters, each the same in every frame:

- *PRNU* (photo response non-uniformity; ``set_sensor_prnu()``): each pixel's signal electrons
  are multiplied by a gain of its own, drawn once from a normal distribution of mean 1 and the
  given standard deviation (0.01 is 1%; never below 0). It applies to the light only, not the
  dark current.
- *Hot pixels* (``set_sensor_hot_pixels(fraction, dark_current, spread)``): each pixel is hot with
  probability ``fraction``, and a hot pixel gathers a dark current of its own, in e⁻/s, on top of
  the sensor's (the single value, or the dark current map): ``dark_current`` times
  :math:`e^{\text{spread} \cdot N(0, 1)}`, a log-normal distribution whose median is
  ``dark_current``. A spread of 0 gives every hot pixel the same; 0.5 spreads them about 1.6x
  either way. A larger fraction keeps a smaller one's hot pixels, with the same dark currents,
  and adds more.

Both are made from the noise seed (``set_sensor_noise_seed()``), on random numbers of their own,
keyed by the seed and the pixel but not the readout: a seed is a detector, with the same
patterns in every frame, and another seed is another detector. They are made again when the
seed, their parameters or the resolution change. Being fixed, they apply with noise off too;
they are among the sensor's imperfections, and are off with them (see Noise and imperfections).
``sensor_prnu_map()`` and ``sensor_hot_pixel_map()`` give them, and ``describe()`` lists them.
Both are off by default.

They stand in for calibration frames, so don't use both for the same thing: a flat from a real
flat field (see Filters and flats) already holds the camera's PRNU, and a dark current map from
a real master dark its hot pixels. PRNU's normal tail can also take a pixel's response, QE times
its gain, a little above 1 electron per photon when the QE is near 1, which isn't warned of.


Filters and flats
-----------------

A camera has a filter wheel (``add_filter()``, ``select_filter()``, by index or by name;
``clear_filters()``), empty by default, which is a clear filter. Each filter has a transmission,
the fraction of the light it passes in each bin, and may have a *flat*: each pixel's response
through it relative to the principal point's. The selected filter's transmission multiplies the
quantum efficiency, and its flat each pixel's signal electrons, before shot noise (see Quantum
Efficiency, above); the dark current and bias are not affected, as they are not light. The flat
is among the sensor's imperfections, and is off with them; the transmission is not (see Noise and
imperfections). A frame is read out through the filter selected when it is rendered. The
camera's ``describe()`` lists the wheel.

**What a flat is.** A flat field is a frame of a uniformly lit scene (a dome, a twilight sky, a
lamp through a diffuser), with the bias and dark subtracted. Every pixel receives the same light,
so each pixel's value is its response, times the scene's brightness and the exposure, which are
the same for every pixel: the frame's scale means nothing, and only the pixels' ratios do. The
response at any pixel is a product: the lens's vignetting, the filter's unevenness, dust shadows,
and the pixel's own sensitivity (PRNU).

**What Huira divides out.** The renderer already gives each pixel the falloff of its geometry:
its solid angle, and the cosine of its angle from the axis (``pixel_radiance_to_power()``). A
flat that still held that falloff would apply it twice. ``flat_from_measured()`` divides it out,
so the flat keeps only what the renderer does not model.

**Normalization.** ``flat_from_measured()`` then divides the result by the median of the pixels
in a square around the principal point, 5% of the shorter side of the sensor on each edge (at
least 8 pixels; ``central_fraction`` sets it). So:

- The flat is 1 at the principal point, and the quantum efficiency and the filter's transmission
  are the response *there*. Pixels that respond more than the principal point's, by their PRNU,
  are above 1, and that is as it should be: a flat has no scale of its own to lose, and the
  camera's efficiency is the QE and transmission. The one physical bound is that a pixel can't
  make more electrons than it receives photons, :math:`QE \times T \times V \le 1`, which the
  sensor warns of when a filter is added or selected or the QE is set.
- The median is used so that a speck of dust or a hot pixel in the square doesn't set the scale,
  and a square rather than the one pixel at the principal point for the same reason.
- The flat is an ordinary image: to normalize it another way, do so in numpy and give the
  result to ``add_filter()``.

The camera's focal length, distortion and principal point must be as they were when the flat
field was taken, since the falloff divided out depends on them. In Python::

    flat_field, _ = huira.read_fits("flat_V.fits")  # bias and dark subtracted
    flat = camera_model.flat_from_measured(flat_field)
    camera_model.add_filter("V", 0.85, flat)
    camera_model.add_filter("R", huira.visible8.SpectralBins([0, 0, 0, 0.1, 0.8, 0.9, 0.9, 0.7]))
    camera_model.select_filter("R")

Per-bin flats (a different flat for each spectral bin of one filter) are not supported: a flat
is one image, applied to the pixel's electrons from every bin.


Noise and imperfections
-----------------------

Two switches on the camera leave parts of the sensor out, each without changing its settings:

- ``enable_sensor_noise(false)`` turns off the **noise**: what is random, and differs from frame
  to frame. That is shot noise, read noise, and row and column noise. Each pixel then collects
  the expected number of electrons.
- ``enable_sensor_imperfections(false)`` turns off the **imperfections**: what makes the sensor
  less than ideal, and is the same in every frame. That is the dark current (the single value,
  the dark current map and hot pixels), each pixel's own gain (PRNU, and the selected filter's
  flat), and the bias map, so the bias is the bias level.

(On a ``SensorModel``, ``enable_noise()`` and ``enable_imperfections()``; in Python, with
``False``.) Neither touches what the sensor *is*: its quantum efficiency, the filter's
transmission, the gain, the full well, the bias level and the ADC's quantization. The bias
level stays because it isn't an imperfection, but the offset an ADC adds so that the read noise
isn't clipped at 0 DN; set it to 0 for none, knowing that, with noise on, dark pixels are then
clipped.

The two switches are independent, so there are four combinations, each with its use:

- **Both on** (the default): a realistic frame, as the camera takes it.
- **Noise off, imperfections on**: the *expected* frame, the mean of many realistic ones. Every
  dark current, map, pattern and flat applies, exactly, so this is how to check them.
- **Noise on, imperfections off**: a frame as from a *perfectly calibrated* camera, after bias,
  dark and flat correction, with only the noise no calibration removes: shot noise and the
  readout's. For testing what runs on calibrated frames.
- **Both off**: an *ideal sensor*, giving the expected electrons, by the gain, on the bias level.

The camera's ``describe()`` says which (its "Ideal:" line).


Missing data
------------

A pixel whose received power is not finite (NaN or infinite, which points to a problem upstream, such as a material or light returning a non-finite radiance) has no meaningful response: it reads out as NaN, and a warning gives the number of such pixels. The image writers record NaN as missing data where the format allows (see :doc:`image_output`).


Random numbers
--------------

Each camera's sensor has its own noise seed (``set_sensor_noise_seed()``; sensors get distinct
seeds by default, in the order they are made, so seed a sensor explicitly for noise that does not
depend on what else was made before it; a copy of a sensor has the same settings and a new seed). The noise of a row of a readout is drawn from random
numbers determined by the seed, the number of readouts the sensor has made since the seed was
set, and the row. A frame's noise therefore does not depend on other cameras, threads or the
order rows are read out in; successive frames differ; and setting the same seed again repeats
the same sequence of frames. Row noise and column noise are drawn from streams of their own,
keyed by the stream too, so that turning either on leaves the pixels' noise as it was. The fixed
patterns (PRNU and hot pixels) are made from the seed too, on streams of their own keyed by the
seed and the pixel, but not the readout, so they are the same in every frame. The random
integers are the same with every standard library; the
normal and Poisson draws made from them use ``std::log``, ``std::exp`` and the like, which can
differ in their last bit between platforms, so frames repeat exactly on one platform and
toolchain.


Changes in v0.9.11
------------------

- Each pixel can have its own dark current and bias, from maps (see Calibration maps), and
  ``dark_current_from_master_dark()`` makes the dark current's from a master dark and a master
  bias. Without maps, frames are as before. Integer FITS and TIFF calibration frames from other
  software are turned into DN as they are read (see :doc:`image_output`), and normalized ones
  are refused.
- Row noise and column noise, temporal banding along rows and columns, can be set (see Read
  Noise). Both are 0 by default, so frames are as before.
- A camera has a filter wheel, whose filters have a transmission per bin and may have a flat
  (see Filters and flats), and ``flat_from_measured()`` makes a flat from a flat field. The wheel
  is empty by default, a clear filter, so frames are as before.
- PRNU and hot pixels can be made from parameters and the noise seed (see PRNU and hot pixels).
  Both are off by default, so frames are as before.
- The sensor's imperfections (dark current, hot pixels, PRNU, the flat and the bias map) can be
  turned off, apart from its noise, for perfectly calibrated frames or, with noise off too, an
  ideal sensor (see Noise and imperfections). They are on by default, so frames are as before,
  and turning noise off does what it did.


Changes in v0.9.10
------------------

These change the images a sensor produces:

- The DN are whole numbers. The ADC's output was continuous before: one electron at 1.22 e⁻/DN
  read out as 0.82 DN.
- The default gain is 4.88 e⁻/DN, matched to the default 20000 e⁻ full well and 12 bits (see
  Saturation). At 1.22 e⁻/DN the ADC saturated at about 5000 e⁻, a quarter of the well. Default
  renders read 4 times fewer DN (the same light, in larger steps).
- Without noise, the dark current and the bias are applied. Before, turning noise off also
  removed them, so a noiseless frame was darker than the mean of a noisy one.
- A pixel that receives a power that is not finite reads out as NaN. With noise on it read out as
  the bias before, a plausible dark pixel that hid the problem.
- Shot noise is Poisson, drawn exactly below a mean of 20 electrons, and read noise is added after
  the full well clamps the charge. Before, shot noise was a normal distribution (fractional, and
  possibly negative, electron counts), and saturated pixels had no read noise.
- The default quantum efficiency peaks at 0.5 in the bluest bin, whatever the spectral type. It
  was normalized to sum to 0.5 over the bins, so the default sensor's sensitivity depended on the
  number of bins; images from the default sensor are brighter by about 2.5x for RGB, 5.9x for
  Visible8 and 5.1x for SWIR8.
- Each sensor has its own reproducible noise (see Random numbers); bit depths above 24 are
  rejected.

These change the API (each old name fails with a message naming the new one):

- ``set_sensor_simulate_noise()`` is ``enable_sensor_noise()``, and ``SensorModel``'s
  ``set_simulate_noise()`` and ``simulate_noise()`` are ``enable_noise()`` and
  ``noise_enabled()``.
- The sensor's rotation about the optical axis is its roll: ``set_sensor_rotation()`` is
  ``set_sensor_roll()``, and ``SensorModel``'s ``set_rotation()`` and ``rotation()`` are
  ``set_roll()`` and ``roll()``. ``CameraModel::sensor_rotation()``, which returned the rotation
  rather than the angle, is ``sensor_orientation()``, which a tilted sensor will add to.
