#pragma once

#include "huira/concepts/numeric_concepts.hpp"

namespace huira {
	template <IsFloatingPoint T>
	constexpr T PI() { return static_cast<T>(3.141592653589793238462643383279502884); }
}