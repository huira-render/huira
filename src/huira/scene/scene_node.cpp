#include "huira/scene/scene_node.hpp"

#include <unordered_map>

#include "glm/glm.hpp"

#include "huira/math/rotation.hpp"
#include "huira/math/types.hpp"
#include "huira/units/units.hpp"

#include "huira/scene/derived_nodes.hpp"

#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/diagnostics/logging.hpp"

namespace huira {
    // ============================== //
    // === Transformation Setters === //
    // ============================== //
    template <IsFloatingPoint Ts>
    void SceneNode<Ts>::setLocalPosition(Vec3<Ts> position)
    {
        this->local_position_ = position;
        this->updateTransforms();
    }

    template <IsFloatingPoint Ts>
    void SceneNode<Ts>::setLocalPosition(Meter x, Meter y, Meter z)
    {
        this->setLocalPosition(Vec3<Ts>{x.getSIValue(), y.getSIValue(), z.getSIValue()});
    }

    template <IsFloatingPoint Ts>
    void SceneNode<Ts>::setLocalRotation(Rotation<Ts> rotation)
    {
        this->local_rotation_ = rotation;
        this->updateTransforms();
    }

    template <IsFloatingPoint Ts>
    void SceneNode<Ts>::setLocalQuaternion(Quaternion<Ts> quaternion)
    {
        this->setLocalRotation(Rotation<Ts>(quaternion));
    }

    template <IsFloatingPoint Ts>
    void SceneNode<Ts>::setLocalShusterQuaternion(ShusterQuaternion<Ts> shuster_quaternion)
    {
        this->setLocalRotation(Rotation<Ts>(shuster_quaternion));
    }

    template <IsFloatingPoint Ts>
    void SceneNode<Ts>::setLocalAxisAngle(Vec3<Ts> axis, Degree angle)
    {
        this->setLocalRotation(Rotation<Ts>(axis, angle));
    }

    template <IsFloatingPoint Ts>
    void SceneNode<Ts>::setLocalEulerAngles(Degree angle1, Degree angle2, Degree angle3, std::string sequence)
    {
        this->setLocalRotation(Rotation<Ts>(angle1, angle2, angle3, sequence));
    }

    template <IsFloatingPoint Ts>
    void SceneNode<Ts>::setLocalScale(Vec3<Ts> scale)
    {
        this->local_scale_ = scale;
        this->updateTransforms();
    }

    template <IsFloatingPoint Ts>
    void SceneNode<Ts>::setLocalScale(double scale)
    {
        Ts s = static_cast<Ts>(scale);
        this->setLocalScale(Vec3<Ts>{s, s, s});
    }

    template <IsFloatingPoint Ts>
    void SceneNode<Ts>::setLocalScale(double scale_x, double scale_y, double scale_z)
    {
        Ts sx = static_cast<Ts>(scale_x);
        Ts sy = static_cast<Ts>(scale_y);
        Ts sz = static_cast<Ts>(scale_z);
        this->setLocalScale(Vec3<Ts>{sx, sy, sz});
    }


    // ================================ //
    // === Transformation Modifiers === //
    // ================================ //
    template <IsFloatingPoint Ts>
    void SceneNode<Ts>::translateBy(Vec3<Ts> position)
    {
        this->setLocalPosition(local_position_ + position);
    }

    template <IsFloatingPoint Ts>
    void SceneNode<Ts>::translateBy(Meter x, Meter y, Meter z)
    {
        this->translateBy(Vec3<Ts>{x.getSIValue(), y.getSIValue(), z.getSIValue()});
    }

    template <IsFloatingPoint Ts>
    void SceneNode<Ts>::rotateBy(Rotation<Ts> rotation)
    {
        this->setLocalRotation(rotation * local_rotation_);
    }

    template <IsFloatingPoint Ts>
    void SceneNode<Ts>::rotateBy(Quaternion<Ts> quaternion)
    {
        this->rotateBy(Rotation<Ts>(quaternion));
    }

    template <IsFloatingPoint Ts>
    void SceneNode<Ts>::rotateBy(ShusterQuaternion<Ts> shuster_quaternion)
    {
        this->rotateBy(Rotation<Ts>(shuster_quaternion));
    }

    template <IsFloatingPoint Ts>
    void SceneNode<Ts>::rotateBy(Vec3<Ts> axis, Degree angle)
    {
        this->rotateBy(Rotation<Ts>(axis, angle));
    }

    template <IsFloatingPoint Ts>
    void SceneNode<Ts>::rotateBy(Degree angle1, Degree angle2, Degree angle3, std::string sequence)
    {
        this->rotateBy(Rotation<Ts>(angle1, angle2, angle3, sequence));
    }

    template <IsFloatingPoint Ts>
    void SceneNode<Ts>::scaleBy(Vec3<Ts> scale)
    {
        this->setLocalScale(scale * local_scale_);
    }

    template <IsFloatingPoint Ts>
    void SceneNode<Ts>::scaleBy(double scale)
    {
        Ts s = static_cast<Ts>(scale);
        this->scaleBy(Vec3<Ts>{s, s, s});
    }

    template <IsFloatingPoint Ts>
    void SceneNode<Ts>::scaleBy(double scale_x, double scale_y, double scale_z)
    {
        Ts sx = static_cast<Ts>(scale_x);
        Ts sy = static_cast<Ts>(scale_y);
        Ts sz = static_cast<Ts>(scale_z);
        this->scaleBy(Vec3<Ts>{sx, sy, sz});
    }


    // ============================== //
    // === Transformation Getters === //
    // ============================== //
    template <IsFloatingPoint Ts>
    Mat4<Ts> SceneNode<Ts>::getLocalTransformation() const { return local_transformation_; }

    template <IsFloatingPoint Ts>
    Vec3<Ts> SceneNode<Ts>::getLocalPosition() const { return local_position_; }

    template <IsFloatingPoint Ts>
    Rotation<Ts> SceneNode<Ts>::getLocalRotation() const { return local_rotation_; }

    template <IsFloatingPoint Ts>
    Vec3<Ts> SceneNode<Ts>::getLocalScale() const { return local_scale_; }


    template <IsFloatingPoint Ts>
    Mat4<Ts> SceneNode<Ts>::getSceneTransformation() const { return scene_transformation_; }

    template <IsFloatingPoint Ts>
    Vec3<Ts> SceneNode<Ts>::getScenePosition() const { return scene_position_; }

    template <IsFloatingPoint Ts>
    Rotation<Ts> SceneNode<Ts>::getSceneRotation() const { return scene_rotation_; }

    template <IsFloatingPoint Ts>
    Vec3<Ts> SceneNode<Ts>::getSceneScale() const { return scene_scale_; }


    // ================================ //
    // === Private Member Functions === //
    // ================================ //
    template <IsFloatingPoint Ts>
    void SceneNode<Ts>::updateTransforms()
    {
        // Recall column-major ordering:
        Mat3<Ts> R = local_rotation_.getMatrix();

        local_transformation_[0][0] = R[0][0] * local_scale_[0];
        local_transformation_[0][1] = R[0][1] * local_scale_[0];
        local_transformation_[0][2] = R[0][2] * local_scale_[0];
        local_transformation_[0][3] = 0;

        local_transformation_[1][0] = R[1][0] * local_scale_[1];
        local_transformation_[1][1] = R[1][1] * local_scale_[1];
        local_transformation_[1][2] = R[1][2] * local_scale_[1];
        local_transformation_[1][3] = 0;

        local_transformation_[2][0] = R[2][0] * local_scale_[2];
        local_transformation_[2][1] = R[2][1] * local_scale_[2];
        local_transformation_[2][2] = R[2][2] * local_scale_[2];
        local_transformation_[2][3] = 0;

        local_transformation_[3][0] = local_position_[0];
        local_transformation_[3][1] = local_position_[1];
        local_transformation_[3][2] = local_position_[2];
        local_transformation_[3][3] = 1;

        this->updateSceneTransformation();

        // Call the virtual function to allow derived classes to take actions
        this->onTransformChanged();
    }

    template <IsFloatingPoint Ts>
    void SceneNode<Ts>::updateSceneTransformation()
    {
        Mat4<Ts> parent_transform = parent_->getSceneTransformation();
        (void)parent_transform;
        // TODO Implement
    }

    template <IsFloatingPoint Ts>
    void SceneNode<Ts>::setParent(GroupNode<Ts>* parent)
    {
        if (parent != nullptr) {
            parent_ = parent;
        }
        else {
            throw detail::FatalError("Parent GroupNode pointer is NULL");
        }
    }

    // Explicit Instantiations:
    template class SceneNode<float>;
    template class SceneNode<double>;
}