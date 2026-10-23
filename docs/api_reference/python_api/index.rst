Python API Reference
====================

The ``huira`` package has two parts.

The modules below are shared: their classes do not depend on how light is
binned across the spectrum, and are imported from ``huira`` directly.

.. grid:: 3
    :gutter: 3

    .. grid-item-card:: Core API
        :link: core/index
        :link-type: doc

        Spectral bins, time and intervals, pose contexts, units, rotations and the vector and matrix types.

    .. grid-item-card:: Camera API
        :link: cameras/index
        :link-type: doc

        Distortion coefficients, pixel conventions, and the PSF sampling, focus and FFT options.

    .. grid-item-card:: Image API
        :link: images/index
        :link-type: doc

        The Image class for reading, writing and working with images.

    .. grid-item-card:: Ephemeris API
        :link: ephemeris/index
        :link-type: doc

        SatRec: TLEs, read and checked, and propagated with SGP4.

Everything that carries light, from the scene graph and its handles to the
renderer, frame buffer and spectral images, comes in one copy per spectral
specialization: a submodule with the same classes and methods, differing only
in its spectral bins. Pick the one that matches the imagery you want, for
example ``from huira.rgb import Scene``.

.. grid:: 3
    :gutter: 3

    .. grid-item-card:: RGB
        :link: RGB/index
        :link-type: doc

        Three bins: red (600-750 nm), green (500-600 nm) and blue (380-500 nm). The usual choice for color imagery.

    .. grid-item-card:: Visible8
        :link: Visible8/index
        :link-type: doc

        Eight uniform bins across the visible spectrum, 380-750 nm.

    .. grid-item-card:: SWIR8
        :link: SWIR8/index
        :link-type: doc

        Eight uniform bins across the short-wave infrared, 900-2500 nm.

.. toctree::
   :hidden:
   :maxdepth: 2

   core/index
   cameras/index
   images/index
   ephemeris/index

   RGB/index
   SWIR8/index
   Visible8/index
