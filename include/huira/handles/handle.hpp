#pragma once

#include <memory>

#include "huira/detail/logger.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"

namespace huira {
    template <typename T>
    class Handle {
    public:
        Handle(std::weak_ptr<T> ptr) : ptr_(ptr) {}

        bool valid() const { return !ptr_.expired(); }

    protected:
        std::shared_ptr<T> get() const {
            std::shared_ptr<T> p = ptr_.lock();

            if (!p) {
                HUIRA_THROW_ERROR("Attempted to access an invalid handle");
            }

            return p;
        }

        std::weak_ptr<T> ptr_;
    };
}
