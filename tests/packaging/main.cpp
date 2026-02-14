#include <iostream>

#include "huira/huira.hpp"

int main() {
    // Create the scene:
    huira::Scene<huira::RGB> scene;

    // Configure a camera model:
    auto camera_model = scene.new_camera_model();

    // Set a time:
    huira::Time time("2016-09-19T16:22:05.728");


    std::cout << "Huira Packaging test PASSED" << std::endl;
    
    return 0;
}
