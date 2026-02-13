#pragma once

#include "pybind11/pybind11.h"
#include "pybind11/operators.h"
#include "pybind11/chrono.h"

#include "huira/core/time.hpp"

namespace py = pybind11;

namespace huira {

    inline void bind_time(py::module_& m) {
        py::enum_<TimeScale>(m, "TimeScale")
            .value("UTC", TimeScale::UTC)
            .value("TAI", TimeScale::TAI)
            .value("TT", TimeScale::TT)
            .value("TDB", TimeScale::TDB);

        py::class_<Time>(m, "Time")
            // Constructor from UTC string
            .def(py::init<const std::string&>(), py::arg("utc_string"),
                "Construct from a UTC date string (e.g. '2024-03-15T12:00:00')")

            // Constructor from Python datetime
            .def(py::init([](py::object dt) {
            // Convert datetime to ISO 8601 string for SPICE
            py::object isoformat = dt.attr("isoformat")();
            std::string iso_str = isoformat.cast<std::string>();

            // If the datetime is timezone-aware (has tzinfo), strip trailing +00:00
            // SPICE expects plain UTC strings without timezone suffixes
            if (iso_str.size() >= 6 && iso_str.substr(iso_str.size() - 6) == "+00:00") {
                iso_str = iso_str.substr(0, iso_str.size() - 6);
            }

            return Time(iso_str);
                }), py::arg("datetime"),
                    "Construct from a Python datetime object (interpreted as UTC)")

            // Static factory methods
            .def_static("from_et", &Time::from_et, py::arg("et"),
                "Create from ephemeris time (TDB seconds past J2000.0)")
            .def_static("from_ephemeris_time", &Time::from_ephemeris_time, py::arg("et"))
            .def_static("from_julian_date", &Time::from_julian_date,
                py::arg("jd"), py::arg("scale"),
                "Create from a Julian Date in the specified timescale")
            .def_static("from_modified_julian_date", &Time::from_modified_julian_date,
                py::arg("mjd"), py::arg("scale"),
                "Create from a Modified Julian Date in the specified timescale")

            // Accessors
            .def("et", &Time::et, "Get ephemeris time (TDB seconds past J2000.0)")
            .def("ephemeris_time", &Time::ephemeris_time)
            .def("to_julian_date", &Time::to_julian_date,
                py::arg("scale") = TimeScale::TDB)
            .def("to_modified_julian_date", &Time::to_modified_julian_date,
                py::arg("scale") = TimeScale::TDB)
            .def("julian_years_since_j2000", &Time::julian_years_since_j2000,
                py::arg("scale") = TimeScale::TT)
            .def("to_iso_8601", &Time::to_iso_8601)
            .def("to_utc_string", &Time::to_utc_string,
                py::arg("format") = "YYYY-MM-DD HR:MN:SC.### UTC")

            // Convert back to Python datetime
            .def("to_datetime", [](const Time& self) {
            py::module_ dt_mod = py::module_::import("datetime");
            py::object datetime_cls = dt_mod.attr("datetime");
            py::object timezone_cls = dt_mod.attr("timezone");
            py::object utc = timezone_cls.attr("utc");

            std::string iso = self.to_iso_8601();
            // Remove trailing 'Z' and parse with fromisoformat
            if (!iso.empty() && iso.back() == 'Z') {
                iso.pop_back();
            }
            py::object naive = datetime_cls.attr("fromisoformat")(iso);
            return naive.attr("replace")(py::arg("tzinfo") = utc);
                }, "Convert to a timezone-aware (UTC) Python datetime object")

            // Time + Second (and any unit that converts to Second)
            .def("__add__", [](const Time& self, const py::object& delta) -> Time {
            double seconds = delta.attr("get_si_value")().cast<double>();
            return self + units::Second(seconds);
                }, py::arg("delta"),
                    "Add a time duration (Second, Minute, Hour, Day, etc.) to this Time")

            // Time - Time -> Second
            .def("__sub__", [](const Time& self, const Time& other) {
            return units::Second(self.et() - other.et());
                }, py::arg("other"),
                    "Subtract two Times to get a duration in Seconds")

            // Time - Second -> Time
            .def("__sub__", [](const Time& self, const py::object& delta) -> Time {
            double seconds = delta.attr("get_si_value")().cast<double>();
            return self + units::Second(-seconds);
                }, py::arg("delta"),
                    "Subtract a time duration from this Time")

            // Comparisons
            .def(py::self == py::self)
            .def(py::self != py::self)
            .def(py::self < py::self)
            .def(py::self <= py::self)
            .def(py::self > py::self)
            .def(py::self >= py::self)

            // Repr
            .def("__repr__", [](const Time& t) {
            return "<Time " + t.to_iso_8601() + ">";
                })

            // Constants
            .def_readonly_static("J2000_JD", &Time::J2000_JD)
            .def_readonly_static("DAYS_PER_JULIAN_YEAR", &Time::DAYS_PER_JULIAN_YEAR)
            .def_readonly_static("MJD_OFFSET", &Time::MJD_OFFSET)
            .def_readonly_static("TT_TAI_OFFSET", &Time::TT_TAI_OFFSET);
    }

}
