TIFF I/O
========

Reading
-------

.. doxygenfunction:: huira::read_image_tiff_rgb(const fs::path&, bool)

.. doxygenfunction:: huira::read_image_tiff_rgb(const unsigned char*, std::size_t, bool)

.. doxygenfunction:: huira::read_image_tiff_mono(const fs::path&, bool)

.. doxygenfunction:: huira::read_image_tiff_mono(const unsigned char*, std::size_t, bool)

Writing
-------

.. doxygenfunction:: huira::write_image_tiff(const fs::path&, const Image<RGB>&, int, const std::string&, const std::string&)

.. doxygenfunction:: huira::write_image_tiff(const fs::path&, const Image<float>&, int, const std::string&, const std::string&)

.. doxygenfunction:: huira::write_image_tiff(const fs::path&, const Image<TSpectral>&, int, const std::string&, const std::string&)

