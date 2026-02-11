#pragma once

#include "huira/core/concepts/numeric_concepts.hpp"

namespace huira {
	template <IsFloatingPoint T>
	constexpr T PI() { return static_cast<T>(3.141592653589793238462643383279502884); }

    template <IsFloatingPoint T>
    constexpr T SPEED_OF_LIGHT() { return static_cast<T>(299792458.0); }

    template <IsFloatingPoint T>
    constexpr T H_PLANCK() { return static_cast<T>(6.62607015e-34); }

    template <IsFloatingPoint T>
    constexpr T K_BOLTZ() { return static_cast<T>(1.380649e-23); }

    template <IsFloatingPoint T>
    constexpr T AU() { return static_cast<T>(149597870700.0); }
}
