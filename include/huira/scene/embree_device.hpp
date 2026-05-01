#pragma once

#include <memory>
#include <string>
#include <type_traits>

#include "embree4/rtcore.h"
#include "huira/util/logger.hpp"

namespace huira {
struct RTCDeviceDeleter {
    void operator()(RTCDevice device) const noexcept
    {
        if (device) {
            rtcReleaseDevice(device);
        }
    }
};

struct RTCSceneDeleter {
    void operator()(RTCScene scene) const noexcept
    {
        if (scene) {
            rtcReleaseScene(scene);
        }
    }
};

using UniqueRTCDevice = std::unique_ptr<std::remove_pointer_t<RTCDevice>, RTCDeviceDeleter>;
using UniqueRTCScene = std::unique_ptr<std::remove_pointer_t<RTCScene>, RTCSceneDeleter>;

class EmbreeDevice {
  public:
    EmbreeDevice(const char* config = nullptr)
    {
        // Use .reset() to take ownership of the newly created device
        device_.reset(rtcNewDevice(config));

        if (!device_) {
            HUIRA_THROW_ERROR(
                "EmbreeDevice::EmbreeDevice - Failed to create Embree device (error: " +
                std::to_string(static_cast<int>(rtcGetDeviceError(nullptr))) + ").");
        }
    }

    ~EmbreeDevice() = default;

    EmbreeDevice(const EmbreeDevice&) = delete;
    EmbreeDevice& operator=(const EmbreeDevice&) = delete;

    EmbreeDevice(EmbreeDevice&&) noexcept = default;
    EmbreeDevice& operator=(EmbreeDevice&&) noexcept = default;

    RTCDevice get() const { return device_.get(); }

  private:
    UniqueRTCDevice device_;
};

} // namespace huira
