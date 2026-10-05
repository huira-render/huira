===================
Aperture and F-Stop
===================

TLDR: a camera is f/2.8 until you set its aperture, whatever its focal length. Once you set
it, its *diameter* is what is kept: changing the focal length afterwards changes the f-number.
Set the f-stop after the focal length.

How the aperture is set
-----------------------

The f-number is the focal length divided by the aperture's diameter, so of the three, two can
be held fixed and the third follows. Which two depends on how the aperture was last set:

.. list-table::
   :header-rows: 1
   :widths: 30 35 35

   * - Aperture
     - When the focal length changes
     - Use it for
   * - Not set
     - The camera stays f/2.8: the diameter follows the focal length.
     - Getting started; nothing to configure.
   * - ``set_fstop(N)``
     - The diameter (focal length / N, at the time) is kept, so the f-number changes. A
       warning is logged the first time, in case that was not intended.
     - The usual case: set the focal length, then the f-stop.
   * - ``set_aperture_diameter(D)`` or ``set_aperture()``
     - The diameter is kept, so the f-number changes, with no warning: the diameter is what
       was asked for.
     - Modelling a physical aperture, e.g. sweeping the focal length with a fixed stop.

``set_intrinsics()`` and ``set_intrinsic_matrix()`` set the focal length through their anchor
focal length, so they behave like ``set_focal_length()`` here.

The diameter of a non-circular aperture means the diameter of a circle of the same area, so
``fstop()`` and ``aperture_diameter()`` are defined for any shape.

Usage
-----

C++:

.. code-block:: cpp

    camera_model.set_focal_length(50_mm);
    camera_model.set_fstop(8.f);                // f/8, a 6.25 mm aperture

    camera_model.set_focal_length(100_mm);      // warns: still 6.25 mm, so now f/16
    camera_model.set_fstop(8.f);                // f/8 again, a 12.5 mm aperture

Python:

.. code-block:: python

    camera_model.set_focal_length(mm(50))
    camera_model.set_aperture_diameter(mm(10))  # f/5; stays 10 mm at any focal length

Checking settings
-----------------

Every camera setter checks its values before changing anything, and throws for values that
are not physically meaningful: a focal length, f-stop, aperture, pixel pitch or sensor size
that is not positive and finite, a resolution under 1x1, a non-finite principal point or
distortion coefficient, and so on. The camera is left exactly as it was, so a failed setter
can be caught and retried.

Changes in v0.9.10
------------------

Before v0.9.10, ``set_fstop()`` behaved as it does now but silently, and the default aperture
was a fixed 17.9 mm (50 mm at f/2.8), so changing the focal length without setting the
aperture changed the f-number. The default now stays f/2.8. ``set_aperture_diameter()`` and
``aperture_diameter()`` are new.

Invalid values that were accepted before now throw: for example ``set_fstop(0)``, which gave
an infinite aperture, and ``set_fstop(-2)``, which gave f/2.
