#include <cmath>
#include <numbers>

#include "catch2/catch_test_macros.hpp"
#include "catch2/catch_template_test_macros.hpp"
#include "catch2/catch_approx.hpp"
#include "catch2/matchers/catch_matchers_floating_point.hpp"

#include "huira/math/rotation.hpp"

using namespace huira;
using Catch::Approx;

// Test both float and double instantiations
TEMPLATE_TEST_CASE("Rotation - Template Instantiations", "[rotation][template]", float, double) {
    using RotationType = Rotation<TestType>;
    using Vec3Type = Vec3<TestType>;
    using Mat3Type = Mat3<TestType>;

    SECTION("Default constructor creates identity rotation") {
        RotationType rot;

        // Identity should preserve test vectors
        Vec3Type x_axis{ 1, 0, 0 };
        Vec3Type result = rot * x_axis;

        REQUIRE_THAT(result.x, Catch::Matchers::WithinAbs(x_axis.x, 1e-6));
        REQUIRE_THAT(result.y, Catch::Matchers::WithinAbs(x_axis.y, 1e-6));
        REQUIRE_THAT(result.z, Catch::Matchers::WithinAbs(x_axis.z, 1e-6));
    }

    SECTION("Matrix accessor returns expected format") {
        RotationType rot;
        Mat3Type matrix = rot.getMatrix();

        // Identity matrix should have 1s on diagonal, 0s elsewhere
        REQUIRE_THAT(matrix[0][0], Catch::Matchers::WithinAbs(TestType(1), 1e-6));
        REQUIRE_THAT(matrix[1][1], Catch::Matchers::WithinAbs(TestType(1), 1e-6));
        REQUIRE_THAT(matrix[2][2], Catch::Matchers::WithinAbs(TestType(1), 1e-6));

        REQUIRE_THAT(matrix[0][1], Catch::Matchers::WithinAbs(TestType(0), 1e-6));
        REQUIRE_THAT(matrix[0][2], Catch::Matchers::WithinAbs(TestType(0), 1e-6));
        REQUIRE_THAT(matrix[1][0], Catch::Matchers::WithinAbs(TestType(0), 1e-6));
    }
}

TEST_CASE("Rotation - Construction Methods", "[rotation][constructor]") {
    using RotationType = Rotation_d;  // Use double for precise testing
    using Vec3Type = Vec3<double>;

    SECTION("Axis-angle constructor") {
        Vec3Type z_axis{ 0.0, 0.0, 1.0 };
        Degree angle_90{ 90.0 };

        RotationType rot(z_axis, angle_90);

        // 90-degree rotation around Z should map X-axis to Y-axis
        Vec3Type x_axis{ 1.0, 0.0, 0.0 };
        Vec3Type rotated = rot * x_axis;

        REQUIRE_THAT(rotated.x, Catch::Matchers::WithinAbs(0.0, 1e-10));
        REQUIRE_THAT(rotated.y, Catch::Matchers::WithinAbs(1.0, 1e-10));
        REQUIRE_THAT(rotated.z, Catch::Matchers::WithinAbs(0.0, 1e-10));
    }

    SECTION("Euler angle constructor - single axis rotations") {
        Degree angle_90{ 90.0 };
        Degree zero{ 0.0 };

        // Test X rotation
        RotationType rot_x(angle_90, zero, zero, "XYZ");
        Vec3Type y_axis{ 0.0, 1.0, 0.0 };
        Vec3Type rotated_y = rot_x * y_axis;

        // 90deg X rotation should map Y to Z
        REQUIRE_THAT(rotated_y.x, Catch::Matchers::WithinAbs(0.0, 1e-10));
        REQUIRE_THAT(rotated_y.y, Catch::Matchers::WithinAbs(0.0, 1e-10));
        REQUIRE_THAT(rotated_y.z, Catch::Matchers::WithinAbs(1.0, 1e-10));

        // Test Y rotation
        RotationType rot_y(zero, angle_90, zero, "XYZ");
        Vec3Type x_axis{ 1.0, 0.0, 0.0 };
        Vec3Type rotated_x = rot_y * x_axis;

        // 90deg Y rotation should map X to -Z
        REQUIRE_THAT(rotated_x.x, Catch::Matchers::WithinAbs(0.0, 1e-10));
        REQUIRE_THAT(rotated_x.y, Catch::Matchers::WithinAbs(0.0, 1e-10));
        REQUIRE_THAT(rotated_x.z, Catch::Matchers::WithinAbs(-1.0, 1e-10));
    }

    SECTION("Matrix constructor preserves rotation") {
        // Create a known rotation matrix (90deg around Z)
        Mat3<double> rot_matrix = RotationType::rotationZ(Degree{ 90.0 });

        RotationType rot(rot_matrix);

        Vec3Type x_axis{ 1.0, 0.0, 0.0 };
        Vec3Type rotated = rot * x_axis;

        REQUIRE_THAT(rotated.x, Catch::Matchers::WithinAbs(0.0, 1e-10));
        REQUIRE_THAT(rotated.y, Catch::Matchers::WithinAbs(1.0, 1e-10));
        REQUIRE_THAT(rotated.z, Catch::Matchers::WithinAbs(0.0, 1e-10));
    }
}

TEST_CASE("Rotation - Operations", "[rotation][operations]") {
    using RotationType = Rotation_d;
    using Vec3Type = Vec3<double>;

    SECTION("Multiplication is associative") {
        // Create three different rotations
        RotationType rot1({ 1.0, 0.0, 0.0 }, Degree{ 30.0 });  // 30deg around X
        RotationType rot2({ 0.0, 1.0, 0.0 }, Degree{ 45.0 });  // 45deg around Y  
        RotationType rot3({ 0.0, 0.0, 1.0 }, Degree{ 60.0 });  // 60deg around Z

        // Test (rot1 * rot2) * rot3 == rot1 * (rot2 * rot3)
        RotationType left_assoc = (rot1 * rot2) * rot3;
        RotationType right_assoc = rot1 * (rot2 * rot3);

        // Compare by applying to a test vector
        Vec3Type test_vec{ 1.0, 1.0, 1.0 };
        Vec3Type left_result = left_assoc * test_vec;
        Vec3Type right_result = right_assoc * test_vec;

        REQUIRE_THAT(left_result.x, Catch::Matchers::WithinAbs(right_result.x, 1e-12));
        REQUIRE_THAT(left_result.y, Catch::Matchers::WithinAbs(right_result.y, 1e-12));
        REQUIRE_THAT(left_result.z, Catch::Matchers::WithinAbs(right_result.z, 1e-12));
    }

    SECTION("Compound assignment operator") {
        RotationType rot1({ 0.0, 0.0, 1.0 }, Degree{ 45.0 });
        RotationType rot2({ 1.0, 0.0, 0.0 }, Degree{ 30.0 });

        RotationType expected = rot1 * rot2;
        rot1 *= rot2;

        Vec3Type test_vec{ 1.0, 2.0, 3.0 };
        Vec3Type result1 = rot1 * test_vec;
        Vec3Type result2 = expected * test_vec;

        REQUIRE_THAT(result1.x, Catch::Matchers::WithinAbs(result2.x, 1e-12));
        REQUIRE_THAT(result1.y, Catch::Matchers::WithinAbs(result2.y, 1e-12));
        REQUIRE_THAT(result1.z, Catch::Matchers::WithinAbs(result2.z, 1e-12));
    }

    SECTION("Inverse operation") {
        Vec3Type axis{ 1.0, 1.0, 1.0 };  // Will be normalized internally
        Degree angle{ 60.0 };
        RotationType rot(axis, angle);
        RotationType inv_rot = rot.inverse();

        // rot * rot.inverse() should be identity
        RotationType identity = rot * inv_rot;

        Vec3Type test_vec{ 2.0, -1.0, 3.0 };
        Vec3Type result = identity * test_vec;

        REQUIRE_THAT(result.x, Catch::Matchers::WithinAbs(test_vec.x, 1e-12));
        REQUIRE_THAT(result.y, Catch::Matchers::WithinAbs(test_vec.y, 1e-12));
        REQUIRE_THAT(result.z, Catch::Matchers::WithinAbs(test_vec.z, 1e-12));
    }
}

TEST_CASE("Rotation - Properties and Invariants", "[rotation][properties]") {
    using RotationType = Rotation_d;
    using Vec3Type = Vec3<double>;

    SECTION("Rotation preserves vector length") {
        RotationType rot({ 1.0, 1.0, 1.0 }, Degree{ 120.0 });

        std::vector<Vec3Type> test_vectors = {
            {3.0, 4.0, 0.0},    // Length 5
            {1.0, 1.0, 1.0},    // Length sqrt(3)
            {0.0, 0.0, 7.0},    // Length 7
            {-2.0, 3.0, -1.0}   // Length sqrt(14)
        };

        for (const auto& vec : test_vectors) {
            Vec3Type rotated = rot * vec;
            double original_length = glm::length(vec);
            double rotated_length = glm::length(rotated);

            REQUIRE_THAT(rotated_length, Catch::Matchers::WithinAbs(original_length, 1e-12));
        }
    }

    SECTION("Rotation preserves angles between vectors") {
        RotationType rot({ 0.5, 0.5, 0.707 }, Degree{ 75.0 });

        Vec3Type vec1{ 1.0, 0.0, 0.0 };
        Vec3Type vec2{ 0.0, 1.0, 0.0 };

        // Calculate original angle (should be 90deg)
        double original_cos = glm::dot(vec1, vec2) / (glm::length(vec1) * glm::length(vec2));

        // Rotate both vectors
        Vec3Type rot_vec1 = rot * vec1;
        Vec3Type rot_vec2 = rot * vec2;

        // Calculate angle after rotation
        double rotated_cos = glm::dot(rot_vec1, rot_vec2) / (glm::length(rot_vec1) * glm::length(rot_vec2));

        REQUIRE_THAT(rotated_cos, Catch::Matchers::WithinAbs(original_cos, 1e-12));
    }

    SECTION("Axis extraction methods") {
        RotationType rot({ 0.0, 0.0, 1.0 }, Degree{ 45.0 });

        Vec3Type x_axis = rot.getXAxis();
        Vec3Type y_axis = rot.getYAxis();
        Vec3Type z_axis = rot.getZAxis();

        // Check orthogonality
        REQUIRE_THAT(glm::dot(x_axis, y_axis), Catch::Matchers::WithinAbs(0.0, 1e-12));
        REQUIRE_THAT(glm::dot(x_axis, z_axis), Catch::Matchers::WithinAbs(0.0, 1e-12));
        REQUIRE_THAT(glm::dot(y_axis, z_axis), Catch::Matchers::WithinAbs(0.0, 1e-12));

        // Check normalization
        REQUIRE_THAT(glm::length(x_axis), Catch::Matchers::WithinAbs(1.0, 1e-12));
        REQUIRE_THAT(glm::length(y_axis), Catch::Matchers::WithinAbs(1.0, 1e-12));
        REQUIRE_THAT(glm::length(z_axis), Catch::Matchers::WithinAbs(1.0, 1e-12));
    }
}

TEST_CASE("Rotation - Quaternion Conversions", "[rotation][quaternion]") {
    using RotationType = Rotation_d;
    using Vec3Type = Vec3<double>;

    SECTION("Quaternion round-trip conversion") {
        RotationType original({ 1.0, 1.0, 1.0 }, Degree{ 75.0 });

        // Convert to quaternion and back
        Quaternion<double> quat = original.getQuaternion();
        RotationType reconstructed(quat);

        // Test that they produce the same rotation
        Vec3Type test_vec{ 2.0, -1.0, 3.0 };
        Vec3Type result1 = original * test_vec;
        Vec3Type result2 = reconstructed * test_vec;

        REQUIRE_THAT(result1.x, Catch::Matchers::WithinAbs(result2.x, 1e-12));
        REQUIRE_THAT(result1.y, Catch::Matchers::WithinAbs(result2.y, 1e-12));
        REQUIRE_THAT(result1.z, Catch::Matchers::WithinAbs(result2.z, 1e-12));
    }

    SECTION("Shuster quaternion round-trip conversion") {
        RotationType original({ 0.0, 1.0, 0.0 }, Degree{ 45.0 });

        // Convert to Shuster quaternion and back
        ShusterQuaternion<double> shuster_quat = original.getShusterQuaternion();
        RotationType reconstructed(shuster_quat);

        // Test equivalence
        Vec3Type test_vec{ 1.0, 2.0, 3.0 };
        Vec3Type result1 = original * test_vec;
        Vec3Type result2 = reconstructed * test_vec;

        REQUIRE_THAT(result1.x, Catch::Matchers::WithinAbs(result2.x, 1e-12));
        REQUIRE_THAT(result1.y, Catch::Matchers::WithinAbs(result2.y, 1e-12));
        REQUIRE_THAT(result1.z, Catch::Matchers::WithinAbs(result2.z, 1e-12));
    }
}

TEST_CASE("Rotation - Static Factory Methods", "[rotation][static]") {
    SECTION("Static rotation matrices") {
        Degree angle_90{ 90.0 };

        // Test static rotation functions
        Mat3<double> x_rot = Rotation_d::rotationX(angle_90);
        Mat3<double> y_rot = Rotation_d::rotationY(angle_90);
        Mat3<double> z_rot = Rotation_d::rotationZ(angle_90);

        // X rotation: [1,0,0] -> [1,0,0], [0,1,0] -> [0,0,1]
        Vec3<double> y_axis{ 0.0, 1.0, 0.0 };
        Vec3<double> rotated_y = x_rot * y_axis;
        REQUIRE_THAT(rotated_y.x, Catch::Matchers::WithinAbs(0.0, 1e-10));
        REQUIRE_THAT(rotated_y.y, Catch::Matchers::WithinAbs(0.0, 1e-10));
        REQUIRE_THAT(rotated_y.z, Catch::Matchers::WithinAbs(1.0, 1e-10));

        // Y rotation: [1,0,0] -> [0,0,-1], [0,0,1] -> [1,0,0]
        Vec3<double> x_axis{ 1.0, 0.0, 0.0 };
        Vec3<double> rotated_x = y_rot * x_axis;
        REQUIRE_THAT(rotated_x.x, Catch::Matchers::WithinAbs(0.0, 1e-10));
        REQUIRE_THAT(rotated_x.y, Catch::Matchers::WithinAbs(0.0, 1e-10));
        REQUIRE_THAT(rotated_x.z, Catch::Matchers::WithinAbs(-1.0, 1e-10));

        // Z rotation: [1,0,0] -> [0,1,0], [0,1,0] -> [-1,0,0]
        Vec3<double> rotated_x_z = z_rot * x_axis;
        REQUIRE_THAT(rotated_x_z.x, Catch::Matchers::WithinAbs(0.0, 1e-10));
        REQUIRE_THAT(rotated_x_z.y, Catch::Matchers::WithinAbs(1.0, 1e-10));
        REQUIRE_THAT(rotated_x_z.z, Catch::Matchers::WithinAbs(0.0, 1e-10));
    }
}

TEST_CASE("Rotation - Edge Cases and Error Conditions", "[rotation][edge_cases]") {
    using RotationType = Rotation_d;
    using Vec3Type = Vec3<double>;

    SECTION("Zero degree rotations") {
        Vec3Type any_axis{ 1.0, 2.0, 3.0 };
        RotationType rot(any_axis, Degree{ 0.0 });

        // Should behave like identity
        Vec3Type test_vec{ 4.0, -2.0, 1.0 };
        Vec3Type result = rot * test_vec;

        REQUIRE_THAT(result.x, Catch::Matchers::WithinAbs(test_vec.x, 1e-12));
        REQUIRE_THAT(result.y, Catch::Matchers::WithinAbs(test_vec.y, 1e-12));
        REQUIRE_THAT(result.z, Catch::Matchers::WithinAbs(test_vec.z, 1e-12));
    }

    SECTION("Large angle rotations") {
        Vec3Type axis{ 0.0, 0.0, 1.0 };
        Degree large_angle{ 720.0 };  // Two full rotations

        RotationType rot(axis, large_angle);

        // Should be equivalent to identity (modulo 360deg)
        Vec3Type test_vec{ 1.0, 1.0, 0.0 };
        Vec3Type result = rot * test_vec;

        REQUIRE_THAT(result.x, Catch::Matchers::WithinAbs(test_vec.x, 1e-10));
        REQUIRE_THAT(result.y, Catch::Matchers::WithinAbs(test_vec.y, 1e-10));
        REQUIRE_THAT(result.z, Catch::Matchers::WithinAbs(test_vec.z, 1e-10));
    }

    SECTION("Very small rotations") {
        Vec3Type axis{ 1.0, 0.0, 0.0 };
        Degree tiny_angle{ 1e-6 };  // Very small angle

        RotationType rot(axis, tiny_angle);
        Vec3Type test_vec{ 0.0, 1.0, 0.0 };
        Vec3Type result = rot * test_vec;

        // Should be very close to original with tiny rotation
        REQUIRE_THAT(result.x, Catch::Matchers::WithinAbs(0.0, 1e-5));
        REQUIRE_THAT(result.y, Catch::Matchers::WithinAbs(1.0, 1e-5));
        // Z component should have small change proportional to angle
        REQUIRE(std::abs(result.z) < 1e-4);
    }
}

TEST_CASE("Rotation - String Representation and Output", "[rotation][output]") {
    using RotationType = Rotation_d;

    SECTION("toString method returns valid string") {
        RotationType rot({ 0.0, 0.0, 1.0 }, Degree{ 45.0 });
        std::string str_rep = rot.toString();

        // Should return a non-empty string
        REQUIRE_FALSE(str_rep.empty());

        // Should contain some numerical content
        REQUIRE(str_rep.find_first_of("0123456789") != std::string::npos);
    }

    SECTION("Stream output operator works") {
        RotationType rot;
        std::ostringstream oss;

        // Should not throw and should produce output
        REQUIRE_NOTHROW(oss << rot);
        REQUIRE_FALSE(oss.str().empty());
    }
}