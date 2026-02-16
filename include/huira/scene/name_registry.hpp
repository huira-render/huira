#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace huira {
    /**
     * @brief Registry for scene objects by name.
     *
     * NameRegistry manages a collection of objects and their unique names, allowing lookup, addition, removal, and renaming.
     *
     * @tparam T Object type
     */
    template <typename T>
    class NameRegistry {
    public:
        void add(std::shared_ptr<T> object, std::string name);
        void remove(std::shared_ptr<T> object);
        void set_name(std::shared_ptr<T> object, std::string name);

        std::size_t size() const { return objects_.size(); }

        auto begin() { return objects_.begin(); }
        auto end() { return objects_.end(); }
        auto begin() const { return objects_.begin(); }
        auto end() const { return objects_.end(); }

        std::shared_ptr<T> lookup(const std::string& name) const;

    private:
        std::vector<std::shared_ptr<T>> objects_;
        std::unordered_map<std::string, std::shared_ptr<T>> name_registry_;

        std::string make_unique_name_(std::string name) const;
    };
}

#include "huira_impl/scene/name_registry.ipp"
