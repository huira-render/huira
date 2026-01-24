Core API
========

The Core module contains the fundamental building blocks of the engine.

Time & Space
------------
Classes for defining where and when things happen.

* :cpp:class:`huira::Time` - High-precision ephemeris time.
* :cpp:class:`huira::Transform` - Position and orientation data.
* :cpp:class:`huira::Rotation` - Quaternion-based rotation logic.

Physics & Light
---------------
Definitions for the physical properties of the simulation.

* :cpp:class:`huira::SpectralBins` - Wavelength discretization.
* :cpp:class:`huira::Ray` - Geometric ray definitions.
* :cpp:class:`huira::RayHit` - Intersection results.

System Utilities
----------------
* :cpp:class:`huira::Logger` - Thread-safe logging.
* :cpp:class:`huira::SPICE` - Interface to NASA's SPICE toolkit.

.. toctree::
   :hidden:

   ray
   ray_hit
   rotation
   scene
   spectral_bins
   spice
   time
   transform
   types
   units
