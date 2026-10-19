#pragma once

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "huira/core/rotation.hpp"
#include "huira/core/time.hpp"
#include "huira/core/types.hpp"
#include "huira/core/types_py.ipp"
#include "huira/scene/node.hpp"
#include "huira/scene/state_callbacks/function_callbacks.hpp"
#include "huira/scene/state_callbacks/state_callbacks.hpp"
#include "huira/units/units_py.ipp"
#include "pybind11/numpy.h"
#include "pybind11/pybind11.h"
#include "pybind11/stl.h"

namespace py = pybind11;

namespace huira {

/**
 * @brief Binds PoseContext, which position and rotation functions are called with. Call it after
 * ObservationMode is bound: its constructor takes one by default.
 */
inline void bind_pose_context(py::module_& m)
{
    py::class_<PoseContext>(m,
                            "PoseContext",
                            "What a pose is evaluated for: what a node's position or rotation "
                            "function is called with.")
        .def(py::init([](const Time& time,
                         std::optional<Time> reference,
                         ObservationMode observation_mode) {
                 return PoseContext{time, reference.value_or(time), observation_mode};
             }),
             py::arg("time"),
             py::arg("reference") = py::none(),
             py::arg("observation_mode") = ObservationMode::ABERRATED_STATE,
             "A context, as for testing a function: its reference is its time by default.")
        .def_readonly("time",
                      &PoseContext::time,
                      "The time the pose is wanted at: the time of observation, less any light "
                      "time.")
        .def_readonly("reference",
                      &PoseContext::reference,
                      "The time that poses set by hand hold at: the reference time of the scene "
                      "view's interval.")
        .def_readonly(
            "observation_mode", &PoseContext::observation_mode, "How the scene view observes.")
        .def("at", &PoseContext::at, py::arg("time"), "The same context at another time.")
        .def("__repr__", [](const PoseContext& context) {
            return "<PoseContext time " + context.time.to_iso_8601() + ", reference " +
                   context.reference.to_iso_8601() + ">";
        });
}

namespace detail {

/**
 * @brief A Python callable held from C++, as a node's position or rotation function. It is called
 * with the GIL held, and released with it held.
 */
class PyPoseFunction {
  public:
    explicit PyPoseFunction(py::function function)
        : function_(new py::function(std::move(function)), [](py::function* held) {
              if (Py_IsInitialized()) {
                  py::gil_scoped_acquire gil;
                  delete held;
              } else {
                  // The interpreter, and the callable with it, is gone:
                  held->release();
                  delete held;
              }
          })
    {
    }

    PositionSample position(const PoseContext& context) const
    {
        py::gil_scoped_acquire gil;
        const py::object result = call_(context);
        if (!py::isinstance<Vec3<double>>(result) && py::hasattr(result, "__len__") &&
            py::len(result) == 2) {
            const py::sequence pair = py::reinterpret_borrow<py::sequence>(result);
            return {to_vec3_(pair[0], "position"), to_vec3_(pair[1], "velocity")};
        }
        return {to_vec3_(result, "position"), std::nullopt};
    }

    RotationSample rotation(const PoseContext& context) const
    {
        py::gil_scoped_acquire gil;
        const py::object result = call_(context);
        if (py::isinstance<Rotation<double>>(result)) {
            return {result.cast<Rotation<double>>(), std::nullopt};
        }
        if (py::isinstance<py::sequence>(result) && py::len(result) == 2) {
            const py::sequence pair = py::reinterpret_borrow<py::sequence>(result);
            if (py::isinstance<Rotation<double>>(pair[0])) {
                return {pair[0].cast<Rotation<double>>(), to_vec3_(pair[1], "angular velocity")};
            }
        }
        throw py::type_error("A rotation function returns a Rotation, or a Rotation and an "
                             "angular velocity (three numbers, in radians per second)");
    }

    /// The callable, for its scene to show Python's garbage collector (see HeldPoseFunctions).
    [[nodiscard]] std::weak_ptr<py::function> held() const { return function_; }

  private:
    std::shared_ptr<py::function> function_;

    py::object call_(const PoseContext& context) const
    {
        if (!*function_) {
            // Its scene was garbage collected, which released it:
            throw py::value_error("This node's position or rotation function was released when "
                                  "its scene was garbage collected");
        }
        return (*function_)(context);
    }

    /// A Vec3, or three numbers (a sequence or array).
    static Vec3<double> to_vec3_(const py::handle& value, const std::string& what)
    {
        if (py::isinstance<Vec3<double>>(value)) {
            return value.cast<Vec3<double>>();
        }
        try {
            const DoubleArray array = py::cast<DoubleArray>(value);
            if (array.ndim() == 1 && array.shape(0) == 3) {
                return Vec3<double>{array.at(0), array.at(1), array.at(2)};
            }
        } catch (const py::cast_error&) {
        } catch (const py::error_already_set&) {
            // numpy could not make numbers of it
        }
        throw py::type_error("A " + what +
                             " from a position or rotation function is a Vec3, or three numbers");
    }
};

/**
 * @brief The Python callables that nodes hold as position or rotation functions, by node.
 *
 * A function that refers to its scene, directly or through anything else that does, makes a
 * cycle with it: the scene holds the function, through its node, and the function holds the
 * scene. Python's garbage collector cannot follow the cycle through C++, so it would keep both
 * until the interpreter exits. A Scene's Python object shows the collector the functions its
 * nodes hold, from here (see bind_scene), so that the cycle is collected. Kept by the GIL.
 */
struct HeldPoseFunctions {
    std::weak_ptr<py::function> position;
    std::weak_ptr<py::function> rotation;
};

inline std::unordered_map<const void*, HeldPoseFunctions>& held_pose_functions()
{
    static std::unordered_map<const void*, HeldPoseFunctions> held;
    return held;
}

/// Record the function a node now holds, as its position or rotation function, and forget those
/// that nodes no longer hold.
template <typename TSpectral>
void hold_pose_function(const Node<TSpectral>* node, const PyPoseFunction& function, bool rotation)
{
    auto& held = held_pose_functions();
    std::erase_if(held, [](const auto& entry) {
        return entry.second.position.expired() && entry.second.rotation.expired();
    });
    HeldPoseFunctions& node_held = held[static_cast<const void*>(node)];
    (rotation ? node_held.rotation : node_held.position) = function.held();
}

/// The functions held by a node and the nodes under it.
template <typename TSpectral>
void collect_held_pose_functions(const Node<TSpectral>& node,
                                 std::vector<std::shared_ptr<py::function>>& functions)
{
    const auto& held = held_pose_functions();
    const auto it = held.find(static_cast<const void*>(&node));
    if (it != held.end()) {
        for (const std::weak_ptr<py::function>* weak :
             {&it->second.position, &it->second.rotation}) {
            if (std::shared_ptr<py::function> function = weak->lock(); function && *function) {
                functions.push_back(std::move(function));
            }
        }
    }
    for (const auto& child : node.get_children()) {
        collect_held_pose_functions(*child, functions);
    }
}

/// Rows of an array of shape (N, 3), as vectors.
inline std::vector<Vec3<double>> vec3_rows(const DoubleArray& array, const std::string& what)
{
    if (array.ndim() != 2 || array.shape(1) != 3) {
        throw py::value_error(what + " must be an array of shape (N, 3)");
    }
    const auto rows = array.unchecked<2>();
    std::vector<Vec3<double>> vectors(static_cast<std::size_t>(rows.shape(0)));
    for (py::ssize_t i = 0; i < rows.shape(0); ++i) {
        vectors[static_cast<std::size_t>(i)] = Vec3<double>{rows(i, 0), rows(i, 1), rows(i, 2)};
    }
    return vectors;
}

} // namespace detail

/**
 * @brief Injects the position and rotation sources (functions, tables, and the Keplerian orbit)
 * into a node handle's binding: see bind_node_handle_methods.
 */
template <typename TSpectral, typename TNode, typename PyClass>
inline void bind_pose_source_methods(PyClass& cls)
{
    using BoundType = typename PyClass::type;

    cls.def(
           "set_position_function",
           [](const BoundType& self, py::function function) {
               const detail::PyPoseFunction held(std::move(function));
               self.template set_custom_position_callback<PositionFunctionCallback>(
                   [held](const PoseContext& context) { return held.position(context); },
                   self.get()->get_info());
               detail::hold_pose_function(
                   static_cast<const Node<TSpectral>*>(self.get().get()), held, false);
           },
           py::arg("function"),
           R"(Place the node by a function of time.

The function is called with a PoseContext, whose time is when the position is
wanted, and returns the node's position in its parent's frame, in meters (a
Vec3, or three numbers), or a position and a velocity, in meters per second.
A velocity it does not return is estimated from positions 2 ms apart.

It is called many times while a SceneView is built, at times in any order
(temporal samples, light time, and estimates of velocity), and never while a
render runs. It must give the same answer for the same context. Any other
position source the node had is replaced.)")
        .def(
            "set_position_table",
            [](const BoundType& self,
               const std::vector<Time>& times,
               const DoubleArray& positions,
               const std::optional<DoubleArray>& velocities) {
                self.set_position_table(times,
                                        detail::vec3_rows(positions, "positions"),
                                        velocities ? detail::vec3_rows(*velocities, "velocities")
                                                   : std::vector<Vec3<double>>{});
            },
            py::arg("times"),
            py::arg("positions"),
            py::arg("velocities") = py::none(),
            R"(Place the node by a table of times and positions.

times: a list of Time, at least two, each later than the one before.
positions: an array of shape (N, 3), in meters, in the parent's frame.
velocities: an array of shape (N, 3), in meters per second, or None to
estimate them from the positions.

Between two entries, the position follows the cubic through both that has
their velocities there. Velocities not given are estimated at each entry from
the quadratic through it and its neighbors. A time outside the table is an
error. Any other position source the node had is replaced.)")
        .def(
            "set_keplerian_orbit",
            [](const BoundType& self,
               const py::object& semi_major_axis,
               double eccentricity,
               const py::object& inclination,
               const py::object& raan,
               const py::object& arg_periapsis,
               const py::object& mean_anomaly,
               const Time& epoch,
               double mu) {
                self.set_keplerian_orbit(detail::unit_from_py<units::Meter>(semi_major_axis),
                                         eccentricity,
                                         detail::unit_from_py<units::Radian>(inclination),
                                         detail::unit_from_py<units::Radian>(raan),
                                         detail::unit_from_py<units::Radian>(arg_periapsis),
                                         detail::unit_from_py<units::Radian>(mean_anomaly),
                                         epoch,
                                         mu);
            },
            py::arg("semi_major_axis"),
            py::arg("eccentricity"),
            py::arg("inclination"),
            py::arg("raan"),
            py::arg("arg_periapsis"),
            py::arg("mean_anomaly"),
            py::arg("epoch"),
            py::arg("mu"),
            R"(Move the node on a Keplerian orbit about its parent.

semi_major_axis: a distance, positive for an ellipse and negative for a
hyperbola.
inclination, raan, arg_periapsis, mean_anomaly: angles, in any angle unit.
epoch: when the body is at mean_anomaly.
mu: the central body's gravitational parameter, in km^3/s^2.
Any other position source the node had is replaced.)")
        .def(
            "set_rotation_function",
            [](const BoundType& self, py::function function) {
                const detail::PyPoseFunction held(std::move(function));
                self.template set_custom_rotation_callback<RotationFunctionCallback>(
                    [held](const PoseContext& context) { return held.rotation(context); },
                    self.get()->get_info());
                detail::hold_pose_function(
                    static_cast<const Node<TSpectral>*>(self.get().get()), held, true);
            },
            py::arg("function"),
            R"(Orient the node by a function of time.

The function is called with a PoseContext, whose time is when the rotation is
wanted, and returns the node's Rotation relative to its parent, or a Rotation
and an angular velocity, in radians per second about the parent's axes. An
angular velocity it does not return is estimated from rotations 2 ms apart.

As for set_position_function, it is called many times while a SceneView is
built, and must give the same answer for the same context. Any other rotation
source the node had is replaced.)")
        .def(
            "set_rotation_table",
            [](const BoundType& self,
               const std::vector<Time>& times,
               const std::vector<Rotation<double>>& rotations,
               const std::optional<DoubleArray>& angular_velocities) {
                self.set_rotation_table(times,
                                        rotations,
                                        angular_velocities ? detail::vec3_rows(*angular_velocities,
                                                                               "angular_velocities")
                                                           : std::vector<Vec3<double>>{});
            },
            py::arg("times"),
            py::arg("rotations"),
            py::arg("angular_velocities") = py::none(),
            R"(Orient the node by a table of times and rotations.

times: a list of Time, at least two, each later than the one before.
rotations: a list of Rotation, relative to the parent.
angular_velocities: an array of shape (N, 3), in radians per second about the
parent's axes, or None to take the steady rate between entries.

Between two entries, the rotation turns at a steady rate about a fixed axis,
the shorter way round, as in a SPICE type 3 C-kernel; angular velocities given
are interpolated linearly. A time outside the table is an error. Any other
rotation source the node had is replaced.)");
}

} // namespace huira
