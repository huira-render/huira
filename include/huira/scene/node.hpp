#pragma once

#include <memory>
#include <string>
#include <stdexcept>
#include <cstdint>
#include <span>

#include "huira/core/transform.hpp"
#include "huira/core/time.hpp"
#include "huira/scene/scene_object.hpp"

#include "huira/core/concepts/numeric_concepts.hpp"
#include "huira/core/concepts/spectral_concepts.hpp"

namespace huira {
    // Forward declare:
    template <IsSpectral TSpectral>
    class Scene;

    template <IsSpectral TSpectral>
    class SceneView;

    template <IsSpectral TSpectral>
    class FrameNode;

    template <IsSpectral TSpectral, typename TNode>
    class NodeHandle;

    enum class TransformMode {
        MANUAL_TRANSFORM,
        SPICE_TRANSFORM
    };

    enum class ObservationMode {
        TRUE_STATE,
        GEOMETRIC_STATE,
        ABERRATED_STATE
    };

    /**
     * @brief Base class for all scene graph nodes.
     *
     * Node represents a transformable entity in the scene graph. It handles:
     * - Local and global transforms (position, rotation, scale)
     * - SPICE-based transforms for celestial mechanics
     * - Parent-child relationships (being a child)
     *
     * Node itself cannot have children - use FrameNode for nodes that need children.
     * Leaf nodes (lights, unresolved objects, etc.) should derive from Node directly.
     */
    template <IsSpectral TSpectral>
    class Node : public SceneObject<Node<TSpectral>, TSpectral> {
    public:
        Node(Scene<TSpectral>* scene);
        virtual ~Node() = default;

        Node(const Node&) = delete;
        Node& operator=(const Node&) = delete;

        std::uint64_t id() const { return id_; }
        void set_name(const std::string& name) { name_ = name; }
        const std::string& name() const noexcept { return name_; }

        void set_position(const Vec3<double>& position);
        void set_rotation(const Rotation<double>& rotation);
        void set_scale(const Vec3<double>& scale);

        void set_velocity(const Vec3<double>& velocity);
        void set_angular_velocity(const Vec3<double>& angular_velocity);

        void set_spice_origin(const std::string& spice_origin);
        void set_spice_frame(const std::string& spice_frame);
        void set_spice(const std::string& spice_origin, const std::string& spice_frame);


        virtual std::string get_info() const { return "Node[" + std::to_string(id_) + "]" + " " + name_; }

        TransformMode get_position_mode() const { return position_mode_; }
        TransformMode get_rotation_mode() const { return rotation_mode_; }

        Transform<double> get_apparent_transform(ObservationMode obs_mode, const Time& t_obs, const Transform<double>& observer_ssb_state) const;

        Vec3<double> get_static_position() const;
        Rotation<double> get_static_rotation() const;
        Vec3<double> get_static_scale() const;
        Vec3<double> get_static_velocity() const;
        Vec3<double> get_static_angular_velocity() const;

        std::string get_spice_origin() const;
        std::string get_spice_frame() const;

        NodeHandle<TSpectral, Node<TSpectral>> get_parent() const;
        
        template <typename TParentNode>
        NodeHandle<TSpectral, TParentNode> get_parent_as() const;

        virtual std::span<const std::shared_ptr<Node<TSpectral>>> get_children() const { return {}; }

    protected:
        Transform<double> local_transform_;

        TransformMode position_mode_ = TransformMode::MANUAL_TRANSFORM;
        TransformMode rotation_mode_ = TransformMode::MANUAL_TRANSFORM;

        std::string spice_origin_ = "";
        std::string spice_frame_ = "";

        bool position_can_be_spice_() const;
        virtual bool position_can_be_manual_() const { return true; }

        bool rotation_can_be_spice_() const;
        virtual bool rotation_can_be_manual_() const { return true; }

        std::uint64_t id_ = 0;
        static inline std::uint64_t next_id_ = 0;
        std::string name_ = "";

        Scene<TSpectral>* scene_;
        Node<TSpectral>* parent_ = nullptr;

        void set_parent_(Node<TSpectral>* parent) { parent_ = parent; }

        std::pair<const Node<TSpectral>*, Transform<double>> find_spice_origin_ancestor_() const;
        std::pair<const Node<TSpectral>*, std::pair<Rotation<double>, Vec3<double>>> find_spice_frame_ancestor_() const;

        std::pair<Transform<double>, double> get_geometric_state_(const Time& t_obs, const Transform<double>& observer_ssb_state, bool iterate, double tol = 1e-12) const;

        Transform<double> get_ssb_transform_(const Time& t_obs, double dt = 0.0) const;
        Transform<double> get_local_position_at_(const Time& t_obs, double dt) const;
        Transform<double> get_local_rotation_at_(const Time& t_obs, double dt) const;

        friend class Scene<TSpectral>;
        friend class FrameNode<TSpectral>;
        friend class SceneView<TSpectral>;
    };
}

#include "huira_impl/scene/node.ipp"
