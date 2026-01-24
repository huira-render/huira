#pragma once

#include <random>

#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/core/types.hpp"

namespace huira {
    template <IsFloatingPoint TFloat>
    class Sampler {
    public:
        virtual ~Sampler() = default;
        virtual TFloat get_1d() = 0;
        virtual Vec2<TFloat> get_2d() = 0;
    };

    template <IsFloatingPoint TFloat>
    class RandomSampler : public Sampler<TFloat> {
    public:
        RandomSampler() : rng(std::random_device{}()) {}
        explicit RandomSampler(uint32_t seed) : rng(seed) {}

        TFloat get_1d() override { return dist(rng); }

        Vec2<TFloat> get_2d() override { return { dist(rng), dist(rng) }; }

    private:
        std::mt19937 rng;
        std::uniform_real_distribution<TFloat> dist{ TFloat(0)(1) };
    };

    template <IsFloatingPoint TFloat>
    class DeterministicSampler : public Sampler<TFloat> {
    public:
        DeterministicSampler() : value_1d(TFloat(0.5)), value_2d{ TFloat(0.5)(0.5) } {}
        DeterministicSampler(TFloat v1d, Vec2<TFloat> v2d) : value_1d(v1d), value_2d(v2d) {}

        TFloat get_1d() override { return value_1d; }

        Vec2<TFloat> get_2d() override { return value_2d; }

    private:
        TFloat value_1d;
        Vec2<TFloat> value_2d;
    };
}
