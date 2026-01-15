#pragma once

#include <memory>
#include <stdexcept>

namespace huira {
    template <typename T>
    class Handle {
    public:
        Handle(std::weak_ptr<T> ptr, const bool* scene_locked)
            : ptr_(ptr), scene_locked_(scene_locked)
        {

        }

        bool valid() const { return !ptr_.expired(); }

    protected:
        std::shared_ptr<T> get() const {
            std::shared_ptr<T> p = ptr_.lock();

            if (!p) {
                throw std::runtime_error("Attempted to access an invalid handle");
            }

            if (*scene_locked_) {
                throw std::runtime_error("Attempted to access a handle while the scene is locked");
            }

            return p;
        }

        std::weak_ptr<T> ptr_;
        const bool* scene_locked_;
    };
}
