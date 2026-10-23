#include <cmath>
#include <limits>

#include "catch2/catch_test_macros.hpp"
#include "huira/core/spectral_bins.hpp"
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

TEST_CASE("depth_map shades the nearest depth black and the farthest light gray", "[images]")
{
    // Pixels that saw nothing, at an infinite depth, are white.
    Image<float> depth(4, 1);
    depth[0] = 1.f;
    depth[1] = 3.f;
    depth[2] = std::numeric_limits<float>::infinity();
    depth[3] = 2.f;

    const Image<RGB> shades = depth_map(depth);
    REQUIRE(shades.resolution() == depth.resolution());
    CHECK(shades[0] == RGB{0.f});
    CHECK(shades[1] == RGB{0.8f});
    CHECK(shades[2] == RGB{1.f});
    CHECK(shades[3] == RGB{0.4f});
}

TEST_CASE("rgb_to_spectral converts each pixel of an RGB image", "[images]")
{
    Image<RGB> colors(2, 1);
    colors[0] = RGB{1.f, 0.f, 0.f};
    colors[1] = RGB{0.f, 0.5f, 1.f};

    // To RGB, the image is returned as it is:
    const Image<RGB> same = rgb_to_spectral<RGB>(colors);
    CHECK(same[0] == colors[0]);
    CHECK(same[1] == colors[1]);

    // To another spectral type, with the given conversion or the default one:
    const Image<Visible8> totals =
        rgb_to_spectral<Visible8>(colors, [](RGB rgb) { return Visible8{rgb.total()}; });
    REQUIRE(totals.resolution() == colors.resolution());
    CHECK(totals[0] == Visible8{1.f});
    CHECK(totals[1] == Visible8{1.5f});

    const Image<Visible8> spectral = rgb_to_spectral<Visible8>(colors);
    CHECK(spectral[0] == convert_rgb_to_spectral<Visible8>(colors[0]));
    CHECK(spectral[1] == convert_rgb_to_spectral<Visible8>(colors[1]));
}

TEST_CASE("depth_map shades a single depth black, not NaN", "[images]")
{
    // With every depth the same, it divided by a range of 0, and every pixel was NaN, which
    // writing the image then warned of.
    const float inf = std::numeric_limits<float>::infinity();
    const Image<RGB> flat = depth_map(Image<float>(3, 2, 5.f));
    for (std::size_t i = 0; i < flat.size(); ++i) {
        CHECK(flat[i] == RGB{0.f});
    }

    Image<float> one(2, 1, inf);
    one[0] = 2.f;
    const Image<RGB> shades = depth_map(one);
    CHECK(shades[0] == RGB{0.f});
    CHECK(shades[1] == RGB{1.f});
}
