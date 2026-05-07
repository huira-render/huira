#pragma once

#include <string>

#include "huira/handles/camera_handle.hpp"
#include "huira/scene/scene.hpp"
#include "pybind11/pybind11.h"
#include "pybind11/stl.h"
#include "pybind11/stl/filesystem.h"

namespace py = pybind11;

namespace huira {

template <typename TSpectral>
inline void bind_scene(py::module_& m)
{
    using SceneType = Scene<TSpectral>;

    py::class_<SceneType>(m, "Scene")
        .def(py::init<>())

        // Root frame access
        .def_readonly("root", &SceneType::root)

        // =============================================================
        // Geometry
        // =============================================================

        // add_mesh — without tangents
        .def("add_mesh",
             static_cast<MeshHandle<TSpectral> (SceneType::*)(
                 const IndexBuffer&, const VertexBuffer<TSpectral>&, std::string)>(
                 &SceneType::add_mesh),
             py::arg("index_buffer"),
             py::arg("vertex_buffer"),
             py::arg("name") = "",
             "Add a mesh from index and vertex buffers")

        // add_mesh — with tangents
        .def("add_mesh",
             static_cast<MeshHandle<TSpectral> (SceneType::*)(const IndexBuffer&,
                                                              const VertexBuffer<TSpectral>&,
                                                              const TangentBuffer&,
                                                              std::string)>(&SceneType::add_mesh),
             py::arg("index_buffer"),
             py::arg("vertex_buffer"),
             py::arg("tangent_buffer"),
             py::arg("name") = "",
             "Add a mesh from index, vertex, and tangent buffers")

        // add_ellipsoid
        .def(
            "add_ellipsoid",
            [](SceneType& self,
               const py::object& x,
               const py::object& y,
               const py::object& z,
               std::string name) {
                return self.add_ellipsoid(detail::unit_from_py<units::Meter>(x),
                                          detail::unit_from_py<units::Meter>(y),
                                          detail::unit_from_py<units::Meter>(z),
                                          std::move(name));
            },
            py::arg("x"),
            py::arg("y"),
            py::arg("z"),
            py::arg("name") = "",
            "Add an ellipsoid geometry (accepts any distance unit)")

        .def("add_geometry",
             &SceneType::add_geometry,
             py::arg("geom"),
             py::arg("name") = "",
             "Add a geometry from a shared pointer")
        .def("get_geometry",
             &SceneType::get_geometry,
             py::arg("name"),
             "Get a geometry handle by name")
        .def("delete_geometry",
             &SceneType::delete_geometry,
             py::arg("geom_handle"),
             "Delete a geometry from the scene")
        .def("set_geometry_name",
             static_cast<void (SceneType::*)(const GeometryHandle<TSpectral>&, const std::string&)>(
                 &SceneType::set_name),
             py::arg("geom_handle"),
             py::arg("name"),
             "Set the name of a geometry")

        // =============================================================
        // Primitives
        // =============================================================

        // add_primitive — geometry only
        .def("add_primitive",
             static_cast<PrimitiveHandle<TSpectral> (SceneType::*)(
                 const GeometryHandle<TSpectral>&, std::string)>(&SceneType::add_primitive),
             py::arg("geom"),
             py::arg("name") = "",
             "Add a primitive from a geometry")

        // add_primitive — geometry + material
        .def("add_primitive",
             static_cast<PrimitiveHandle<TSpectral> (SceneType::*)(
                 const GeometryHandle<TSpectral>&, const MaterialHandle<TSpectral>&, std::string)>(
                 &SceneType::add_primitive),
             py::arg("geom"),
             py::arg("mat"),
             py::arg("name") = "",
             "Add a primitive from a geometry and material")

        // add_primitive — geometry + medium
        .def("add_primitive",
             static_cast<PrimitiveHandle<TSpectral> (SceneType::*)(
                 const GeometryHandle<TSpectral>&, const MediumHandle<TSpectral>&, std::string)>(
                 &SceneType::add_primitive),
             py::arg("geom"),
             py::arg("medium"),
             py::arg("name") = "",
             "Add a primitive from a geometry and medium")

        // add_primitive — geometry + material + medium
        .def("add_primitive",
             static_cast<PrimitiveHandle<TSpectral> (SceneType::*)(const GeometryHandle<TSpectral>&,
                                                                   const MaterialHandle<TSpectral>&,
                                                                   const MediumHandle<TSpectral>&,
                                                                   std::string)>(
                 &SceneType::add_primitive),
             py::arg("geom"),
             py::arg("mat"),
             py::arg("medium"),
             py::arg("name") = "",
             "Add a primitive from a geometry, material, and medium")

        .def("get_primitive",
             &SceneType::get_primitive,
             py::arg("name"),
             "Get a primitive handle by name")
        .def("delete_primitive",
             &SceneType::delete_primitive,
             py::arg("primitive_handle"),
             "Delete a primitive from the scene")
        .def(
            "set_primitive_name",
            static_cast<void (SceneType::*)(const PrimitiveHandle<TSpectral>&, const std::string&)>(
                &SceneType::set_name),
            py::arg("primitive_handle"),
            py::arg("name"),
            "Set the name of a primitive")

        // =============================================================
        // Lights
        // =============================================================

        // new_sphere_light — Spectral Radiance
        .def("new_sphere_light",
             static_cast<LightHandle<TSpectral> (SceneType::*)(
                 const units::Meter&,
                 const units::SpectralWattsPerMeterSquaredSteradian<TSpectral>&,
                 std::string)>(&SceneType::new_sphere_light),
             py::arg("radius"),
             py::arg("spectral_radiance"),
             py::arg("name") = "",
             "Create a sphere light from spectral radiance")

        // new_sphere_light — Spectral Power
        .def("new_sphere_light",
             static_cast<LightHandle<TSpectral> (SceneType::*)(
                 const units::Meter&, const units::SpectralWatts<TSpectral>&, std::string)>(
                 &SceneType::new_sphere_light),
             py::arg("radius"),
             py::arg("spectral_power"),
             py::arg("name") = "",
             "Create a sphere light from total spectral power")

        // new_sphere_light — Scalar Power
        .def("new_sphere_light",
             static_cast<LightHandle<TSpectral> (SceneType::*)(
                 const units::Meter&, const units::Watt&, std::string)>(
                 &SceneType::new_sphere_light),
             py::arg("radius"),
             py::arg("total_power"),
             py::arg("name") = "",
             "Create a sphere light from total scalar power (Watts)")

        .def("new_sun_light", &SceneType::new_sun_light, "Create a sun light source")

        .def("get_light", &SceneType::get_light, py::arg("name"), "Get a light handle by name")
        .def("delete_light", &SceneType::delete_light, py::arg("light_handle"))
        .def("set_light_name",
             static_cast<void (SceneType::*)(const LightHandle<TSpectral>&, const std::string&)>(
                 &SceneType::set_name),
             py::arg("light_handle"),
             py::arg("name"),
             "Set the name of a light")

        // =============================================================
        // Unresolved Objects
        // =============================================================

        // new_unresolved_object — spectral irradiance
        .def("new_unresolved_object",
             static_cast<UnresolvedObjectHandle<TSpectral> (SceneType::*)(
                 const units::SpectralWattsPerMeterSquared<TSpectral>&, std::string)>(
                 &SceneType::new_unresolved_object),
             py::arg("spectral_irradiance"),
             py::arg("name") = "",
             "Create an unresolved object from spectral irradiance")

        // new_unresolved_object — scalar irradiance
        .def(
            "new_unresolved_object",
            [](SceneType& self, const py::object& irradiance, std::string name) {
                return self.new_unresolved_object(
                    detail::unit_from_py<units::WattsPerMeterSquared>(irradiance), std::move(name));
            },
            py::arg("irradiance"),
            py::arg("name") = "",
            "Create an unresolved object from scalar irradiance (any irradiance unit)")

        // new_unresolved_object_from_magnitude — no albedo
        .def("new_unresolved_object_from_magnitude",
             static_cast<UnresolvedObjectHandle<TSpectral> (SceneType::*)(double, std::string)>(
                 &SceneType::new_unresolved_object_from_magnitude),
             py::arg("visual_magnitude"),
             py::arg("name") = "",
             "Create an unresolved object from visual magnitude")

        // new_unresolved_object_from_magnitude — with albedo
        .def("new_unresolved_object_from_magnitude",
             static_cast<UnresolvedObjectHandle<TSpectral> (SceneType::*)(
                 double, TSpectral, std::string)>(&SceneType::new_unresolved_object_from_magnitude),
             py::arg("visual_magnitude"),
             py::arg("albedo"),
             py::arg("name") = "",
             "Create an unresolved object from visual magnitude with spectral albedo")

        // new_unresolved_emitter — spectral power
        .def("new_unresolved_emitter",
             static_cast<UnresolvedObjectHandle<TSpectral> (SceneType::*)(
                 const units::SpectralWatts<TSpectral>&, std::string)>(
                 &SceneType::new_unresolved_emitter),
             py::arg("spectral_power"),
             py::arg("name") = "",
             "Create an unresolved emitter from spectral power")

        // new_unresolved_emitter — scalar power
        .def(
            "new_unresolved_emitter",
            [](SceneType& self, const py::object& power, std::string name) {
                return self.new_unresolved_emitter(detail::unit_from_py<units::Watt>(power),
                                                   std::move(name));
            },
            py::arg("power"),
            py::arg("name") = "",
            "Create an unresolved emitter from total power (any power unit)")

        // new_unresolved_sphere — no albedo
        .def(
            "new_unresolved_sphere",
            [](SceneType& self,
               const py::object& radius,
               InstanceHandle<TSpectral> sun,
               std::string name) {
                return self.new_unresolved_sphere(
                    detail::unit_from_py<units::Meter>(radius), std::move(sun), std::move(name));
            },
            py::arg("radius"),
            py::arg("sun"),
            py::arg("name") = "",
            "Create an unresolved sphere (accepts any distance unit for radius)")

        // new_unresolved_sphere — spectral albedo
        .def(
            "new_unresolved_sphere",
            [](SceneType& self,
               const py::object& radius,
               InstanceHandle<TSpectral> sun,
               TSpectral albedo,
               std::string name) {
                return self.new_unresolved_sphere(detail::unit_from_py<units::Meter>(radius),
                                                  std::move(sun),
                                                  std::move(albedo),
                                                  std::move(name));
            },
            py::arg("radius"),
            py::arg("sun"),
            py::arg("albedo"),
            py::arg("name") = "",
            "Create an unresolved sphere with spectral albedo")

        // new_unresolved_sphere — scalar albedo
        .def(
            "new_unresolved_sphere",
            [](SceneType& self,
               const py::object& radius,
               InstanceHandle<TSpectral> sun,
               float albedo,
               std::string name) {
                return self.new_unresolved_sphere(detail::unit_from_py<units::Meter>(radius),
                                                  std::move(sun),
                                                  albedo,
                                                  std::move(name));
            },
            py::arg("radius"),
            py::arg("sun"),
            py::arg("scalar_albedo"),
            py::arg("name") = "",
            "Create an unresolved sphere with scalar albedo")

        // new_unresolved_asteroid — no albedo
        .def("new_unresolved_asteroid",
             static_cast<UnresolvedObjectHandle<TSpectral> (SceneType::*)(
                 double, double, InstanceHandle<TSpectral>, std::string)>(
                 &SceneType::new_unresolved_asteroid),
             py::arg("H"),
             py::arg("G"),
             py::arg("sun"),
             py::arg("name") = "",
             "Create an unresolved asteroid from H, G magnitude parameters")

        // new_unresolved_asteroid — spectral albedo
        .def("new_unresolved_asteroid",
             static_cast<UnresolvedObjectHandle<TSpectral> (SceneType::*)(
                 double, double, InstanceHandle<TSpectral>, TSpectral, std::string)>(
                 &SceneType::new_unresolved_asteroid),
             py::arg("H"),
             py::arg("G"),
             py::arg("sun"),
             py::arg("albedo"),
             py::arg("name") = "",
             "Create an unresolved asteroid with spectral albedo")

        // new_unresolved_asteroid — scalar albedo
        .def("new_unresolved_asteroid",
             static_cast<UnresolvedObjectHandle<TSpectral> (SceneType::*)(
                 double, double, InstanceHandle<TSpectral>, float, std::string)>(
                 &SceneType::new_unresolved_asteroid),
             py::arg("H"),
             py::arg("G"),
             py::arg("sun"),
             py::arg("scalar_albedo"),
             py::arg("name") = "",
             "Create an unresolved asteroid with scalar albedo")

        .def("get_unresolved_object",
             &SceneType::get_unresolved_object,
             py::arg("name"),
             "Get an unresolved object handle by name")
        .def("delete_unresolved_object",
             &SceneType::delete_unresolved_object,
             py::arg("unresolved_object_handle"))
        .def("set_unresolved_object_name",
             static_cast<void (SceneType::*)(const UnresolvedObjectHandle<TSpectral>&,
                                             const std::string&)>(&SceneType::set_name),
             py::arg("unresolved_object_handle"),
             py::arg("name"),
             "Set the name of an unresolved object")

        // =============================================================
        // Camera models
        // =============================================================
        .def("new_camera_model",
             &SceneType::new_camera_model,
             py::arg("name") = "",
             "Create a new camera model and return its handle")
        .def("get_camera_model",
             &SceneType::get_camera_model,
             py::arg("name"),
             "Get a camera model handle by name")
        .def("delete_camera_model", &SceneType::delete_camera_model, py::arg("camera_model_handle"))
        .def("set_camera_model_name",
             static_cast<void (SceneType::*)(const CameraModelHandle<TSpectral>&,
                                             const std::string&)>(&SceneType::set_name),
             py::arg("camera_model_handle"),
             py::arg("name"),
             "Set the name of a camera model")

        // =============================================================
        // BSDFs
        // =============================================================
        .def("new_bsdf_cook_torrance",
             &SceneType::new_bsdf_cook_torrance,
             py::arg("name") = "",
             "Create a Cook-Torrance BSDF")
        .def("new_bsdf_hapke",
             &SceneType::new_bsdf_hapke,
             py::arg("h"),
             py::arg("B0"),
             py::arg("b"),
             py::arg("c"),
             py::arg("name") = "",
             "Create a Hapke BSDF")
        .def("new_bsdf_lambertian",
             &SceneType::new_bsdf_lambertian,
             py::arg("name") = "",
             "Create a Lambertian BSDF")
        .def("new_bsdf_lommel_seeliger",
             &SceneType::new_bsdf_lommel_seeliger,
             py::arg("name") = "",
             "Create a Lommel-Seeliger BSDF")
        .def("new_bsdf_mcewen",
             &SceneType::new_bsdf_mcewen,
             py::arg("name") = "",
             "Create a McEwen BSDF")
        .def("new_bsdf_null",
             &SceneType::new_bsdf_null,
             py::arg("name") = "",
             "Create a null (transparent/passthrough) BSDF")
        .def("new_bsdf_oren_nayar",
             &SceneType::new_bsdf_oren_nayar,
             py::arg("name") = "",
             "Create an Oren-Nayar BSDF")
        .def("add_bsdf",
             &SceneType::add_bsdf,
             py::arg("bsdf"),
             py::arg("name") = "",
             "Add a BSDF from a shared pointer")

        // =============================================================
        // Materials
        // =============================================================
        .def("new_material",
             &SceneType::new_material,
             py::arg("bsdf"),
             py::arg("name") = "",
             "Create a new material with the specified BSDF")
        .def("add_material",
             &SceneType::add_material,
             py::arg("material"),
             py::arg("name") = "",
             "Add a material from a shared pointer")

        // =============================================================
        // Volumes
        // =============================================================
        .def(
            "new_constant_density_field",
            &SceneType::new_constant_density_field,
            py::arg("absorption"),
            py::arg("scattering"),
            py::arg("name") = "",
            "Create a constant density field with the given absorption and scattering coefficients")
        .def("add_density_field",
             &SceneType::add_density_field,
             py::arg("density_field"),
             py::arg("name") = "",
             "Add a density field from a shared pointer")

        .def("new_isotropic_phase_function",
             &SceneType::new_isotropic_phase_function,
             py::arg("name") = "",
             "Create an isotropic phase function")
        .def("add_phase_function",
             &SceneType::add_phase_function,
             py::arg("phase_function"),
             py::arg("name") = "",
             "Add a phase function from a shared pointer")

        .def("new_medium",
             &SceneType::new_medium,
             py::arg("density_field_handle"),
             py::arg("phase_function_handle"),
             py::arg("name") = "",
             "Create a new medium from a density field and phase function")
        .def("add_medium",
             &SceneType::add_medium,
             py::arg("medium"),
             py::arg("name") = "",
             "Add a medium from a shared pointer")

        // =============================================================
        // Background
        // =============================================================

        // set_background_radiance — Image
        .def(
            "set_background_radiance",
            static_cast<void (SceneType::*)(Image<TSpectral>)>(&SceneType::set_background_radiance),
            py::arg("background"),
            "Set background radiance from an Image")

        // set_background_radiance — TSpectral
        .def("set_background_radiance",
             static_cast<void (SceneType::*)(TSpectral)>(&SceneType::set_background_radiance),
             py::arg("background"),
             "Set background radiance from a spectral value")

        // set_background_radiance — float
        .def("set_background_radiance",
             static_cast<void (SceneType::*)(float)>(&SceneType::set_background_radiance),
             py::arg("background"),
             "Set background radiance from a total power float")

        // =============================================================
        // Textures
        // =============================================================

        .def(
            "add_texture",
            [](SceneType& self,
               Image<TSpectral> image,
               std::string name) { return self.add_texture(std::move(image), std::move(name)); },
            py::arg("image"),
            py::arg("name") = "",
            "Add a spectral texture from an image")
        .def(
            "add_texture",
            [](SceneType& self,
               Image<float> image,
               std::string name) { return self.add_texture(std::move(image), std::move(name)); },
            py::arg("image"),
            py::arg("name") = "",
            "Add a float texture from a single-channel image")
        .def(
            "add_texture",
            [](SceneType& self,
               Image<Vec3<float>> image,
               std::string name) { return self.add_texture(std::move(image), std::move(name)); },
            py::arg("image"),
            py::arg("name") = "",
            "Add a Vec3 texture from a three-channel image")

        // add_normal_texture — dispatches on image type, matching C++ API
        .def(
            "add_normal_texture",
            [](SceneType& self,
               Image<Vec3<float>> image,
               std::string
                   name) { return self.add_normal_texture(std::move(image), std::move(name)); },
            py::arg("image"),
            py::arg("name") = "",
            "Add a normal map texture from a Vec3 image")
        .def(
            "add_normal_texture",
            [](SceneType& self,
               Image<RGB> image,
               std::string
                   name) { return self.add_normal_texture(std::move(image), std::move(name)); },
            py::arg("image"),
            py::arg("name") = "",
            "Add a normal map texture from an RGB image")

        // =============================================================
        // Models
        // =============================================================

        .def(
            "load_model",
            [](SceneType& self,
               const fs::path& file,
               std::string name) { return self.load_model(file, std::move(name)); },
            py::arg("file"),
            py::arg("name") = "",
            "Load a model from a file")
        .def("get_model", &SceneType::get_model, py::arg("name"), "Get a model handle by name")
        .def("delete_model", &SceneType::delete_model, py::arg("model_handle"))
        .def("set_model_name",
             static_cast<void (SceneType::*)(const ModelHandle<TSpectral>&, const std::string&)>(
                 &SceneType::set_name),
             py::arg("model_handle"),
             py::arg("name"),
             "Set the name of a model")

        // =============================================================
        // Star loading
        // =============================================================

        .def("load_stars",
             &SceneType::load_stars,
             py::arg("star_catalog_path"),
             py::arg("time"),
             py::arg("min_magnitude") = 100.f,
             "Load stars from a catalog file for the given observation time")

        .def("load_dynamic_stars",
             &SceneType::load_dynamic_stars,
             py::arg("star_catalog_path"),
             py::arg("time"),
             py::arg("min_magnitude") = 100.f,
             "Load dynamic stars from a catalog file for the given observation time")

        .def("update_star_epoch",
             &SceneType::update_star_epoch,
             py::arg("time"),
             "Update the epoch time for dynamic stars, causing their positions to be updated "
             "accordingly")

        // =============================================================
        // Utilities
        // =============================================================

        .def("prune_unreferenced_assets",
             &SceneType::prune_unreferenced_assets,
             "Remove any assets that are no longer referenced by the scene graph")

        // =============================================================
        // Debug printing
        // =============================================================

        .def("print_meshes", &SceneType::print_meshes)
        .def("print_lights", &SceneType::print_lights)
        .def("print_unresolved_objects", &SceneType::print_unresolved_objects)
        .def("print_camera_models", &SceneType::print_camera_models)
        .def("print_models", &SceneType::print_models)
        .def("print_graph", &SceneType::print_graph)
        .def("print_contents", &SceneType::print_contents)

        .def("__repr__", [](const SceneType&) { return "<Scene>"; });
}

} // namespace huira
