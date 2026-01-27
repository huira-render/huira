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



6. Gain & ADC (Quantization):
-----------------------------

An Analog-to-Digital Converter (ADC) converts the number of electrons to Digital Numbers (DN). This involves applying a *System Gain* (:math:`e^-/ADU`, where ADU is the Analog-to-Digital Unit) and adding a *Bias* level to avoid negative values. Finally, the result is quantized to integer values and clamped to the maximum representable value based on the sensor's bit depth.

.. math::

    DN = \text{floor}\left( \min\left( \frac{N_{electrons\_read}}{\text{Gain}}, \text{MaxDN} \right) \right) + \text{Bias}
