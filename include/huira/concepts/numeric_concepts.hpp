#pragma once

#include <type_traits>
#include <concepts>

#include <ratio>

namespace huira {
	template<typename T>
	concept IsFloatingPoint = std::same_as<T, float> || std::same_as<T, double>;

	template<typename T>
	concept IsNumeric = std::is_arithmetic_v<T>;


	// =============================== //
	// === Concepts For std::ratio === //
	// =============================== //
	template<typename T>
	struct is_std_ratio_impl : std::false_type {};

	template<std::intmax_t Num, std::intmax_t Den>
	struct is_std_ratio_impl<std::ratio<Num, Den>> : std::true_type {};

	template<typename T>
	concept IsRatio = is_std_ratio_impl<T>::value;


}