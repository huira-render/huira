#pragma once

#include <type_traits>
#include <concepts>

namespace huira {
	template<typename T>
	concept IsFloatingPoint = std::same_as<T, float> || std::same_as<T, double>;
};