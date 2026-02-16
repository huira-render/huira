Units
=====

All unit types share a common interface. They can be constructed from numeric values,
compared and combined with other units of the same physical dimension, and are
implicitly converted to SI base units when passed to functions.

.. autoclass:: huira.Meter
   :members:
   :undoc-members:

Available Units
---------------

.. list-table::
   :header-rows: 1
   :widths: 20 30 30

   * - Dimension
     - Types
     - SI Base Unit
   * - Distance
     - Kilometer, Meter, Centimeter, Millimeter, Micrometer, Nanometer, Foot, Yard, Mile, AstronomicalUnit
     - Meter
   * - Mass
     - Kilogram, Gram, Milligram
     - Kilogram
   * - Time
     - Second, Millisecond, Microsecond, Nanosecond, Femtosecond, Minute, Hour, Day, SiderealDay
     - Second
   * - Temperature
     - Kelvin, Celsius, Fahrenheit
     - Kelvin
   * - Angle
     - Radian, Degree, ArcMinute, ArcSecond
     - Radian
   * - Solid Angle
     - Steradian, SquareDegree
     - Steradian
   * - Frequency
     - Hertz, KiloHertz, MegaHertz, GigaHertz, TeraHertz
     - Hertz
   * - Force
     - Newton, KiloNewton
     - Newton
   * - Pressure
     - Pascal, Kilopascal
     - Pascal
   * - Energy
     - Joule, Kilojoule, Megajoule, ElectronVolt
     - Joule
   * - Area
     - SquareMeter, SquareCentimeter, SquareMillimeter
     - SquareMeter
   * - Power
     - Watt, Kilowatt, Megawatt, Gigawatt
     - Watt
   * - Current
     - Ampere
     - Ampere
   * - Charge
     - Coulomb
     - Coulomb
   * - Luminous Intensity
     - Candela
     - Candela
   * - Luminous Flux
     - Lumen
     - Lumen
   * - Radiometric
     - WattsPerMeterSquaredSteradian, WattsPerMeterSquared, WattsPerSteradian
     - (SI derived)