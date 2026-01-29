#pragma once

#include "huira/core/concepts/numeric_concepts.hpp"

namespace huira {
	template <IsFloatingPoint T>
	constexpr T PI() { return static_cast<T>(3.141592653589793238462643383279502884); }

    template <IsFloatingPoint T>
    constexpr T SPEED_OF_LIGHT() { return static_cast<T>(299792458.0); }
}
