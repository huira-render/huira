#pragma once

#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtx/string_cast.hpp"

#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/diagnostics/exceptions.hpp"

namespace huira::detail {
	template <IsFloatingPoint T>
	void validateReal(T value, const std::string& name)
	{
		if (std::isinf(value) || std::isnan(value)) {
			throw FatalError("Provided " + name + " contains INF or NaN",
				name + " = " + std::to_string(value));
		}
	}
	
	template <IsFloatingPoint T>
	void validateStrictlyPositive(T value, const std::string& name)
	{
		validateReal(value, name);
		if (value <= 0) {
			throw FatalError("Provided " + name + " is negative or zero",
				name + " = " + std::to_string(value));
		}
	}
	
	template <IsVec T>
	void validateReal(const T& vec, const std::string& name)
	{
		for (int i = 0; i < T::length(); ++i) {
			if (std::isinf(vec[i]) || std::isnan(vec[i])) {
				throw FatalError("Provided " + name + " contains INF or NaN",
					name + " = " + glm::to_string(vec));
			}
		}
	}
	
	template <IsVec T>
	void validateStrictlyPositive(const T& vec, const std::string& name)
	{
		validateReal(vec, name);
		for (int i = 0; i < T::length(); ++i) {
			if (vec[i] <= 0) {
				throw FatalError("Provided " + name + " contains negative values",
					name + " = " + glm::to_string(vec));
			}
		}
	}
	
	template <IsMat T>
	void validateReal(const T& mat, const std::string& name)
	{
		for (int col = 0; col < mat.length(); ++col) {
			for (int row = 0; row < mat[col].length(); ++row) {
				if (std::isinf(mat[col][row]) || std::isnan(mat[col][row])) {
					throw FatalError("Provided " + name + " contains INF or NaN",
						name + " = " + glm::to_string(mat));
				}
			}
		}
	}
}
