#pragma once

#include <vector>
#include <memory>
#include <unordered_map>
#include <string>

#include "huira/math/rotation.hpp"
#include "huira/math/types.hpp"
#include "huira/units/units.hpp"

#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/diagnostics/exceptions.hpp"
#include "huira/detail/platform/huira_export.hpp"

namespace huira {
    enum class NodeType { GroupNode, Camera, Instance, Light };

    // Forward Declaration:
    template <IsFloatingPoint Ts>
    class GroupNode;

	template <IsFloatingPoint Ts>
	class HUIRA_EXPORT SceneNode {
    public:
        virtual NodeType getType() const = 0;
        virtual ~SceneNode() = default;

        // Transformation setting member functions
        void setLocalPosition(Vec3<Ts> position);
        void setLocalPosition(Meter x, Meter y, Meter z);

        void setLocalRotation(Rotation<Ts> rotation);
        void setLocalQuaternion(Quaternion<Ts> quaternion);
        void setLocalShusterQuaternion(ShusterQuaternion<Ts> shuster_quaternion);
        void setLocalAxisAngle(Vec3<Ts> axis, Degree angle);
        void setLocalEulerAngles(Degree angle1, Degree angle2, Degree angle3, std::string sequence = "XYZ");

        void setLocalScale(Vec3<Ts> scale);
        void setLocalScale(double scale);
        void setLocalScale(double scale_x, double scale_y, double scale_z);

        // Transformation modifying member functions:
        void translateBy(Vec3<Ts> position);
        void translateBy(Meter x, Meter y, Meter z);

        void rotateBy(Rotation<Ts> rotation);
        void rotateBy(Quaternion<Ts> quaternion);
        void rotateBy(ShusterQuaternion<Ts> shuster_quaternion);
        void rotateBy(Vec3<Ts> axis, Degree angle);
        void rotateBy(Degree angle1, Degree angle2, Degree angle3, std::string sequence = "XYZ");

        void scaleBy(Vec3<Ts> scale);
        void scaleBy(double scale);
        void scaleBy(double scale_x, double scale_y, double scale_z);
        

        // Getter methods:
        Mat4<Ts> getLocalTransformation() const;
        Vec3<Ts> getLocalPosition() const;
        Rotation<Ts> getLocalRotation() const;
        Vec3<Ts> getLocalScale() const;

        Mat4<Ts> getSceneTransformation() const;
        Vec3<Ts> getScenePosition() const;
        Rotation<Ts> getSceneRotation() const;
        Vec3<Ts> getSceneScale() const;

        // Type-safe casting helpers
        template<typename T> bool is() const { return getType() == T::TYPE; }

        template<typename T> 
        T* as()
        {
            if (is<T>()) {
                return static_cast<T*>(this);
            }
            else {
                throw detail::FatalError("Cannot convert to the specified type");
            }
        }

        template<typename T> 
        const T* as() const
        {
            if (is<T>()) {
                return static_cast<const T*>(this);
            }
            else {
                throw detail::FatalError("Cannot convert to the specified type");
            }
        }

    protected:
        virtual void onTransformChanged() {}

    private:
        Mat4<Ts> scene_transformation_{ 1 };
        Vec3<Ts> scene_position_{ 0,0,0 };
        Rotation<Ts> scene_rotation_{};
        Vec3<Ts> scene_scale_{ 1,1,1 };

        Mat4<Ts> local_transformation_{ 1 };
        Vec3<Ts> local_position_{ 0,0,0 };
        Rotation<Ts> local_rotation_{};
        Vec3<Ts> local_scale_{ 1,1,1 };

        void updateTransforms();
        void updateSceneTransformation();

        GroupNode<Ts>* parent_;
        void setParent(GroupNode<Ts>* parent);

        friend class GroupNode<Ts>;
	};

    // Declare Explicit Instantiations:
    extern template class SceneNode<float>;
    extern template class SceneNode<double>;
}