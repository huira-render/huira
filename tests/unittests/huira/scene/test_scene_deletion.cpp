#include "catch2/catch_test_macros.hpp"
#include "huira/huira.hpp"

using namespace huira;
using namespace huira::units::literals;

// Nodes made through the API used to be impossible to delete: deleting one tried to remove it
// from the scene's registry of named nodes, which only holds nodes loaded from model files, and
// threw "does not exist in the scene" without deleting anything. Deleting an asset that had an
// instance in the scene graph failed the same way.

TEST_CASE("An instance made through the API can be deleted", "[scene][deletion]")
{
    Scene<RGB> scene;
    auto light = scene.new_sun_light();
    auto instance = scene.root.new_instance(light);
    REQUIRE(instance.valid());

    CHECK_NOTHROW(scene.root.delete_instance(instance));
    CHECK_FALSE(instance.valid());
    CHECK(light.valid()); // the asset stays
}

TEST_CASE("A subframe made through the API can be deleted, with everything in it",
          "[scene][deletion]")
{
    Scene<RGB> scene;
    auto frame = scene.root.new_subframe();
    auto inner = frame.new_subframe();
    auto instance = inner.new_instance(scene.add_primitive(scene.add_ellipsoid(1_m, 1_m, 1_m)));
    REQUIRE(instance.valid());

    CHECK_NOTHROW(scene.root.delete_subframe(frame));
    CHECK_FALSE(frame.valid());
    CHECK_FALSE(inner.valid());
    CHECK_FALSE(instance.valid());

    // Deleting it again is an error, but not the old one:
    CHECK_THROWS(scene.root.delete_subframe(frame));
}

TEST_CASE("An asset with instances in the scene graph can be deleted", "[scene][deletion]")
{
    Scene<RGB> scene;
    auto light = scene.new_sun_light();
    auto frame = scene.root.new_subframe();
    auto instance = frame.new_instance(light);

    CHECK_NOTHROW(scene.delete_light(light));
    CHECK_FALSE(light.valid());
    CHECK_FALSE(instance.valid());
    CHECK(frame.valid());

    auto primitive = scene.add_primitive(scene.add_ellipsoid(1_m, 1_m, 1_m));
    auto primitive_instance = scene.root.new_instance(primitive);
    CHECK_NOTHROW(scene.delete_primitive(primitive));
    CHECK_FALSE(primitive_instance.valid());
}
