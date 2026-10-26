#include "bindings.hpp"
#include "huira/cameras/distortion_coeffs_py.ipp"
#include "huira/cameras/pixel_convention_py.ipp"
#include "huira/cameras/psfs/measured_psf.hpp"
#include "huira/cameras/sensor_calibration_py.ipp"
#include "huira/core/interval_py.ipp"
#include "huira/core/rotation_py.ipp"
#include "huira/core/spectral_bin_py.ipp"
#include "huira/core/spice_py.ipp"
#include "huira/core/time_py.ipp"
#include "huira/core/types_py.ipp"
#include "huira/handles/materials/texture_handle_py.ipp"
#include "huira/images/fft_convolver.hpp"
#include "huira/images/fits_metadata_py.ipp"
#include "huira/images/image_py.ipp"
#include "huira/render/ray_py.ipp"
#include "huira/scene/node.hpp"
#include "huira/scene/object_ids_py.ipp"
#include "huira/scene/pose_sources_py.ipp"
#include "huira/scene/scene_view.hpp"
#include "huira/units/units_py.ipp"
#include "huira/util/logger_py.ipp"
#include "huira/util/paths_py.ipp"
#include "pybind11/pybind11.h"
#include "pybind11/stl.h"

namespace py = pybind11;

namespace huira::python {

void bind_core(py::module_& m)
{
    // Bind core types and utilities, the math types first, which much else takes and returns:
    huira::bind_types(m);
    huira::bind_rotation(m);

    huira::bind_units(m);
    huira::bind_time(m);
    huira::bind_interval(m);
    huira::bind_bin(m);
    huira::bind_hit_record(m);
    huira::bind_object_ids(m);

    huira::bind_paths(m);
    huira::bind_logger(m);

    huira::spice::bind_spice(m);

    huira::bind_distortion_coefficients(m);
    huira::bind_pixel_convention(m);

    huira::bind_fits_metadata(m);
    huira::bind_common_images(m);

    // After the images and units it takes; before the spectral submodules, whose camera handles
    // take a BiasMapKind with a default:
    huira::bind_sensor_calibration(m);

    huira::bind_texture_handle<float>(m, "TextureHandle_float");
    huira::bind_texture_handle<huira::Vec3<float>>(m, "TextureHandle_vec3");

    // Observation Mode Enum:
    py::enum_<huira::ObservationMode>(m, "ObservationMode", py::arithmetic())
        .value("TRUE_STATE", huira::ObservationMode::TRUE_STATE)
        .value("GEOMETRIC_STATE", huira::ObservationMode::GEOMETRIC_STATE)
        .value("ABERRATED_STATE", huira::ObservationMode::ABERRATED_STATE);

    // The frame a point given to look_at() is in:
    py::enum_<huira::TargetFrame>(m, "TargetFrame")
        .value("PARENT", huira::TargetFrame::PARENT)
        .value("ROOT", huira::TargetFrame::ROOT);

    // What position and rotation functions are called with (after ObservationMode, which its
    // constructor takes by default):
    huira::bind_pose_context(m);

    // Alpha Mode Enum:
    // Not templated on the spectral type, so it is registered once here rather than
    // per spectral submodule. Must precede the spectral submodules, whose bind_spectral_render()
    // binds SceneView::evaluate_transmittance with an AlphaMode default argument - pybind11 casts
    // default values at definition time and would throw on an unregistered type.
    py::enum_<huira::AlphaMode>(m, "AlphaMode")
        .value("STOCHASTIC", huira::AlphaMode::Stochastic)
        .value("EXPECTED", huira::AlphaMode::Expected);

    // What a measured PSF's samples are (CameraModelHandle.set_measured_psf()):
    py::enum_<huira::PSFSampling>(m,
                                  "PSFSampling",
                                  "What the values of a measured PSF are, so "
                                  "that they become pixel values correctly.")
        .value("PointSampled",
               huira::PSFSampling::PointSampled,
               "The PSF's brightness at each sample's position: a PSF from an optical design "
               "software, from a model, or measured with pixels much smaller than the simulated "
               "sensor's.  Huira interpolates between the samples and adds up the pattern over "
               "each pixel.  Sample it finely, 4 or more samples per pixel.")
        .value("PixelIntegrated",
               huira::PSFSampling::PixelIntegrated,
               "What a pixel of the simulated sensor collects with its center at each sample: a "
               "star image taken with that sensor (1 sample per pixel), or an effective PSF "
               "combined from many such images at subpixel offsets (more). The pixel's area is "
               "already included, so Huira uses the values as they are.");

    // Whether a measured PSF holds the camera's defocus (CameraModelHandle.set_measured_psf()):
    py::enum_<huira::PSFFocus>(m,
                               "PSFFocus",
                               "Whether a measured PSF is that of a point in focus, or already "
                               "holds the defocus the camera's focus gives it.")
        .value("InFocus",
               huira::PSFFocus::InFocus,
               "The PSF of a point in focus: the camera's focus blurs it as it blurs the "
               "aperture's diffraction, for stars and for each unresolved object at its own "
               "depth.")
        .value("IncludesDefocus",
               huira::PSFFocus::IncludesDefocus,
               "The PSF as the camera, focused as it is, images a star: no defocus is added to "
               "it, and every unresolved source is drawn with it as it is, whatever its depth. "
               "Resolved bodies are still traced with depth of field and then blurred by "
               "this PSF too, so their defocus is approximate at best.");

    // How hard FFTW plans the transforms that convolve bodies with the PSF
    // (Renderer.set_fft_planning()):
    py::enum_<huira::FftPlanEffort>(m,
                                    "FftPlanEffort",
                                    "How hard FFTW plans its transforms: plans are kept for "
                                    "the rest of the process, for each size and precision.")
        .value("Estimate", huira::FftPlanEffort::Estimate, "Plans at once, by heuristics.")
        .value("Measure",
               huira::FftPlanEffort::Measure,
               "Times candidate transforms to find the fastest: a few seconds the first time a "
               "size is used, for transforms about 15% faster.");
}

void bind_core_functions(py::module_& m)
{
    // Reading and writing images, and converting their color: these take and return RGB images
    // (huira.rgb.Image), so they are bound after huira.rgb.
    huira::bind_image_io(m);
    huira::bind_image_utils(m);
}

} // namespace huira::python
