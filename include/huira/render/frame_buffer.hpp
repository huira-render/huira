#pragma once

#include <cstddef>
#include <limits>
#include <type_traits>

#include "huira/images/image.hpp"
#include "huira/scene/object_ids.hpp"

namespace huira {
template <IsSpectral TSpectral>
class CameraModel;

template <IsSpectral TSpectral>
class FrameBuffer {
  public:
    using SensorT = std::conditional_t<std::is_same_v<TSpectral, RGB>, RGB, float>;

    FrameBuffer() = delete;

    Resolution resolution() const { return resolution_; }
    int width() const { return resolution_.width; }
    int height() const { return resolution_.height; }

    void enable_depth(bool enable = true)
    {
        enable_(depth_, std::numeric_limits<float>::infinity(), enable);
    }
    Image<float>& depth() { return depth_; }
    bool has_depth() const { return has_(depth_); }

    void enable_albedo(bool enable = true) { enable_(albedo_, TSpectral{0}, enable); }
    Image<TSpectral>& albedo() { return albedo_; }
    bool has_albedo() const { return has_(albedo_); }

    /**
     * @brief Enable the IDs of the geometry (mesh or ellipsoid) each pixel sees.
     *
     * This and the model, instance and part IDs are those of the nearest surface the pixel's
     * camera rays hit, the one depth() records (see ObjectIds). A pixel that sees nothing, or
     * only a light, holds NO_OBJECT_ID.
     */
    void enable_geometry_ids(bool enable = true) { enable_(geometry_ids_, NO_OBJECT_ID, enable); }
    Image<uint64_t>& geometry_ids() { return geometry_ids_; }
    bool has_geometry_ids() const { return has_(geometry_ids_); }

    /// Enable the IDs of the model each pixel sees: NO_OBJECT_ID where the primitive seen was
    /// placed directly rather than by a model.
    void enable_model_ids(bool enable = true) { enable_(model_ids_, NO_OBJECT_ID, enable); }
    Image<uint64_t>& model_ids() { return model_ids_; }
    bool has_model_ids() const { return has_(model_ids_); }

    /// Enable the IDs of the instance in the scene graph that placed what each pixel sees.
    void enable_instance_ids(bool enable = true) { enable_(instance_ids_, NO_OBJECT_ID, enable); }
    Image<uint64_t>& instance_ids() { return instance_ids_; }
    bool has_instance_ids() const { return has_(instance_ids_); }

    /// Enable the IDs of the part each pixel sees: the innermost instance that placed the
    /// primitive, within a model, or the instance itself for a primitive placed directly.
    void enable_part_ids(bool enable = true) { enable_(part_ids_, NO_OBJECT_ID, enable); }
    Image<uint64_t>& part_ids() { return part_ids_; }
    bool has_part_ids() const { return has_(part_ids_); }

    /// Whether any of the object ID images is enabled.
    bool has_object_ids() const
    {
        return has_geometry_ids() || has_model_ids() || has_instance_ids() || has_part_ids();
    }

    /// Record the IDs of what pixel (x, y) sees in the enabled object ID images.
    void set_object_ids(int x, int y, const ObjectIds& ids)
    {
        if (has_geometry_ids()) {
            geometry_ids_(x, y) = ids.geometry;
        }
        if (has_model_ids()) {
            model_ids_(x, y) = ids.model;
        }
        if (has_instance_ids()) {
            instance_ids_(x, y) = ids.instance;
        }
        if (has_part_ids()) {
            part_ids_(x, y) = ids.part;
        }
    }

    void enable_camera_normals(bool enable = true)
    {
        enable_(camera_normals_, Vec3<float>{0, 0, 0}, enable);
    }
    Image<Vec3<float>>& camera_normals() { return camera_normals_; }
    bool has_camera_normals() const { return has_(camera_normals_); }

    void enable_world_normals(bool enable = true)
    {
        enable_(world_normals_, Vec3<float>{0, 0, 0}, enable);
    }
    Image<Vec3<float>>& world_normals() { return world_normals_; }
    bool has_world_normals() const { return has_(world_normals_); }

    /**
     * @brief Enable the direct-illumination component of received power.
     *
     * Also enables received_power(), which the direct and indirect components are a
     * decomposition of: the renderer's unresolved-source, veiling-glare and sensor
     * stages all key off the total, and the invariant
     * received_power == received_direct_power + received_indirect_power
     * only holds if the total is actually produced.
     */
    void enable_received_direct_power(bool enable = true)
    {
        if (enable) {
            enable_(received_power_, TSpectral{0}, true);
        }
        enable_(received_direct_power_, TSpectral{0}, enable);
    }
    Image<TSpectral>& received_direct_power() { return received_direct_power_; }
    bool has_received_direct_power() const { return has_(received_direct_power_); }

    /**
     * @brief Enable the indirect-illumination component of received power.
     *
     * See enable_received_direct_power() for why this also enables the total.
     */
    void enable_received_indirect_power(bool enable = true)
    {
        if (enable) {
            enable_(received_power_, TSpectral{0}, true);
        }
        enable_(received_indirect_power_, TSpectral{0}, enable);
    }
    Image<TSpectral>& received_indirect_power() { return received_indirect_power_; }
    bool has_received_indirect_power() const { return has_(received_indirect_power_); }

    void enable_received_power(bool enable = true)
    {
        enable_(received_power_, TSpectral{0}, enable);
    }
    Image<TSpectral>& received_power() { return received_power_; }
    bool has_received_power() const { return has_(received_power_); }

    /**
     * @brief Enable or disable the sensor response.
     *
     * Enabling it also enables received_power(), which the sensor reads out. Disabling it leaves
     * received_power() as it is.
     */
    void enable_sensor_response(bool enable = true)
    {
        if (enable) {
            enable_(received_power_, TSpectral{0}, true);
        }
        enable_(sensor_response_, SensorT{}, enable);
    }
    Image<SensorT>& sensor_response() { return sensor_response_; }
    bool has_sensor_response() const { return has_(sensor_response_); }

    void clear()
    {
        if (has_depth()) {
            depth_.fill(std::numeric_limits<float>::infinity());
        }
        if (has_albedo()) {
            albedo_.fill(TSpectral{0});
        }
        if (has_geometry_ids()) {
            geometry_ids_.fill(NO_OBJECT_ID);
        }
        if (has_model_ids()) {
            model_ids_.fill(NO_OBJECT_ID);
        }
        if (has_instance_ids()) {
            instance_ids_.fill(NO_OBJECT_ID);
        }
        if (has_part_ids()) {
            part_ids_.fill(NO_OBJECT_ID);
        }
        if (has_camera_normals()) {
            camera_normals_.fill(Vec3<float>{0, 0, 0});
        }
        if (has_world_normals()) {
            world_normals_.fill(Vec3<float>{0, 0, 0});
        }
        if (has_received_power()) {
            received_power_.fill(TSpectral{0});
        }
        if (has_received_direct_power()) {
            received_direct_power_.fill(TSpectral{0});
        }
        if (has_received_indirect_power()) {
            received_indirect_power_.fill(TSpectral{0});
        }
        if (has_sensor_response()) {
            sensor_response_.fill(SensorT{});
        }
    }

  private:
    FrameBuffer(Resolution resolution) : resolution_{resolution} {}

    Resolution resolution_;

    Image<float> depth_;
    Image<TSpectral> albedo_;
    Image<uint64_t> geometry_ids_;
    Image<uint64_t> model_ids_;
    Image<uint64_t> instance_ids_;
    Image<uint64_t> part_ids_;
    Image<Vec3<float>> camera_normals_;
    Image<Vec3<float>> world_normals_;

    Image<TSpectral> received_power_;
    Image<TSpectral> received_direct_power_;
    Image<TSpectral> received_indirect_power_;
    Image<SensorT> sensor_response_;

    template <IsImagePixel T>
    bool has_(const Image<T>& image) const
    {
        return (image.width() == resolution_.width && image.height() == resolution_.height);
    }

    template <IsImagePixel T>
    void enable_(Image<T>& image, const T& fill_value, bool enable)
    {
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
} // namespace huira
