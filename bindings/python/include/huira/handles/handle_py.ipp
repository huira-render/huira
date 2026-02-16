#pragma once

// Handle<T> is not directly exposed to Python.
// Derived handle types (CameraModelHandle, FrameHandle, etc.) are each
// registered as standalone pybind11 classes without declaring pybind11
// inheritance, using bind_node_handle_methods() to inject shared methods.
