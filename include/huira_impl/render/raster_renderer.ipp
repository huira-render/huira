#include "huira/detail/concepts/spectral_concepts.hpp"
#include "huira/core/scene_view.hpp"
#include "huira/render/frame_buffer.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    void RasterRenderer<TSpectral>::render(
        SceneView<TSpectral>& scene_view,
        FrameBuffer<TSpectral>& frame_buffer)
    {
        // Extract the camera:
        auto camera = this->get_camera(scene_view);

        // Extract meshes and their instances from the scene view
        auto meshes = this->get_meshes(scene_view);

        float res_x = static_cast<float>(camera->res_x());
        float res_y = static_cast<float>(camera->res_y());

        // Buffer we'll be writing to:
        Image<float>& depth_buffer = frame_buffer.depth();

        // Loop over all instances:
        for (auto& batch : meshes) {
            auto& mesh = batch.mesh;
            auto vertices = mesh->vertex_buffer();
            auto indices = mesh->index_buffer();

            for (const Transform<float>& instance_tf : batch.instances) {


                for (auto& vertex : vertices) {
                    auto v = instance_tf.apply_to_point(vertex.position);

                    //Pixel v_p{ 17*v.x + 0.5f * res_x, 17*v.y + 0.5f * res_y };
                    //depth_buffer(v_p) = 0.f;

                    auto v_p = camera->project_point(v);
                    if (std::isnan(v_p.x) || std::isnan(v_p.y)) {
                        // Vertex is behind the camera; skip
                        continue;
                    }

                    if (v_p.x < 0 || v_p.x >= res_x || v_p.y < 0 || v_p.y >= res_y) {
                        // Vertex is outside the image bounds; skip
                        continue;
                    }

                    depth_buffer(v_p) = 0.f;
                }
                //// Loop over triangles (from index buffer):
                //for (size_t i = 0; i + 2 < indices.size(); i += 3) {
                //    auto idx0 = indices[i];
                //    auto idx1 = indices[i + 1];
                //    auto idx2 = indices[i + 2];
                //
                //    auto v0 = instance_tf.apply_to_point(vertices[idx0].position);
                //    auto v1 = instance_tf.apply_to_point(vertices[idx1].position);
                //    auto v2 = instance_tf.apply_to_point(vertices[idx2].position);
                //
                //    // Triangle (v0, v1, v2) can be processed here
                //    Pixel v0_p = camera->project_point(v0);
                //    Pixel v1_p = camera->project_point(v1);
                //    Pixel v2_p = camera->project_point(v2);
                //
                //    //if (std::isnan(v0_p.x) || std::isnan(v0_p.y) ||
                //    //    std::isnan(v1_p.x) || std::isnan(v1_p.y) ||
                //    //    std::isnan(v2_p.x) || std::isnan(v2_p.y)) {
                //    //    // One or more vertices are behind the camera; skip this triangle
                //    //    continue;
                //    //}
                //    //
                //    //if (v0_p.x < 0 || v0_p.x >= res_x || v0_p.y < 0 || v0_p.y >= res_y ||
                //    //    v1_p.x < 0 || v1_p.x >= res_x || v1_p.y < 0 || v1_p.y >= res_y ||
                //    //    v2_p.x < 0 || v2_p.x >= res_x || v2_p.y < 0 || v2_p.y >= res_y) {
                //    //    // One or more vertices are outside the image bounds; skip this triangle
                //    //    continue;
                //    //}
                //    //
                //    //depth_buffer(v0_p) = 0.f;
                //    //depth_buffer(v1_p) = 0.f;
                //    //depth_buffer(v2_p) = 0.f;
                //
                //    v0_p.x = (v0_p.x + 0.5f) / res_x;
                //    v0_p.y = (v0_p.y + 0.5f) / res_y;
                //    v1_p.x = (v1_p.x + 0.5f) / res_x;
                //    v1_p.y = (v1_p.y + 0.5f) / res_y;
                //    v2_p.x = (v2_p.x + 0.5f) / res_x;
                //    v2_p.y = (v2_p.y + 0.5f) / res_y;
                //
                //    if (v0_p.x < 0.f || v0_p.x >= 1.f || v0_p.y < 0.f || v0_p.y >= 1.f ||
                //        v1_p.x < 0.f || v1_p.x >= 1.f || v1_p.y < 0.f || v1_p.y >= 1.f ||
                //        v2_p.x < 0.f || v2_p.x >= 1.f || v2_p.y < 0.f || v2_p.y >= 1.f) {
                //        // One or more vertices are outside the image bounds; skip this triangle
                //        continue;
                //    }
                //
                //    depth_buffer(v0_p) = 0.f;
                //    depth_buffer(v1_p) = 0.f;
                //    depth_buffer(v2_p) = 0.f;
                //}
            }
        }
    }
}
