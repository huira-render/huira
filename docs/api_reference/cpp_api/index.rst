C++ API Reference
=================

The Huira C++ API is organized into the modules below. Select one to explore it.

.. grid:: 2
    :gutter: 3

    .. grid-item-card:: Core API
        :link: core/index
        :link-type: doc

        Types, constants, physics, time and intervals, rotations and transforms, spectral bins, units, the SPICE interface, and the concepts the templates are written against.

    .. grid-item-card:: Scene Graph
        :link: scene/index
        :link-type: doc

        Nodes, frames and instances; the Scene that holds them and the SceneView that fixes it at an instant for rendering.

    .. grid-item-card:: Handles API
        :link: handles/index
        :link-type: doc

        Safe, smart-pointer wrappers for working with the Scene Graph: cameras, instances, lights, unresolved objects, meshes, models and frames.

    .. grid-item-card:: Assets
        :link: assets/index
        :link-type: doc

        Meshes and models, the glTF loader, lights, and unresolved objects: distant bodies rendered as points.

    .. grid-item-card:: Cameras
        :link: cameras/index
        :link-type: doc

        The camera model and pixel conventions, with apertures, distortion models, point spread functions and sensors.

    .. grid-item-card:: Render API
        :link: render/index
        :link-type: doc

        The renderer and its frame buffer, rays and ray hits, and the sampler behind path tracing.

    .. grid-item-card:: Images API
        :link: images/index
        :link-type: doc

        The Image class and readers and writers for PNG, JPEG, TIFF, FITS, HDR, BMP and TGA.

    .. grid-item-card:: Stars
        :link: stars/index
        :link-type: doc

        The Star class behind catalog rendering.

    .. grid-item-card:: Ephemeris
        :link: ephemeris/index
        :link-type: doc

        Satellites' element sets: TLEs, read and checked, and propagated with SGP4.

    .. grid-item-card:: Logger
        :link: util/logger
        :link-type: doc

        Huira's logging utilities.

.. toctree::
   :hidden:
   :maxdepth: 2

   core/index
   scene/index
   handles/index
   assets/index
   cameras/index
   render/index
   images/index
   stars/index
   ephemeris/index
   util/logger
