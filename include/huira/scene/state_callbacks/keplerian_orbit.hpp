#pragma once

#include <array>

#include "huira/core/spice.hpp"

#include "huira/units/units.hpp"
#include "huira/core/time.hpp"
#include "huira/scene/state_callbacks/state_callbacks.hpp"

namespace huira {
    struct KeplerianOrbitCallback : public PositionCallback {
        KeplerianOrbitCallback(units::Meter a, double e, units::Radian i, units::Radian raan, units::Radian argp, units::Radian M0, const Time& epoch, double mu)
        {
            elements_[0] = a.to_si() / 1000.0;
            elements_[1] = e;
            elements_[2] = i.to_si();
            elements_[3] = raan.to_si();
            elements_[4] = argp.to_si();
            elements_[5] = M0.to_si();
            elements_[6] = epoch.et();
            elements_[7] = mu;
        }

        void evaluate(const Time& t, const Rotation<double>& rotation, const Vec3<double>& angular_rate) override
        {
            (void)rotation;
            (void)angular_rate;
            
            auto [r, v] = spice::conics<double>(elements_, t);

            this->position = r;
            this->velocity = v;
        }

        std::array<double, 8> elements_;
    };
}
