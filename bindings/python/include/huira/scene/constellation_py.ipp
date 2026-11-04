#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "huira/core/time.hpp"
#include "huira/ephemeris/element_set_catalog.hpp"
#include "huira/ephemeris/element_set_catalog_py.ipp"
#include "huira/handles/handle_py.ipp"
#include "huira/handles/scene/constellation_handle.hpp"
#include "huira/scene/constellation.hpp"
#include "huira/scene/scene_view_types.hpp"
#include "pybind11/pybind11.h"
#include "pybind11/stl.h"
#include "pybind11/stl/filesystem.h"

namespace py = pybind11;

namespace huira {

/// What adding satellites to a constellation reports, and the satellites a scene view leaves out:
/// the same for every spectral type.
inline void bind_constellation_reports(py::module_& m)
{
    py::class_<ConstellationReport>(m, "ConstellationReport", "What adding satellites did")
        .def_readonly("added", &ConstellationReport::added, "The satellites added")
        .def_readonly("replaced",
                      &ConstellationReport::replaced,
                      "The satellites it held already, whose element sets were replaced")
        .def_readonly("skipped",
                      &ConstellationReport::skipped,
                      "The element sets left out, and why (SkippedElementSet): by the catalog's "
                      "read, or as something else places their satellites (SkipReason.CLAIMED)")
        .def("__repr__", [](const ConstellationReport& self) {
            return "<ConstellationReport: " + std::to_string(self.added) + " added, " +
                   std::to_string(self.replaced) + " replaced, " +
                   std::to_string(self.skipped.size()) + " skipped>";
        });

    py::class_<SkippedSatellite>(m,
                                 "SkippedSatellite",
                                 "A satellite of a constellation a scene view left out, as SGP4 "
                                 "can't place it during the exposure")
        .def_readonly(
            "constellation_id", &SkippedSatellite::constellation_id, "Its constellation's id")
        .def_readonly(
            "catalog_number", &SkippedSatellite::catalog_number, "Its NORAD catalog number")
        .def_readonly("name", &SkippedSatellite::name, "Its name, or ''")
        .def_readonly("message", &SkippedSatellite::message, "Why, in words")
        .def("__repr__", [](const SkippedSatellite& self) {
            return "<SkippedSatellite " + std::to_string(self.catalog_number) + ": " +
                   self.message + ">";
        });
}

template <IsSpectral TSpectral>
inline void bind_constellation_handle(py::module_& m)
{
    using HandleType = ConstellationHandle<TSpectral>;

    static const std::string load_doc =
        std::string("Read a catalog of element sets from a file, and add its satellites, or bring "
                    "those it holds up to date. Returns a ConstellationReport.\n\nformat: an "
                    "ElementSetFormat, or None to work it out from the text.") +
        detail::ELEMENT_SET_OPTIONS_DOC;

    auto cls =
        py::class_<HandleType>(
            m,
            "ConstellationHandle",
            "Satellites, many of them, each placed by SGP4 from its element set, as a node in the "
            "scene's TEME frame would be, and seen as an instance of one unresolved object. A "
            "scene view places them all at once, each with the temporal samples its motion "
            "needs, and leaves out, with a warning, any SGP4 can't place during its exposure.")
            .def("add",
                 py::overload_cast<const std::vector<SatRec>&>(&HandleType::add, py::const_),
                 py::arg("element_sets"),
                 "Add satellites from their element sets (SatRecs), or replace the element sets "
                 "of those it holds. A satellite something else places is left out. Returns a "
                 "ConstellationReport.")
            .def("add",
                 py::overload_cast<const ElementSetCatalog&>(&HandleType::add, py::const_),
                 py::arg("catalog"),
                 "Add the satellites of a catalog read (ElementSetCatalog), or replace the element "
                 "sets of those it holds, reporting the entries the read left out too")
            .def(
                "load",
                [](const HandleType& self,
                   const std::filesystem::path& path,
                   std::optional<ElementSetFormat> format,
                   Sgp4Mode mode,
                   std::vector<int> include,
                   std::vector<int> exclude,
                   std::optional<Time> reference_time,
                   const py::object& max_age,
                   bool strict) {
                    const ElementSetOptions options =
                        detail::element_set_options(mode,
                                                    std::move(include),
                                                    std::move(exclude),
                                                    reference_time,
                                                    max_age,
                                                    strict);
                    return format ? self.load(path, *format, options) : self.load(path, options);
                },
                py::arg("path"),
                py::kw_only(),
                py::arg("format") = py::none(),
                py::arg("mode") = Sgp4Mode::AFSPC,
                py::arg("include") = std::vector<int>{},
                py::arg("exclude") = std::vector<int>{},
                py::arg("reference_time") = py::none(),
                py::arg("max_age") = py::none(),
                py::arg("strict") = false,
                load_doc.c_str())
            .def("remove",
                 &HandleType::remove,
                 py::arg("catalog_number"),
                 "Remove a satellite, which something else can then place; False if it held none")
            .def("clear", &HandleType::clear, "Remove every satellite")
            .def("size", &HandleType::size, "The number of satellites it holds")
            .def("__len__", &HandleType::size)
            .def("contains",
                 &HandleType::contains,
                 py::arg("catalog_number"),
                 "Whether it holds a satellite")
            .def("__contains__", &HandleType::contains, py::arg("catalog_number"))
            .def("element_set",
                 &HandleType::element_set,
                 py::arg("catalog_number"),
                 "The element set (SatRec) it holds for a satellite; raises if it holds none")
            .def("element_sets",
                 &HandleType::element_sets,
                 "The element sets it holds, one for each satellite, in no particular order")
            .def("set_unresolved_object",
                 &HandleType::set_unresolved_object,
                 py::arg("unresolved_object"),
                 "Set the unresolved object each satellite is seen as")
            .def("get_unresolved_object",
                 &HandleType::get_unresolved_object,
                 "The unresolved object each satellite is seen as")
            .def("set_visible",
                 &HandleType::set_visible,
                 py::arg("visible"),
                 "Show or hide its satellites: a scene view leaves out a hidden constellation's")
            .def("is_visible", &HandleType::is_visible, "Whether its satellites are shown")
            .def("__bool__", &HandleType::valid)
            .def("__repr__", [](const HandleType& self) {
                if (!self.valid()) {
                    return std::string("<ConstellationHandle (deleted)>");
                }
                return "<ConstellationHandle " + self.get()->get_info() + ": " +
                       std::to_string(self.size()) + " satellites>";
            });

    bind_handle_methods<Constellation<TSpectral>>(cls);
}

} // namespace huira
