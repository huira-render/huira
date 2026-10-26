#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

#include "huira/cameras/pixel_convention.hpp"
#include "huira/images/color_map.hpp"
#include "huira/images/image.hpp"
#include "huira/images/io/color_space.hpp"
#include "huira/images/io/fits_io.hpp"
#include "huira/images/io/jpeg_io.hpp"
#include "huira/images/io/png_io.hpp"
#include "huira/images/io/read_image.hpp"
#include "huira/images/io/tiff_io.hpp"
#include "pybind11/numpy.h"
#include "pybind11/pybind11.h"
#include "pybind11/stl.h"
#include "pybind11/stl/filesystem.h"

namespace py = pybind11;

namespace huira {

// ---------------------------------------------------------------------------
// bind_image<PixelT>  --  registers Image<PixelT> plus numpy interop
// ---------------------------------------------------------------------------
template <IsImagePixel PixelT>
void bind_image(py::module_& m, const char* class_name)
{
    using Img = Image<PixelT>;
    using Traits = ImagePixelTraits<PixelT>;
    using Scalar = typename Traits::Scalar;
    constexpr auto C = Traits::channels;
    // A pixel value from Python: Python's vector is a huira.Vec3, in double precision.
    using ValueT = std::conditional_t<std::is_same_v<PixelT, Vec3<float>>, Vec3<double>, PixelT>;

    py::class_<Img>(
        m,
        class_name,
        "An image, stored with its top row first: pixel (x, y) is in column x and row y, counted "
        "down from the top.\n\n"
        "Arrays: Image(array, origin=...) copies a numpy array in, and to_numpy(origin=...) copies "
        "one out, of shape (height, width) for one channel and (height, width, channels) for "
        "more. origin says which row of the array is the top of the image: PixelOrigin.TopLeft "
        "if its first row is the top, as most image libraries and matplotlib have it, or "
        "PixelOrigin.BottomLeft if its first row is the bottom, as in FITS files (and the arrays "
        "astropy reads from them). An array doesn't record which, so origin must be given.\n\n"
        "For convenience, an image is also an array in its own layout, top row first: "
        "np.asarray(image) is a view of it, without copying (writing to the view changes the "
        "image), and numpy and matplotlib take it directly (plt.imshow(image) draws it the right "
        "way up). Nothing checks that layout against where the array goes next: to write an image "
        "with astropy, whose FITS files have their first row at the bottom, use "
        "to_numpy(origin=PixelOrigin.BottomLeft), or huira's write_fits(), which flips it.",
        py::buffer_protocol())

        // -----------------------------------------------------------------
        // Constructors
        // -----------------------------------------------------------------
        .def(py::init<>())
        .def(py::init<int, int>(), py::arg("width"), py::arg("height"))
        .def(py::init([](int width, int height, const ValueT& fill_value) {
                 return Img(width, height, PixelT(fill_value));
             }),
             py::arg("width"),
             py::arg("height"),
             py::arg("fill_value"))

        // Construct from a numpy array ------------------------------------
        .def(py::init([](py::array_t<Scalar, py::array::c_style | py::array::forcecast> arr,
                         PixelOrigin origin) {
                 auto buf = arr.request();
                 int h, w;

                 if constexpr (C == 1) {
                     // Accept (H, W) or (H, W, 1)
                     if (buf.ndim == 2) {
                         h = static_cast<int>(buf.shape[0]);
                         w = static_cast<int>(buf.shape[1]);
                     } else if (buf.ndim == 3 && buf.shape[2] == 1) {
                         h = static_cast<int>(buf.shape[0]);
                         w = static_cast<int>(buf.shape[1]);
                     } else {
                         throw std::runtime_error(
                             "Expected a 2-D array (H, W) for single-channel Image");
                     }
                 } else {
                     if (buf.ndim != 3 || static_cast<std::size_t>(buf.shape[2]) != C) {
                         std::ostringstream os;
                         os << "Expected array of shape (H, W, " << C << ")";
                         throw std::runtime_error(os.str());
                     }
                     h = static_cast<int>(buf.shape[0]);
                     w = static_cast<int>(buf.shape[1]);
                 }

                 Img img(w, h);
                 const Scalar* src = static_cast<const Scalar*>(buf.ptr);

                 // PixelT must be tightly packed scalars for memcpy to work.
                 static_assert(sizeof(PixelT) == sizeof(Scalar) * C,
                               "PixelT layout is not contiguous scalars");
                 const auto width = static_cast<std::size_t>(w);
                 for (int row = 0; row < h; ++row) {
                     // The array's row, as a row of the image, which is stored top row first:
                     const int y = origin == PixelOrigin::TopLeft ? row : h - 1 - row;
                     const Scalar* src_row = src + static_cast<std::size_t>(row) * width * C;
                     PixelT* dst_row = img.data() + static_cast<std::size_t>(y) * width;
                     for (std::size_t i = 0; i < width; ++i) {
                         if constexpr (C == 1) {
                             dst_row[i] = src_row[i];
                         } else if constexpr (IsNonSpectralPixel<PixelT>) {
                             for (std::size_t c = 0; c < C; ++c) {
                                 dst_row[i][static_cast<int>(c)] = src_row[i * C + c];
                             }
                         } else {
                             for (std::size_t c = 0; c < C; ++c) {
                                 dst_row[i][c] = src_row[i * C + c];
                             }
                         }
                     }
                 }
                 return img;
             }),
             py::arg("array"),
             py::kw_only(),
             py::arg("origin"),
             "Copy a numpy array into a new image: (height, width) for one channel, "
             "(height, width, channels) for more. origin says which row of the array is the top "
             "of the image: PixelOrigin.TopLeft if its first row is, PixelOrigin.BottomLeft if "
             "its first row is the bottom (as in FITS files, and the arrays astropy reads from "
             "them).")

        // -----------------------------------------------------------------
        // Buffer protocol  --  np.asarray(img) gives a zero-copy view, in the image's own
        // layout: top row first. See the class docstring.
        // -----------------------------------------------------------------
        .def_buffer([](Img& img) -> py::buffer_info {
            if constexpr (C == 1) {
                return py::buffer_info(
                    static_cast<void*>(img.data()),
                    static_cast<py::ssize_t>(sizeof(Scalar)),
                    py::format_descriptor<Scalar>::format(),
                    2,
                    std::vector<py::ssize_t>{static_cast<py::ssize_t>(img.height()),
                                             static_cast<py::ssize_t>(img.width())},
                    std::vector<py::ssize_t>{
                        static_cast<py::ssize_t>(sizeof(Scalar) *
                                                 static_cast<std::size_t>(img.width())),
                        static_cast<py::ssize_t>(sizeof(Scalar))});
            } else {
                return py::buffer_info(
                    static_cast<void*>(img.data()),
                    static_cast<py::ssize_t>(sizeof(Scalar)),
                    py::format_descriptor<Scalar>::format(),
                    3,
                    std::vector<py::ssize_t>{static_cast<py::ssize_t>(img.height()),
                                             static_cast<py::ssize_t>(img.width()),
                                             static_cast<py::ssize_t>(C)},
                    std::vector<py::ssize_t>{
                        static_cast<py::ssize_t>(sizeof(Scalar) * C *
                                                 static_cast<std::size_t>(img.width())),
                        static_cast<py::ssize_t>(sizeof(Scalar) * C),
                        static_cast<py::ssize_t>(sizeof(Scalar))});
            }
        })

        // -----------------------------------------------------------------
        // to_numpy()  --  always returns an owning copy, in the row order asked for
        // -----------------------------------------------------------------
        .def(
            "to_numpy",
            [](const Img& img, PixelOrigin origin) {
                std::vector<py::ssize_t> shape{static_cast<py::ssize_t>(img.height()),
                                               static_cast<py::ssize_t>(img.width())};
                if constexpr (C != 1) {
                    shape.push_back(static_cast<py::ssize_t>(C));
                }
                py::array_t<Scalar> arr(shape);
                const auto width = static_cast<std::size_t>(img.width());
                const std::size_t row_values = width * C;
                Scalar* dst = arr.mutable_data();
                for (int y = 0; y < img.height(); ++y) {
                    // The image's row, as a row of the array:
                    const int row = origin == PixelOrigin::TopLeft ? y : img.height() - 1 - y;
                    std::memcpy(dst + static_cast<std::size_t>(row) * row_values,
                                img.data() + static_cast<std::size_t>(y) * width,
                                row_values * sizeof(Scalar));
                }
                return arr;
            },
            py::kw_only(),
            py::arg("origin"),
            "Copy the image into a new numpy array: (height, width) for one channel, "
            "(height, width, channels) for more. origin says which row of the array is to be the "
            "top of the image: PixelOrigin.TopLeft for the first row, as np.asarray(image) has "
            "it, or PixelOrigin.BottomLeft for the last, as FITS files have it (and astropy "
            "writes them).")

        // -----------------------------------------------------------------
        // Properties and methods
        // -----------------------------------------------------------------
        .def_property_readonly("width", &Img::width)
        .def_property_readonly("height", &Img::height)
        .def_property_readonly("resolution",
                               [](const Img& img) {
                                   auto r = img.resolution();
                                   return py::make_tuple(r.width, r.height);
                               })
        .def_property_readonly("shape",
                               [](const Img& img) {
                                   constexpr auto ch = Traits::channels;
                                   if constexpr (ch == 1) {
                                       return py::make_tuple(img.height(), img.width());
                                   } else {
                                       return py::make_tuple(img.height(), img.width(), ch);
                                   }
                               })
        .def_property_readonly("channels", [](const Img&) { return static_cast<int>(C); })
        .def_property_readonly("empty", &Img::empty)

        .def_property("sensor_bit_depth", &Img::sensor_bit_depth, &Img::set_sensor_bit_depth)

        .def(
            "fill",
            [](Img& img, const ValueT& value) { img.fill(PixelT(value)); },
            py::arg("value"))
        .def("clear", &Img::clear)

        .def("get_channel",
             &Img::get_channel,
             py::arg("channel"),
             "Extract a single channel as an Image_f32 (0-indexed).")

        .def("__repr__", [class_name](const Img& img) {
            std::ostringstream os;
            os << class_name << "(" << img.width() << "x" << img.height() << ", channels=" << C
               << ")";
            return os.str();
        });
}

template <IsSpectral TSpectral>
void bind_rgb_to_spectral(py::module_& m)
{
    m.def(
        "rgb_to_spectral",
        [](Image<RGB> rgb_image, const py::object& spectral_conversion) {
            if (spectral_conversion.is_none()) {
                py::gil_scoped_release release;
                return rgb_to_spectral<TSpectral>(std::move(rgb_image));
            }
            auto fn = spectral_conversion.cast<std::function<TSpectral(RGB)>>();
            return rgb_to_spectral<TSpectral>(std::move(rgb_image), std::move(fn));
        },
        py::arg("rgb_image"),
        py::arg("spectral_conversion") = py::none(),
        "Convert an Image_rgb to this module's spectral representation.\n"
        "spectral_conversion: optional callable mapping an RGB pixel to this module's\n"
        "SpectralBins. Defaults to huira's band-integration conversion.");
}

// ---------------------------------------------------------------------------
// bind_image_io  --  free-standing read / write functions
// ---------------------------------------------------------------------------
inline void bind_image_io(py::module_& m)
{
    // =============== //
    // === PNG I/O === //
    // =============== //
    m.def(
        "read_png",
        [](const fs::path& path, bool read_alpha) { return read_image_png(path, read_alpha); },
        py::arg("filepath"),
        py::arg("read_alpha") = true,
        "Read a PNG into an RGB Bundle");

    m.def(
        "read_png_mono",
        [](const fs::path& path, bool read_alpha) { return read_image_png_mono(path, read_alpha); },
        py::arg("filepath"),
        py::arg("read_alpha") = true,
        "Read a PNG into a Float Bundle");

    m.def(
        "write_png",
        [](const fs::path& p, const ImageBundle<float>& bundle) { write_image_png(p, bundle); },
        py::arg("filepath"),
        py::arg("bundle"),
        "Write a PNG of bundle.bit_depth (8 or 16) bits. Values are written as they are, "
        "labelled with bundle.color_space (convert with linear_to_srgb for a picture). A sensor "
        "response is written as its DN (scaled by left-bit replication, with an sBIT chunk) "
        "unless bundle.scaling is FullRange. Values that are not finite are written as 0.");

    m.def(
        "write_png",
        [](const fs::path& p, const ImageBundle<RGB>& bundle) { write_image_png(p, bundle); },
        py::arg("filepath"),
        py::arg("bundle"),
        "Write a PNG of bundle.bit_depth (8 or 16) bits. Values are written as they are, "
        "labelled with bundle.color_space (convert with linear_to_srgb for a picture). A sensor "
        "response is written as its DN (scaled by left-bit replication, with an sBIT chunk) "
        "unless bundle.scaling is FullRange. Values that are not finite are written as 0.");

    // ================ //
    // === JPEG I/O === //
    // ================ //
    m.def(
        "read_jpeg",
        [](const fs::path& path) { return read_image_jpeg(path); },
        py::arg("filepath"),
        "Read a JPEG into an RGB Bundle");

    m.def(
        "read_jpeg_mono",
        [](const fs::path& path) { return read_image_jpeg_mono(path); },
        py::arg("filepath"),
        "Read a JPEG into a Float Bundle");

    m.def(
        "write_jpeg",
        [](const fs::path& p, const ImageBundle<float>& bundle, int quality) {
            write_image_jpeg(p, bundle, quality);
        },
        py::arg("filepath"),
        py::arg("bundle"),
        py::arg("quality") = 95);

    m.def(
        "write_jpeg",
        [](const fs::path& p, const ImageBundle<RGB>& bundle, int quality) {
            write_image_jpeg(p, bundle, quality);
        },
        py::arg("filepath"),
        py::arg("bundle"),
        py::arg("quality") = 95);

    // ================ //
    // === TIFF I/O === //
    // ================ //
    m.def(
        "read_tiff",
        [](const fs::path& path, bool read_alpha) { return read_image_tiff_rgb(path, read_alpha); },
        py::arg("filepath"),
        py::arg("read_alpha") = true,
        "Read a TIFF into an RGB Bundle");

    m.def(
        "read_tiff_mono",
        [](const fs::path& path, bool read_alpha) {
            return read_image_tiff_mono(path, read_alpha);
        },
        py::arg("filepath"),
        py::arg("read_alpha") = true,
        "Read a TIFF into a Float Bundle");

    m.def(
        "write_tiff",
        [](const fs::path& p,
           const ImageBundle<float>& bundle,
           const std::string& desc,
           const std::string& artist) { write_image_tiff(p, bundle, desc, artist); },
        py::arg("filepath"),
        py::arg("bundle"),
        py::arg("description") = "",
        py::arg("artist") = "",
        "Write a Float Bundle to a TIFF file of bundle.bit_depth (8 or 16) bits. A sensor "
        "response is written as its raw DN (0-4095 for 12 bits), with MaxSampleValue, unless "
        "bundle.scaling is FullRange. Values that are not finite are written as 0.");

    m.def(
        "write_tiff",
        [](const fs::path& p,
           const ImageBundle<RGB>& bundle,
           const std::string& desc,
           const std::string& artist) { write_image_tiff(p, bundle, desc, artist); },
        py::arg("filepath"),
        py::arg("bundle"),
        py::arg("description") = "",
        py::arg("artist") = "",
        "Write an RGB Bundle to a TIFF file. See the Float Bundle overload.");

    // ================ //
    // === FITS I/O === //
    // ================ //
    m.def(
        "read_fits",
        [](const fs::path& path) {
            auto [image, metadata] = read_image_fits(path);
            return py::make_tuple(std::move(image), std::move(metadata));
        },
        py::arg("filepath"),
        "Read a single-plane FITS image: returns (Image_f32, FitsMetadata). A sensor response "
        "written by write_fits (with ADCBITS) comes back as it was, DN / (2^bits - 1), with its "
        "sensor_bit_depth; other integer images are normalized by SATURATE or their type's range; "
        "other float images are returned as they are. Undefined pixels are NaN.");

    m.def(
        "write_fits",
        [](const fs::path& path,
           const Image<float>& image,
           int bit_depth,
           const FitsMetadata& metadata,
           PixelScaling scaling) { write_image_fits(path, image, bit_depth, metadata, scaling); },
        py::arg("filepath"),
        py::arg("image"),
        py::arg("bit_depth") = -32,
        py::arg("metadata") = FitsMetadata{},
        py::arg("scaling") = PixelScaling::Counts,
        "Write an Image_f32 to FITS, with BITPIX 8, 16 or 32 (unsigned integers), -32 or -64 "
        "(floats). A sensor response (an image with a sensor_bit_depth) is written as the "
        "sensor's digital numbers at every BITPIX, unless scaling is FullRange; with fewer bits "
        "than the sensor, the lowest are dropped and BSCALE scales the stored values back to DN. "
        "Undefined pixels are NaN in a float file and BLANK in an integer one.");

    // ========================================== //
    // === Generic Image I/O (Format Agnostic) === //
    // ========================================== //

    m.def(
        "read_image",
        [](const fs::path& path, bool read_alpha) { return read_image(path, read_alpha); },
        py::arg("filepath"),
        py::arg("read_alpha") = true,
        "Read an image from disk into an RGB Bundle, auto-detecting format");

    m.def(
        "read_image_mono",
        [](const fs::path& path, bool read_alpha) { return read_image_mono(path, read_alpha); },
        py::arg("filepath"),
        py::arg("read_alpha") = true,
        "Read an image from disk into a Float Bundle, auto-detecting format");

    // ======================================================== //
    // === Color Space Conversions (Direct Image Overloads) === //
    // ======================================================== //

    // Linear -> sRGB (Accepting a raw RGB Image reference)
    m.def(
        "linear_to_srgb",
        [](const Image<RGB>& img) {
            Image<RGB> srgb_image(img.width(), img.height());
            for (int y = 0; y < img.height(); ++y) {
                for (int x = 0; x < img.width(); ++x) {
                    const RGB& linear_pixel = img(x, y);
                    srgb_image(x, y) = RGB{linear_to_srgb(linear_pixel[0]),
                                           linear_to_srgb(linear_pixel[1]),
                                           linear_to_srgb(linear_pixel[2])};
                }
            }
            ImageBundle<RGB> output_bundle(std::move(srgb_image));
            output_bundle.color_space = ColorSpaceHint::sRGB;
            return output_bundle;
        },
        py::arg("image"),
        "Convert a raw linear RGB image reference directly to an sRGB bundle");

    // Linear -> sRGB (Accepting a raw Float Image reference)
    m.def(
        "linear_to_srgb",
        [](const Image<float>& img) {
            Image<float> srgb_image(img.width(), img.height());
            for (int y = 0; y < img.height(); ++y) {
                for (int x = 0; x < img.width(); ++x) {
                    const float& linear_pixel = img(x, y);
                    srgb_image(x, y) = linear_to_srgb(linear_pixel);
                }
            }
            ImageBundle<float> output_bundle(std::move(srgb_image));
            output_bundle.color_space = ColorSpaceHint::sRGB;
            return output_bundle;
        },
        py::arg("image"),
        "Convert a raw linear float image reference directly to an sRGB bundle");

    // sRGB -> Linear (Accepting a raw RGB Image reference)
    m.def(
        "srgb_to_linear",
        [](const Image<RGB>& img) {
            Image<RGB> linear_image(img.width(), img.height());
            for (int y = 0; y < img.height(); ++y) {
                for (int x = 0; x < img.width(); ++x) {
                    const RGB& srgb_pixel = img(x, y);
                    linear_image(x, y) = RGB{srgb_to_linear(srgb_pixel[0]),
                                             srgb_to_linear(srgb_pixel[1]),
                                             srgb_to_linear(srgb_pixel[2])};
                }
            }
            ImageBundle<RGB> output_bundle(std::move(linear_image));
            output_bundle.color_space = ColorSpaceHint::Linear;
            return output_bundle;
        },
        py::arg("image"),
        "Convert a raw sRGB image reference directly to a linear bundle");

    // sRGB -> Linear (Accepting a raw Float Image reference)
    m.def(
        "srgb_to_linear",
        [](const Image<float>& img) {
            Image<float> linear_image(img.width(), img.height());
            for (int y = 0; y < img.height(); ++y) {
                for (int x = 0; x < img.width(); ++x) {
                    const float& srgb_pixel = img(x, y);
                    linear_image(x, y) = srgb_to_linear(srgb_pixel);
                }
            }
            ImageBundle<float> output_bundle(std::move(linear_image));
            output_bundle.color_space = ColorSpaceHint::Linear;
            return output_bundle;
        },
        py::arg("image"),
        "Convert a raw sRGB float image reference directly to a linear bundle");

    // The same for bundles, as in C++ (read_image() and the other readers return bundles). The
    // bundle's alpha and its other settings are kept.
    m.def(
        "linear_to_srgb",
        [](const ImageBundle<RGB>& bundle) { return linear_to_srgb(bundle); },
        py::arg("bundle"),
        "Convert a linear RGB bundle to an sRGB one, keeping its alpha");
    m.def(
        "linear_to_srgb",
        [](const ImageBundle<float>& bundle) { return linear_to_srgb(bundle); },
        py::arg("bundle"),
        "Convert a linear float bundle to an sRGB one, keeping its alpha");
    m.def(
        "srgb_to_linear",
        [](const ImageBundle<RGB>& bundle) { return srgb_to_linear(bundle); },
        py::arg("bundle"),
        "Convert an sRGB RGB bundle, as read_image() returns for a JPEG or PNG, to a linear one, "
        "keeping its alpha");
    m.def(
        "srgb_to_linear",
        [](const ImageBundle<float>& bundle) { return srgb_to_linear(bundle); },
        py::arg("bundle"),
        "Convert an sRGB float bundle to a linear one, keeping its alpha");
}

// ---------------------------------------------------------------------------
// bind_image_utils  --  visualization and map conversion tools
// ---------------------------------------------------------------------------
inline void bind_image_utils(py::module_& m)
{
    m.def("normal_map",
          &normal_map,
          py::arg("normals"),
          "Convert a normal map (Image_Vec3f) into an RGB visualization image mapped to.");

    m.def("depth_map",
          &depth_map,
          py::arg("depth"),
          "Convert a depth map (Image_f32) into an auto-normalized RGB visualization image.");
}

template <IsImagePixel PixelT>
void bind_image_bundle(py::module_& m, const std::string& class_name)
{
    using Bundle = ImageBundle<PixelT>;
    using Img = Image<PixelT>;

    py::class_<Bundle>(m, class_name.c_str())
        // Expose the constructors
        .def(py::init<Img>(), py::arg("image"))
        .def(py::init<Img, Image<float>>(), py::arg("image"), py::arg("alpha"))

        // Expose the metadata fields
        .def_readwrite("image", &Bundle::image)
        .def_readwrite("alpha", &Bundle::alpha)
        .def_readwrite("color_space", &Bundle::color_space)
        .def_readwrite("gamma_value", &Bundle::gamma_value)
        .def_readwrite("bit_depth", &Bundle::bit_depth)
        .def_readwrite("scaling", &Bundle::scaling);

    py::implicitly_convertible<Img, Bundle>();
}

// ---------------------------------------------------------------------------
// bind_all_images  --  call this once from PYBIND11_MODULE
// ---------------------------------------------------------------------------
inline void bind_common_images(py::module_& m)
{
    py::enum_<ColorSpaceHint>(m, "ColorSpaceHint")
        .value("Linear", ColorSpaceHint::Linear)
        .value("sRGB", ColorSpaceHint::sRGB)
        .value("Gamma", ColorSpaceHint::Gamma)
        .value("Unknown", ColorSpaceHint::Unknown)
        .export_values();

    py::enum_<PixelScaling>(m,
                            "PixelScaling",
                            "How an image with a sensor bit depth (a sensor response) is written: "
                            "Counts, as the sensor's digital numbers (the default), or FullRange, "
                            "stretched over the file's range for display.")
        .value("Counts", PixelScaling::Counts)
        .value("FullRange", PixelScaling::FullRange);

    // Scalar images
    bind_image<float>(m, "Image_f32");
    bind_image<double>(m, "Image_f64");
    bind_image<uint8_t>(m, "Image_u8");
    bind_image<uint16_t>(m, "Image_u16");
    bind_image<uint32_t>(m, "Image_u32");
    bind_image<uint64_t>(m, "Image_u64");

    bind_image<Vec3<float>>(m, "Image_Vec3f");

    bind_image_bundle<float>(m, "ImageBundle_f32");
    bind_image_bundle<double>(m, "ImageBundle_f64");
    bind_image_bundle<uint8_t>(m, "ImageBundle_u8");
    bind_image_bundle<uint16_t>(m, "ImageBundle_u16");
    bind_image_bundle<uint32_t>(m, "ImageBundle_u32");
    bind_image_bundle<uint64_t>(m, "ImageBundle_u64");

    bind_image_bundle<Vec3<float>>(m, "ImageBundle_Vec3f");

    // The functions that read, write and convert images are bound by bind_image_io() and
    // bind_image_utils(), after the spectral submodules, whose RGB images they take and return.
}

} // namespace huira
