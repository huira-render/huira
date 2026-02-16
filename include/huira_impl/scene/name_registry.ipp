#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "huira/util/logger.hpp"

namespace huira {

    /**
     * @brief Add an object to the registry with a given name.
     * @param object Shared pointer to object
     * @param name Desired name (may be made unique)
     */
    template <typename T>
    void NameRegistry<T>::add(std::shared_ptr<T> object, std::string name)
    {
        if (name.empty()) {
            name = object->type() + "." + std::to_string(object->id());
        }
        objects_.push_back(object);

        name = make_unique_name_(name);

        name_registry_[name] = object;
        object->name_ = name;

        HUIRA_LOG_INFO("Scene - Added: " + object->get_info());
    }


    /**
     * @brief Remove an object from the registry.
     * @param object Shared pointer to object
     */
    template <typename T>
    void NameRegistry<T>::remove(std::shared_ptr<T> object)
    {
        auto it = std::find(objects_.begin(), objects_.end(), object);
        if (it == objects_.end()) {
            HUIRA_THROW_ERROR(object->get_info() + " does not exist in the scene");
        }

        object->set_scene_owned(false);

        objects_.erase(it);
        name_registry_.erase(object->name());

        HUIRA_LOG_INFO("Scene - Deleted: " + object->get_info());
    }


    /**
     * @brief Set a new name for an object.
     * @param object Shared pointer to object
     * @param name New name (may be made unique)
     */
    template <typename T>
    void NameRegistry<T>::set_name(std::shared_ptr<T> object, std::string name)
    {
        std::string new_name = make_unique_name_(name);
        std::string old_name = object->name();

        if (new_name == old_name) return;

        name_registry_.erase(old_name);
        name_registry_[new_name] = object;
        object->name_ = new_name;
    }


    /**
     * @brief Lookup an object by name.
     * @param name Name to search for
     * @return std::shared_ptr<T> Shared pointer to object
     */
    template <typename T>
    std::shared_ptr<T> NameRegistry<T>::lookup(const std::string& name) const
    {
        auto it = name_registry_.find(name);
        if (it == name_registry_.end()) {
            HUIRA_THROW_ERROR("NameRegistry - " + name + " does not exist in the scene");
        }
        return it->second;
    }


    /**
     * @brief Make a name unique within the registry.
     * @param name Desired name
     * @return std::string Unique name
     */
    template <typename T>
    std::string NameRegistry<T>::make_unique_name_(std::string name) const
    {
        if (name_registry_.contains(name)) {
            int counter = 1;
            std::string base_name = name;
            do {
                name = base_name + "_" + std::to_string(counter++);
            } while (name_registry_.contains(name));
        }
        return name;
    }
}
