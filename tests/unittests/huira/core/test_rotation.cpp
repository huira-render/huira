#include <cmath>
#include <numbers>

#include "catch2/catch_test_macros.hpp"
#include "catch2/catch_template_test_macros.hpp"
#include "catch2/catch_approx.hpp"
#include "catch2/matchers/catch_matchers_floating_point.hpp"

#include "huira/core/rotation.hpp"

using namespace huira;
using Catch::Approx;

// Helper to check if two matrices are equal (fuzzy comparison)
template<typename T>
void CHECK_MAT3_EQUAL(const Mat3<T>& a, const Mat3<T>& b, double epsilon = 1e-10) {
    for (int col = 0; col < 3; ++col) {
        for (int row = 0; row < 3; ++row) {
            REQUIRE_THAT(a[col][row], Catch::Matchers::WithinAbs(b[col][row], epsilon));
        }
    }
}

// Test both float and double instantiations
TEMPLATE_TEST_CASE("Rotation - Template Instantiations", "[rotation][template]", float, double) {
    using RotationType = Rotation<TestType>;
    using Vec3Type = Vec3<TestType>;
    using Mat3Type = Mat3<TestType>;

    SECTION("Default constructor creates identity rotation") {
        // Default corresponds to "No Rotation" (Local aligned with Parent)
        RotationType rot;

        Vec3Type x_axis{ 1, 0, 0 };
        Vec3Type result = rot * x_axis; // Active transform

        REQUIRE_THAT(result.x, Catch::Matchers::WithinAbs(x_axis.x, 1e-6));
        REQUIRE_THAT(result.y, Catch::Matchers::WithinAbs(x_axis.y, 1e-6));
        REQUIRE_THAT(result.z, Catch::Matchers::WithinAbs(x_axis.z, 1e-6));
    }

    SECTION("Matrix accessor returns expected format") {
        RotationType rot;
        // Updated API: explicit getter
        Mat3Type matrix = rot.local_to_parent_matrix();

        // Identity matrix checks
        REQUIRE_THAT(matrix[0][0], Catch::Matchers::WithinAbs(TestType(1), 1e-6));
        REQUIRE_THAT(matrix[1][1], Catch::Matchers::WithinAbs(TestType(1), 1e-6));
        REQUIRE_THAT(matrix[2][2], Catch::Matchers::WithinAbs(TestType(1), 1e-6));
    }
}

TEST_CASE("Rotation - Construction Methods", "[rotation][constructor]") {
    using RotationType = Rotation_d;
    using Vec3Type = Vec3<double>;

    SECTION("Axis-angle constructor (Active/Local-to-Parent)") {
        Vec3Type z_axis{ 0.0, 0.0, 1.0 };
        units::Degree angle_90{ 90.0 };

        // Updated API: Explicit Factory
        RotationType rot = RotationType::from_local_to_parent(z_axis, angle_90);

        // 90-degree active rotation around Z maps X -> Y
        Vec3Type x_axis{ 1.0, 0.0, 0.0 };
        Vec3Type rotated = rot * x_axis;

        REQUIRE_THAT(rotated.x, Catch::Matchers::WithinAbs(0.0, 1e-10));
        REQUIRE_THAT(rotated.y, Catch::Matchers::WithinAbs(1.0, 1e-10));
        REQUIRE_THAT(rotated.z, Catch::Matchers::WithinAbs(0.0, 1e-10));
    }

    SECTION("Euler angle constructor - Extrinsic (Blender Style)") {
        units::Degree angle_90{ 90.0 };
        units::Degree zero{ 0.0 };

        // Updated API: Explicit Extrinsic
        RotationType rot_x = RotationType::extrinsic_euler_angles(angle_90, zero, zero, "XYZ");

        Vec3Type y_axis{ 0.0, 1.0, 0.0 };
        Vec3Type rotated_y = rot_x * y_axis;

        // 90deg X rotation maps Y -> Z
        REQUIRE_THAT(rotated_y.x, Catch::Matchers::WithinAbs(0.0, 1e-10));
        REQUIRE_THAT(rotated_y.y, Catch::Matchers::WithinAbs(0.0, 1e-10));
        REQUIRE_THAT(rotated_y.z, Catch::Matchers::WithinAbs(1.0, 1e-10));
    }

    SECTION("Parent-to-Local (Passive) Construction") {
        // Create a rotation that transforms Parent -> Local by 90 deg around Z.
        // This means the Frame rotates +90. 
        // A vector fixed in space at X=(1,0,0) will appear at Y=(-1,0,0) in the new frame? 
        // Wait: Passive rotation of frame by +90 around Z:
        // Global X axis aligns with Local Y axis? No.
        // Let's use the property: Passive(Angle) == Active(-Angle).

        Vec3Type axis{ 0,0,1 };
        units::Degree angle{ 90.0 };

        // Construct from passive data
        RotationType passive_rot = RotationType::from_parent_to_local(axis, angle);

        // Construct equivalent active data (-90)
        RotationType active_rot = RotationType::from_local_to_parent(axis, -angle);

        CHECK_MAT3_EQUAL(passive_rot.local_to_parent_matrix(), active_rot.local_to_parent_matrix());
    }
}

TEST_CASE("Rotation - Operations", "[rotation][operations]") {
    using RotationType = Rotation_d;
    using Vec3Type = Vec3<double>;

    SECTION("Multiplication is associative") {
        // Updated API: Explicit Factories
        RotationType rot1 = RotationType::from_local_to_parent({ 1.0, 0.0, 0.0 }, units::Degree{ 30.0 });
        RotationType rot2 = RotationType::from_local_to_parent({ 0.0, 1.0, 0.0 }, units::Degree{ 45.0 });
        RotationType rot3 = RotationType::from_local_to_parent({ 0.0, 0.0, 1.0 }, units::Degree{ 60.0 });

        RotationType left_assoc = (rot1 * rot2) * rot3;
        RotationType right_assoc = rot1 * (rot2 * rot3);

        Vec3Type test_vec{ 1.0, 1.0, 1.0 };
        Vec3Type left_result = left_assoc * test_vec;
        Vec3Type right_result = right_assoc * test_vec;

        REQUIRE_THAT(left_result.x, Catch::Matchers::WithinAbs(right_result.x, 1e-12));
        REQUIRE_THAT(left_result.y, Catch::Matchers::WithinAbs(right_result.y, 1e-12));
        REQUIRE_THAT(left_result.z, Catch::Matchers::WithinAbs(right_result.z, 1e-12));
    }

    SECTION("Inverse operation") {
        Vec3Type axis{ 1.0, 1.0, 1.0 };
        units::Degree angle{ 60.0 };

        RotationType rot = RotationType::from_local_to_parent(axis, angle);
        RotationType inv_rot = rot.inverse();

        RotationType identity = rot * inv_rot;
        Vec3Type test_vec{ 2.0, -1.0, 3.0 };
        Vec3Type result = identity * test_vec; // Should match test_vec

        REQUIRE_THAT(result.x, Catch::Matchers::WithinAbs(test_vec.x, 1e-12));
        REQUIRE_THAT(result.y, Catch::Matchers::WithinAbs(test_vec.y, 1e-12));
        REQUIRE_THAT(result.z, Catch::Matchers::WithinAbs(test_vec.z, 1e-12));
    }
}

TEST_CASE("Rotation - Properties and Invariants", "[rotation][properties]") {
    using RotationType = Rotation_d;
    using Vec3Type = Vec3<double>;

    SECTION("Axis extraction methods") {
        // Rotate 90 degrees around Z
        // Old X axis (1,0,0) should become Y axis (0,1,0)
        RotationType rot = RotationType::from_local_to_parent({ 0.0, 0.0, 1.0 }, units::Degree{ 90.0 });

        // Updated API: get_x_axis -> local_x_axis
        // The columns of the matrix represent the Local Axes in Parent Space
        Vec3Type x_axis_in_parent = rot.x_axis();
        Vec3Type y_axis_in_parent = rot.y_axis();
        //Vec3Type z_axis_in_parent = rot.z_axis();

        // Local X should now be pointing along Parent Y (0,1,0)
        REQUIRE_THAT(x_axis_in_parent.x, Catch::Matchers::WithinAbs(0.0, 1e-12));
        REQUIRE_THAT(x_axis_in_parent.y, Catch::Matchers::WithinAbs(1.0, 1e-12));

        // Local Y should now be pointing along Parent -X (-1,0,0)
        REQUIRE_THAT(y_axis_in_parent.x, Catch::Matchers::WithinAbs(-1.0, 1e-12));
        REQUIRE_THAT(y_axis_in_parent.y, Catch::Matchers::WithinAbs(0.0, 1e-12));

        // Check orthogonality
        REQUIRE_THAT(glm::dot(x_axis_in_parent, y_axis_in_parent), Catch::Matchers::WithinAbs(0.0, 1e-12));
    }
}

TEST_CASE("Rotation - Quaternion Conversions", "[rotation][quaternion]") {
    using RotationType = Rotation_d;
    using Vec3Type = Vec3<double>;

    SECTION("Quaternion round-trip conversion") {
        RotationType original = RotationType::from_local_to_parent({ 1.0, 1.0, 1.0 }, units::Degree{ 75.0 });

        // Updated API: Explicit getter
        Quaternion<double> quat = original.local_to_parent_quaternion();

        // Updated API: Explicit Factory
        RotationType reconstructed = RotationType::from_local_to_parent(quat);

        Vec3Type test_vec{ 2.0, -1.0, 3.0 };
        Vec3Type result1 = original * test_vec;
        Vec3Type result2 = reconstructed * test_vec;

        REQUIRE_THAT(result1.x, Catch::Matchers::WithinAbs(result2.x, 1e-12));
        REQUIRE_THAT(result1.y, Catch::Matchers::WithinAbs(result2.y, 1e-12));
    }

    SECTION("SPICE/Passive Quaternion Round Trip") {
        // Simulate a PDS quaternion (Parent->Local)
        RotationType original = RotationType::from_local_to_parent({ 0.0, 1.0, 0.0 }, units::Degree{ 45.0 });

        // Get it as a passive quaternion (like writing to a SPICE file)
        Quaternion<double> spice_quat = original.parent_to_local_quaternion();

        // Load it back (like reading from a SPICE file)
        RotationType reconstructed = RotationType::from_parent_to_local(spice_quat);

        CHECK_MAT3_EQUAL(original.local_to_parent_matrix(), reconstructed.local_to_parent_matrix());
    }
}

TEST_CASE("Rotation - Static Factory Methods", "[rotation][static]") {
    SECTION("Static rotation matrices") {
        units::Degree angle_90{ 90.0 };

        // Updated API: rotation_x -> local_to_parent_x
        Mat3<double> x_rot = Rotation_d::local_to_parent_x(angle_90);
        //Mat3<double> y_rot = Rotation_d::local_to_parent_y(angle_90);
        Mat3<double> z_rot = Rotation_d::local_to_parent_z(angle_90);

        // X rotation: [1,0,0] -> [1,0,0], [0,1,0] -> [0,0,1]
        Vec3<double> y_axis{ 0.0, 1.0, 0.0 };
        Vec3<double> rotated_y = x_rot * y_axis;
        REQUIRE_THAT(rotated_y.z, Catch::Matchers::WithinAbs(1.0, 1e-10));

        // Z rotation: [1,0,0] -> [0,1,0]
        Vec3<double> x_axis{ 1.0, 0.0, 0.0 };
        Vec3<double> rotated_x = z_rot * x_axis;
        REQUIRE_THAT(rotated_x.y, Catch::Matchers::WithinAbs(1.0, 1e-10));
    }
}
