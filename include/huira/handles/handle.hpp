#pragma once

#include <memory>
#include <concepts>

#include "huira/util/logger.hpp"
#include "huira/core/concepts/spectral_concepts.hpp"

namespace huira {
    template <typename T>
    concept IsSceneObject = requires(const T t) {
        { t.is_scene_owned() } -> std::convertible_to<bool>;
    };

    template <IsSceneObject T>
    class Handle {
    public:
        Handle(std::weak_ptr<T> ptr) : ptr_(ptr) {}

        bool valid() const {
            std::shared_ptr<T> p = ptr_.lock();
            if (!p) {
                return false;
            }
            if (!p->is_scene_owned()) {
                return false;
            }
            return true;
        }

    protected:
        std::shared_ptr<T> get() const {
            std::shared_ptr<T> p = ptr_.lock();
            if (!p) {
                HUIRA_THROW_ERROR("Attempted to access an invalid handle");
            }
            if (!p->is_scene_owned()) {
                HUIRA_THROW_ERROR("Attempted to access an invalid handle");
            }
            return p;
        }

        std::weak_ptr<T> ptr_;
    };
}
