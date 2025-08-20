#pragma once

#include <vector>
#include <memory>
#include <stdexcept>
#include <unordered_map>

#include "huira/concepts/numeric_concepts.hpp"

namespace huira {
    enum class NodeType { GroupNode, Camera, Instance, Light };


    // Forward Declaration:
    template <IsFloatingPoint Ts>
    class GroupNode;

	template <IsFloatingPoint Ts>
	class SceneNode {
    public:
        virtual NodeType getType() const = 0;
        virtual ~SceneNode() = default;

        // Pose setting member functions

        // Type-safe casting helpers
        template<typename T> bool is() const
        {
            return getType() == T::TYPE;
        }

        template<typename T> T* as() 
        {
            if (is<T>()) {
                return static_cast<T*>(this);
            }
            else {
                throw std::runtime_error("Cannot convert to the specified type");
            }
        }
        template<typename T> const T* as() const 
        {
            if (is<T>()) {
                return static_cast<const T*>(this);
            }
            else {
                throw std::runtime_error("Cannot convert to the specified type");
            }
        }

    private:
        GroupNode<Ts>* parent_;
        void setParent(GroupNode<Ts>* parent)
        {
            if (parent != nullptr) {
                parent_ = parent;
            }
            else {
                throw std::runtime_error("Parent GroupNode pointer is NULL");
            }
        };

        friend class GroupNode<Ts>;
	};

    template <IsFloatingPoint Ts>
    class GroupNode : public SceneNode<Ts> {
    public:
        static constexpr NodeType TYPE = NodeType::GroupNode;
        NodeType getType() const override { return TYPE; }

        template <typename T>
        void addChild(std::unique_ptr<T>&& new_child)
        {
            new_child->setParent(this);
            children_.push_back(std::move(new_child));
        }

        void removeChild(size_t index)
        {
            children_.erase(children_.begin() + index);
        }

    private:
        std::vector<std::unique_ptr<SceneNode<Ts>>> children_;
    };

    template <IsFloatingPoint Ts> 
    class Camera : public SceneNode<Ts> {
    public:
        static constexpr NodeType TYPE = NodeType::Camera;
        NodeType getType() const override { return TYPE; }
    };

    template <IsFloatingPoint Ts>
    class Instance : public SceneNode<Ts> {
    public:
        static constexpr NodeType TYPE = NodeType::Instance;
        NodeType getType() const override { return TYPE; }
    };

    template <IsFloatingPoint Ts>
    class Light : public SceneNode<Ts> {
    public:
        static constexpr NodeType TYPE = NodeType::Light;
        NodeType getType() const override { return TYPE; }
    };


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

        std::string newCamera(std::string name= "", std::string parent_name = "")
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


    };
}