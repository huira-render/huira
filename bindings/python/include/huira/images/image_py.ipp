#pragma once

#include <cstdint>
#include <cstring>
#include <sstream>
#include <string>

#include "pybind11/pybind11.h"
#include "pybind11/numpy.h"
#include "pybind11/stl.h"

#include "huira/images/image.hpp"
#include "huira/images/io/png_io.hpp"
#include "huira/images/io/fits_io.hpp"

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
                    std::memcpy(img.data(), src,
                        static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * C * sizeof(Scalar));
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
                    static_cast<std::size_t>(img.width()) * img.height()
                    * sizeof(Scalar));
                return arr;
            }
            else {
                py::array_t<Scalar> arr(std::vector<py::ssize_t>{
                    static_cast<py::ssize_t>(img.height()),
                        static_cast<py::ssize_t>(img.width()),
                        static_cast<py::ssize_t>(C) });
                std::memcpy(arr.mutable_data(), img.data(),
                    static_cast<std::size_t>(img.width()) * img.height()
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

        // -- PNG read ----------------------------------------------------------

        m.def("read_png_f32", [](const std::string& path) {
            auto [img, alpha] = read_image_png<float>(fs::path(path));
            return py::make_tuple(std::move(img), std::move(alpha));
            }, py::arg("filepath"),
                "Read a PNG into (Image_f32, Image_f32 alpha).");

        m.def("read_png_u8", [](const std::string& path) {
            auto [img, alpha] = read_image_png<uint8_t>(fs::path(path));
            return py::make_tuple(std::move(img), std::move(alpha));
            }, py::arg("filepath"),
                "Read a PNG into (Image_u8, Image_f32 alpha).");

        m.def("read_png_u16", [](const std::string& path) {
            auto [img, alpha] = read_image_png<uint16_t>(fs::path(path));
            return py::make_tuple(std::move(img), std::move(alpha));
            }, py::arg("filepath"),
                "Read a PNG into (Image_u16, Image_f32 alpha).");

        // -- PNG write (no alpha) ----------------------------------------------

        m.def("write_png",
            [](const std::string& p, const Image<float>& img, int bd) {
                write_image_png<float>(fs::path(p), img, bd);
            },
            py::arg("filepath"), py::arg("image"), py::arg("bit_depth") = 8);

        m.def("write_png",
            [](const std::string& p, const Image<uint8_t>& img, int bd) {
                write_image_png<uint8_t>(fs::path(p), img, bd);
            },
            py::arg("filepath"), py::arg("image"), py::arg("bit_depth") = 8);

        m.def("write_png",
            [](const std::string& p, const Image<uint16_t>& img, int bd) {
                write_image_png<uint16_t>(fs::path(p), img, bd);
            },
            py::arg("filepath"), py::arg("image"), py::arg("bit_depth") = 16);

        m.def("write_png",
            [](const std::string& p, const Image<Vec3<float>>& img, int bd) {
                write_image_png<Vec3<float>>(fs::path(p), img, bd);
            },
            py::arg("filepath"), py::arg("image"), py::arg("bit_depth") = 8);

        // -- PNG write (with alpha) --------------------------------------------

        m.def("write_png",
            [](const std::string& p, const Image<float>& img,
                const Image<float>& alpha, int bd) {
                    write_image_png<float>(fs::path(p), img, alpha, bd);
            },
            py::arg("filepath"), py::arg("image"),
            py::arg("alpha"), py::arg("bit_depth") = 8);

        // -- FITS --------------------------------------------------------------

        m.def("read_fits", [](const std::string& path) {
            auto [img, meta] = read_image_fits(fs::path(path));
            return py::make_tuple(std::move(img), std::move(meta));
            }, py::arg("filepath"),
                "Read a FITS file into (Image_f32, FitsMetadata).");

        m.def("write_fits",
            [](const std::string& p, const Image<float>& img,
                int bit_depth, const FitsMetadata& meta) {
                    write_image_fits(fs::path(p), img, bit_depth, meta);
            },
            py::arg("filepath"), py::arg("image"),
            py::arg("bit_depth") = -32, py::arg("metadata") = FitsMetadata{},
            "Write an Image_f32 to a FITS file.");
    }

    // ---------------------------------------------------------------------------
    // bind_all_images  --  call this once from PYBIND11_MODULE
    // ---------------------------------------------------------------------------
    inline void bind_all_images(py::module_& m) {
        // Scalar images
        bind_image<float>(m, "Image_f32");
        bind_image<double>(m, "Image_f64");
        bind_image<uint8_t>(m, "Image_u8");
        bind_image<uint16_t>(m, "Image_u16");
        bind_image<uint32_t>(m, "Image_u32");

        // Vec3 (RGB) images
        bind_image<Vec3<float>>(m, "Image_rgb_f32");
        bind_image<Vec3<double>>(m, "Image_rgb_f64");

        // IO functions
        bind_image_io(m);
    }

}
