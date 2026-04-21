#pragma once

#include <cstdint>
#include <cstring>
#include <sstream>
#include <string>

#include "pybind11/pybind11.h"
#include "pybind11/numpy.h"
#include "pybind11/stl.h"

#include "huira/images/image.hpp"
#include "huira/images/io/color_space.hpp"
#include "huira/images/color_map.hpp"
#include "huira/images/io/jpeg_io.hpp"
#include "huira/images/io/png_io.hpp"
#include "huira/images/io/read_image.hpp"
#include "huira/images/io/tiff_io.hpp"

namespace py = pybind11;

namespace huira {

    // ---------------------------------------------------------------------------
    // bind_image<PixelT>  --  registers Image<PixelT> plus numpy interop
    // ---------------------------------------------------------------------------
    template <IsImagePixel PixelT>
    void bind_image(py::module_& m, const char* class_name) {
        using Img = Image<PixelT>;
        using Traits = ImagePixelTraits<PixelT>;
        using Scalar = typename Traits::Scalar;
        constexpr auto C = Traits::channels;

        py::class_<Img>(m, class_name, py::buffer_protocol())

            // -----------------------------------------------------------------
            // Constructors
            // -----------------------------------------------------------------
            .def(py::init<>())
            .def(py::init<int, int>(), py::arg("width"), py::arg("height"))
            .def(py::init<int, int, const PixelT&>(),
                py::arg("width"), py::arg("height"), py::arg("fill_value"))

            // Construct from a numpy array ------------------------------------
            .def(py::init([](py::array_t<Scalar, py::array::c_style |
                py::array::forcecast> arr) {
                    auto buf = arr.request();
                    int h, w;

                    if constexpr (C == 1) {
                        // Accept (H, W) or (H, W, 1)
                        if (buf.ndim == 2) {
                            h = static_cast<int>(buf.shape[0]);
                            w = static_cast<int>(buf.shape[1]);
                        }
                        else if (buf.ndim == 3 && buf.shape[2] == 1) {
                            h = static_cast<int>(buf.shape[0]);
                            w = static_cast<int>(buf.shape[1]);
                        }
                        else {
                            throw std::runtime_error(
                                "Expected a 2-D array (H, W) for single-channel Image");
                        }
                    }
                    else {
                        if (buf.ndim != 3 ||
                            static_cast<std::size_t>(buf.shape[2]) != C) {
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
                    PixelT* dst = img.data();
                    const std::size_t num_pixels = static_cast<std::size_t>(w) * static_cast<std::size_t>(h);
                    for (std::size_t i = 0; i < num_pixels; ++i) {
                        if constexpr (C == 1) {
                            dst[i] = src[i];
                        }
                        else {
                            if constexpr (IsNonSpectralPixel<PixelT>) {
                                for (std::size_t c = 0; c < C; ++c) {
                                    dst[i][static_cast<int>(c)] = src[i * C + c];
                                }
                            }
                            else {
                                for (std::size_t c = 0; c < C; ++c) {
                                    dst[i][c] = src[i * C + c];
                                }
                            }
                        }
                    }
                    return img;
                }),
                py::arg("array"),
                "Construct from a numpy array. "
                "Single-channel: (H, W). Multi-channel: (H, W, C).")

            // -----------------------------------------------------------------
            // Buffer protocol  --  np.asarray(img) gives a zero-copy view
            // -----------------------------------------------------------------
            .def_buffer([](Img& img) -> py::buffer_info {
            if constexpr (C == 1) {
                return py::buffer_info(
                    static_cast<void*>(img.data()),
                    static_cast<py::ssize_t>(sizeof(Scalar)),
                    py::format_descriptor<Scalar>::format(),
                    2,
                    std::vector<py::ssize_t>{ static_cast<py::ssize_t>(img.height()),
                    static_cast<py::ssize_t>(img.width()) },
                    std::vector<py::ssize_t>{ static_cast<py::ssize_t>(sizeof(Scalar)* static_cast<std::size_t>(img.width())),
                    static_cast<py::ssize_t>(sizeof(Scalar)) }
                );
            }
            else {
                return py::buffer_info(
                    static_cast<void*>(img.data()),
                    static_cast<py::ssize_t>(sizeof(Scalar)),
                    py::format_descriptor<Scalar>::format(),
                    3,
                    std::vector<py::ssize_t>{ static_cast<py::ssize_t>(img.height()),
                    static_cast<py::ssize_t>(img.width()),
                    static_cast<py::ssize_t>(C) },
                    std::vector<py::ssize_t>{ static_cast<py::ssize_t>(sizeof(Scalar)* C* static_cast<std::size_t>(img.width())),
                    static_cast<py::ssize_t>(sizeof(Scalar)* C),
                    static_cast<py::ssize_t>(sizeof(Scalar)) }
                );
            }
                })

            // -----------------------------------------------------------------
            // to_numpy()  --  always returns an owning copy
            // -----------------------------------------------------------------
            .def("to_numpy", [](const Img& img) {
            if constexpr (C == 1) {
                py::array_t<Scalar> arr(std::vector<py::ssize_t>{
                    static_cast<py::ssize_t>(img.height()),
                        static_cast<py::ssize_t>(img.width()) });
                std::memcpy(arr.mutable_data(), img.data(),
                    static_cast<std::size_t>(img.width()) * static_cast<std::size_t>(img.height())
                    * sizeof(Scalar));
                return arr;
            }
            else {
                py::array_t<Scalar> arr(std::vector<py::ssize_t>{
                    static_cast<py::ssize_t>(img.height()),
                        static_cast<py::ssize_t>(img.width()),
                        static_cast<py::ssize_t>(C) });
                std::memcpy(arr.mutable_data(), img.data(),
                    static_cast<std::size_t>(img.width()) * static_cast<std::size_t>(img.height())
                    * C * sizeof(Scalar));
                return arr;
            }
                }, "Return image data as a numpy array (always copies).")

            // -----------------------------------------------------------------
            // Properties and methods
            // -----------------------------------------------------------------
            .def_property_readonly("width", &Img::width)
            .def_property_readonly("height", &Img::height)
            .def_property_readonly("resolution", [](const Img& img) {
            auto r = img.resolution();
            return py::make_tuple(r.width, r.height);
                })
            .def_property_readonly("shape", [](const Img& img) {
                constexpr auto ch = Traits::channels;
                if constexpr (ch == 1)
                    return py::make_tuple(img.height(), img.width());
                else
                    return py::make_tuple(img.height(), img.width(), ch);
            })
            .def_property_readonly("channels", [](const Img&) {
            return static_cast<int>(C);
                })
            .def_property_readonly("empty", &Img::empty)

            .def_property("sensor_bit_depth",
                &Img::sensor_bit_depth,
                &Img::set_sensor_bit_depth)

            .def("fill", &Img::fill, py::arg("value"))
            .def("clear", &Img::clear)

            .def("get_channel", &Img::get_channel, py::arg("channel"),
                "Extract a single channel as an Image_f32 (0-indexed).")

            .def("__repr__", [class_name](const Img& img) {
            std::ostringstream os;
            os << class_name << "(" << img.width() << "x" << img.height()
                << ", channels=" << C << ")";
            return os.str();
                })
            ;
    }

    // ---------------------------------------------------------------------------
    // bind_image_io  --  free-standing read / write functions
    // ---------------------------------------------------------------------------
    inline void bind_image_io(py::module_& m) {
        // =============== //
        // === PNG I/O === //
        // =============== //
        m.def("read_png", [](const std::string& path, bool read_alpha) {
            return read_image_png(fs::path(path), read_alpha);
            }, py::arg("filepath"), py::arg("read_alpha") = true, "Read a PNG into an RGB Bundle");

        m.def("read_png_mono", [](const std::string& path, bool read_alpha) {
            return read_image_png_mono(fs::path(path), read_alpha);
            }, py::arg("filepath"), py::arg("read_alpha") = true, "Read a PNG into a Float Bundle");

        // Thanks to implicitly_convertible, this ONE binding accepts both:
        // huira.write_png("out.png", my_bundle) AND huira.write_png("out.png", my_image)
        m.def("write_png", [](const std::string& p, const ImageBundle<float>& bundle) {
            write_image_png(fs::path(p), bundle);
            }, py::arg("filepath"), py::arg("bundle"));

        m.def("write_png", [](const std::string& p, const ImageBundle<RGB>& bundle) {
            write_image_png(fs::path(p), bundle);
            }, py::arg("filepath"), py::arg("bundle"));


        // ================ //
        // === JPEG I/O === //
        // ================ //
        m.def("read_jpeg", [](const std::string& path) {
            return read_image_jpeg(fs::path(path));
            }, py::arg("filepath"), "Read a JPEG into an RGB Bundle");

        m.def("read_jpeg_mono", [](const std::string& path) {
            return read_image_jpeg_mono(fs::path(path));
            }, py::arg("filepath"), "Read a JPEG into a Float Bundle");

        m.def("write_jpeg", [](const std::string& p, const ImageBundle<float>& bundle, int quality) {
            write_image_jpeg(fs::path(p), bundle, quality);
            }, py::arg("filepath"), py::arg("bundle"), py::arg("quality") = 95);

        m.def("write_jpeg", [](const std::string& p, const ImageBundle<RGB>& bundle, int quality) {
            write_image_jpeg(fs::path(p), bundle, quality);
            }, py::arg("filepath"), py::arg("bundle"), py::arg("quality") = 95);

        
        // ================ //
        // === TIFF I/O === //
        // ================ //
        m.def("read_tiff", [](const std::string& path, bool read_alpha) {
            return read_image_tiff_rgb(fs::path(path), read_alpha);
            }, py::arg("filepath"), py::arg("read_alpha") = true, "Read a TIFF into an RGB Bundle");

        m.def("read_tiff_mono", [](const std::string& path, bool read_alpha) {
            return read_image_tiff_mono(fs::path(path), read_alpha);
            }, py::arg("filepath"), py::arg("read_alpha") = true, "Read a TIFF into a Float Bundle");

        m.def("write_tiff", [](const std::string& p, const ImageBundle<float>& bundle,
            const std::string& desc, const std::string& artist) {
                write_image_tiff(fs::path(p), bundle, desc, artist);
            }, py::arg("filepath"), py::arg("bundle"),
                py::arg("description") = "", py::arg("artist") = "",
                "Write a Float Bundle to a TIFF file");

        m.def("write_tiff", [](const std::string& p, const ImageBundle<RGB>& bundle,
            const std::string& desc, const std::string& artist) {
                write_image_tiff(fs::path(p), bundle, desc, artist);
            }, py::arg("filepath"), py::arg("bundle"),
                py::arg("description") = "", py::arg("artist") = "",
                "Write an RGB Bundle to a TIFF file");


        // ========================================== //
        // === Generic Image I/O (Format Agnostic) === //
        // ========================================== //
        
        m.def("read_image", [](const std::string& path, bool read_alpha) {
            return read_image(fs::path(path), read_alpha);
            }, py::arg("filepath"), py::arg("read_alpha") = true,
                "Read an image from disk into an RGB Bundle, auto-detecting format");

        m.def("read_image_mono", [](const std::string& path, bool read_alpha) {
            return read_image_mono(fs::path(path), read_alpha);
            }, py::arg("filepath"), py::arg("read_alpha") = true,
                "Read an image from disk into a Float Bundle, auto-detecting format");


        // ======================================================== //
        // === Color Space Conversions (Direct Image Overloads) === //
        // ======================================================== //

        // Linear -> sRGB (Accepting a raw RGB Image reference)
        m.def("linear_to_srgb", [](const Image<RGB>& img) {
            Image<RGB> srgb_image(img.width(), img.height());
            for (int y = 0; y < img.height(); ++y) {
                for (int x = 0; x < img.width(); ++x) {
                    const RGB& linear_pixel = img(x, y);
                    srgb_image(x, y) = RGB{
                        linear_to_srgb(linear_pixel[0]),
                        linear_to_srgb(linear_pixel[1]),
                        linear_to_srgb(linear_pixel[2])
                    };
                }
            }
            ImageBundle<RGB> output_bundle(std::move(srgb_image));
            output_bundle.color_space = ColorSpaceHint::sRGB;
            return output_bundle;
            }, py::arg("image"), "Convert a raw linear RGB image reference directly to an sRGB bundle");

        // Linear -> sRGB (Accepting a raw Float Image reference)
        m.def("linear_to_srgb", [](const Image<float>& img) {
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
            }, py::arg("image"), "Convert a raw linear float image reference directly to an sRGB bundle");

        // sRGB -> Linear (Accepting a raw RGB Image reference)
        m.def("srgb_to_linear", [](const Image<RGB>& img) {
            Image<RGB> linear_image(img.width(), img.height());
            for (int y = 0; y < img.height(); ++y) {
                for (int x = 0; x < img.width(); ++x) {
                    const RGB& srgb_pixel = img(x, y);
                    linear_image(x, y) = RGB{
                        srgb_to_linear(srgb_pixel[0]),
                        srgb_to_linear(srgb_pixel[1]),
                        srgb_to_linear(srgb_pixel[2])
                    };
                }
            }
            ImageBundle<RGB> output_bundle(std::move(linear_image));
            output_bundle.color_space = ColorSpaceHint::Linear;
            return output_bundle;
            }, py::arg("image"), "Convert a raw sRGB image reference directly to a linear bundle");

        // sRGB -> Linear (Accepting a raw Float Image reference)
        m.def("srgb_to_linear", [](const Image<float>& img) {
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
            }, py::arg("image"), "Convert a raw sRGB float image reference directly to a linear bundle");
    }

    // ---------------------------------------------------------------------------
    // bind_image_utils  --  visualization and map conversion tools
    // ---------------------------------------------------------------------------
    inline void bind_image_utils(py::module_& m) {
        m.def("normal_map", &normal_map, py::arg("normals"),
            "Convert a normal map (Image_Vec3f) into an RGB visualization image mapped to.");

        m.def("depth_map", &depth_map, py::arg("depth"),
            "Convert a depth map (Image_f32) into an auto-normalized RGB visualization image.");
    }

    template <IsImagePixel PixelT>
    void bind_image_bundle(py::module_& m, const std::string& class_name) {
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
            .def_readwrite("bit_depth", &Bundle::bit_depth);

        py::implicitly_convertible<Img, Bundle>();
    }

    // ---------------------------------------------------------------------------
    // bind_all_images  --  call this once from PYBIND11_MODULE
    // ---------------------------------------------------------------------------
    inline void bind_common_images(py::module_& m) {
        py::enum_<ColorSpaceHint>(m, "ColorSpaceHint")
            .value("Linear", ColorSpaceHint::Linear)
            .value("sRGB", ColorSpaceHint::sRGB)
            .value("Gamma", ColorSpaceHint::Gamma)
            .value("Unknown", ColorSpaceHint::Unknown)
            .export_values();

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

        // IO functions
        bind_image_io(m);

        // Utility functions
        bind_image_utils(m);
    }

}
