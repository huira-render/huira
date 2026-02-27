#include <tuple>

#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/render/frame_buffer.hpp"
#include "huira/scene/scene_view.hpp"

namespace huira {

    /**
     * @brief Compute barycentric coordinates of a point within a triangle.
     * 
     * Given a triangle defined by vertices v0, v1, v2 and a point p, this function
     * computes the barycentric coordinates (u, v, w) such that:
     *   p = u*v0 + v*v1 + w*v2, where u + v + w = 1
     * 
     * @param v0 First vertex of the triangle
     * @param v1 Second vertex of the triangle
     * @param v2 Third vertex of the triangle
     * @param p Point to compute barycentric coordinates for
     * @return Vec3<float> Barycentric coordinates (u, v, w)
     */
    static inline Vec3<float> barycentric_coordinates(
        const Pixel& v0,
        const Pixel& v1,
        const Pixel& v2,
        const Pixel& p)
    {
        float denom = (v1.y - v2.y) * (v0.x - v2.x) + (v2.x - v1.x) * (v0.y - v2.y);
        if (std::abs(denom) < 1e-7f) {
            return Vec3<float>{ -1.0f, -1.0f, -1.0f };
        }
        float u = ((v1.y - v2.y) * (p.x - v2.x) + (v2.x - v1.x) * (p.y - v2.y)) / denom;
        float v = ((v2.y - v0.y) * (p.x - v2.x) + (v0.x - v2.x) * (p.y - v2.y)) / denom;
        float w = 1.0f - u - v;
        return Vec3<float>{ u, v, w };
    }

    /**
     * @brief Render a scene view into a frame buffer.
     * 
     * This method performs the complete rendering pipeline: rasterizes mesh geometry,
     * renders unresolved objects (stars/planets), and applies camera readout with the
     * specified exposure time.
     * 
     * @tparam TSpectral Spectral type for the rendering pipeline
     * @param scene_view The scene view to render
     * @param frame_buffer The frame buffer to render into
     * @param exposure_time Exposure time in seconds for camera readout
     */
    template <IsSpectral TSpectral>
    void RasterRenderer<TSpectral>::render(
        SceneView<TSpectral>& scene_view,
        FrameBuffer<TSpectral>& frame_buffer,
        float exposure_time)
    {
        this->rasterize_(scene_view, frame_buffer);

        this->render_unresolved_(scene_view, frame_buffer);

        this->get_camera(scene_view)->readout(frame_buffer, exposure_time);
    }

    template <IsSpectral TSpectral>
    void RasterRenderer<TSpectral>::set_supersample(int super_sample)
    {
        ss_factor_ = super_sample;
        if (ss_factor_ <= 0) {
            ss_factor_ = 1;  // 1 corresponds to no super-sampling
        }
        HUIRA_LOG_INFO("RasterRenderer::set_supersample - " + std::to_string(super_sample) + " interpreted as " + std::to_string(ss_factor_));
    }

    template <IsSpectral TSpectral>
    void RasterRenderer<TSpectral>::rasterize_instance_(
        const Transform<float>& instance_tf,
        const std::shared_ptr<CameraModel<TSpectral>>& camera, FrameBuffer<TSpectral>& frame_buffer,
        const std::shared_ptr<Mesh<TSpectral>>& mesh, const std::vector<LightInstance<TSpectral>>& lights)
    {
        int res_x = camera->resolution().x;
        int res_y = camera->resolution().y;
        Vec3<float> camera_origin{ 0,0,0 }; // Camera is at the origin by definition

        auto vertices = mesh->vertex_buffer();
        auto indices = mesh->index_buffer();
        auto tangents = mesh->tangent_buffer();
        bool has_tangents = mesh->has_tangents();
        auto material = mesh->material();

        Image<float>& depth_buffer = frame_buffer.depth();

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

            Vec3<float> t0{ 0 }, t1{ 0 }, t2{ 0 }, bt0{ 0 }, bt1{ 0 }, bt2{ 0 };
            if (has_tangents) {
                t0 = instance_tf.apply_to_direction(tangents[idx0].tangent);
                t1 = instance_tf.apply_to_direction(tangents[idx1].tangent);
                t2 = instance_tf.apply_to_direction(tangents[idx2].tangent);

                bt0 = instance_tf.apply_to_direction(tangents[idx0].bitangent);
                bt1 = instance_tf.apply_to_direction(tangents[idx1].bitangent);
                bt2 = instance_tf.apply_to_direction(tangents[idx2].bitangent);
            }

            // Triangle (v0, v1, v2) can be processed here
            Pixel v0_p = camera->project_point_no_distortion(v0);
            Pixel v1_p = camera->project_point_no_distortion(v1);
            Pixel v2_p = camera->project_point_no_distortion(v2);

            if (std::isnan(v0_p[0]) || std::isnan(v1_p[0]) || std::isnan(v2_p[0]))
                continue;
            

            // Compute bounding box in pixel coordinates
            int min_x = static_cast<int>(std::floor(std::min(std::min(v0_p.x, v1_p.x), v2_p.x)));
            int max_x = static_cast<int>(std::ceil(std::max(std::max(v0_p.x, v1_p.x), v2_p.x)));
            int min_y = static_cast<int>(std::floor(std::min(std::min(v0_p.y, v1_p.y), v2_p.y)));
            int max_y = static_cast<int>(std::ceil(std::max(std::max(v0_p.y, v1_p.y), v2_p.y)));

            // Clamp to image dimensions:
            min_x = std::max(min_x, 0);
            max_x = std::min(max_x, res_x - 1);
            min_y = std::max(min_y, 0);
            max_y = std::min(max_y, res_y - 1);

            auto uv0 = vertices[idx0].uv;
            auto uv1 = vertices[idx1].uv;
            auto uv2 = vertices[idx2].uv;

            // Rasterize the triangle within the bounding box
            Interaction<TSpectral> interaction;
            for (int y = min_y; y <= max_y; ++y) {
                for (int x = min_x; x <= max_x; ++x) {
                    float sub_step = 1.0f / static_cast<float>(ss_factor_);
                    float sub_offset = sub_step * 0.5f; // center each sub-sample within its cell

                    TSpectral accum_radiance{ 0 };
                    Vec3<float> accum_normals{ 0.0f };
                    TSpectral accum_albedo{ 0 };
                    int hit_count = 0;
                    float min_depth = depth_buffer(x, y); // track best depth across sub-samples
                    int best_mesh_id = -1;

                    for (int sy = 0; sy < ss_factor_; ++sy) {
                        for (int sx = 0; sx < ss_factor_; ++sx) {
                            float sample_x = static_cast<float>(x) + sub_offset + static_cast<float>(sx) * sub_step;
                            float sample_y = static_cast<float>(y) + sub_offset + static_cast<float>(sy) * sub_step;

                            Pixel p{ sample_x, sample_y };
                            Vec3<float> uvw = barycentric_coordinates(v0_p, v1_p, v2_p, p);
                            float u = uvw[0];
                            float v = uvw[1];
                            float w = uvw[2];

                            if (u < 0 || v < 0 || w < 0) continue;

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
                            if (depth >= min_depth) continue;
                            depth_buffer(x, y) = depth;

                            interaction.position = depth * (
                                u * v0 / z0 +
                                v * v1 / z1 +
                                w * v2 / z2);

                            interaction.normal_s = glm::normalize(
                                u * n0 / z0 +
                                v * n1 / z1 +
                                w * n2 / z2);

                            interaction.uv = depth * (
                                u * uv0 / z0 +
                                v * uv1 / z1 +
                                w * uv2 / z2);

                            interaction.tangent = Vec3<float>{ 0.0f };
                            interaction.bitangent = Vec3<float>{ 0.0f };
                            if (has_tangents) {
                                interaction.tangent = -glm::normalize(u * t0 + v * t1 + w * t2);
                                interaction.bitangent = -glm::normalize(u * bt0 + v * bt1 + w * bt2);
                            }
                            interaction.tangent = Vec3<float>{ 0.0f };
                            interaction.bitangent = Vec3<float>{ 0.0f };
                            if (has_tangents) {
                                interaction.tangent = glm::normalize(
                                    depth * (u * t0 / z0 + v * t1 / z1 + w * t2 / z2));
                                interaction.bitangent = glm::normalize(
                                    depth * (u * bt0 / z0 + v * bt1 / z1 + w * bt2 / z2));
                            }

                            // Evaluate material textures once for this fragment
                            auto [params, shading_isect] = material->evaluate(interaction);

                            TSpectral fragment_radiance{ 0 };
                            if (frame_buffer.has_received_power()) {
                                for (auto& light_instance : lights) {
                                    auto& light = light_instance.light;
                                    auto& light_transform = light_instance.transform;

                                    auto sample = light->sample_li(interaction, light_transform, this->sampler_);

                                    if (sample) {
                                        auto wo = glm::normalize(camera_origin - interaction.position);
                                        const auto& s = *sample;
                                        TSpectral f = material->bsdf_eval(
                                            wo,
                                            s.wi,
                                            { params, shading_isect });
                                        float cos_theta = std::max(0.0f, glm::dot(shading_isect.normal_s, s.wi));
                                        //float cos_theta = std::max(0.0f, glm::dot(interaction.normal_s, s.wi));
                                        //TSpectral f = params.albedo * INV_PI<float>();

                                        fragment_radiance += s.Li * f * cos_theta;
                                    }
                                }
                            }

                            accum_radiance += fragment_radiance;
                            accum_normals += shading_isect.normal_s;
                            accum_albedo += params.albedo;
                            hit_count++;

                            if (depth < min_depth) {
                                min_depth = depth;
                                best_mesh_id = static_cast<int>(mesh->id());
                            }
                        }
                    }

                    if (hit_count > 0) {
                        float inv = 1.0f / static_cast<float>(hit_count);
                        depth_buffer(x, y) = min_depth;

                        if (frame_buffer.has_received_power())
                            frame_buffer.received_power()(x, y) = camera->pixel_radiance_to_power(x,y) * (accum_radiance * inv);
                        if (frame_buffer.has_albedo())
                            frame_buffer.albedo()(x, y) = accum_albedo * inv;
                        if (frame_buffer.has_mesh_ids())
                            frame_buffer.mesh_ids()(x, y) = static_cast<std::uint64_t>(best_mesh_id);
                        if (frame_buffer.has_camera_normals()) {
                            frame_buffer.camera_normals()(x, y) = glm::normalize(accum_normals);
                        }
                    }
                }
            }
        }
    }

    /**
     * @brief Rasterize mesh geometry from the scene view into the frame buffer.
     * 
     * This method implements a basic triangle rasterization pipeline. For each mesh instance
     * in the scene, it projects triangles to screen space, performs per-pixel rasterization
     * with depth testing, and computes lighting using a simple Lambertian shading model.
     * 
     * The method supports multiple frame buffer outputs:
     * - Received power (radiance with Lambertian shading)
     * - Mesh IDs (for segmentation)
     * - Camera-space normals (for debugging/analysis)
     * 
     * @tparam TSpectral Spectral type for the rendering pipeline
     * @param scene_view The scene view containing meshes and lights to render
     * @param frame_buffer The frame buffer to rasterize into
     */
    template <IsSpectral TSpectral>
    void RasterRenderer<TSpectral>::rasterize_(
        SceneView<TSpectral>& scene_view,
        FrameBuffer<TSpectral>& frame_buffer)
    {
        // Extract the camera:
        auto camera = this->get_camera(scene_view);


        // Extract meshes and their instances from the scene view
        auto meshes = this->get_meshes(scene_view);


        // Extract the lights:
        auto lights = this->get_lights(scene_view);
        
        // Reset any existing data in the frame buffer:
        frame_buffer.clear();

        // Make sure depth buffer is enabled:
        frame_buffer.enable_depth();

        // Loop over all instances:
        for (auto& batch : meshes) {
            auto& mesh = batch.mesh;
            for (const Transform<float>& instance_tf : batch.instances) {
                rasterize_instance_(instance_tf, camera, frame_buffer, mesh, lights);
            }
        }
    }
}

