#include <array>
#include <cmath>
#include <cstddef>
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

TEST_CASE("Transform - as a 4x4 matrix, it places points as it does", "[transform]")
{
    Transform<double> transform;
    transform.position = Vec3<double>{3.0, -2.0, 7.0};
    transform.rotation = Rotation<double>::from_local_to_parent(
        glm::normalize(Vec3<double>{1.0, 2.0, 3.0}), units::Radian(0.7));
    transform.scale = Vec3<double>{2.0, 0.5, 1.5};

    const Mat4<double> m = transform.to_matrix();
    for (const Vec3<double>& point :
         {Vec3<double>{0.0, 0.0, 0.0}, Vec3<double>{1.0, 0.0, 0.0}, Vec3<double>{-0.3, 2.0, 0.5}}) {
        const glm::dvec4 placed = m * glm::dvec4(point, 1.0);
        CHECK(placed.w == 1.0);
        CHECK_VEC3_EQUAL(Vec3<double>{placed}, transform.apply_to_point(point), 1e-12);
    }

    // The same matrix as Embree's, which is tested below, with a bottom row of (0, 0, 0, 1):
    const std::array<float, 12> embree = transform.to_embree_matrix();
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 3; ++row) {
            const std::size_t k = static_cast<std::size_t>(3 * column + row);
            CHECK(std::abs(m[column][row] - static_cast<double>(embree[k])) < 1e-5);
        }
        CHECK(m[column][3] == (column == 3 ? 1.0 : 0.0));
    }
}

TEST_CASE("Transform - as Embree's matrix, it places points as it does", "[transform]")
{
    Transform<double> transform;
    transform.position = Vec3<double>{3.0, -2.0, 7.0};
    transform.rotation = Rotation<double>::from_local_to_parent(
        glm::normalize(Vec3<double>{1.0, 2.0, 3.0}), units::Radian(0.7));
    transform.scale = Vec3<double>{2.0, 0.5, 1.5};

    // Column-major 3x4: three columns for the axes, then the translation.
    const std::array<float, 12> m = transform.to_embree_matrix();
    for (const Vec3<double>& point :
         {Vec3<double>{0.0, 0.0, 0.0}, Vec3<double>{1.0, 0.0, 0.0}, Vec3<double>{-0.3, 2.0, 0.5}}) {
        const Vec3<double> placed{m[0] * point.x + m[3] * point.y + m[6] * point.z + m[9],
                                  m[1] * point.x + m[4] * point.y + m[7] * point.z + m[10],
                                  m[2] * point.x + m[5] * point.y + m[8] * point.z + m[11]};
        CHECK_VEC3_EQUAL(placed, transform.apply_to_point(point), 1e-5);
    }
}
