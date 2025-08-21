#include "huira/scene/scene_node.hpp"

#include "huira/detail/concepts/numeric_concepts.hpp"

namespace huira {
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