Image
=====

Scalar Images
-------------

.. autoclass:: huira.Image_f32
   :members:
   :undoc-members:

.. autoclass:: huira.Image_f64
   :members:
   :undoc-members:

.. autoclass:: huira.Image_u8
   :members:
   :undoc-members:

.. autoclass:: huira.Image_u16
   :members:
   :undoc-members:

.. autoclass:: huira.Image_u32
   :members:
   :undoc-members:

.. autoclass:: huira.Image_u64
   :members:
   :undoc-members:


Image Bundles
-------------

.. autoclass:: huira.ImageBundle_f32
   :members:
   :undoc-members:

.. autoclass:: huira.ColorSpaceHint
   :members:
   :undoc-members:

.. autoclass:: huira.PixelScaling
   :members:
   :undoc-members:


Image I/O
---------

See :doc:`/design_overview/image_output` for how a sensor response is written to each format.

.. autofunction:: huira.read_png

.. autofunction:: huira.read_png_mono

.. autofunction:: huira.write_png


.. autofunction:: huira.read_jpeg

.. autofunction:: huira.read_jpeg_mono

.. autofunction:: huira.write_jpeg


.. autofunction:: huira.read_tiff

.. autofunction:: huira.read_tiff_mono

.. autofunction:: huira.write_tiff


.. autofunction:: huira.read_fits

.. autofunction:: huira.write_fits
