#include <tuple>

#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/scene/scene_view.hpp"
#include "huira/render/frame_buffer.hpp"

namespace huira {

    static inline Vec3<float> barycentric_coordinates(
        const Pixel& v0,
        const Pixel& v1,
        const Pixel& v2,
        const Pixel& p)
    {
        float denom = (v1.y - v2.y) * (v0.x - v2.x) + (v2.x - v1.x) * (v0.y - v2.y);
        float u = ((v1.y - v2.y) * (p.x - v2.x) + (v2.x - v1.x) * (p.y - v2.y)) / denom;
        float v = ((v2.y - v0.y) * (p.x - v2.x) + (v0.x - v2.x) * (p.y - v2.y)) / denom;
        float w = 1.0f - u - v;
        return Vec3<float>{ u, v, w };
    }

    template <IsSpectral TSpectral>
    void RasterRenderer<TSpectral>::render(
        SceneView<TSpectral>& scene_view,
        FrameBuffer<TSpectral>& frame_buffer,
        float exposure_time
    )
    {
        this->rasterize_(scene_view, frame_buffer);

        this->render_unresolved_(scene_view, frame_buffer);

        this->get_camera(scene_view)->readout(frame_buffer, exposure_time);
    }

    template <IsSpectral TSpectral>
    void RasterRenderer<TSpectral>::rasterize_(
        SceneView<TSpectral>& scene_view,
        FrameBuffer<TSpectral>& frame_buffer)
    {
        // Extract the camera:
        auto camera = this->get_camera(scene_view);
        float res_x = static_cast<float>(camera->resolution().x);
        float res_y = static_cast<float>(camera->resolution().y);


        // Extract meshes and their instances from the scene view
        auto meshes = this->get_meshes(scene_view);


        // Extract the lights:
        auto lights = this->get_lights(scene_view);

        
        // Reset any existing data in the frame buffer:
        frame_buffer.clear();

        // Make sure depth buffer is enabled:
        frame_buffer.enable_depth();
        Image<float>& depth_buffer = frame_buffer.depth();

        // Loop over all instances:
        for (auto& batch : meshes) {
            auto& mesh = batch.mesh;
            auto vertices = mesh->vertex_buffer();
            auto indices = mesh->index_buffer();

            for (const Transform<float>& instance_tf : batch.instances) {
                // Loop over triangles (from index buffer):
                for (size_t i = 0; i + 2 < indices.size(); i += 3) {
                    auto idx0 = indices[i];
                    auto idx1 = indices[i + 1];
                    auto idx2 = indices[i + 2];
                
                    auto v0 = instance_tf.apply_to_point(vertices[idx0].position);
                    auto v1 = instance_tf.apply_to_point(vertices[idx1].position);
                    auto v2 = instance_tf.apply_to_point(vertices[idx2].position);

                    auto n0 = glm::normalize(instance_tf.apply_to_direction(vertices[idx0].normal));
                    auto n1 = glm::normalize(instance_tf.apply_to_direction(vertices[idx1].normal));
                    auto n2 = glm::normalize(instance_tf.apply_to_direction(vertices[idx2].normal));

                    // Triangle (v0, v1, v2) can be processed here
                    Pixel v0_p = camera->project_point(v0);
                    Pixel v1_p = camera->project_point(v1);
                    Pixel v2_p = camera->project_point(v2);
                
                    // Compute bounding box in pixel coordinates
                    int min_x = static_cast<int>(std::floor(std::min(std::min(v0_p.x, v1_p.x), v2_p.x)));
                    int max_x = static_cast<int>(std::ceil(std::max(std::max(v0_p.x, v1_p.x), v2_p.x)));
                    int min_y = static_cast<int>(std::floor(std::min(std::min(v0_p.y, v1_p.y), v2_p.y)));
                    int max_y = static_cast<int>(std::ceil(std::max(std::max(v0_p.y, v1_p.y), v2_p.y)));

                    // Clamp to image dimensions:
                    min_x = std::max(min_x, 0);
                    max_x = std::min(max_x, static_cast<int>(res_x) - 1);
                    min_y = std::max(min_y, 0);
                    max_y = std::min(max_y, static_cast<int>(res_y) - 1);

                    // Rasterize the triangle within the bounding box
                    Interaction<TSpectral> interaction;
                    for (int y = min_y; y <= max_y; ++y) {
                        for (int x = min_x; x <= max_x; ++x) {
                            Pixel p{ static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f };
                            interaction.uvw = barycentric_coordinates(v0_p, v1_p, v2_p, p);
                            float u = interaction.uvw[0];
                            float v = interaction.uvw[1];
                            float w = interaction.uvw[2];
                            if (u >= 0 && v >= 0 && w >= 0) {
                                // Interpolate depth
                                // Perspective-correct depth interpolation
                                float z0, z1, z2;
                                if (camera->is_blender_convention()) {
                                    z0 = -v0.z;
                                    z1 = -v1.z;
                                    z2 = -v2.z;
                                }
                                else {
                                    z0 = v0.z;
                                    z1 = v1.z;
                                    z2 = v2.z;
                                }
                                float denom = u / z0 + v / z1 + w / z2;
                                float depth = 1.0f / denom;

                                // Depth test
                                if (depth < depth_buffer(x, y)) {
                                    depth_buffer(x, y) = depth;

                                    interaction.position = depth * (
                                        u * v0 / z0 +
                                        v * v1 / z1 +
                                        w * v2 / z2);

                                    interaction.normal_s = glm::normalize(
                                        u * n0 / z0 +
                                        v * n1 / z1 +
                                        w * n2 / z2);

                                    if (frame_buffer.has_received_power()) {
                                        TSpectral fragment_radiance{ 0 };
                                        for (auto& light_instance : lights) {
                                            auto& light = light_instance.light;
                                            auto& light_transform = light_instance.transform;

                                            auto sample = light->sample_li(interaction, light_transform, this->sampler_);

                                            // Apply lambertian:
                                            if (sample) {
                                                const auto& s = *sample;
                                                fragment_radiance += s.Li * std::max(0.0f, glm::dot(interaction.normal_s, s.wi));
                                            }
                                        }
                                        frame_buffer.received_power()(x, y) = fragment_radiance;
                                    }

                                    if (frame_buffer.has_mesh_ids()) {
                                        frame_buffer.mesh_ids()(x, y) = mesh->id();
                                    }

                                    if (frame_buffer.has_camera_normals()) {
                                        // Interpolate normals
                                        Vec3<float> n_display = interaction.normal_s;
                                        n_display[0] = 0.5f * (n_display[0] + 1.0f);
                                        n_display[1] = 0.5f * (n_display[1] + 1.0f);
                                        n_display[2] = 0.5f * (n_display[2] + 1.0f);
                                        frame_buffer.camera_normals()(x, y) = n_display;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

