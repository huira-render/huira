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
maps are not noise, so they apply with noise off too. The camera keeps them until they are
cleared (``clear_sensor_dark_current_map()``, ``clear_sensor_bias_map()``) or replaced, and a
copy of a sensor shares them. ``sensor_dark_current_map()`` and ``sensor_bias_map()`` give each
pixel's value, from the map or the single value; the camera's ``describe()`` summarizes them.

Values are checked when set: a dark current must be non-negative and finite, and each pixel's
bias (a relative map's added to the bias level) finite and in :math:`[0, \text{MaxDN})`, as for
the bias level; a later bias level or bit depth that would take a pixel out of that range is
refused. A map that is no longer at the sensor's resolution, after the camera's sensor is
configured again, stays set, with a warning, and a readout throws until it is replaced or
cleared.

Calibration frames are in DN. A sensor response, and a frame read from a FITS file that records
the bit depth it was taken at (the ``ADCBITS`` keyword, or ``SATURATE`` at an ADC's ceiling, such
as 4095), hold :math:`DN / \text{MaxDN}` instead, with a sensor bit depth
(``Image::sensor_bit_depth()``; ``sensor_bit_depth`` in Python), and are turned into DN: these
can be given to ``dark_current_from_master_dark()`` and as an absolute bias map as they are. Any
other image's values are taken as DN. An integer FITS file without those keywords is read
normalized by its type's range (65535 for 16 bits), without a bit depth; a warning points out an
image whose values are all at most 1 DN, which looks normalized, and multiplying it by the range
gives back its DN. A relative bias map is offsets in DN, so one with a sensor bit depth is
refused.

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

    # Or a bias pattern made in numpy, as offsets from the bias level:
    columns = np.tile(np.random.default_rng(1).normal(0, 2, 1024), (1024, 1))
    camera_model.set_sensor_bias_map(
        huira.Image_f32(columns, origin=huira.PixelOrigin.TopLeft), huira.BiasMapKind.Relative)


Noise off
---------

``enable_sensor_noise(false)`` on the camera (``enable_noise(false)`` on a ``SensorModel``; ``enable_sensor_noise(False)`` in Python) removes what is random: shot noise and read noise. Each pixel then collects the expected number of electrons, signal and dark current together. The dark current's electrons and the bias, and their maps, are still added, since they are not noise, and the result is still quantized.


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
the same sequence of frames. The random integers are the same with every standard library; the
normal and Poisson draws made from them use ``std::log``, ``std::exp`` and the like, which can
differ in their last bit between platforms, so frames repeat exactly on one platform and
toolchain.


Changes in v0.9.11
------------------

- Each pixel can have its own dark current and bias, from maps (see Calibration maps), and
  ``dark_current_from_master_dark()`` makes the dark current's from a master dark and a master
  bias. Without maps, frames are as before.


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
