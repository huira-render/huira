Sensor Calibration
==================

Each pixel's own dark current and bias, from a real sensor's calibration frames, for
``SensorModel::set_dark_current_map()`` and ``set_bias_map()``. See
:doc:`/design_overview/sensor_modeling`.

.. doxygenenum:: huira::BiasMapKind

.. doxygenfunction:: huira::dark_current_from_master_dark(const Image<float>&, const Image<float>&, float, units::Second)

.. doxygenfunction:: huira::dark_current_from_master_dark(const Image<float>&, float, float, units::Second)
