#pragma once

#include <memory>

#include "embree4/rtcore.h"

#include "huira/util/logger.hpp"

namespace huira {
    class EmbreeDevice {
    public:
        EmbreeDevice(const char* config = nullptr) {
            device_ = rtcNewDevice(config);
            if (!device_) {
                HUIRA_THROW_ERROR("EmbreeDevice::EmbreeDevice - Failed to create Embree device (error: "
                    + std::to_string(static_cast<int>(rtcGetDeviceError(nullptr))) + ").");
            }
        }

        ~EmbreeDevice() {
            if (device_) {
                rtcReleaseDevice(device_);
            }
        }

        // Prevent copying to maintain strict RAII
        EmbreeDevice(const EmbreeDevice&) = delete;
        EmbreeDevice& operator=(const EmbreeDevice&) = delete;

        // Allow moving if you need it, or delete it too

        RTCDevice get() const { return device_; }

    private:
        RTCDevice device_;
    };
}
