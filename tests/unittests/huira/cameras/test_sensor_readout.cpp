#include <cstddef>

#include "catch2/catch_test_macros.hpp"
#include "huira/cameras/camera_model.hpp"
#include "huira/core/spectral_bins.hpp"
#include "huira/units/units.hpp"

using namespace huira;

TEST_CASE("Sensor readout respects the frame buffer's enabled outputs", "[cameras][sensor]")
{
    CameraModel<RGB> camera;
    camera.configure_sensor_from_pitch(Resolution{8, 6}, units::Micrometer(5.0));
    auto frame_buffer = camera.make_frame_buffer();

    // A few thousand photons per pixel: well above the noise floor and below full well.
    const RGB power{1e-15f};

    SECTION("Without a sensor response, readout leaves the frame buffer untouched")
    {
        frame_buffer.enable_received_power();
        frame_buffer.received_power().fill(power);

        camera.readout(frame_buffer, units::Second(1.0));

        REQUIRE_FALSE(frame_buffer.has_sensor_response());
        const Image<RGB>& received = frame_buffer.received_power();
        for (std::size_t i = 0; i < received.size(); ++i) {
            for (std::size_t c = 0; c < 3; ++c) {
                REQUIRE(received[i][c] == power[c]);
            }
        }
    }

    SECTION("With a sensor response, readout fills it")
    {
        frame_buffer.enable_sensor_response();
        frame_buffer.received_power().fill(power);

        camera.readout(frame_buffer, units::Second(1.0));

        const Image<RGB>& response = frame_buffer.sensor_response();
        REQUIRE(response.width() == 8);
        REQUIRE(response.height() == 6);
        for (std::size_t i = 0; i < response.size(); ++i) {
            for (std::size_t c = 0; c < 3; ++c) {
                REQUIRE(response[i][c] > 0.f);
            }
        }
    }
}
