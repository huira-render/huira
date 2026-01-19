#pragma once

#include <memory>
#include <stdexcept>

namespace huira {
    template <typename T>
    class Handle {
    public:
        Handle(std::weak_ptr<T> ptr)
            : ptr_(ptr)
        {

        }

        bool valid() const { return !ptr_.expired(); }

    protected:
        std::shared_ptr<T> safe_get() const {
            std::shared_ptr<T> p = ptr_.lock();

            if (!p) {
                throw std::runtime_error("Attempted to access an invalid handle");
            }

            return p;
        }

        std::weak_ptr<T> ptr_;
    };
}
