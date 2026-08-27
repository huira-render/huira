#pragma once

#include <optional>

#include "huira/cameras/optics.hpp"
#include "pybind11/pybind11.h"
#include "pybind11/stl.h"

namespace py = pybind11;

namespace huira {

/**
 * @brief Bind the spectral-independent optics types.
 *
 * DiffractionCore, HarveyShack, StrayLight, IdealCore, and PSFApplication carry no spectral
 * data, so they are bound once into the top-level module rather than per spectral type.
 */
inline void bind_optics_types(py::module_& m)
{
    py::class_<IdealCore>(m, "IdealCore", "Perfect optics: point sources land as delta functions.")
        .def(py::init<>())
        .def("__repr__", [](const IdealCore&) { return "IdealCore()"; });

    py::class_<DiffractionCore>(
        m,
        "DiffractionCore",
        "Diffraction core derived from the aperture, focal length, and pixel pitch.")
        .def(py::init([](std::optional<int> radius, int banks) {
                 DiffractionCore core;
                 core.radius = radius;
                 core.banks = banks;
                 return core;
             }),
             py::arg("radius") = py::none(),
             py::arg("banks") = 16)
        .def_readwrite("radius",
                       &DiffractionCore::radius,
                       "Stamping kernel radius in pixels. None derives it from the Airy radius.")
        .def_readwrite("banks", &DiffractionCore::banks, "Polyphase banks per axis.")
        .def_static("airy_radius_pixels",
                    &DiffractionCore::airy_radius_pixels,
                    py::arg("max_wavelength"),
                    py::arg("f_number"),
                    py::arg("min_pitch"),
                    "Radius of the first Airy zero, in pixels.")
        .def("__repr__", [](const DiffractionCore& self) {
            return "DiffractionCore(radius=" +
                   (self.radius.has_value() ? std::to_string(self.radius.value())
                                            : std::string("None")) +
                   ", banks=" + std::to_string(self.banks) + ")";
        });

    py::class_<HarveyShack>(m, "HarveyShack", "Harvey-Shack scattered-light wings.")
        .def(py::init([](float fraction,
                         float exponent,
                         float r0,
                         float captured_energy,
                         std::optional<int> kernel_radius,
                         std::optional<float> cutoff_radius) {
                 HarveyShack scatter;
                 scatter.fraction = fraction;
                 scatter.exponent = exponent;
                 scatter.r0 = r0;
                 scatter.captured_energy = captured_energy;
                 scatter.kernel_radius = kernel_radius;
                 scatter.cutoff_radius = cutoff_radius;
                 return scatter;
             }),
             py::arg("fraction") = 0.f,
             py::arg("exponent") = 2.5f,
             py::arg("r0") = 0.5f,
             py::arg("captured_energy") = 0.98f,
             py::arg("kernel_radius") = py::none(),
             py::arg("cutoff_radius") = py::none())
        .def_readwrite(
            "fraction", &HarveyShack::fraction, "Fraction of total energy diverted into the wings.")
        .def_readwrite("exponent",
                       &HarveyShack::exponent,
                       "Power-law falloff exponent. Must exceed 2 unless cutoff_radius is set.")
        .def_readwrite("r0", &HarveyShack::r0, "Shoulder radius in pixels.")
        .def_readwrite("captured_energy",
                       &HarveyShack::captured_energy,
                       "Fraction of the scattered energy the kernel must span.")
        .def_readwrite("kernel_radius",
                       &HarveyShack::kernel_radius,
                       "Explicit kernel radius in pixels, or None to derive it.")
        .def_readwrite("cutoff_radius",
                       &HarveyShack::cutoff_radius,
                       "Radius beyond which the profile is zero, or None to leave it untruncated.")
        .def_static("radius_for_energy",
                    &HarveyShack::radius_for_energy,
                    py::arg("captured_energy"),
                    py::arg("exponent"),
                    py::arg("r0"),
                    "Radius enclosing the requested fraction of the scattered energy.")
        .def_static("energy_within",
                    &HarveyShack::energy_within,
                    py::arg("radius"),
                    py::arg("exponent"),
                    py::arg("r0"),
                    "Fraction of the scattered energy falling inside the given radius.")
        .def("validate", &HarveyShack::validate, "Raise if any parameter is invalid.")
        .def("__repr__", [](const HarveyShack& self) {
            return "HarveyShack(fraction=" + std::to_string(self.fraction) +
                   ", exponent=" + std::to_string(self.exponent) +
                   ", r0=" + std::to_string(self.r0) + ")";
        });

    py::class_<StrayLight>(
        m, "StrayLight", "Energy diverted out of the PSF core by scattering and glare.")
        .def(py::init([](std::optional<HarveyShack> scatter, float veiling_glare) {
                 StrayLight stray;
                 stray.scatter = scatter;
                 stray.veiling_glare = veiling_glare;
                 return stray;
             }),
             py::arg("scatter") = py::none(),
             py::arg("veiling_glare") = 0.f)
        .def_readwrite("scatter", &StrayLight::scatter, "Scattered-light wings, or None.")
        .def_readwrite("veiling_glare",
                       &StrayLight::veiling_glare,
                       "Fraction of energy redistributed uniformly across the frame.")
        .def("validate", &StrayLight::validate, "Raise if the budget is inconsistent.");

    py::enum_<PSFApplication>(
        m, "PSFApplication", "Selects which parts of the image the optical model is applied to.")
        .value("Full", PSFApplication::Full, "Apply to geometry and unresolved sources.")
        .value("PointsOnly", PSFApplication::PointsOnly, "Apply to unresolved sources only.")
        .value("Off", PSFApplication::Off, "Render as though the optics were ideal.");
}

/**
 * @brief Bind the spectral-dependent optics types.
 *
 * MeasuredCore and CustomCore hold spectral data, so Optics and its core variant are bound
 * once per spectral module.
 *
 * @tparam TSpectral The spectral type (e.g., @ref RGB, @ref Visible8)
 */
template <IsSpectral TSpectral>
void bind_optics(py::module_& m)
{
    using MeasuredCoreType = MeasuredCore<TSpectral>;
    using OpticsType = Optics<TSpectral>;

    py::class_<MeasuredCoreType>(
        m, "MeasuredCore", "PSF core built from user-supplied measured data.")
        .def(py::init([](const Image<TSpectral>& data,
                         float samples_per_pixel,
                         std::optional<int> radius,
                         int banks) {
                 MeasuredCoreType core;
                 core.data = data;
                 core.samples_per_pixel = samples_per_pixel;
                 core.radius = radius;
                 core.banks = banks;
                 return core;
             }),
             py::arg("data"),
             py::arg("samples_per_pixel") = 1.f,
             py::arg("radius") = py::none(),
             py::arg("banks") = 16)
        .def_readwrite("data", &MeasuredCoreType::data, "Measured samples, centered on the image.")
        .def_readwrite("samples_per_pixel",
                       &MeasuredCoreType::samples_per_pixel,
                       "Measurement samples per sensor pixel per axis.")
        .def_readwrite("radius",
                       &MeasuredCoreType::radius,
                       "Stamping radius in pixels, or None to use the measured extent.")
        .def_readwrite("banks", &MeasuredCoreType::banks, "Polyphase banks per axis.");

    py::class_<OpticsType>(m, "Optics", "Complete optical description of a camera.")
        .def(py::init([](std::optional<typename OpticsType::Core> core,
                         std::optional<HarveyShack> scatter,
                         float veiling_glare) {
                 OpticsType optics;
                 if (core.has_value()) {
                     optics.core = core.value();
                 }
                 optics.stray_light.scatter = scatter;
                 optics.stray_light.veiling_glare = veiling_glare;
                 return optics;
             }),
             py::arg("core") = py::none(),
             py::arg("scatter") = py::none(),
             py::arg("veiling_glare") = 0.f)
        .def_readwrite("core", &OpticsType::core, "The PSF core.")
        .def_readwrite("stray_light", &OpticsType::stray_light, "Scatter and veiling glare.")
        .def_static("ideal", &OpticsType::ideal, "Optics with no PSF and no stray light.")
        .def_static("diffraction_limited",
                    &OpticsType::diffraction_limited,
                    "Diffraction-limited optics with no stray light.")
        .def_static("realistic",
                    &OpticsType::realistic,
                    "Diffraction-limited optics plus representative scatter and veiling glare. "
                    "The stray-light values describe a clean, well-baffled instrument and should "
                    "be replaced with measured values before photometric use.")
        .def("validate", &OpticsType::validate, "Raise if any parameter is invalid.");
}
} // namespace huira
