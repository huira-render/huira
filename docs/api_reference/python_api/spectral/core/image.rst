Image
=====

Images of spectral values, such as the received power in a frame buffer, a spectral texture, or
a measured PSF. Each spectral submodule has its own: ``huira.rgb.Image``,
``huira.visible8.Image`` and so on. Their arrays are ``(height, width, bins)``, and they are
copied to and from numpy arrays as other images are, with the array's row order given as
``origin``: see :doc:`/api_reference/python_api/images/image`.

.. autoclass-spectral:: Image
   :members:
   :undoc-members:

.. autoclass-spectral:: ImageBundle
   :members:
   :undoc-members:
