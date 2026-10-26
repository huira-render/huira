Sensor Calibration
==================

Each pixel's own dark current and bias, from a real sensor's calibration frames, for
``CameraModelHandle.set_sensor_dark_current_map()`` and ``set_sensor_bias_map()``. See
:doc:`/design_overview/sensor_modeling`.

.. autoclass:: huira.BiasMapKind
   :members:
   :undoc-members:

.. autofunction:: huira.dark_current_from_master_dark
