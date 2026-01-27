#pragma once

#include <cstddef>
#include <limits>

#include "huira/images/image.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    class CameraModel;

    template <IsSpectral TSpectral>
    class FrameBuffer {
    public:
        FrameBuffer() = delete;

        std::size_t width() const { return width_; }
        std::size_t height() const { return height_; }


        void enable_depth(bool enable = true) { enable_(depth_, std::numeric_limits<float>::infinity(), enable); }
        Image<float>& depth() { return depth_; }
        bool has_depth() const { return has_(depth_); }


        void enable_mesh_ids(bool enable = true) { enable_(mesh_ids_, uint64_t{ 0 }, enable); }
        Image<uint64_t>& mesh_ids() { return mesh_ids_; }
        bool has_mesh_ids() const { return has_(mesh_ids_); }


        void enable_camera_normals(bool enable = true) { enable_(camera_normals_, Vec3<float>{0, 0, 0}, enable); }
        Image<Vec3<float>>& camera_normals() { return camera_normals_; }
        bool has_camera_normals() const { return has_(camera_normals_); }


        void enable_world_normals(bool enable = true) { enable_(world_normals_, Vec3<float>{0, 0, 0}, enable); }
        Image<Vec3<float>>& world_normals() { return world_normals_; }
        bool has_world_normals() const { return has_(world_normals_); }



        void enable_received_power(bool enable = true) { enable_(received_power_, TSpectral{ 0 }, enable); }
        Image<TSpectral>& received_power() { return received_power_; }
        bool has_received_power() const { return has_(received_power_); }

        void enable_sensor_response(bool enable = true) {
            enable_(received_power_, 0.f, enable); // Sensor response requires received power
            enable_(sensor_response_, 0.f, enable);
        }
        Image<float>& sensor_response() { return sensor_response_; }
        bool has_sensor_response() const { return has_(sensor_response_); }

        void enable_sensor_response_rgb(bool enable = true) {
            enable_(received_power_, 0.f, enable); // Sensor response requires received power
            enable_(sensor_response_rgb_, Vec3<float>{0, 0, 0}, enable);
        }
        Image<Vec3<float>>& sensor_response_rgb() { return sensor_response_rgb_; }
        bool has_sensor_response_rgb() const { return has_(sensor_response_rgb_); }


        void clear() {
            if (has_depth()) {
                depth_.fill(std::numeric_limits<float>::infinity());
            }
            if (has_mesh_ids()) {
                mesh_ids_.fill(uint64_t{ 0 });
            }
            if (has_camera_normals()) {
                camera_normals_.fill(Vec3<float>{ 0, 0, 0 });
            }
            if (has_world_normals()) {
                world_normals_.fill(Vec3<float>{ 0, 0, 0 });
            }
            if (has_received_power()) {
                received_power_.fill(TSpectral{ 0 });
            }
            if (has_sensor_response()) {
                sensor_response_.fill(0.f);
            }
            if (has_sensor_response_rgb()) {
                sensor_response_rgb_.fill(Vec3<float>{0, 0, 0});
            }
        }

    private:
        FrameBuffer(int width, int height)
            : width_(width), height_(height) {}

        int width_;
        int height_;

        Image<float> depth_;
        Image<uint64_t> mesh_ids_;
        Image<Vec3<float>> camera_normals_;
        Image<Vec3<float>> world_normals_;

        Image<TSpectral> received_power_;
        Image<float> sensor_response_;
        Image<Vec3<float>> sensor_response_rgb_;


        template <IsImagePixel T>
        bool has_(const Image<T>& image) const {
            return (image.width() == width_ && image.height() == height_);
        }

        template <IsImagePixel T>
        void enable_(Image<T>& image, const T& fill_value, bool enable) {
            if (!enable) {
                image = Image<T>(0, 0);
                return;
            }
            if (image.width() != width_ || image.height() != height_) {
                image = Image<T>(width_, height_, fill_value);
            }
        }

        friend class CameraModel<TSpectral>;
    };
}
