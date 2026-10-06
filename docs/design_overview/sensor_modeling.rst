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

An ideal ADC rounds to the nearest DN, as here. Many real ADCs round down instead, which differs only by a constant half a DN that bias calibration absorbs.

The sensor response (``FrameBuffer::sensor_response()``) holds :math:`DN / \text{MaxDN}`, a value in [0, 1], and records the bit depth (``Image::sensor_bit_depth()``), so the image writers can write the DN themselves: see :doc:`image_output`. Every DN is recovered exactly from the stored value, as :math:`\text{round}(\text{value} \times \text{MaxDN})`, for bit depths up to 24, the most a sensor accepts (``SensorModel::MAX_BIT_DEPTH``). This is a limit of how the response is stored, as a single-precision float; it is far beyond real ADCs, which reach 16 to 18 bits.


Saturation
----------

A pixel saturates at whichever limit it reaches first: the full well, at :math:`\text{WellCapacity} / \text{Gain} + \text{Bias}` DN, or the ADC, at MaxDN. Both happen in real cameras. The default gain is matched to the full well: :math:`\text{WellCapacity} / (\text{MaxDN} - \text{Bias})` = 20000 / (4095 - 10) = 4.896 e⁻/DN would make a full well reach exactly the ADC's largest DN. As in many cameras, it is set slightly lower, at 4.88 e⁻/DN, so that the ADC saturates just before the well is full (at 19935 e⁻): nearly the whole well is used, saturated pixels read exactly MaxDN, and the response near full well, which is not quite linear in a real sensor, is cut off. The conversion gain in e⁻/DN is the inverse of the "overall system gain" *K*, in DN/e⁻, of the EMVA 1288 standard.


Noise off
---------

``set_simulate_noise(false)`` (``set_sensor_simulate_noise(False)`` in Python) removes what is random: shot noise and read noise. Each pixel then collects the expected number of electrons, signal and dark current together. The dark current's electrons and the bias are still added, since they are not noise, and the result is still quantized.


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
