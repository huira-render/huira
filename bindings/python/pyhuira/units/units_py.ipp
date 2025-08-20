#pragma once

#include <string>

#include "pybind11/pybind11.h"
#include "pybind11/stl.h"
#include "pybind11/operators.h"

#include "huira/concepts/numeric_concepts.hpp"
#include "huira/units/units.hpp"

namespace py = pybind11;

namespace huira {
    template<typename UnitType>
    void bind_unit_type(py::module& m, const std::string& name) {
        py::class_<UnitType>(m, name.c_str())
            .def(py::init<>())
            .def(py::init<double>())
            .def(py::init<int>())
            .def(py::init<float>())
            .def("get_si_value", &UnitType::getSIValue, "Get the value in SI base units")
            .def("raw_value", &UnitType::rawValue, "Get the raw value in this unit")
            .def("__str__", &UnitType::toString)
            .def("__repr__", [name](const UnitType& self) {
            return name + "(" + std::to_string(self.rawValue()) + ")";
                })
            // Arithmetic operators
            .def(py::self + py::self)
            .def(py::self - py::self)
            .def(py::self += py::self)
            .def(py::self -= py::self)
            .def(py::self * double())
            .def(py::self / double())
            .def(py::self *= double())
            .def(py::self /= double())
            .def(double() * py::self)
            // Comparison operators
            .def(py::self == py::self)
            .def(py::self != py::self)
            .def(py::self < py::self)
            .def(py::self > py::self)
            .def(py::self <= py::self)
            .def(py::self >= py::self);
    }

    void bind_units(py::module& m) {
        // Distance Units
        bind_unit_type<Kilometer>(m, "Kilometer");
        bind_unit_type<Meter>(m, "Meter");
        bind_unit_type<Cenimeter>(m, "Centimeter");
        bind_unit_type<Millimeter>(m, "Millimeter");
        bind_unit_type<Micrometer>(m, "Micrometer");
        bind_unit_type<Nanometer>(m, "Nanometer");

        bind_unit_type<AstronomicalUnit>(m, "AstronomicalUnit");
        m.attr("AU") = m.attr("AstronomicalUnit");

        bind_unit_type<Foot>(m, "Foot");
        bind_unit_type<Yard>(m, "Yard");
        bind_unit_type<Mile>(m, "Mile");

        // Mass Units
        bind_unit_type<Kilogram>(m, "Kilogram");
        bind_unit_type<Gram>(m, "Gram");
        bind_unit_type<Milligram>(m, "Milligram");

        // Time Units
        bind_unit_type<SidrealDay>(m, "SiderealDay");
        bind_unit_type<Day>(m, "Day");
        bind_unit_type<Hour>(m, "Hour");
        bind_unit_type<Minute>(m, "Minute");
        bind_unit_type<Second>(m, "Second");
        bind_unit_type<Millisecond>(m, "Millisecond");
        bind_unit_type<Microsecond>(m, "Microsecond");
        bind_unit_type<Nanosecond>(m, "Nanosecond");
        bind_unit_type<Femtosecond>(m, "Femtosecond");

        // Current Units
        bind_unit_type<Ampere>(m, "Ampere");

        // Temperature Units
        bind_unit_type<Kelvin>(m, "Kelvin");
        bind_unit_type<Celsius>(m, "Celsius");
        bind_unit_type<Fahrenheit>(m, "Fahrenheit");

        // Amount of Substance Units
        bind_unit_type<Mole>(m, "Mole");

        // Luminosity Units
        bind_unit_type<Candela>(m, "Candela");

        // Angular Units
        bind_unit_type<Radian>(m, "Radian");
        bind_unit_type<Degree>(m, "Degree");
        bind_unit_type<ArcMinute>(m, "ArcMinute");
        bind_unit_type<ArcSecond>(m, "ArcSecond");

        // Solid Angle Units
        bind_unit_type<Steradian>(m, "Steradian");

        // Frequency Units
        bind_unit_type<Hertz>(m, "Hertz");
        bind_unit_type<Kilohertz>(m, "Kilohertz");
        bind_unit_type<Megahertz>(m, "Megahertz");
        bind_unit_type<Gigahertz>(m, "Gigahertz");
        bind_unit_type<Terahertz>(m, "Terahertz");

        // Force Units
        bind_unit_type<Newton>(m, "Newton");

        // Pressure Units
        bind_unit_type<Pascal>(m, "Pascal");

        // Energy Units
        bind_unit_type<Joule>(m, "Joule");
        bind_unit_type<ElectronVolt>(m, "ElectronVolt");

        // Power Units
        bind_unit_type<Watt>(m, "Watt");

        // Electric Charge Units
        bind_unit_type<Coulomb>(m, "Coulomb");

        // Radiometric Units
        bind_unit_type<WattsPerMeterSquaredSteradian>(m, "WattsPerMeterSquaredSteradian");
        bind_unit_type<WattsPerMeterSquared>(m, "WattsPerMeterSquared");
        bind_unit_type<WattsPerSteradian>(m, "WattsPerSteradian");

        // Photometric Units
        bind_unit_type<Lumen>(m, "Lumen");
    }
}