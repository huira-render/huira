#include <cmath>
#include <limits>

#include "catch2/catch_test_macros.hpp"
#include "huira/images/color_map.hpp"

using namespace huira;

TEST_CASE("normal_map colors each normal, and pixels that saw nothing black", "[images]")
{
    // A frame buffer's normals are NaN where a pixel sees nothing. normal_map passed them through,
    // and writing the image then warned of every one of them.
    const float nan = std::numeric_limits<float>::quiet_NaN();
    Image<Vec3<float>> normals(3, 1);
    normals[0] = Vec3<float>{0.f, 0.f, 1.f};
    normals[1] = Vec3<float>{-1.f, 1.f, 0.f};
    normals[2] = Vec3<float>{nan, nan, nan};

    const Image<RGB> colors = normal_map(normals);
    for (std::size_t i = 0; i < colors.size(); ++i) {
        for (std::size_t c = 0; c < 3; ++c) {
            REQUIRE(std::isfinite(colors[i][c]));
        }
    }
    CHECK(colors[0][0] == 0.5f);
    CHECK(colors[0][1] == 0.5f);
    CHECK(colors[0][2] == 1.f);
    CHECK(colors[1][0] == 0.f);
    CHECK(colors[1][1] == 1.f);
    CHECK(colors[1][2] == 0.5f);
    CHECK(colors[2][0] == 0.f);
    CHECK(colors[2][1] == 0.f);
    CHECK(colors[2][2] == 0.f);
}
