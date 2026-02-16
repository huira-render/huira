#pragma once

#include <memory>
#include <concepts>

#include "huira/core/concepts/spectral_concepts.hpp"

namespace huira {
    template <typename T>
    concept IsSceneObject = requires(const T t) {
        { t.is_scene_owned() } -> std::convertible_to<bool>;
    };

    /**
     * @brief Strongly-typed handle for scene objects.
     *
     * Handle provides safe, type-checked access to scene objects managed by shared pointers.
     * It ensures that the referenced object is still valid and owned by the scene, and allows
     * for type-safe downcasting to derived types. Handles are used throughout the scene graph
     * and asset management system to avoid raw pointer usage and to enforce object lifetime.
     *
     * @tparam T Scene object type
     */
    template <IsSceneObject T>
    class Handle {
    public:
        Handle(std::weak_ptr<T> ptr) : ptr_(ptr) {}

        bool valid() const;

        template <typename U = T>
            requires std::derived_from<U, T> || std::same_as<U, T>
        std::shared_ptr<U> get() const;

    protected:
        std::shared_ptr<T> get_() const;

        std::weak_ptr<T> ptr_;
    };
}

#include "huira_impl/handles/handle.ipp"
