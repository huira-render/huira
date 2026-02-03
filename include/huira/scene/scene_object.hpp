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

        bool is_scene_owned() const { return scene_owned_; }
        void set_scene_owned(bool owned) { scene_owned_ = owned; }


        std::string name() const { return name_; }

        virtual std::uint64_t id() const = 0;
        virtual std::string type() const = 0;

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
