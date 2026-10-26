#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers_string.hpp"
#include "huira/huira.hpp"

using namespace huira;
using namespace huira::units::literals;
using Catch::Matchers::ContainsSubstring;

namespace {
/// A pose source that the root frame must refuse.
struct FixedState final : StateCallback {
    [[nodiscard]] Transform<double> evaluate(const PoseContext&) const override { return {}; }
};

/// The root is placed by SPICE at the solar system barycenter, with the J2000 axes.
template <typename THandle>
void check_root_unchanged(const THandle& root)
{
    CHECK(root.get_spice_origin() == "SOLAR SYSTEM BARYCENTER");
    CHECK(root.get_spice_frame() == "J2000");
    CHECK(root.get()->get_position_mode() == TransformMode::SPICE_TRANSFORM);
    CHECK(root.get()->get_rotation_mode() == TransformMode::SPICE_TRANSFORM);
}
} // namespace

TEST_CASE("A scene's root is the solar system barycenter, with the J2000 axes", "[scene][root]")
{
    Scene<RGB> scene;
    CHECK(scene.root.type() == "RootFrameNode");
    check_root_unchanged(scene.root);
}

TEST_CASE("The root frame's pose is fixed, however it is reached", "[scene][root]")
{
    Scene<RGB> scene;
    const FrameHandle<RGB> child = scene.root.new_subframe();

    // A child's get_parent() is a handle with every setter that RootFrameHandle leaves out
    const NodeHandle<RGB, Node<RGB>> root = child.get_parent();
    CHECK(root.id() == scene.root.id());
    CHECK_THROWS_WITH(root.set_position(1_m, 0_m, 0_m), ContainsSubstring("its pose is fixed"));
    CHECK_THROWS(root.set_rotation(Rotation<double>{}));
    CHECK_THROWS(root.set_scale(2.0));
    CHECK_THROWS(root.set_velocity(
        units::MetersPerSecond(1.0), units::MetersPerSecond(0.0), units::MetersPerSecond(0.0)));
    CHECK_THROWS(root.set_angular_velocity(
        units::RadiansPerSecond(1.0), units::RadiansPerSecond(0.0), units::RadiansPerSecond(0.0)));
    CHECK_THROWS(root.set_spice("MARS", "IAU_MARS"));
    CHECK_THROWS(root.set_spice_origin("MARS"));
    CHECK_THROWS(root.set_spice_frame("IAU_MARS"));
    CHECK_THROWS(root.set_position_function([](const PoseContext&) { return Vec3<double>{}; }));
    CHECK_THROWS(root.set_rotation_function([](const PoseContext&) { return Rotation<double>{}; }));
    CHECK_THROWS(root.get()->set_manual_transform(Transform<double>{}));
    CHECK_THROWS(root.get()->set_custom_state_callback<FixedState>());

    // As a frame, too
    const NodeHandle<RGB, FrameNode<RGB>> root_frame = child.get_parent_as<FrameNode<RGB>>();
    CHECK_THROWS(root_frame.set_position(1_m, 0_m, 0_m));
    CHECK_THROWS(root_frame.set_spice("MARS", "IAU_MARS"));

    // Nothing changed:
    check_root_unchanged(scene.root);

    // What it holds is placed freely, and it can still be hidden:
    CHECK_NOTHROW(child.set_position(units::Meter(7000e3), 0_m, 0_m));
    CHECK_NOTHROW(scene.root.set_visible(false));
}
