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
    class CameraNode : public SceneNode<Ts> {
    public:
        static constexpr NodeType TYPE = NodeType::Camera;
        NodeType getType() const override { return TYPE; }
    };

    template <IsFloatingPoint Ts>
    class InstanceNode : public SceneNode<Ts> {
    public:
        static constexpr NodeType TYPE = NodeType::Instance;
        NodeType getType() const override { return TYPE; }
    };

    template <IsFloatingPoint Ts>
    class LightNode : public SceneNode<Ts> {
    public:
        static constexpr NodeType TYPE = NodeType::Light;
        NodeType getType() const override { return TYPE; }
    };
}