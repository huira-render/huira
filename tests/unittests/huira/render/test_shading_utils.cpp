#include <cmath>
#include <vector>

#include "catch2/catch_test_macros.hpp"
#include "huira/huira.hpp"
#include "huira/render/shading_utils.hpp"

using namespace huira;

namespace {

/// The largest difference between two rotations' matrices.
float rotation_difference(const Rotation<float>& a, const Rotation<float>& b)
{
    const Mat3<float> difference = a.local_to_parent_matrix() - b.local_to_parent_matrix();
    float largest = 0.f;
    for (int column = 0; column < 3; ++column) {
        for (int row = 0; row < 3; ++row) {
            largest = std::max(largest, std::abs(difference[column][row]));
        }
    }
    return largest;
}

Rotation<float> about_z(double radians)
{
    return Rotation<float>::from_local_to_parent(Vec3<float>{0.f, 0.f, 1.f},
                                                 units::Radian(radians));
}

Transform<float> pose(const Vec3<float>& position, const Rotation<float>& rotation, float scale)
{
    Transform<float> transform;
    transform.position = position;
    transform.rotation = rotation;
    transform.scale = Vec3<float>{scale};
    return transform;
}

} // namespace

TEST_CASE("An instance's pose is interpolated between its samples as Embree interpolates it",
          "[render][motion_blur]")
{
    // Lights are placed with it too, so that a moving light is sampled where rays find it: its
    // rotation and scale were lost before.
    const std::vector<Transform<float>> poses{
        pose(Vec3<float>{0.f, 0.f, 0.f}, about_z(0.0), 1.f),
        pose(Vec3<float>{2.f, 4.f, 6.f}, about_z(0.5 * PI<double>()), 3.f)};

    // Halfway: position and scale halfway along straight lines, and rotation halfway along the
    // arc, an eighth of a turn:
    const Transform<float> halfway = instance_transform_at(poses, 0.5f);
    CHECK(glm::length(halfway.position - Vec3<float>{1.f, 2.f, 3.f}) < 1e-6f);
    CHECK(glm::length(halfway.scale - Vec3<float>{2.f}) < 1e-6f);
    CHECK(rotation_difference(halfway.rotation, about_z(0.25 * PI<double>())) < 1e-6f);

    // At the samples, the samples:
    CHECK(rotation_difference(instance_transform_at(poses, 1.f).rotation,
                              about_z(0.5 * PI<double>())) < 1e-6f);
    CHECK(glm::length(instance_transform_at(poses, 0.f).position) == 0.f);

    // Along the shorter arc: the same rotation given by its other quaternion turns no further.
    const Quaternion<float> q = about_z(0.5 * PI<double>()).local_to_parent_quaternion();
    const std::vector<Transform<float>> flipped{
        poses[0], pose(Vec3<float>{0.f}, Rotation<float>::from_local_to_parent(-q), 1.f)};
    CHECK(rotation_difference(instance_transform_at(flipped, 0.5f).rotation,
                              about_z(0.25 * PI<double>())) < 1e-6f);

    // With more samples, each interval between two is its own: a quarter turn per interval.
    const std::vector<Transform<float>> three{
        poses[0],
        pose(Vec3<float>{0.f}, about_z(0.5 * PI<double>()), 1.f),
        pose(Vec3<float>{0.f}, about_z(PI<double>()), 1.f)};
    CHECK(rotation_difference(instance_transform_at(three, 0.75f).rotation,
                              about_z(0.75 * PI<double>())) < 1e-6f);

    // One sample is held throughout:
    CHECK(rotation_difference(instance_transform_at({poses[1]}, 0.3f).rotation,
                              poses[1].rotation) == 0.f);
}
