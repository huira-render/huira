#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "huira/core/time.hpp"
#include "huira/core/types.hpp"
#include "huira/ephemeris/satrec.hpp"
#include "pybind11/pybind11.h"
#include "pybind11/stl.h"

namespace py = pybind11;

namespace huira {

inline void bind_satrec(py::module_& m)
{
    // Registered before SatRec, whose factories take it as a default argument
    py::enum_<Sgp4Mode>(
        m,
        "Sgp4Mode",
        "Which of its two behaviors Vallado's SGP4 propagates an element set with. They differ "
        "only for deep-space orbits inclined less than about 11.46 degrees, much of the "
        "geostationary belt among them. Use AFSPC, the default, unless you have a specific "
        "reason not to: TLEs are fitted with the US Space Force's own SGP4, which it "
        "reproduces. IMPROVED is for matching python-sgp4 and Skyfield, which always use it.")
        .value("AFSPC",
               Sgp4Mode::AFSPC,
               "As the US Space Force's operational SGP4 does (opsmode 'a'). The default.")
        .value("IMPROVED",
               Sgp4Mode::IMPROVED,
               "Vallado's improved handling (opsmode 'i'), as python-sgp4 and Skyfield use.");

    py::class_<SatRec>(m,
                       "SatRec",
                       "A satellite's element set, read from a two-line element set (TLE), "
                       "checked, and ready to be propagated with SGP4. Make one with from_tle().")
        .def_static(
            "from_tle",
            py::overload_cast<std::string_view, std::string_view, Sgp4Mode>(&SatRec::from_tle),
            py::arg("line1"),
            py::arg("line2"),
            py::arg("mode") = Sgp4Mode::AFSPC,
            "Read a TLE's two lines. Raises RuntimeError if a line is not 69 characters "
            "long, isn't laid out as a TLE line is, or fails its checksum; if the lines "
            "are for different satellites; if the element set is for SGP4-XP; or if SGP4 "
            "can't propagate it at its epoch.")
        .def_static(
            "from_tle",
            py::overload_cast<std::string_view, std::string_view, std::string_view, Sgp4Mode>(
                &SatRec::from_tle),
            py::arg("name"),
            py::arg("line1"),
            py::arg("line2"),
            py::arg("mode") = Sgp4Mode::AFSPC,
            "Read a three-line element set: a name line, which may start with '0 ', then a TLE's "
            "two lines.")

        .def("catalog_number",
             &SatRec::catalog_number,
             "The NORAD catalog number, decoded from the Alpha-5 form if it is in it")
        .def("name", &SatRec::name, "The name from the name line, or '' for a two-line TLE")
        .def("international_designator",
             &SatRec::international_designator,
             "The international designator (COSPAR ID), such as '98067A', or '' if blank")
        .def("epoch", &SatRec::epoch, "The time the element set describes the orbit at")
        .def("mode", &SatRec::mode, "The mode SGP4 propagates the element set with")

        .def(
            "evaluate",
            [](const SatRec& self,
               const Time& time) -> std::optional<std::pair<Vec3<double>, Vec3<double>>> {
                const std::optional<PositionState> state = self.evaluate(time);
                if (!state.has_value()) {
                    return std::nullopt;
                }
                return std::make_pair(state->position, state->velocity);
            },
            py::arg("time"),
            "The position, in meters, and velocity, in meters per second, at a time, in the TEME "
            "frame (HUIRA_EARTH_TEME, centered on the Earth), as a tuple; or None if SGP4 can't "
            "propagate the element set to it. error_message_at() says why.")
        .def("error_message_at",
             &SatRec::error_message_at,
             py::arg("time"),
             "Why SGP4 can't propagate the element set to a time, or '' if it can")

        .def("__repr__", [](const SatRec& self) {
            std::string repr = "<SatRec " + std::to_string(self.catalog_number());
            if (!self.name().empty()) {
                repr += " \"" + self.name() + "\"";
            }
            return repr + ", epoch " + self.epoch().to_iso_8601() + ", " +
                   (self.mode() == Sgp4Mode::AFSPC ? "AFSPC" : "IMPROVED") + " mode>";
        });
}

} // namespace huira
