#pragma once

#include <cstddef>
#include <limits>
#include <type_traits>

#include "huira/images/image.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    class CameraModel;

    template <IsSpectral TSpectral>
    class FrameBuffer {
    public:
        using SensorT = std::conditional_t<std::is_same_v<TSpectral, RGB>, Vec3<float>, float>;

        FrameBuffer() = delete;

        Resolution resolution() const { return resolution_; }
        int width() const { return resolution_.width; }
        int height() const { return resolution_.height; }

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
            enable_(received_power_, TSpectral{ 0 }, enable); // Sensor response requires received power
            enable_(sensor_response_, SensorT{}, enable);
        }
        Image<SensorT>& sensor_response() { return sensor_response_; }
        bool has_sensor_response() const { return has_(sensor_response_); }


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
                sensor_response_.fill(SensorT{});
            }
        }

    private:
        FrameBuffer(Resolution resolution)
            : resolution_{ resolution } {
        }

        Resolution resolution_;

        Image<float> depth_;
        Image<uint64_t> mesh_ids_;
        Image<Vec3<float>> camera_normals_;
        Image<Vec3<float>> world_normals_;

        Image<TSpectral> received_power_;
        Image<SensorT> sensor_response_;


        template <IsImagePixel T>
        bool has_(const Image<T>& image) const {
            return (image.width() == resolution_.width && image.height() == resolution_.height);
        }

        template <IsImagePixel T>
        void enable_(Image<T>& image, const T& fill_value, bool enable) {
            if (!enable) {
                image = Image<T>(0, 0);
                return;
            }
            if (image.width() != resolution_.width || image.height() != resolution_.height) {
                image = Image<T>(resolution_.width, resolution_.height, fill_value);
            }
        }

        friend class CameraModel<TSpectral>;
    };
}
