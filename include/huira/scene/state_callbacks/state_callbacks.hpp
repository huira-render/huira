#pragma once

#include "huira/core/rotation.hpp"
#include "huira/core/time.hpp"
#include "huira/core/transform.hpp"
#include "huira/core/types.hpp"

namespace huira {
    struct PositionCallback {
        virtual ~PositionCallback() = default;
        virtual void evaluate(const Time& epoch, const Rotation<double>& rotation, const Vec3<double>& angular_rate) = 0;

        Vec3<double> position{};
        Vec3<double> velocity{};
    };

    template <typename T>
    concept IsPositionCallback = std::derived_from<T, PositionCallback>;



    struct RotationCallback {
        virtual ~RotationCallback() = default;
        virtual void evaluate(const Time& epoch, const Vec3<double>& position, const Vec3<double>& velocity) = 0;

        Rotation<double> rotation{};
        Vec3<double> angular_velocity{};
    };

    template <typename T>
    concept IsRotationCallback = std::derived_from<T, RotationCallback>;



    struct StateCallback {
        virtual ~StateCallback() = default;
        virtual void evaluate(const Time& epoch) = 0;

        Transform<double> state{};
    };
    
    template <typename T>
    concept IsTransformCallback = std::derived_from<T, StateCallback>;
}
