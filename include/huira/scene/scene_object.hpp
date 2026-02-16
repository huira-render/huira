#pragma once

#include <atomic>
#include <memory>
#include <string>

#include "huira/core/concepts/spectral_concepts.hpp"

namespace huira {
    // Forward Declare:
    template <typename T>
    class NameRegistry;

    template <IsSpectral TSpectral>
    class Scene;

    /**
     * @brief Base class for all scene objects (nodes, lights, meshes, etc.).
     *
     * SceneObject provides common interface for scene ownership, naming, and info reporting.
     *
     * @tparam TDerived Derived type (CRTP)
     * @tparam TSpectral Spectral type (e.g., RGB, Spectral)
     */
    template <typename TDerived, IsSpectral TSpectral>
    class SceneObject : public std::enable_shared_from_this<TDerived> {
    public:
        SceneObject() = default;

        virtual ~SceneObject() = default;

        // Delete copy
        SceneObject(const SceneObject&) = delete;
        SceneObject& operator=(const SceneObject&) = delete;

        // Allow move (copy the atomic's value)
        SceneObject(SceneObject&& other) noexcept
            : scene_owned_(other.scene_owned_.load()) {
        }

        SceneObject& operator=(SceneObject&& other) noexcept {
            scene_owned_.store(other.scene_owned_.load());
            return *this;
        }

        /**
         * @brief Check if the object is owned by the scene.
         * @return bool True if owned
         */
        bool is_scene_owned() const { return scene_owned_; }

        /**
         * @brief Set scene ownership flag.
         * @param owned True if owned
         */
        void set_scene_owned(bool owned) { scene_owned_ = owned; }

        /**
         * @brief Get the object's name.
         * @return std::string Name
         */
        std::string name() const { return name_; }

        /**
         * @brief Get the object's unique ID.
         * @return std::uint64_t ID
         */
        virtual std::uint64_t id() const = 0;

        /**
         * @brief Get the object's type string.
         * @return std::string Type
         */
        virtual std::string type() const = 0;

        /**
         * @brief Get a descriptive info string for the object.
         * @return std::string Info string
         */
        virtual std::string get_info() const {
            return type() + "[" + std::to_string(this->id()) + "]" +
                (name_.empty() ? "" : " " + name_);
        }

    private:
        std::atomic<bool> scene_owned_{ true }; // Only scene should modify this
        friend class Scene<TSpectral>;

        std::string name_ = ""; // Only NameRegistry should modify this
        
        friend class NameRegistry<TDerived>;
    };
}
