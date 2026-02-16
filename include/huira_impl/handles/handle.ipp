#include "huira/util/logger.hpp"

namespace huira {
    /**
     * @brief Checks if the handle points to a valid, scene-owned object.
     * @return true if valid, false otherwise
     */
    template <IsSceneObject T>
    bool Handle<T>::valid() const {
        std::shared_ptr<T> p = ptr_.lock();
        if (!p) {
            return false;
        }
        if (!p->is_scene_owned()) {
            return false;
        }
        return true;
    }

    /**
     * @brief Gets a shared_ptr to the referenced object, optionally downcasting to a derived type.
     * @tparam U Type to cast to (default: T)
     * @return std::shared_ptr<U> Shared pointer to the object
     * @throws std::runtime_error if the handle is invalid or the cast fails
     */
    template <IsSceneObject T>
    template <typename U>
        requires std::derived_from<U, T> || std::same_as<U, T>
    std::shared_ptr<U> Handle<T>::get() const {
        std::shared_ptr<T> p = get_();

        if constexpr (std::same_as<U, T>) {
            return p;
        }
        else {
            std::shared_ptr<U> derived = std::dynamic_pointer_cast<U>(p);
            if (!derived) {
                HUIRA_THROW_ERROR("Handle does not point to the requested type");
            }
            return derived;
        }
    }

    /**
     * @brief Gets a shared_ptr to the referenced object, enforcing validity.
     * @return std::shared_ptr<T> Shared pointer to the object
     * @throws std::runtime_error if the handle is invalid
     */
    template <IsSceneObject T>
    std::shared_ptr<T> Handle<T>::get_() const {
        std::shared_ptr<T> p = ptr_.lock();
        if (!p) {
            HUIRA_THROW_ERROR("Attempted to access an invalid handle");
        }
        if (!p->is_scene_owned()) {
            HUIRA_THROW_ERROR("Attempted to access an invalid handle");
        }
        return p;
    }
}
