#include <iostream>

#include "huira/huira.hpp"

int main() {
    // Create the scene:
    huira::Scene<huira::RGB> scene;

    // Configure a camera model:
    auto camera_model = scene.new_camera_model();

    // Set a time:
    huira::Time time("2016-09-19T16:22:05.728");
    huira::Interval exposure_interval = huira::Interval::from_centered(time, 9.984285275_s);

    // Create the renderer
    huira::Renderer<TSpectral> renderer;

    //  Create a scene view over the exposure interval
    auto scene_view = huira::SceneView<TSpectral>(scene, exposure_interval, mapcam, huira::ObservationMode::ABERRATED_STATE, 3);

    // Render the current scene view
    renderer.render(scene_view, frame_buffer);
    renderer.set_samples_per_pixel(1);

    auto frame_buffer = camera_model.make_frame_buffer();
    frame_buffer.enable_depth();

    huira::write_image_fits("demo.fits", frame_buffer.depth(), 16);

    std::cout << "Huira Packaging test PASSED" << std::endl;
    
    return 0;
}
