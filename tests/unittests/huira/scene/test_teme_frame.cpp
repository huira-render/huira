#include <array>
#include <cmath>

#include "catch2/catch_test_macros.hpp"
#include "cspice/SpiceUsr.h"
#include "huira/huira.hpp"

using namespace huira;
using namespace huira::units::literals;

namespace {
/// A TEME position, and where Vallado's own teme2eci puts it in J2000 at a time.
struct TemeReference {
    double et;              ///< Seconds past J2000, TDB.
    Vec3<double> r_j2000_m; ///< The TEME position below, in J2000, in meters.
};

/// The TEME position of Vallado's Example 3-15, in meters.
const Vec3<double> R_TEME_M{5094.18016210e3, 6127.64465950e3, 6380.34453270e3};

/// From teme2eci in valladopy (github.com/CelesTrak/fundamentals-of-astrodynamics), with no
/// EOP corrections (ddpsi = ddeps = 0), as SGP4 users apply it. Times from naif0012.tls.
const std::array<TemeReference, 4> TEME_REFERENCES{{
    // 1985-07-01 00:00:00 UTC
    {-457703944.815895, Vec3<double>{5065.1380293122e3, 6144.2980826396e3, 6387.4466548593e3}},
    // 2004-04-06 07:51:28.386009 UTC, Vallado's Example 3-15
    {134509952.571664, Vec3<double>{5102.5096549005e3, 6123.0114742867e3, 6378.1362999223e3}},
    // 2026-10-22 00:00:00 UTC
    {845899269.182408, Vec3<double>{5147.5217171433e3, 6097.1961094394e3, 6366.6880501217e3}},
    // 2049-12-31 12:00:00 UTC
    {1577836869.183899, Vec3<double>{5193.5002131453e3, 6069.9587392600e3, 6355.3641432044e3}},
}};

/// A pose source that the TEME frame must refuse.
struct FixedState final : StateCallback {
    [[nodiscard]] Transform<double> evaluate(const PoseContext&) const override { return {}; }
};
} // namespace

TEST_CASE("A scene's TEME frame is a child of the root, placed at the Earth by SPICE",
          "[scene][teme]")
{
    Scene<RGB> scene;
    const FrameHandle<RGB> teme = scene.teme_frame();

    CHECK(teme.type() == "TemeFrameNode");
    CHECK(teme.get_spice_origin() == "EARTH");
    CHECK(teme.get_spice_frame() == "HUIRA_EARTH_TEME");
    CHECK(teme.get_parent().id() == scene.root.id());

    // The same frame every time:
    CHECK(scene.teme_frame().id() == teme.id());
}

TEST_CASE("The TEME frame's axes are SGP4's, as Vallado's teme2eci gives them", "[scene][teme]")
{
    Scene<RGB> scene;
    scene.teme_frame(); // loads the frame kernel

    // The kernel reproduces teme2eci to within 0.2 mas (0.14 mas at these times). 1 mas at this
    // position's 10,250 km is 5 cm.
    constexpr double tolerance_m = 0.05;
    for (const TemeReference& reference : TEME_REFERENCES) {
        const Rotation<double> teme_to_j2000 =
            spice::pxform<double>("J2000", "HUIRA_EARTH_TEME", Time::from_et(reference.et));
        const Vec3<double> r_j2000_m = teme_to_j2000 * R_TEME_M;
        INFO("ET " << reference.et);
        CHECK(glm::length(r_j2000_m - reference.r_j2000_m) < tolerance_m);
    }
}

TEST_CASE("The TEME frame's pose is fixed", "[scene][teme]")
{
    Scene<RGB> scene;
    const FrameHandle<RGB> teme = scene.teme_frame();

    CHECK_THROWS(teme.set_position(1_m, 0_m, 0_m));
    CHECK_THROWS(teme.set_rotation(Rotation<double>{}));
    CHECK_THROWS(teme.set_scale(2.0));
    CHECK_THROWS(teme.set_velocity(
        units::MetersPerSecond(1.0), units::MetersPerSecond(0.0), units::MetersPerSecond(0.0)));
    CHECK_THROWS(teme.set_angular_velocity(
        units::RadiansPerSecond(1.0), units::RadiansPerSecond(0.0), units::RadiansPerSecond(0.0)));
    CHECK_THROWS(teme.set_spice("MARS", "IAU_MARS"));
    CHECK_THROWS(teme.set_spice_origin("MARS"));
    CHECK_THROWS(teme.set_spice_frame("J2000"));
    CHECK_THROWS(teme.set_position_function([](const PoseContext&) { return Vec3<double>{}; }));
    CHECK_THROWS(teme.set_rotation_function([](const PoseContext&) { return Rotation<double>{}; }));
    CHECK_THROWS(teme.get()->set_manual_transform(Transform<double>{}));
    CHECK_THROWS(teme.get()->set_custom_state_callback<FixedState>());

    // Nothing changed:
    CHECK(teme.get_spice_origin() == "EARTH");
    CHECK(teme.get_spice_frame() == "HUIRA_EARTH_TEME");
    CHECK(teme.get()->get_position_mode() == TransformMode::SPICE_TRANSFORM);
    CHECK(teme.get()->get_rotation_mode() == TransformMode::SPICE_TRANSFORM);

    // What it holds is placed freely, and it can still be hidden:
    const FrameHandle<RGB> child = teme.new_subframe();
    CHECK_NOTHROW(child.set_position(units::Meter(7000e3), 0_m, 0_m));
    CHECK_NOTHROW(teme.set_visible(false));
}

TEST_CASE("Deleting the TEME frame, then asking for it again, makes a new one", "[scene][teme]")
{
    Scene<RGB> scene;
    const FrameHandle<RGB> first = scene.teme_frame();
    const auto first_id = first.id();

    scene.root.delete_subframe(first);
    CHECK_FALSE(first.valid());

    const FrameHandle<RGB> second = scene.teme_frame();
    CHECK(second.valid());
    CHECK(second.id() != first_id);
    CHECK(second.get_parent().id() == scene.root.id());
}

TEST_CASE("The TEME frame kernel is loaded once, however many scenes ask for the frame",
          "[scene][teme]")
{
    Scene<RGB> scene;
    scene.teme_frame();

    SpiceInt frame_id = 0;
    namfrm_c("HUIRA_EARTH_TEME", &frame_id);
    CHECK(frame_id == 1489003);

    SpiceInt kernels_loaded = 0;
    ktotal_c("TEXT", &kernels_loaded);

    scene.teme_frame();
    Scene<RGB> another_scene;
    another_scene.teme_frame();

    SpiceInt kernels_loaded_after = 0;
    ktotal_c("TEXT", &kernels_loaded_after);
    CHECK(kernels_loaded_after == kernels_loaded);
}
