#include "catch2/catch_test_macros.hpp"
#include "huira/core/spectral_bins.hpp"

using namespace huira;

TEST_CASE("RGB converts to a spectral type that covers only part of the visible band",
          "[core][spectral]")
{
    // The conversion took RGB's range from its first bin, red (600-750 nm), rather than all
    // three (380-750 nm), so a type covering only blue and green found too little overlap and
    // fell back to gray.
    using BlueGreen = UniformSpectralBins<4, 400, 580>;

    const BlueGreen blue = convert_rgb_to_spectral<BlueGreen>(RGB{0.f, 0.f, 1.f});
    const BlueGreen green = convert_rgb_to_spectral<BlueGreen>(RGB{0.f, 1.f, 0.f});

    // Pure blue lands in the blue bins (400-490 nm) and not the green ones (490-580 nm):
    CHECK(blue[0] > 0.f);
    CHECK(blue[3] == 0.f);
    CHECK(green[0] == 0.f);
    CHECK(green[3] > 0.f);

    // The shipped types are unchanged: Visible8 overlaps RGB, SWIR8 does not and gets gray.
    const Visible8 visible = convert_rgb_to_spectral<Visible8>(RGB{0.f, 0.f, 1.f});
    CHECK(visible[0] > 0.f);
    CHECK(visible[7] == 0.f);
    const SWIR8 swir = convert_rgb_to_spectral<SWIR8>(RGB{0.f, 0.f, 3.f});
    CHECK(swir[0] == 1.f);
    CHECK(swir[7] == 1.f);
}
