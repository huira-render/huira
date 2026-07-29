#include <cmath>
#include <numbers>

#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers_floating_point.hpp"
#include "huira/core/rotation.hpp"
#include "huira/core/transform.hpp"
#include "huira/units/units.hpp"

using namespace huira;

namespace {
constexpr double kPi = std::numbers::pi;

void CHECK_VEC3_EQUAL(const Vec3<double>& a, const Vec3<double>& b, double epsilon = 1e-12)
{
    REQUIRE_THAT(a.x, Catch::Matchers::WithinAbs(b.x, epsilon));
    REQUIRE_THAT(a.y, Catch::Matchers::WithinAbs(b.y, epsilon));
    REQUIRE_THAT(a.z, Catch::Matchers::WithinAbs(b.z, epsilon));
}
} // namespace

TEST_CASE("Transform - angular velocity frame conventions", "[transform][rates]")
{
    // Transform::velocity and Transform::angular_velocity are, by convention,
    // expressed in the PARENT frame's axes. These tests pin that convention
    // down so that it cannot drift silently (Node::get_local_rotation_at_
    // relies on it when converting body-frame rates).

    SECTION("operator* rotates the child's angular velocity by the parent rotation only")
    {
        Transform<double> parent{};
        parent.rotation =
            Rotation<double>::from_local_to_parent(Vec3<double>{0, 0, 1}, units::Radian{kPi / 2});
        parent.angular_velocity = Vec3<double>{0, 0, 1};

        Transform<double> child{};
        // A child rotation must NOT affect how the child's angular velocity is
        // composed: the vector is expressed in the child's parent frame.
        child.rotation =
            Rotation<double>::from_local_to_parent(Vec3<double>{1, 0, 0}, units::Radian{kPi / 3});
        child.angular_velocity = Vec3<double>{0, 1, 0};

        Transform<double> composed = parent * child;

        // omega = omega_p + R_p * omega_c = (0,0,1) + R_z(90deg)*(0,1,0) = (-1,0,1)
        CHECK_VEC3_EQUAL(composed.angular_velocity, Vec3<double>{-1, 0, 1});
    }

    SECTION("operator* adds the omega-cross-r term to the child's velocity")
    {
        Transform<double> parent{};
        parent.rotation =
            Rotation<double>::from_local_to_parent(Vec3<double>{0, 0, 1}, units::Radian{kPi / 2});
        parent.angular_velocity = Vec3<double>{0, 0, 1};

        Transform<double> child{};
        child.position = Vec3<double>{1, 0, 0};

        Transform<double> composed = parent * child;

        // v = v_p + R_p * v_c + omega_p x (R_p * p_c)
        //   = 0 + 0 + (0,0,1) x (0,1,0) = (-1,0,0)
        CHECK_VEC3_EQUAL(composed.velocity, Vec3<double>{-1, 0, 0});
    }

    SECTION("velocity_of_point computes v + omega cross r")
    {
        Transform<double> frame{};
        frame.angular_velocity = Vec3<double>{0, 0, 1};

        Vec3<double> vel = frame.velocity_of_point(Vec3<double>{1, 0, 0});
        CHECK_VEC3_EQUAL(vel, Vec3<double>{0, 1, 0});
    }

    SECTION("apply_to_angular_velocity rotates then adds")
    {
        Transform<double> frame{};
        frame.rotation =
            Rotation<double>::from_local_to_parent(Vec3<double>{0, 0, 1}, units::Radian{kPi / 2});
        frame.angular_velocity = Vec3<double>{0, 0, 2};

        Vec3<double> result = frame.apply_to_angular_velocity(Vec3<double>{1, 0, 0});
        // R_z(90deg)*(1,0,0) + (0,0,2) = (0,1,2)
        CHECK_VEC3_EQUAL(result, Vec3<double>{0, 1, 2});
    }

    SECTION("T * T.inverse() has zero residual velocity and angular velocity")
    {
        Transform<double> t{};
        t.position = Vec3<double>{1, 2, 3};
        t.rotation =
            Rotation<double>::from_local_to_parent(Vec3<double>{0, 1, 0}, units::Radian{kPi / 4});
        t.velocity = Vec3<double>{0.5, -1.0, 2.0};
        t.angular_velocity = Vec3<double>{0, 0, 2};

        Transform<double> identity = t * t.inverse();

        CHECK_VEC3_EQUAL(identity.position, Vec3<double>{0, 0, 0}, 1e-10);
        CHECK_VEC3_EQUAL(identity.angular_velocity, Vec3<double>{0, 0, 0}, 1e-10);
        // Residual velocity picks up both the rotated inverse velocity and the
        // omega-cross-r term; both must cancel for a rigid frame.
        CHECK_VEC3_EQUAL(identity.velocity, Vec3<double>{0, 0, 0}, 1e-10);

        Vec3<double> x = identity.rotation * Vec3<double>{1, 0, 0};
        CHECK_VEC3_EQUAL(x, Vec3<double>{1, 0, 0}, 1e-10);
    }
}
