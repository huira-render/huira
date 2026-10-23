Pose Sources
============

A node's position and rotation each come from a source: set by hand (``set_position``,
``set_velocity`` and so on, which hold at the scene view's reference time), SPICE, or a
callback. Callbacks are evaluated while a scene view is built, as many times as it needs, and
never while it renders. Each is given a :cpp:struct:`huira::PoseContext` and returns the state.

The callbacks Huira provides are a function of the pose context (``set_position_function``,
``set_rotation_function``), a table of times and states (``set_position_table``,
``set_rotation_table``), a Keplerian orbit (``set_keplerian_orbit``), a satellite's TLE
(``set_tle_position``, by SGP4, for a child of the scene's TEME frame), and ``look_at``. A class of
one's own, derived from :cpp:struct:`huira::PositionCallback` or
:cpp:struct:`huira::RotationCallback`, is set with ``set_custom_position_callback`` or
``set_custom_rotation_callback``.

.. doxygenstruct:: huira::PoseContext
   :members:

.. doxygenstruct:: huira::PositionState
   :members:

.. doxygenstruct:: huira::RotationState
   :members:

.. doxygenstruct:: huira::PositionCallback
   :members:

.. doxygenstruct:: huira::RotationCallback
   :members:

.. doxygenstruct:: huira::StateCallback
   :members:

Functions
---------

.. doxygenstruct:: huira::PositionSample
   :members:

.. doxygenstruct:: huira::RotationSample
   :members:

.. doxygenclass:: huira::PositionFunctionCallback

.. doxygenclass:: huira::RotationFunctionCallback

.. doxygenvariable:: huira::RATE_ESTIMATE_SPAN

Tables
------

.. doxygenclass:: huira::PositionTableCallback
   :members:

.. doxygenclass:: huira::RotationTableCallback
   :members:

Rotation vectors
----------------

.. doxygenfunction:: huira::rotation_vector

.. doxygenfunction:: huira::rotation_from_vector
