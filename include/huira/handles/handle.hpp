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

        template <typename U = T>
            requires std::derived_from<U, T> || std::same_as<U, T>
        std::shared_ptr<U> get() const {
            std::shared_ptr<T> p = get_();
            
            if constexpr (std::same_as<U, T>) {
                return p;
            } else {
                std::shared_ptr<U> derived = std::dynamic_pointer_cast<U>(p);
                if (!derived) {
                    HUIRA_THROW_ERROR("Handle does not point to the requested type");
                }
                return derived;
            }
        }

    protected:
        std::shared_ptr<T> get_() const {
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
