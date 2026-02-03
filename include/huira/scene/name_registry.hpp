#pragma once

#include <vector>
#include <unordered_map>
#include <memory>
#include <string>
#include <cstddef>

#include "huira/util/logger.hpp"

namespace huira {
    template <typename T>
    class NameRegistry {
    public:
        void add(std::shared_ptr<T> object, std::string name)
        {
            if (name.empty()) {
                name = object->type() + "." + std::to_string(object->id());
            }
            objects_.push_back(object);

            name = make_unique_name_(name);

            name_registry_[name] = object;
            object->name_ = name;

            HUIRA_LOG_INFO("Scene - Added: " + object->get_info());
        };

        void remove(std::shared_ptr<T> object)
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

        void set_name(std::shared_ptr<T> object, std::string name)
        {
            std::string new_name = make_unique_name_(name);
            std::string old_name = object->name();

            if (new_name == old_name) return;

            name_registry_.erase(old_name);
            name_registry_[new_name] = object;
            object->name_ = new_name;
        }


        // Expose objects_
        std::size_t size() const { return objects_.size(); }
        auto begin() { return objects_.begin(); }
        auto end() { return objects_.end(); }
        auto begin() const { return objects_.begin(); }
        auto end() const { return objects_.end(); }
        std::shared_ptr<T> lookup(const std::string& name) const
        {
            auto it = name_registry_.find(name);
            if (it == name_registry_.end()) {
                HUIRA_THROW_ERROR("NameRegistry - " + name + " does not exist in the scene");
            }
            return it->second;
        }

    private:
        std::vector<std::shared_ptr<T>> objects_;
        std::unordered_map<std::string, std::shared_ptr<T>> name_registry_;

        std::string make_unique_name_(std::string name)
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
    };
}
