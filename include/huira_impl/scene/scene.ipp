#include <stdexcept>
#include <iostream>

#include "tbb/parallel_for.h"
#include "tbb/blocked_range.h"

#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/assets/io/model_loader.hpp"
#include "huira/handles/model_handle.hpp"
#include "huira/handles/frame_handle.hpp"
#include "huira/util/logger.hpp"
#include "huira/util/colorful_text.hpp"
#include "huira/stars/io/star_catalog.hpp"
#include "huira/assets/lights/light.hpp"
#include "huira/assets/lights/point_light.hpp"
#include "huira/assets/unresolved/unresolved_object.hpp"
#include "huira/assets/unresolved/unresolved_sphere.hpp"
#include "huira/assets/unresolved/unresolved_asteroid.hpp"
#include "huira/assets/unresolved/unresolved_emitter.hpp"

namespace huira {
    // Suppressing C4355: 'this' is passed to FrameNode constructor, but FrameNode only stores
    // the pointer without calling back into the incomplete Scene object.
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4355)
#endif

    /**
     * @brief Constructs a Scene and initializes the root node.
     */
    template <IsSpectral TSpectral>
    Scene<TSpectral>::Scene()
        : root_node_(std::make_shared<FrameNode<TSpectral>>(this))
        , root(root_node_)
    {
        root_node_->set_spice("SOLAR SYSTEM BARYCENTER", "J2000");
    };

#ifdef _MSC_VER
#pragma warning(pop)
#endif

    /**
     * @brief Adds a mesh to the scene.
     * @param mesh Mesh to add (moved)
     * @param name Optional name for the mesh
     * @return MeshHandle<TSpectral> Handle to the added mesh
     */
    template <IsSpectral TSpectral>
    MeshHandle<TSpectral> Scene<TSpectral>::add_mesh(Mesh<TSpectral>&& mesh, std::string name)
    {
        auto mesh_shared = std::make_shared<Mesh<TSpectral>>(std::move(mesh));
        meshes_.add(mesh_shared, name);
        return MeshHandle<TSpectral>{ mesh_shared };
    };

    /**
     * @brief Sets the name for a mesh asset.
     * @param mesh_handle Handle to the mesh
     * @param name New name
     */
    template <IsSpectral TSpectral>
    void Scene<TSpectral>::set_name(const MeshHandle<TSpectral>& mesh_handle, const std::string& name)
    {
        meshes_.set_name(mesh_handle.get(), name);
    };

    /**
     * @brief Retrieves a mesh by name.
     * @param name Name of the mesh
     * @return MeshHandle<TSpectral> Handle to the mesh
     */
    template <IsSpectral TSpectral>
    MeshHandle<TSpectral> Scene<TSpectral>::get_mesh(const std::string& name)
    {
        return MeshHandle<TSpectral>{ meshes_.lookup(name) };
    }

    /**
     * @brief Deletes a mesh from the scene.
     * @param mesh_handle Handle to the mesh
     */
    template <IsSpectral TSpectral>
    void Scene<TSpectral>::delete_mesh(const MeshHandle<TSpectral>& mesh_handle)
    {
        auto mesh_shared = mesh_handle.get();
        prune_graph_references_(mesh_shared.get());
        meshes_.remove(mesh_shared);
    }


    /**
     * @brief Creates a new point light with spectral power.
     * @param spectral_power Spectral power of the light
     * @param name Optional name
     * @return LightHandle<TSpectral> Handle to the new light
     */
    template <IsSpectral TSpectral>
    LightHandle<TSpectral> Scene<TSpectral>::new_point_light(const units::SpectralWatts<TSpectral>& spectral_power, std::string name)
    {
        auto light_shared = std::make_shared<PointLight<TSpectral>>(spectral_power);
        return this->add_light(light_shared, name);
    }

    /**
     * @brief Creates a new point light with total power.
     * @param total_power Total power of the light
     * @param name Optional name
     * @return LightHandle<TSpectral> Handle to the new light
     */
    template <IsSpectral TSpectral>
    LightHandle<TSpectral> Scene<TSpectral>::new_point_light(const units::Watt& total_power, std::string name)
    {
        auto light_shared = std::make_shared<PointLight<TSpectral>>(total_power);
        return this->add_light(light_shared, name);
    }

    /**
     * @brief Creates a new sun light with solar spectral radiance.
     * @return LightHandle<TSpectral> Handle to the new sun light
     */
    template <IsSpectral TSpectral>
    LightHandle<TSpectral> Scene<TSpectral>::new_sun_light()
    {
        // TODO Make this a sphere light once implemented
        TSpectral spectral_radiance = black_body<TSpectral>(5800, 1000);

        // TODO Move solar radius into constants somehow?
        constexpr float sun_radius = 6.957e8f;
        constexpr float sun_area = 4.f * PI<float>() * sun_radius * sun_radius;
        TSpectral spectral_power = spectral_radiance * PI<float>() * sun_area;

        units::SpectralWatts<TSpectral> spectral_power_watts(spectral_power);

        return this->new_point_light(spectral_power_watts, "Sun");
    }

    /**
     * @brief Adds a light to the scene.
     * @param light Shared pointer to the light
     * @param name Optional name
     * @return LightHandle<TSpectral> Handle to the added light
     */
    template <IsSpectral TSpectral>
    LightHandle<TSpectral> Scene<TSpectral>::add_light(std::shared_ptr<Light<TSpectral>> light, std::string name)
    {
        lights_.add(light, name);
        return LightHandle<TSpectral>{ light };
    }

    /**
     * @brief Sets the name for a light asset.
     * @param light_handle Handle to the light
     * @param name New name
     */
    template <IsSpectral TSpectral>
    void Scene<TSpectral>::set_name(const LightHandle<TSpectral>& light_handle, const std::string& name)
    {
        lights_.set_name(light_handle.get(), name);
    };

    /**
     * @brief Retrieves a light by name.
     * @param name Name of the light
     * @return LightHandle<TSpectral> Handle to the light
     */
    template <IsSpectral TSpectral>
    LightHandle<TSpectral> Scene<TSpectral>::get_light(const std::string& name)
    {
        return LightHandle<TSpectral>{ lights_.lookup(name) };
    }

    /**
     * @brief Deletes a light from the scene.
     * @param light_handle Handle to the light
     */
    template <IsSpectral TSpectral>
    void Scene<TSpectral>::delete_light(const LightHandle<TSpectral>& light_handle)
    {
        auto light_shared = light_handle.get();
        prune_graph_references_(light_shared.get());
        lights_.remove(light_shared);
    }

    /**
     * @brief Creates a new unresolved object with spectral irradiance.
     * @param spectral_irradiance Spectral irradiance
     * @param name Optional name
     * @return UnresolvedObjectHandle<TSpectral> Handle to the new object
     */
    template <IsSpectral TSpectral>
    UnresolvedObjectHandle<TSpectral> Scene<TSpectral>::new_unresolved_object(const units::SpectralWattsPerMeterSquared<TSpectral>& spectral_irradiance, std::string name)
    {
        auto unresolved_shared = std::make_shared<UnresolvedObject<TSpectral>>(spectral_irradiance);
        return add_unresolved_object(unresolved_shared, name);
    }

    /**
     * @brief Creates a new unresolved object with total irradiance.
     * @param irradiance Total irradiance
     * @param name Optional name
     * @return UnresolvedObjectHandle<TSpectral> Handle to the new object
     */
    template <IsSpectral TSpectral>
    UnresolvedObjectHandle<TSpectral> Scene<TSpectral>::new_unresolved_object(const units::WattsPerMeterSquared& irradiance, std::string name)
    {
        auto unresolved_shared = std::make_shared<UnresolvedObject<TSpectral>>(irradiance);
        return add_unresolved_object(unresolved_shared, name);
    }

    /**
     * @brief Creates a new unresolved object from visual magnitude.
     * @param visual_magnitude Visual magnitude
     * @param name Optional name
     * @return UnresolvedObjectHandle<TSpectral> Handle to the new object
     */
    template <IsSpectral TSpectral>
    UnresolvedObjectHandle<TSpectral> Scene<TSpectral>::new_unresolved_object_from_magnitude(double visual_magnitude, std::string name)
    {
        return this->new_unresolved_object_from_magnitude(visual_magnitude, TSpectral{ 1.f }, name);
    }

    /**
     * @brief Creates a new unresolved object from visual magnitude and albedo.
     * @param visual_magnitude Visual magnitude
     * @param albedo Albedo value
     * @param name Optional name
     * @return UnresolvedObjectHandle<TSpectral> Handle to the new object
     */
    template <IsSpectral TSpectral>
    UnresolvedObjectHandle<TSpectral> Scene<TSpectral>::new_unresolved_object_from_magnitude(double visual_magnitude, TSpectral albedo, std::string name)
    {
        TSpectral irradiance = visual_magnitude_to_irradiance<TSpectral>(visual_magnitude, albedo);
        units::SpectralWattsPerMeterSquared<TSpectral> irradiance_watts(irradiance);
        return this->new_unresolved_object(irradiance_watts, name);
    }

    /**
     * @brief Creates a new unresolved emitter with spectral power.
     * @param spectral_power Spectral power
     * @param name Optional name
     * @return UnresolvedObjectHandle<TSpectral> Handle to the new emitter
     */
    template <IsSpectral TSpectral>
    UnresolvedObjectHandle<TSpectral> Scene<TSpectral>::new_unresolved_emitter(const units::SpectralWatts<TSpectral>& spectral_power, std::string name)
    {
        auto unresolved_shared = std::make_shared<UnresolvedEmitter<TSpectral>>(spectral_power);
        return add_unresolved_object(unresolved_shared, name);
    }

    /**
     * @brief Creates a new unresolved emitter with total power.
     * @param power Total power
     * @param name Optional name
     * @return UnresolvedObjectHandle<TSpectral> Handle to the new emitter
     */
    template <IsSpectral TSpectral>
    UnresolvedObjectHandle<TSpectral> Scene<TSpectral>::new_unresolved_emitter(const units::Watt& power, std::string name)
    {
        auto unresolved_shared = std::make_shared<UnresolvedEmitter<TSpectral>>(power);
        return add_unresolved_object(unresolved_shared, name);
    }

    /**
     * @brief Creates a new unresolved sphere with radius and sun instance.
     * @param radius Sphere radius
     * @param sun Sun instance handle
     * @param name Optional name
     * @return UnresolvedObjectHandle<TSpectral> Handle to the new sphere
     */
    template <IsSpectral TSpectral>
    UnresolvedObjectHandle<TSpectral> Scene<TSpectral>::new_unresolved_sphere(units::Meter radius, InstanceHandle<TSpectral> sun, std::string name)
    {
        return this->new_unresolved_sphere(radius, sun, TSpectral{ 1.f }, name);
    }

    /**
     * @brief Creates a new unresolved sphere with radius, sun instance, and albedo.
     * @param radius Sphere radius
     * @param sun Sun instance handle
     * @param albedo Albedo value (spectral)
     * @param name Optional name
     * @return UnresolvedObjectHandle<TSpectral> Handle to the new sphere
     */
    template <IsSpectral TSpectral>
    UnresolvedObjectHandle<TSpectral> Scene<TSpectral>::new_unresolved_sphere(units::Meter radius, InstanceHandle<TSpectral> sun, TSpectral albedo, std::string name)
    {
        auto unresolved_lambertian_sphere = std::make_shared<UnresolvedLambertianSphere<TSpectral>>(radius, sun, albedo);
        return this->add_unresolved_object(unresolved_lambertian_sphere, name);
    }

    /**
     * @brief Creates a new unresolved sphere with radius, sun instance, and albedo.
     * @param radius Sphere radius
     * @param sun Sun instance handle
     * @param albedo Albedo value (constant)
     * @param name Optional name
     * @return UnresolvedObjectHandle<TSpectral> Handle to the new sphere
     */
    template <IsSpectral TSpectral>
    UnresolvedObjectHandle<TSpectral> Scene<TSpectral>::new_unresolved_sphere(units::Meter radius, InstanceHandle<TSpectral> sun, float albedo, std::string name)
    {
        auto unresolved_lambertian_sphere = std::make_shared<UnresolvedLambertianSphere<TSpectral>>(radius, sun, albedo);
        return this->add_unresolved_object(unresolved_lambertian_sphere, name);
    }

    /**
     * @brief Creates a new unresolved asteroid with H, G, and sun instance.
     * @param H Absolute magnitude
     * @param G Slope parameter
     * @param sun Sun instance handle
     * @param name Optional name
     * @return UnresolvedObjectHandle<TSpectral> Handle to the new asteroid
     */
    template <IsSpectral TSpectral>
    UnresolvedObjectHandle<TSpectral> Scene<TSpectral>::new_unresolved_asteroid(double H, double G, InstanceHandle<TSpectral> sun, std::string name)
    {
        return this->new_unresolved_asteroid(H, G, sun, TSpectral{ 1.f }, name);
    }

    /**
     * @brief Creates a new unresolved asteroid with H, G, sun instance, and albedo.
     * @param H Absolute magnitude
     * @param G Slope parameter
     * @param sun Sun instance handle
     * @param albedo Albedo value (spectral)
     * @param name Optional name
     * @return UnresolvedObjectHandle<TSpectral> Handle to the new asteroid
     */
    template <IsSpectral TSpectral>
    UnresolvedObjectHandle<TSpectral> Scene<TSpectral>::new_unresolved_asteroid(double H, double G, InstanceHandle<TSpectral> sun, TSpectral albedo, std::string name)
    {
        auto unresolved_asteroid = std::make_shared<UnresolvedAsteroid<TSpectral>>(H, G, sun, albedo);
        return this->add_unresolved_object(unresolved_asteroid, name);
    }

    /**
     * @brief Creates a new unresolved asteroid with H, G, sun instance, and albedo.
     * @param H Absolute magnitude
     * @param G Slope parameter
     * @param sun Sun instance handle
     * @param albedo Albedo value (constant)
     * @param name Optional name
     * @return UnresolvedObjectHandle<TSpectral> Handle to the new asteroid
     */
    template <IsSpectral TSpectral>
    UnresolvedObjectHandle<TSpectral> Scene<TSpectral>::new_unresolved_asteroid(double H, double G, InstanceHandle<TSpectral> sun, float albedo, std::string name)
    {
        auto unresolved_asteroid = std::make_shared<UnresolvedAsteroid<TSpectral>>(H, G, sun, albedo);
        return this->add_unresolved_object(unresolved_asteroid, name);
    }

    /**
     * @brief Adds an unresolved object to the scene.
     * @param unresolved_object Shared pointer to the unresolved object
     * @param name Optional name
     * @return UnresolvedObjectHandle<TSpectral> Handle to the added object
     */
    template <IsSpectral TSpectral>
    UnresolvedObjectHandle<TSpectral> Scene<TSpectral>::add_unresolved_object(std::shared_ptr<UnresolvedObject<TSpectral>> unresolved_object, std::string name)
    {
        unresolved_objects_.add(unresolved_object, name);
        return UnresolvedObjectHandle<TSpectral>{ unresolved_object };
    }

    /**
     * @brief Sets the name for an unresolved object asset.
     * @param unresolved Handle to the unresolved object
     * @param name New name
     */
    template <IsSpectral TSpectral>
    void Scene<TSpectral>::set_name(const UnresolvedObjectHandle<TSpectral>& unresolved, const std::string& name)
    {
        unresolved_objects_.set_name(unresolved.get(), name);
    }

    /**
     * @brief Retrieves an unresolved object by name.
     * @param name Name of the unresolved object
     * @return UnresolvedObjectHandle<TSpectral> Handle to the unresolved object
     */
    template <IsSpectral TSpectral>
    UnresolvedObjectHandle<TSpectral> Scene<TSpectral>::get_unresolved_object(const std::string& name)
    {
        return UnresolvedObjectHandle<TSpectral>{ unresolved_objects_.lookup(name) };
    }

    /**
     * @brief Deletes an unresolved object from the scene.
     * @param unresolved_object_handle Handle to the unresolved object
     */
    template <IsSpectral TSpectral>
    void Scene<TSpectral>::delete_unresolved_object(const UnresolvedObjectHandle<TSpectral>& unresolved_object_handle)
    {
        auto unresolved_object_shared = unresolved_object_handle.get();
        prune_graph_references_(unresolved_object_shared.get());
        unresolved_objects_.remove(unresolved_object_shared);
    }


    /**
     * @brief Creates a new camera model.
     * @param name Optional name
     * @return CameraModelHandle<TSpectral> Handle to the new camera model
     */
    template <IsSpectral TSpectral>
    CameraModelHandle<TSpectral> Scene<TSpectral>::new_camera_model(std::string name)
    {
        auto camera_shared = std::make_shared<CameraModel<TSpectral>>();
        camera_models_.add(camera_shared, name);
        return CameraModelHandle<TSpectral>{ camera_shared };
    }

    /**
     * @brief Sets the name for a camera model asset.
     * @param camera_model_handle Handle to the camera model
     * @param name New name
     */
    template <IsSpectral TSpectral>
    void Scene<TSpectral>::set_name(const CameraModelHandle<TSpectral>& camera_model_handle, const std::string& name)
    {
        camera_models_.set_name(camera_model_handle.get(), name);
    }

    /**
     * @brief Retrieves a camera model by name.
     * @param name Name of the camera model
     * @return CameraModelHandle<TSpectral> Handle to the camera model
     */
    template <IsSpectral TSpectral>
    CameraModelHandle<TSpectral> Scene<TSpectral>::get_camera_model(const std::string& name)
    {
        return CameraModelHandle<TSpectral>{ camera_models_.lookup(name) };
    }

    /**
     * @brief Deletes a camera model from the scene.
     * @param camera_model_handle Handle to the camera model
     */
    template <IsSpectral TSpectral>
    void Scene<TSpectral>::delete_camera_model(const CameraModelHandle<TSpectral>& camera_model_handle)
    {
        auto camera_model_shared = camera_model_handle.get();
        prune_graph_references_(camera_model_shared.get());
        camera_models_.remove(camera_model_shared);
    }


    /**
     * @brief Loads a model from file and adds it to the scene.
     * @param file Path to the model file
     * @param name Optional name
     * @param post_process_flags Flags for post-processing
     * @return ModelHandle<TSpectral> Handle to the loaded model
     */
    template <IsSpectral TSpectral>
    ModelHandle<TSpectral> Scene<TSpectral>::load_model(const fs::path& file, std::string name, unsigned int post_process_flags)
    {
        // Load the model using ModelLoader
        auto model_shared = ModelLoader<TSpectral>::load(*this, file, name, post_process_flags);
        return ModelHandle<TSpectral>{ model_shared };
    }

    /**
     * @brief Sets the name for a model asset.
     * @param model_handle Handle to the model
     * @param name New name
     */
    template <IsSpectral TSpectral>
    void Scene<TSpectral>::set_name(const ModelHandle<TSpectral>& model_handle, const std::string& name)
    {
        models_.set_name(model_handle.get(), name);
    }

    /**
     * @brief Retrieves a model by name.
     * @param name Name of the model
     * @return ModelHandle<TSpectral> Handle to the model
     */
    template <IsSpectral TSpectral>
    ModelHandle<TSpectral> Scene<TSpectral>::get_model(const std::string& name)
    {
        return ModelHandle<TSpectral>{ models_.lookup(name) };
    }

    /**
     * @brief Deletes a model from the scene.
     * @param model_handle Handle to the model
     */
    template <IsSpectral TSpectral>
    void Scene<TSpectral>::delete_model(const ModelHandle<TSpectral>& model_handle)
    {
        auto model_shared = model_handle.get();
        prune_graph_references_(model_shared.get());
        models_.remove(model_shared);
    }


    /**
     * @brief Adds a star to the scene.
     * @param star Star to add
     */
    template <IsSpectral TSpectral>
    void Scene<TSpectral>::add_star(const Star<TSpectral>& star)
    {
        stars_.push_back(star);
    }

    /**
     * @brief Sets the stars in the scene.
     * @param stars Vector of stars
     */
    template <IsSpectral TSpectral>
    void Scene<TSpectral>::set_stars(const std::vector<Star<TSpectral>>& stars)
    {
        stars_ = stars;
    }

    /**
     * @brief Loads stars from a catalog file.
     * @param star_catalog_path Path to the star catalog
     * @param time Time for proper motion
     * @param min_magnitude Minimum magnitude to load
     */
    template <IsSpectral TSpectral>
    void Scene<TSpectral>::load_stars(const fs::path& star_catalog_path, const Time& time, float min_magnitude)
    {
        // Read the catalog:
        StarCatalog star_catalog = StarCatalog::read_star_data(star_catalog_path, min_magnitude);
        const std::vector<StarData>& star_data = star_catalog.get_star_data();

        double tsince = time.julian_years_since_j2000(TimeScale::TT);

        // Create the stars:
        std::vector<Star<TSpectral>> stars(star_data.size());
        tbb::parallel_for(
            tbb::blocked_range<std::size_t>(0, star_data.size()),
            [&](const tbb::blocked_range<std::size_t>& r) {
                for (std::size_t i = r.begin(); i != r.end(); ++i) {
                    stars[i] = Star<TSpectral>(star_data[i], tsince);
                }
            }
        );

        // Add the stars to the scene:
        set_stars(stars);
    }

    /**
     * @brief Removes assets not referenced in the scene graph.
     */
    template <IsSpectral TSpectral>
    void Scene<TSpectral>::prune_unreferenced_assets()
    {
        
    }


    /**
     * @brief Removes references to an asset from the scene graph.
     * @tparam TAssetPtr Pointer type of the asset
     * @param target_ptr Pointer to the asset to prune
     */
    template <IsSpectral TSpectral>
    template <typename TAssetPtr>
    void Scene<TSpectral>::prune_graph_references_(TAssetPtr target_ptr)
    {
        std::function<void(Node<TSpectral>*)> prune_references =
            [&](Node<TSpectral>* parent)
            {
                std::vector<std::shared_ptr<Node<TSpectral>>> children_snapshot(
                    parent->get_children().begin(),
                    parent->get_children().end()
                );

                for (const auto& child_node : children_snapshot) {
                    bool deleted = false;

                    if (auto instance = std::dynamic_pointer_cast<Instance<TSpectral>>(child_node)) {
                        const auto& asset_var = instance->asset();

                        // We check if the variant holds specific pointer type:
                        if (std::holds_alternative<TAssetPtr>(asset_var)) {
                            // Compare the pointers
                            if (std::get<TAssetPtr>(asset_var) == target_ptr) {

                                if (auto* frame_parent = dynamic_cast<FrameNode<TSpectral>*>(parent)) {
                                    frame_parent->delete_child(child_node);
                                    deleted = true;
                                }
                                else {
                                    HUIRA_THROW_ERROR("Attempted to delete child from a non-FrameNode!");
                                }
                            }
                        }
                    }

                    // Recurse if the node itself wasn't deleted
                    if (!deleted) {
                        prune_references(child_node.get());
                    }
                }
            };
        if (root_node_) {
            prune_references(root_node_.get());
        }
    }

    /**
     * @brief Prints information about all meshes in the scene.
     */
    template <IsSpectral TSpectral>
    void Scene<TSpectral>::print_meshes() const {
        std::cout << green("Meshes: " + std::to_string(meshes_.size()) + " loaded") << "\n";
        for (const auto& mesh : meshes_) {
            std::cout << " - " << mesh->get_info();
        }
    }

    /**
     * @brief Prints information about all lights in the scene.
     */
    template <IsSpectral TSpectral>
    void Scene<TSpectral>::print_lights() const {
        std::cout << yellow("Lights: " + std::to_string(lights_.size()) + " loaded") << "\n";
        for (const auto& light : lights_) {
            std::cout << " - " << light->get_info();
        }
    }

    /**
     * @brief Prints information about all unresolved objects in the scene.
     */
    template <IsSpectral TSpectral>
    void Scene<TSpectral>::print_unresolved_objects() const {
        std::cout << cyan("UnresolvedObjects: " + std::to_string(unresolved_objects_.size()) + " loaded") << "\n";
        for (const auto& unresolved_object : unresolved_objects_) {
            std::cout << " - " << unresolved_object->get_info();
        }
    }

    /**
     * @brief Prints information about all camera models in the scene.
     */
    template <IsSpectral TSpectral>
    void Scene<TSpectral>::print_camera_models() const {
        std::cout << blue("CameraModels: " + std::to_string(camera_models_.size()) + " loaded") << "\n";
        for (const auto& camera_model : camera_models_) {
            std::cout << " - " << camera_model->get_info();
        }
    }

    /**
     * @brief Prints information about all models in the scene.
     */
    template <IsSpectral TSpectral>
    void Scene<TSpectral>::print_models() const {
        std::cout << magenta("Models: " + std::to_string(models_.size()) + " loaded") << "\n";
        for (const auto& model : models_) {
            std::cout << " - " << model->get_info();
        }
    }

    /**
     * @brief Prints the scene graph hierarchy.
     */
    template <IsSpectral TSpectral>
    void Scene<TSpectral>::print_graph() const {
        std::cout << on_blue("root");
        print_node_details_(root_node_.get());
        std::cout << "\n";

        const auto children = root_node_->get_children();
        for (size_t i = 0; i < children.size(); ++i) {
            bool is_last = (i == children.size() - 1);
            print_node_((children)[i].get(), "", is_last);
        }
    }

    /**
     * @brief Prints a summary of the scene contents and graph.
     */
    template <IsSpectral TSpectral>
    void Scene<TSpectral>::print_contents() const {
        std::cout << "Scene Contents:\n";
        std::cout << " - " << "Stars: " << std::to_string(stars_.size()) << " loaded\n";
        std::cout << " - " << blue("CameraModels: " + std::to_string(camera_models_.size()) + " loaded") << "\n";
        std::cout << " - " << green("Meshes: " + std::to_string(meshes_.size()) + " loaded") << "\n";
        std::cout << " - " << yellow("Lights: " + std::to_string(lights_.size()) + " loaded") << "\n";
        std::cout << " - " << cyan("UnresolvedObjects: " + std::to_string(unresolved_objects_.size()) + " loaded") << "\n";
        std::cout << " - " << magenta("Models: " + std::to_string(models_.size()) + " loaded") << "\n";
        std::cout << "Scene Graph:\n";
        print_graph();
    }


    // ======================= //
    // === Private Members === //
    // ======================= //

    /**
     * @brief Prints details for a node in the scene graph.
     * @param node Node to print
     * @param prefix Prefix for formatting
     * @param is_last Whether this is the last child
     */
    template <IsSpectral TSpectral>
    void Scene<TSpectral>::print_node_(const Node<TSpectral>* node, const std::string& prefix, bool is_last) const {
        std::cout << prefix;
        std::cout << (is_last ? "+-- " : "|-- ");
        
        // Check if the node is an Instance
        if (const auto* instance_node = dynamic_cast<const Instance<TSpectral>*>(node)) {
            std::string instance_str = "Instance[" + std::to_string(instance_node->id()) + "]";
            instance_str += instance_node->name_.empty() ? "" : " " + instance_node->name_;
            std::cout << on_green(instance_str) << " -> ";
            std::visit([&](auto* raw_ptr) noexcept {
                using AssetType = std::decay_t<decltype(*raw_ptr)>;
                if constexpr (std::is_same_v<AssetType, Mesh<TSpectral>>) {
                    std::cout << green(raw_ptr->get_info());
                }
                else if constexpr (std::is_same_v<AssetType, Light<TSpectral>>) {
                    std::cout << yellow(raw_ptr->get_info());
                }
                else if constexpr (std::is_same_v<AssetType, UnresolvedObject<TSpectral>>) {
                    std::cout << cyan(raw_ptr->get_info());
                }
                else if constexpr (std::is_same_v<AssetType, CameraModel<TSpectral>>) {
                    std::cout << blue(raw_ptr->get_info());
                }
                else if constexpr (std::is_same_v<AssetType, Model<TSpectral>>) {
                    std::cout << magenta(raw_ptr->get_info());
                }
                }, instance_node->asset_);
        }
        else {
            std::cout << on_blue(node->get_info());
        }
        print_node_details_(node);
        std::cout << "\n";

        const std::string child_prefix = prefix + (is_last ? "    " : "|   ");

        const auto children = node->get_children();
        for (size_t i = 0; i < children.size(); ++i) {
            bool child_is_last = (i == children.size() - 1);
            print_node_((children)[i].get(), child_prefix, child_is_last);
        }
    }

    /**
     * @brief Prints SPICE details for a node.
     * @param node Node to print
     */
    template <IsSpectral TSpectral>
    void Scene<TSpectral>::print_node_details_(const Node<TSpectral>* node) const {
        if (node->spice_origin_ != "" || node->spice_frame_ != "") {
            std::cout << " (";
            if (node->spice_origin_ != "") {
                std::cout << node->spice_origin_;
                if (node->spice_frame_ != "") {
                    std::cout << ", ";
                }
            }

            if (node->spice_frame_ != "") {
                std::cout << node->spice_frame_;
            }
            std::cout << ")";
        }
    }


    /**
     * @brief Finds the shared_ptr for a given raw Node pointer.
     *
     * Recursively searches the scene graph starting from the root to find
     * the shared_ptr corresponding to the given raw pointer. This is needed
     * for creating handles to existing nodes (e.g., parent nodes).
     *
     * @param target The raw pointer to search for
     * @return std::shared_ptr<Node<TSpectral>> The shared_ptr if found, nullptr otherwise
     */
    template <IsSpectral TSpectral>
    std::shared_ptr<Node<TSpectral>> Scene<TSpectral>::find_node_shared_ptr_(const Node<TSpectral>* target) const {
        // Check if target is the root
        if (root_node_.get() == target) {
            return root_node_;
        }

        // Otherwise recursively search the tree
        return find_node_in_tree_(root_node_, target);
    }


    /**
     * @brief Recursively searches for a node in the scene graph tree.
     *
     * Helper function for find_node_shared_ptr_ that traverses the scene graph.
     *
     * @param current The current node being examined
     * @param target The raw pointer to search for
     * @return std::shared_ptr<Node<TSpectral>> The shared_ptr if found, nullptr otherwise
     */
    template <IsSpectral TSpectral>
    std::shared_ptr<Node<TSpectral>> Scene<TSpectral>::find_node_in_tree_(
        const std::shared_ptr<Node<TSpectral>>& current, 
        const Node<TSpectral>* target) const 
    {
        // Check if current node is the target
        if (current.get() == target) {
            return current;
        }

        // Get children if this node has any
        const auto children = current->get_children();
            for (const auto& child : children) {
                // Check this child
                if (child.get() == target) {
                    return child;
                }

                // Recursively search this child's subtree
                auto result = find_node_in_tree_(child, target);
                if (result) {
                    return result;
                }
            }

        return nullptr;
    }

    /**
     * @brief Registers a node name in the node registry.
     * @param node Node to register
     * @param name Name to associate
     */
    template <IsSpectral TSpectral>
    void Scene<TSpectral>::register_node_name_(const std::shared_ptr<Node<TSpectral>>& node, const std::string& name)
    {
        node_registry_.add(node, name);
    }
}
