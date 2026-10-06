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

.. doxygenfunction:: huira::write_image_tiff(const fs::path&, const ImageBundle<RGB>&, const std::string&, const std::string&)

.. doxygenfunction:: huira::write_image_tiff(const fs::path&, const ImageBundle<float>&, const std::string&, const std::string&)

.. doxygenfunction:: huira::write_image_tiff(const fs::path&, const ImageBundle<TSpectral>&, const std::string&, const std::string&)

