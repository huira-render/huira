#pragma once

#include <vector>
#include <memory>
#include <unordered_map>
#include <stdexcept>
#include <string>

#include "huira/radiometry/spectral_bins.hpp"

#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"

#include "huira/scene/reference_frame.hpp"

#include "huira/handles/reference_frame_handle.hpp"

namespace huira {
	template <IsSpectral TSpectral, IsFloatingPoint TFloat>
	class Scene {
	public:
        void lock() {
            locked_ = true;
        }

        void unlock() {
            locked_ = false;
        }

        ReferenceFrameHandle<TSpectral, TFloat> create_reference_frame(const std::string& name) {
            if (locked_) {
                throw std::runtime_error("Attempted to modify a locked scene");
            }

            if (reference_frames_.contains(name)) {
                throw std::runtime_error("Reference frame already exists: " + name);
            }

            auto frame = std::make_shared<ReferenceFrame<TSpectral, TFloat>>(this);
            reference_frames_[name] = frame;
            return ReferenceFrameHandle<TSpectral, TFloat>{ frame, &locked_ };
        }

        ReferenceFrameHandle<TSpectral, TFloat> get_handle(const std::string& name) {
            if (auto it = reference_frames_.find(name); it != reference_frames_.end()) {
                return ReferenceFrameHandle<TSpectral, TFloat>{ it->second, &locked_ };
            }
            throw std::runtime_error("No ReferenceFrame named: " + name);
        }

        const std::string& name_of(const ReferenceFrame<TSpectral, TFloat>* frame) const {
            for (const auto& [name, ptr] : reference_frames_) {
                if (ptr.get() == frame) {
                    return name;
                }
            }
            throw std::runtime_error("ReferenceFrame not found in scene");
        }

        const std::string& name_of(const ReferenceFrameHandle<TSpectral, TFloat>& handle) const {
            return handle.name();
        }

	private:
        bool locked_ = false;

        std::unordered_map<std::string, std::shared_ptr<ReferenceFrame<TSpectral, TFloat>>> reference_frames_;
	};
}

#include "huira_impl/scene/scene.ipp"

