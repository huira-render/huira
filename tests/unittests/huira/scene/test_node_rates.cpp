#include <cmath>
#include <numbers>

#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers_floating_point.hpp"
#include "huira/core/rotation.hpp"
#include "huira/core/spectral_bins.hpp"
#include "huira/core/time.hpp"
#include "huira/core/transform.hpp"
#include "huira/scene/node.hpp"
#include "huira/units/units.hpp"

using namespace huira;

namespace {
constexpr double kPi = std::numbers::pi;

void CHECK_VEC3_EQUAL(const Vec3<double>& a, const Vec3<double>& b, double epsilon = 1e-10)
{
    REQUIRE_THAT(a.x, Catch::Matchers::WithinAbs(b.x, epsilon));
    REQUIRE_THAT(a.y, Catch::Matchers::WithinAbs(b.y, epsilon));
    REQUIRE_THAT(a.z, Catch::Matchers::WithinAbs(b.z, epsilon));
}

/// Test fixture exposing the protected rotation propagation of Node.
class TestNode : public Node<RGB> {
  public:
    TestNode() : Node<RGB>(nullptr) {}

    Transform<double> propagate_rotation(double dt_seconds) const
    {
        return this->get_local_rotation_at_(Time::from_et(0.0), Time::from_et(dt_seconds), 0.0);
    }

    bool body_frame_rates() const { return this->body_frame_rates_; }
};

/// Camera-like initial attitude: body +z (the boresight, OpenCV convention)
/// pointing along the parent's +x axis (90 degree rotation about parent +y).
Rotation<double> boresight_along_parent_x()
{
    return Rotation<double>::from_local_to_parent(Vec3<double>{0, 1, 0}, units::Radian{kPi / 2});
}
} // namespace

TEST_CASE("Node - body-frame angular rates", "[node][rates][body]")
{
    // Regression test for the "lateral star streak" bug: a camera pointed away
    // from its parent's axes, commanded with a z-rate, must roll about its own
    // boresight -- not pan about the parent's z-axis.

    const double omega_z = 0.1; // rad/s about the body z-axis (boresight roll)

    TestNode node;
    node.set_rotation(boresight_along_parent_x());
    node.set_body_angular_velocity(units::RadiansPerSecond{0.0},
                                   units::RadiansPerSecond{0.0},
                                   units::RadiansPerSecond{omega_z});
    REQUIRE(node.body_frame_rates());

    SECTION("Boresight direction is invariant under a body-z roll")
    {
        for (double t : {0.0, 1.0, 5.0, 10.0, 25.0}) {
            Transform<double> local = node.propagate_rotation(t);
            Vec3<double> boresight = local.rotation * Vec3<double>{0, 0, 1};
            CHECK_VEC3_EQUAL(boresight, Vec3<double>{1, 0, 0});
        }
    }

    SECTION("Transverse body axes precess about the boresight")
    {
        // After rolling 90 degrees, the body x-axis (initially parent -z after
        // the 90 degree pitch) must have swept a quarter turn about the
        // boresight: q(t)*(1,0,0) = q0 * (cos a, sin a, 0) with a = omega*t.
        const double t = (kPi / 2) / omega_z;
        Transform<double> local = node.propagate_rotation(t);
        Vec3<double> body_x = local.rotation * Vec3<double>{1, 0, 0};
        CHECK_VEC3_EQUAL(body_x, Vec3<double>{0, 1, 0});
    }

    SECTION("Returned angular velocity is re-expressed in the parent frame")
    {
        // Transform::angular_velocity is parent-frame by convention. A body-z
        // rate on a boresight-along-parent-x attitude is a parent-x rate:
        // omega_parent = q0 * (0, 0, omega) = (omega, 0, 0).
        Transform<double> local = node.propagate_rotation(3.0);
        CHECK_VEC3_EQUAL(local.angular_velocity, Vec3<double>{omega_z, 0, 0});
    }

    SECTION("Parent-frame conversion also applies in the zero-elapsed-time branch")
    {
        Transform<double> local = node.propagate_rotation(0.0);
        CHECK_VEC3_EQUAL(local.angular_velocity, Vec3<double>{omega_z, 0, 0});
    }
}

TEST_CASE("Node - parent-frame angular rates", "[node][rates][parent]")
{
    const double omega_z = 0.1; // rad/s about the PARENT z-axis

    TestNode node;
    node.set_rotation(boresight_along_parent_x());
    node.set_angular_velocity(units::RadiansPerSecond{0.0},
                              units::RadiansPerSecond{0.0},
                              units::RadiansPerSecond{omega_z});
    REQUIRE_FALSE(node.body_frame_rates());

    SECTION("Boresight pans in the parent's xy-plane (the 'lateral streak' motion)")
    {
        for (double t : {0.0, 1.0, 5.0, 10.0}) {
            const double a = omega_z * t;
            Transform<double> local = node.propagate_rotation(t);
            Vec3<double> boresight = local.rotation * Vec3<double>{0, 0, 1};
            CHECK_VEC3_EQUAL(boresight, Vec3<double>{std::cos(a), std::sin(a), 0.0});
        }
    }

    SECTION("Returned angular velocity is passed through unchanged")
    {
        Transform<double> local = node.propagate_rotation(3.0);
        CHECK_VEC3_EQUAL(local.angular_velocity, Vec3<double>{0, 0, omega_z});
    }
}

TEST_CASE("Node - rate frame flag is reset by every angular velocity setter", "[node][rates]")
{
    const double omega_z = 0.1;

    TestNode node;
    node.set_rotation(boresight_along_parent_x());

    // Enter body-frame mode...
    node.set_body_angular_velocity(units::RadiansPerSecond{0.0},
                                   units::RadiansPerSecond{0.0},
                                   units::RadiansPerSecond{omega_z});
    REQUIRE(node.body_frame_rates());

    SECTION("Vec3 overload of set_angular_velocity switches back to parent-frame")
    {
        node.set_angular_velocity(Vec3<double>{0.0, 0.0, omega_z});
        REQUIRE_FALSE(node.body_frame_rates());

        // Behavioral check: the boresight now pans instead of rolling.
        const double t = 5.0;
        const double a = omega_z * t;
        Transform<double> local = node.propagate_rotation(t);
        Vec3<double> boresight = local.rotation * Vec3<double>{0, 0, 1};
        CHECK_VEC3_EQUAL(boresight, Vec3<double>{std::cos(a), std::sin(a), 0.0});
    }

    SECTION("Unit-typed overload of set_angular_velocity switches back to parent-frame")
    {
        node.set_angular_velocity(units::RadiansPerSecond{0.0},
                                  units::RadiansPerSecond{0.0},
                                  units::RadiansPerSecond{omega_z});
        REQUIRE_FALSE(node.body_frame_rates());
    }

    SECTION("set_manual_transform switches back to parent-frame")
    {
        Transform<double> manual{};
        manual.rotation = boresight_along_parent_x();
        manual.angular_velocity = Vec3<double>{0.0, 0.0, omega_z};
        node.set_manual_transform(manual);
        REQUIRE_FALSE(node.body_frame_rates());
    }
}
