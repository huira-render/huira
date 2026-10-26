#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "huira/core/time.hpp"
#include "huira/ephemeris/element_set_catalog.hpp"
#include "huira/units/units_py.ipp"
#include "pybind11/pybind11.h"
#include "pybind11/stl.h"
#include "pybind11/stl/filesystem.h"

namespace py = pybind11;

namespace huira {
namespace detail {
/// The options, as Python gives them: max_age as a duration in any unit of time, or None.
inline ElementSetOptions element_set_options(Sgp4Mode mode,
                                             std::vector<int> include,
                                             std::vector<int> exclude,
                                             std::optional<Time> reference_time,
                                             const py::object& max_age,
                                             bool strict)
{
    ElementSetOptions options;
    options.mode = mode;
    options.include = std::move(include);
    options.exclude = std::move(exclude);
    options.reference_time = reference_time;
    if (!max_age.is_none()) {
        options.max_age = unit_from_py<units::Day>(max_age);
    }
    options.strict = strict;
    return options;
}

inline constexpr const char* ELEMENT_SET_OPTIONS_DOC = R"(
mode: the Sgp4Mode to propagate them in; leave it AFSPC unless you have a
reason not to.
include: keep only the satellites with these catalog numbers, or every one if
it is empty.
exclude: leave out the satellites with these catalog numbers.
reference_time: when the element sets are to be used. Of a satellite's element
sets, the one whose epoch is nearest it is kept, or with none, the newest.
max_age: leave out element sets whose epoch is further than this from
reference_time, as a duration such as huira.units.Day(14).
strict: raise on the first entry that isn't an element set SGP4 can propagate,
rather than skip it.)";
} // namespace detail

inline void bind_element_set_catalog(py::module_& m)
{
    // pybind11 copies a docstring as it binds a function, but these are kept for the module's life
    static const std::string read_doc =
        std::string("Read a catalog of element sets from a file.\n\nformat: an ElementSetFormat, "
                    "or None to work it out from the text.") +
        detail::ELEMENT_SET_OPTIONS_DOC +
        "\n\nRaises RuntimeError if the file can't be read or split into entries, or, with "
        "strict, at the first bad entry.";
    static const std::string parse_doc =
        std::string("Read a catalog of element sets from its text, in the ElementSetFormat "
                    "given.\n") +
        detail::ELEMENT_SET_OPTIONS_DOC;

    py::enum_<ElementSetFormat>(
        m, "ElementSetFormat", "The formats a catalog of element sets is read from")
        .value("TLE", ElementSetFormat::TLE, "TLEs, each with a name line before it or not")
        .value("OMM_KVN", ElementSetFormat::OMM_KVN, "OMMs in CCSDS keyword = value notation")
        .value("OMM_CSV", ElementSetFormat::OMM_CSV, "OMMs as CSV, under a header of keywords")
        .value("OMM_JSON", ElementSetFormat::OMM_JSON, "OMMs as JSON: an array of objects");

    py::enum_<SkipReason>(m, "SkipReason", "Why an entry of a catalog was left out of it")
        .value("INVALID",
               SkipReason::INVALID,
               "It isn't an element set SGP4 can propagate, or isn't one at all")
        .value("NOT_INCLUDED", SkipReason::NOT_INCLUDED, "Its satellite isn't one of include")
        .value("EXCLUDED", SkipReason::EXCLUDED, "Its satellite is one of exclude")
        .value("TOO_OLD",
               SkipReason::TOO_OLD,
               "Its epoch is further than max_age from the reference time")
        .value(
            "SUPERSEDED", SkipReason::SUPERSEDED, "Another element set for its satellite was kept");

    py::class_<SkippedElementSet>(m, "SkippedElementSet", "An entry of a catalog left out of it")
        .def_readonly(
            "entry", &SkippedElementSet::entry, "Which entry of the catalog it was, counted from 0")
        .def_readonly("line",
                      &SkippedElementSet::line,
                      "The line its entry starts on, counted from 1, or 0 in a JSON catalog")
        .def_readonly("catalog_number",
                      &SkippedElementSet::catalog_number,
                      "Its satellite's NORAD catalog number, or 0 if it couldn't be read")
        .def_readonly("name", &SkippedElementSet::name, "Its satellite's name, or ''")
        .def_readonly("reason", &SkippedElementSet::reason, "Why, as a SkipReason")
        .def_readonly("message", &SkippedElementSet::message, "Why, in words")
        .def("__repr__", [](const SkippedElementSet& self) {
            return "<SkippedElementSet entry " + std::to_string(self.entry) +
                   (self.line != 0 ? ", line " + std::to_string(self.line) : std::string()) + ": " +
                   self.message + ">";
        });

    py::class_<ElementSetCatalog>(m,
                                  "ElementSetCatalog",
                                  "Satellites' element sets read from a catalog of TLEs or OMMs, "
                                  "with a report of the entries left out of it and why. Make one "
                                  "with read() or parse().")
        .def_readonly(
            "element_sets", &ElementSetCatalog::element_sets, "The element sets kept (SatRec)")
        .def_readonly("skipped",
                      &ElementSetCatalog::skipped,
                      "The entries left out, and why (SkippedElementSet), in the catalog's order")
        .def_static(
            "read",
            [](const std::filesystem::path& path,
               std::optional<ElementSetFormat> format,
               Sgp4Mode mode,
               std::vector<int> include,
               std::vector<int> exclude,
               std::optional<Time> reference_time,
               const py::object& max_age,
               bool strict) {
                const ElementSetOptions options = detail::element_set_options(
                    mode, std::move(include), std::move(exclude), reference_time, max_age, strict);
                return format ? ElementSetCatalog::read(path, *format, options)
                              : ElementSetCatalog::read(path, options);
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
            read_doc.c_str())
        .def_static(
            "parse",
            [](std::string_view text,
               ElementSetFormat format,
               Sgp4Mode mode,
               std::vector<int> include,
               std::vector<int> exclude,
               std::optional<Time> reference_time,
               const py::object& max_age,
               bool strict) {
                return ElementSetCatalog::parse(text,
                                                format,
                                                detail::element_set_options(mode,
                                                                            std::move(include),
                                                                            std::move(exclude),
                                                                            reference_time,
                                                                            max_age,
                                                                            strict));
            },
            py::arg("text"),
            py::arg("format"),
            py::kw_only(),
            py::arg("mode") = Sgp4Mode::AFSPC,
            py::arg("include") = std::vector<int>{},
            py::arg("exclude") = std::vector<int>{},
            py::arg("reference_time") = py::none(),
            py::arg("max_age") = py::none(),
            py::arg("strict") = false,
            parse_doc.c_str())
        .def_static("detect_format",
                    &ElementSetCatalog::detect_format,
                    py::arg("text"),
                    "The ElementSetFormat a catalog's text is in, from how it starts")
        .def("__len__", [](const ElementSetCatalog& self) { return self.element_sets.size(); })
        .def("__repr__", [](const ElementSetCatalog& self) {
            return "<ElementSetCatalog: " + std::to_string(self.element_sets.size()) +
                   " element sets, " + std::to_string(self.skipped.size()) + " entries skipped>";
        });
}

} // namespace huira
