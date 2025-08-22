#include "huira/scene/scene_node.hpp"

#include <unordered_map>

#include "glm/glm.hpp"

#include "huira/math/rotation.hpp"
#include "huira/math/types.hpp"
#include "huira/units/units.hpp"

#include "huira/scene/derived_nodes.hpp"

#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/diagnostics/logging.hpp"
#include "huira/detail/validate.hpp"

namespace huira {
    // ============================== //
    // === Transformation Setters === //
    // ============================== //
    template <IsFloatingPoint Ts>
    void SceneNode<Ts>::setLocalPosition(Vec3<Ts> position)
    {
        detail::validateReal(position, "Position");
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
        detail::validateStrictlyPositive(scale, "Scale");
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

	template <IsFloatingPoint Ts>
    void SceneNode<Ts>::setLocalTransformation(Mat4<Ts> transformation)
    {
        detail::validateReal(transformation, "Transformation");
        local_transformation_ = transformation;

		this->getTransformationComponents(local_transformation_,
            local_position_, local_rotation_, local_scale_
        );

        this->updateTransforms();
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


    template <IsFloatingPoint Ts>
    Mat4<Ts> SceneNode<Ts>::getModelMatrix() const { return scene_transformation_; }

    template <IsFloatingPoint Ts>
    Mat3<Ts> SceneNode<Ts>::getNormalMatrix() const { return scene_rotation_.getMatrix(); }


    // ================================ //
    // === Private Member Functions === //
    // ================================ //
    template <IsFloatingPoint Ts>
    void SceneNode<Ts>::updateTransforms()
    {
        local_transformation_ = constructTransformation(local_position_, local_rotation_, local_scale_);

        this->updateSceneTransformation();

        // Call the virtual function to allow derived classes to take actions
        this->onTransformChanged();
    }

    template <IsFloatingPoint Ts>
    void SceneNode<Ts>::updateSceneTransformation()
    {
        if (parent_ == nullptr) {
			throw detail::FatalError("Parent GroupNode pointer is NULL");
        }
        Mat4<Ts> parent_transformation = parent_->getSceneTransformation();
        
        scene_transformation_ = parent_transformation * local_transformation_;

        getTransformationComponents(
            scene_transformation_,
            scene_position_, scene_rotation_, scene_scale_
        );
    }

    template <IsFloatingPoint Ts>
    Mat4<Ts> SceneNode<Ts>::constructTransformation(const Vec3<Ts>& position, const Rotation<Ts>& rotation, const Vec3<Ts>& scale) const
    {
        // Recall column-major ordering:
        Mat3<Ts> R = rotation.getMatrix();

        Mat4<Ts> transformation;
        transformation[0][0] = R[0][0] * scale[0];
        transformation[0][1] = R[0][1] * scale[0];
        transformation[0][2] = R[0][2] * scale[0];
        transformation[0][3] = 0;

        transformation[1][0] = R[1][0] * scale[1];
        transformation[1][1] = R[1][1] * scale[1];
        transformation[1][2] = R[1][2] * scale[1];
        transformation[1][3] = 0;

        transformation[2][0] = R[2][0] * scale[2];
        transformation[2][1] = R[2][1] * scale[2];
        transformation[2][2] = R[2][2] * scale[2];
        transformation[2][3] = 0;

        transformation[3][0] = position[0];
        transformation[3][1] = position[1];
        transformation[3][2] = position[2];
        transformation[3][3] = 1;

        return transformation;
    }

    template <IsFloatingPoint Ts>
    void SceneNode<Ts>::getTransformationComponents(const Mat4<Ts>& T, Vec3<Ts>& position, Rotation<Ts>& rotation, Vec3<Ts>& scale) const
    {
        position = Vec3<Ts>(T[3][0], T[3][1], T[3][2]);

        // Extract scale from the length of the first three columns:
        Ts sx = glm::length(Vec3<Ts>(T[0][0], T[0][1], T[0][2]));
        Ts sy = glm::length(Vec3<Ts>(T[1][0], T[1][1], T[1][2]));
        Ts sz = glm::length(Vec3<Ts>(T[2][0], T[2][1], T[2][2]));
        scale = Vec3<Ts>(sx, sy, sz);

        Mat3<Ts> R;
        R[0] = Vec3<Ts>(T[0][0], T[0][1], T[0][2]) / sx;
        R[1] = Vec3<Ts>(T[1][0], T[1][1], T[1][2]) / sy;
        R[2] = Vec3<Ts>(T[2][0], T[2][1], T[2][2]) / sz;

        rotation = Rotation<Ts>(R);
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