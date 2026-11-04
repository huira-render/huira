#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers_string.hpp"
#include "cspice/SpiceUsr.h"
#include "huira/huira.hpp"
#include "tle_test_support.hpp"

using namespace huira;
using namespace huira::test;
using namespace huira::units::literals;
using Catch::Matchers::ContainsSubstring;

namespace {

/// A node's pose relative to the solar system barycenter, with the J2000 axes, at a time.
template <typename THandle>
Transform<double> pose_at(const THandle& node, const Time& time)
{
    return node.get()->get_apparent_transform(
        ObservationMode::TRUE_STATE, time, time, Transform<double>{});
}

} // namespace

TEST_CASE("A node placed by a TLE is where SGP4, the TEME frame and the Earth put it",
          "[scene][tle]")
{
    Scene<RGB> scene;
    FrameHandle<RGB> satellite = scene.teme_frame().new_subframe();
    TestEarthEphemeris::load();

    const SatRec satrec = iss();
    satellite.set_tle_position(satrec);
    CHECK(satellite.get()->get_position_mode() == TransformMode::POSITION_CALLBACK);

    for (const double hours : {0.0, 1.0, -1.0, 24.0, -72.0}) {
        INFO("Hours from epoch: " << hours);
        const Time time = satrec.epoch() + units::Second(hours * 3600.0);
        const Transform<double> pose = pose_at(satellite, time);
        const auto [position, velocity] = expected_state(satrec, time);
        CHECK(glm::length(pose.position - position) < 1e-3); // m
        CHECK(glm::length(pose.velocity - velocity) < 1e-6); // m/s
    }

    // What it holds moves with it: a meter along its x axis, which, its rotation left alone, is
    // the TEME frame's
    const FrameHandle<RGB> payload = satellite.new_subframe();
    payload.set_position(1_m, 0_m, 0_m);
    const Time time = satrec.epoch() + units::Second(600.0);
    SpiceDouble teme_to_j2000[3][3];
    spice::call_spice(pxform_c, "HUIRA_EARTH_TEME", "J2000", time.et(), teme_to_j2000);
    const Vec3<double> x_axis{teme_to_j2000[0][0], teme_to_j2000[1][0], teme_to_j2000[2][0]};
    CHECK(glm::length(pose_at(payload, time).position - pose_at(satellite, time).position -
                      x_axis) < 1e-4); // Each position is some 1.5e11 m from the barycenter
}

TEST_CASE("Only a child of the scene's TEME frame is placed by a TLE", "[scene][tle]")
{
    Scene<RGB> scene;
    const FrameHandle<RGB> teme = scene.teme_frame();

    CHECK_THROWS_WITH(scene.root.new_subframe().set_tle_position(iss()),
                      ContainsSubstring("child of the scene's TEME frame (Scene::teme_frame())"));
    CHECK_THROWS_WITH(teme.new_subframe().new_subframe().set_tle_position(iss()),
                      ContainsSubstring("child of the scene's TEME frame"));
    CHECK_THROWS_WITH(teme.set_tle_position(iss()), ContainsSubstring("its pose is fixed"));
    CHECK_NOTHROW(teme.new_subframe().set_tle_position(iss()));
}

TEST_CASE("A satellite places one node", "[scene][tle]")
{
    Scene<RGB> scene;
    const FrameHandle<RGB> teme = scene.teme_frame();
    const FrameHandle<RGB> first = teme.new_subframe();
    const FrameHandle<RGB> second = teme.new_subframe();

    first.set_tle_position(iss());
    CHECK_THROWS_WITH(second.set_tle_position(iss()),
                      ContainsSubstring("cannot be placed by satellite 25544's TLE: " +
                                        first.get()->get_info() + " is"));
    CHECK_THROWS(second.set_tle_position(iss_a_day_later())); // The same satellite

    // The failed claim left the second node as it was, and other satellites are free
    CHECK(second.get()->get_position_mode() == TransformMode::MANUAL_TRANSFORM);
    CHECK_NOTHROW(second.set_tle_position(decaying()));

    // Each scene has satellites of its own
    Scene<RGB> other;
    CHECK_NOTHROW(other.teme_frame().new_subframe().set_tle_position(iss()));
}

TEST_CASE("A node lets its satellite go when it is deleted or placed another way", "[scene][tle]")
{
    Scene<RGB> scene;
    const FrameHandle<RGB> teme = scene.teme_frame();
    const FrameHandle<RGB> holder = teme.new_subframe();
    const FrameHandle<RGB> next = teme.new_subframe();

    SECTION("Deleted")
    {
        holder.set_tle_position(iss());
        teme.delete_subframe(holder);
        CHECK_NOTHROW(next.set_tle_position(iss()));
    }

    SECTION("Deleted with the TEME frame")
    {
        holder.set_tle_position(iss());
        scene.root.delete_subframe(teme);
        CHECK_NOTHROW(scene.teme_frame().new_subframe().set_tle_position(iss()));
    }

    SECTION("Given a position by hand")
    {
        holder.set_tle_position(iss());
        holder.set_position(units::Meter(7e6), 0_m, 0_m);
        CHECK_NOTHROW(next.set_tle_position(iss()));
    }

    SECTION("Given another position source")
    {
        holder.set_tle_position(iss());
        holder.set_position_function([](const PoseContext&) { return Vec3<double>{7e6, 0, 0}; });
        CHECK_NOTHROW(next.set_tle_position(iss()));
    }

    SECTION("Placed by another satellite")
    {
        holder.set_tle_position(iss());
        holder.set_tle_position(decaying());
        CHECK_NOTHROW(next.set_tle_position(iss()));
        CHECK_THROWS(next.set_tle_position(decaying()));
    }
}

TEST_CASE("A newer element set for a node's satellite replaces the old one", "[scene][tle]")
{
    Scene<RGB> scene;
    const FrameHandle<RGB> satellite = scene.teme_frame().new_subframe();
    TestEarthEphemeris::load();

    satellite.set_tle_position(iss());
    const SatRec newer = iss_a_day_later();
    satellite.set_tle_position(newer);

    const Time time = newer.epoch() + units::Second(1800.0);
    CHECK(glm::length(pose_at(satellite, time).position - expected_state(newer, time).first) <
          1e-3);
    CHECK(glm::length(pose_at(satellite, time).position - expected_state(iss(), time).first) >
          1000.0); // The old element set puts it elsewhere
}

TEST_CASE("A node placed by a TLE can't be evaluated where SGP4 can't place it", "[scene][tle]")
{
    Scene<RGB> scene;
    const FrameHandle<RGB> satellite = scene.teme_frame().new_subframe();
    TestEarthEphemeris::load();

    const SatRec satrec = decaying();
    satellite.set_tle_position(satrec);
    CHECK_NOTHROW(pose_at(satellite, satrec.epoch()));
    CHECK_NOTHROW(pose_at(satellite, satrec.epoch() + units::Second(6.0 * 3600.0)));

    // Decayed, eight hours from its epoch
    const Time later = satrec.epoch() + units::Second(8.0 * 3600.0);
    CHECK_THROWS_WITH(pose_at(satellite, later),
                      ContainsSubstring("SGP4 can't place satellite 29141") &&
                          ContainsSubstring("0.3 days from its element set's epoch") &&
                          ContainsSubstring("SGP4 error 6"));
}
