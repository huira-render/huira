#pragma once

#include <string>

#include "pybind11/pybind11.h"
#include "pybind11/stl.h"
#include "pybind11/operators.h"

#include "huira/core/concepts/numeric_concepts.hpp"
#include "huira/core/units/units.hpp"
#include "huira/core/units/quantity.hpp"

namespace py = pybind11;

namespace huira {

    template<typename SIUnit, typename... OtherUnits>
    void register_unit_conversions(py::module& m) {
        (void)m;
        (py::implicitly_convertible<OtherUnits, SIUnit>(), ...);
    }

    template<typename UnitType>
    void bind_unit_type(py::module& m, const std::string& name) {
        using Dim = typename UnitType::dimension_type;

        std::string class_doc = name + " unit.\n\n"
            "Supports arithmetic with other units of the same dimension.\n"
            "Implicit conversion to SI base unit when passed to functions.\n\n"
            "Example:\n"
            "    >>> x = huira." + name + "(1.0)\n"
            "    >>> x.get_si_value()\n";

        py::class_<UnitType>(m, name.c_str(), class_doc.c_str())
            .def(py::init<>(), "Default constructor (value = 0).")
            .def(py::init<double>(), py::arg("value"),
                ("Create a " + name + " from a float.").c_str())
            .def(py::init<int>(), py::arg("value"),
                ("Create a " + name + " from an int.").c_str())
            .def(py::init<float>(), py::arg("value"),
                ("Create a " + name + " from a float.").c_str())
            .def("get_si_value", &UnitType::get_si_value,
                "Return the value converted to SI base units.")
            .def("raw_value", &UnitType::raw_value,
                "Return the raw value in this unit's scale.")
            .def("__str__", &UnitType::to_string)
            .def("__repr__", [name](const UnitType& self) {
                return name + "(" + std::to_string(self.raw_value()) + ")";
            })
            // Cross-unit addition
            .def("__add__", [](const UnitType& self, const py::object& other) -> UnitType {
                double other_si = other.attr("get_si_value")().cast<double>();
                double result_si = self.get_si_value() + other_si;
                using SIUnit = huira::units::Quantity<Dim, std::ratio<1,1>>;
                SIUnit si_result(result_si);
                return UnitType(si_result);
            }, py::arg("other"),
            "Add another unit of the same dimension. Result is in this unit's scale.")
            .def("__sub__", [](const UnitType& self, const py::object& other) -> UnitType {
                double other_si = other.attr("get_si_value")().cast<double>();
                double result_si = self.get_si_value() - other_si;
                using SIUnit = huira::units::Quantity<Dim, std::ratio<1,1>>;
                SIUnit si_result(result_si);
                return UnitType(si_result);
            }, py::arg("other"),
            "Subtract another unit of the same dimension. Result is in this unit's scale.")
            // Same-type operators
            .def(py::self += py::self)
            .def(py::self -= py::self)
            .def(py::self * double(), "Multiply by a scalar.")
            .def(py::self / double(), "Divide by a scalar.")
            .def(py::self *= double())
            .def(py::self /= double())
            .def(double() * py::self, "Multiply a scalar by this unit.")
            // Cross-unit comparison via SI values
            .def("__eq__", [](const UnitType& self, const py::object& other) {
                return self.get_si_value() == other.attr("get_si_value")().cast<double>();
            }, py::arg("other"), "Equal comparison via SI values.")
            .def("__lt__", [](const UnitType& self, const py::object& other) {
                return self.get_si_value() < other.attr("get_si_value")().cast<double>();
            }, py::arg("other"), "Less-than comparison via SI values.")
            .def("__le__", [](const UnitType& self, const py::object& other) {
                return self.get_si_value() <= other.attr("get_si_value")().cast<double>();
            }, py::arg("other"), "Less-or-equal comparison via SI values.")
            .def("__gt__", [](const UnitType& self, const py::object& other) {
                return self.get_si_value() > other.attr("get_si_value")().cast<double>();
            }, py::arg("other"), "Greater-than comparison via SI values.")
            .def("__ge__", [](const UnitType& self, const py::object& other) {
                return self.get_si_value() >= other.attr("get_si_value")().cast<double>();
            }, py::arg("other"), "Greater-or-equal comparison via SI values.");
    }

    static void bind_units(py::module& m) {
        // ======================== //
        // === Bind Unit Types  === //
        // ======================== //

        // Distance Units
        bind_unit_type<huira::units::Kilometer>(m, "Kilometer");
        bind_unit_type<huira::units::Meter>(m, "Meter");
        bind_unit_type<huira::units::Centimeter>(m, "Centimeter");
        bind_unit_type<huira::units::Millimeter>(m, "Millimeter");
        bind_unit_type<huira::units::Micrometer>(m, "Micrometer");
        bind_unit_type<huira::units::Nanometer>(m, "Nanometer");

        bind_unit_type<huira::units::AstronomicalUnit>(m, "AstronomicalUnit");
        m.attr("AU") = m.attr("AstronomicalUnit");

        bind_unit_type<huira::units::Foot>(m, "Foot");
        bind_unit_type<huira::units::Yard>(m, "Yard");
        bind_unit_type<huira::units::Mile>(m, "Mile");

        // Mass Units
        bind_unit_type<huira::units::Kilogram>(m, "Kilogram");
        bind_unit_type<huira::units::Gram>(m, "Gram");
        bind_unit_type<huira::units::Milligram>(m, "Milligram");

        // Time Units
        bind_unit_type<huira::units::SiderealDay>(m, "SiderealDay");
        bind_unit_type<huira::units::Day>(m, "Day");
        bind_unit_type<huira::units::Hour>(m, "Hour");
        bind_unit_type<huira::units::Minute>(m, "Minute");
        bind_unit_type<huira::units::Second>(m, "Second");
        bind_unit_type<huira::units::Millisecond>(m, "Millisecond");
        bind_unit_type<huira::units::Microsecond>(m, "Microsecond");
        bind_unit_type<huira::units::Nanosecond>(m, "Nanosecond");
        bind_unit_type<huira::units::Femtosecond>(m, "Femtosecond");

        // Current Units
        bind_unit_type<huira::units::Ampere>(m, "Ampere");
        
        // Temperature Units
        bind_unit_type<huira::units::Kelvin>(m, "Kelvin");
        bind_unit_type<huira::units::Celsius>(m, "Celsius");
        bind_unit_type<huira::units::Fahrenheit>(m, "Fahrenheit");

        // Amount of Substance Units
        bind_unit_type<huira::units::Mole>(m, "Mole");

        // Luminosity Units
        bind_unit_type<huira::units::Candela>(m, "Candela");

        // Angular Units
        bind_unit_type<huira::units::Radian>(m, "Radian");
        bind_unit_type<huira::units::Degree>(m, "Degree");
        bind_unit_type<huira::units::Arcminute>(m, "Arcminute");
        bind_unit_type<huira::units::Arcsecond>(m, "Arcsecond");

        // Solid Angle Units
        bind_unit_type<huira::units::Steradian>(m, "Steradian");
        bind_unit_type<huira::units::SquareDegree>(m, "SquareDegree");

        // Frequency Units
        bind_unit_type<huira::units::Hertz>(m, "Hertz");
        bind_unit_type<huira::units::Kilohertz>(m, "Kilohertz");
        bind_unit_type<huira::units::Megahertz>(m, "Megahertz");
        bind_unit_type<huira::units::Gigahertz>(m, "Gigahertz");
        bind_unit_type<huira::units::Terahertz>(m, "Terahertz");

        // Force Units
        bind_unit_type<huira::units::Newton>(m, "Newton");
        bind_unit_type<huira::units::Kilonewton>(m, "Kilonewton");

        // Pressure Units
        bind_unit_type<huira::units::Pascal>(m, "Pascal");
        bind_unit_type<huira::units::Kilopascal>(m, "Kilopascal");

        // Energy Units
        bind_unit_type<huira::units::Joule>(m, "Joule");
        bind_unit_type<huira::units::Kilojoule>(m, "Kilojoule");
        bind_unit_type<huira::units::Megajoule>(m, "Megajoule");
        bind_unit_type<huira::units::ElectronVolt>(m, "ElectronVolt");

        // Power Units
        bind_unit_type<huira::units::Watt>(m, "Watt");
        bind_unit_type<huira::units::Kilowatt>(m, "Kilowatt");
        bind_unit_type<huira::units::Megawatt>(m, "Megawatt");
        bind_unit_type<huira::units::Gigawatt>(m, "Gigawatt");

        // Electric Charge Units
        bind_unit_type<huira::units::Coulomb>(m, "Coulomb");

        // Radiometric Units
        bind_unit_type<huira::units::WattsPerMeterSquaredSteradian>(m, "WattsPerMeterSquaredSteradian");
        bind_unit_type<huira::units::WattsPerMeterSquared>(m, "WattsPerMeterSquared");
        bind_unit_type<huira::units::WattsPerSteradian>(m, "WattsPerSteradian");

        // Photometric Units
        bind_unit_type<huira::units::Lumen>(m, "Lumen");

        // ================================== //
        // === Register Implicit Conversions  //
        // ================================== //
        // Allows users to pass e.g. Degree where Radian is expected in function signatures.
        // Each call registers conversions from all listed types -> the SI base unit (first arg).

        // Distance -> Meter
        register_unit_conversions<huira::units::Meter,
            huira::units::Kilometer, huira::units::Centimeter, huira::units::Millimeter,
            huira::units::Micrometer, huira::units::Nanometer, huira::units::AstronomicalUnit,
            huira::units::Foot, huira::units::Yard, huira::units::Mile>(m);

        // Mass -> Kilogram
        register_unit_conversions<huira::units::Kilogram,
            huira::units::Gram, huira::units::Milligram>(m);

        // Time -> Second
        register_unit_conversions<huira::units::Second,
            huira::units::Femtosecond, huira::units::Nanosecond, huira::units::Microsecond,
            huira::units::Millisecond, huira::units::Minute, huira::units::Hour,
            huira::units::Day, huira::units::SiderealDay>(m);

        // Temperature -> Kelvin
        register_unit_conversions<huira::units::Kelvin,
            huira::units::Celsius, huira::units::Fahrenheit>(m);

        // Angle -> Radian
        register_unit_conversions<huira::units::Radian,
            huira::units::Degree, huira::units::Arcminute, huira::units::Arcsecond>(m);

        // Solid Angle -> Steradian
        register_unit_conversions<huira::units::Steradian,
            huira::units::SquareDegree>(m);

        // Frequency -> Hertz
        register_unit_conversions<huira::units::Hertz,
            huira::units::Kilohertz, huira::units::Megahertz,
            huira::units::Gigahertz, huira::units::Terahertz>(m);

        // Force -> Newton
        register_unit_conversions<huira::units::Newton,
            huira::units::Kilonewton>(m);

        // Pressure -> Pascal
        register_unit_conversions<huira::units::Pascal,
            huira::units::Kilopascal>(m);

        // Energy -> Joule
        register_unit_conversions<huira::units::Joule,
            huira::units::Kilojoule, huira::units::Megajoule,
            huira::units::ElectronVolt>(m);

        // Power -> Watt
        register_unit_conversions<huira::units::Watt,
            huira::units::Kilowatt, huira::units::Megawatt,
            huira::units::Gigawatt>(m);
    }
}
