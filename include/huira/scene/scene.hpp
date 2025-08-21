#pragma once

#include <string>
#include <memory>

#include "huira/scene/scene_node.hpp"

#include "huira/detail/concepts/numeric_concepts.hpp"

namespace huira {
    template <IsFloatingPoint Ts>
    class Scene {

        std::string newGroup(std::string name = "", std::string parent_name = "")
        {
            std::unique_ptr<GroupNode<Ts>> new_group(new GroupNode<Ts>());

            GroupNode<Ts>* parent = root_.get();
            if (!parent_name.empty()) {
                parent = findGroup(parent_name);
            }

            SceneNode<Ts>* raw_ptr = new_group.get();
            parent->addChild(std::move(new_group));

            return updateNameRegistry(raw_ptr, name);
        };

        std::string newCamera(std::string name = "", std::string parent_name = "")
        {
            std::unique_ptr<Camera<Ts>> new_camera(new Camera<Ts>());

            GroupNode<Ts>* parent = root_.get();
            if (!parent_name.empty()) {
                parent = findGroup(parent_name);
            }

            SceneNode<Ts>* raw_ptr = new_camera.get();
            parent->addChild(std::move(new_camera));

            return updateNameRegistry(raw_ptr, name);
        };

    private:
        std::unique_ptr<GroupNode<Ts>> root_;

        std::unordered_map<std::string, SceneNode<Ts>*> name_registry_;

        GroupNode<Ts>* findGroup(std::string name)
        {
            auto it = name_registry_.find(name);

            if (it != name_registry_.end()) {
                if (it->second->getType() == NodeType::GroupNode) {
                    return static_cast<GroupNode<Ts>*>(it->second);
                }
            }

            throw std::runtime_error("No GroupNode was found with the name: " + name);
        };

        std::string updateNameRegistry(SceneNode<Ts>* ptr, std::string name)
        {
            // Create a default name if none is provided:
            if (name.empty()) {
                switch (ptr->getType()) {
                case NodeType::GroupNode:
                    name = "GroupNode";
                    break;
                case NodeType::Camera:
                    name = "Camera";
                    break;
                case NodeType::Instance:
                    name = "Instance";
                    break;
                case NodeType::Light:
                    name = "Light";
                    break;
                default:
                    throw std::runtime_error("An unexpected error occured.  SceneNode::Type not recognized");
                }
            }

            // TOODO Make the name unique (I'll handle this later, its not important for my illustration now):


            // Add to the registry:
            name_registry_.emplace(name, ptr);

            return name;
        };
    }
}