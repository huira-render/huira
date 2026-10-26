Types
=====

Vectors, matrices and numpy
---------------------------

Wherever a function takes a ``Vec3``, it also takes any list, tuple or numpy array of three
numbers, and wherever it takes a ``Mat3``, any 3x3 numpy array or nested list, written row by
row. These are the same:

.. code-block:: python

    camera.look_at(huira.Vec3(0, 0, 0), up=huira.Vec3(0, 0, 1))
    camera.look_at((0, 0, 0), up=[0, 0, 1])
    camera.look_at(np.zeros(3), up=np.array([0.0, 0.0, 1.0]))

The values Huira returns are its own types, and each works with numpy as the array its
``to_numpy()`` gives: a ``Vec3`` as ``[x, y, z]``, a ``Mat3`` as a 3x3 array, row by row.
``np.asarray(v)`` and numpy's functions take them, they index (``v[0]``, ``K[0, 2]``), and they
have a ``len()`` and iterate:

.. code-block:: python

    position = camera.get_static_position()
    distance = np.linalg.norm(position)
    K = camera_model.intrinsic_matrix()
    cx, cy = K[0, 2], K[1, 2]

Spectral values (``SpectralBins``) work the same way, with one value per bin.

Quaternions are the exception: a function that takes one takes only a ``Quaternion`` (Hamilton,
``w, x, y, z``) or a ``ShusterQuaternion`` (``x, y, z, w``), never four bare numbers, which don't
say which order they are in. Both still work with numpy, in their own order.

Changes in v0.9.11
------------------

- Functions that took a ``Vec3`` or ``Mat3`` take lists, tuples and numpy arrays too, as above,
  and functions that took only a numpy array take a ``Vec3`` or ``Mat3`` too.
- ``Rotation.from_local_to_parent_matrix()`` and ``from_local_to_parent_axis_angle()``, and
  their ``from_parent_to_local`` equivalents, are removed: ``Rotation.from_local_to_parent()``
  and ``from_parent_to_local()`` take a matrix or an axis and angle in any of these forms.
- ``Rotation.apply()`` returns a ``Vec3``, as ``rotation * vector`` does, and
  ``CameraModelHandle.intrinsic_matrix()`` returns a ``Mat3``. Both were numpy arrays, and
  ``np.asarray()`` gives one back.
- ``Vec3``, ``Mat3``, ``Quaternion``, ``ShusterQuaternion`` and ``SpectralBins`` work with numpy
  directly, and ``SpectralBins`` gains ``to_numpy()``.
- A ``Mat3`` prints row by row. It printed column by column, which reads as its transpose.

.. autoclass:: huira.Vec3
   :members:
   :undoc-members:

.. autoclass:: huira.Mat3
   :members:
   :undoc-members:

.. autoclass:: huira.Quaternion
   :members:
   :undoc-members:

.. autoclass:: huira.ShusterQuaternion
   :members:
   :undoc-members:

.. autofunction:: huira.to_shuster

.. autofunction:: huira.to_hamilton
