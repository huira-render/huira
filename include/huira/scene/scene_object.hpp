#pragma once

#include <atomic>
#include <memory>

#include "huira/core/concepts/spectral_concepts.hpp"

namespace huira {
    template <typename TDerived, IsSpectral TSpectral>
    class SceneObject : public std::enable_shared_from_this<TDerived> {
    public:
        SceneObject() = default;

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

    protected:
        std::atomic<bool> scene_owned_{ true };
    };
}
