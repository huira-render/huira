#pragma once

#include "pybind11/pybind11.h"
#include "pybind11/stl.h"

#include "huira/images/io/fits_metadata.hpp"

namespace py = pybind11;

namespace huira {

    inline void bind_fits_metadata(py::module_& m) {

        // -- FitsKeyword ------------------------------------------------------
        py::class_<FitsKeyword>(m, "FitsKeyword")
            .def(py::init<>())
            .def(py::init([](const std::string& key,
                py::object value,
                const std::string& comment) {
                    FitsKeyword kw;
                    kw.key = key;
                    kw.comment = comment;
                    if (py::isinstance<py::bool_>(value))
                        kw.value = value.cast<bool>();
                    else if (py::isinstance<py::int_>(value))
                        kw.value = value.cast<int>();
                    else if (py::isinstance<py::float_>(value))
                        kw.value = value.cast<double>();
                    else
                        kw.value = value.cast<std::string>();
                    return kw;
                }), py::arg("key"), py::arg("value"), py::arg("comment") = "")
            .def_readwrite("key", &FitsKeyword::key)
            .def_readwrite("comment", &FitsKeyword::comment)
            .def_property("value",
                [](const FitsKeyword& kw) -> py::object {
                    return std::visit([](auto&& v) -> py::object {
                        return py::cast(v);
                        }, kw.value);
                },
                [](FitsKeyword& kw, py::object val) {
                    if (py::isinstance<py::bool_>(val))
                        kw.value = val.cast<bool>();
                    else if (py::isinstance<py::int_>(val))
                        kw.value = val.cast<int>();
                    else if (py::isinstance<py::float_>(val))
                        kw.value = val.cast<double>();
                    else
                        kw.value = val.cast<std::string>();
                })
            .def("__repr__", [](const FitsKeyword& kw) {
            std::ostringstream os;
            os << "FitsKeyword('" << kw.key << "', ";
            std::visit([&](auto&& v) {
                using V = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<V, std::string>)
                    os << "'" << v << "'";
                else if constexpr (std::is_same_v<V, bool>)
                    os << (v ? "True" : "False");
                else
                    os << v;
                }, kw.value);
            if (!kw.comment.empty())
                os << ", '" << kw.comment << "'";
            os << ")";
            return os.str();
                })
            ;

        // -- FitsMetadata -----------------------------------------------------
        py::class_<FitsMetadata>(m, "FitsMetadata")
            .def(py::init<>())

            // Observation
            .def_readwrite("object", &FitsMetadata::object)
            .def_readwrite("telescop", &FitsMetadata::telescop)
            .def_readwrite("instrume", &FitsMetadata::instrume)
            .def_readwrite("observer", &FitsMetadata::observer)
            .def_readwrite("date_obs", &FitsMetadata::date_obs)
            .def_readwrite("origin", &FitsMetadata::origin)

            // Exposure / photometric
            .def_readwrite("exptime", &FitsMetadata::exptime)
            .def_readwrite("filter", &FitsMetadata::filter)
            .def_readwrite("bunit", &FitsMetadata::bunit)

            // Data range
            .def_readwrite("datamin", &FitsMetadata::datamin)
            .def_readwrite("datamax", &FitsMetadata::datamax)
            .def_readwrite("saturate", &FitsMetadata::saturate)

            // WCS
            .def_readwrite("crpix1", &FitsMetadata::crpix1)
            .def_readwrite("crpix2", &FitsMetadata::crpix2)
            .def_readwrite("crval1", &FitsMetadata::crval1)
            .def_readwrite("crval2", &FitsMetadata::crval2)
            .def_readwrite("cdelt1", &FitsMetadata::cdelt1)
            .def_readwrite("cdelt2", &FitsMetadata::cdelt2)
            .def_readwrite("ctype1", &FitsMetadata::ctype1)
            .def_readwrite("ctype2", &FitsMetadata::ctype2)
            .def_readwrite("equinox", &FitsMetadata::equinox)
            .def_readwrite("radesys", &FitsMetadata::radesys)

            // Free-form text
            .def_readwrite("comments", &FitsMetadata::comments)
            .def_readwrite("history", &FitsMetadata::history)

            // Custom keywords
            .def_readwrite("custom_keywords", &FitsMetadata::custom_keywords)

            // Methods
            .def("has_wcs", &FitsMetadata::has_wcs)

            .def("__repr__", [](const FitsMetadata& meta) {
            std::ostringstream os;
            os << "FitsMetadata(";
            if (!meta.object.empty()) os << "object='" << meta.object << "', ";
            if (!meta.telescop.empty()) os << "telescop='" << meta.telescop << "', ";
            if (meta.has_wcs()) os << "wcs=True, ";
            os << "custom_keywords=" << meta.custom_keywords.size();
            os << ")";
            return os.str();
                })
            ;
    }

}
