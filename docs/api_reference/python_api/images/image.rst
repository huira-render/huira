Image
=====

Arrays and row order
--------------------

Images are stored with their top row first: pixel ``(x, y)`` is in column ``x`` and row ``y``,
counted down from the top. A numpy array doesn't record which of its rows is the top, so copying
one into an image, or an image into one, takes it as ``origin``, which has no default:

.. code-block:: python

    import huira
    from astropy.io import fits

    # FITS files have their first row at the bottom, and astropy reads them that way:
    flat = huira.Image_f32(fits.getdata("flat.fits"), origin=huira.PixelOrigin.BottomLeft)

    # An array with the top row first, as most image libraries and matplotlib have it:
    array = image.to_numpy(origin=huira.PixelOrigin.TopLeft)

Arrays are ``(height, width)`` for one channel and ``(height, width, channels)`` for more.

For convenience, an image is also an array in its own layout, top row first, without copying:
``np.asarray(image)`` is a view of it (writing to the view changes the image), and numpy and
matplotlib take an image directly: ``plt.imshow(image)`` draws it the right way up, and
``np.mean(image)`` averages it. Nothing checks that layout against where the array goes next.
To write an image with astropy, use ``to_numpy(origin=huira.PixelOrigin.BottomLeft)``, or
Huira's own ``write_fits()``, which flips the rows as FITS has them.

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
