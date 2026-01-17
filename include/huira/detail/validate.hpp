#pragma once

#include "huira/core/types.hpp"

#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/logger.hpp"

namespace huira::detail {
	template <IsFloatingPoint T>
	void validateReal(T value, const std::string& name)
	{
		if (std::isinf(value) || std::isnan(value)) {
			HUIRA_THROW_ERROR("Provided " + name + " contains INF or NaN" + std::to_string(value));
		}
	}
	
	template <IsFloatingPoint T>
	void validateStrictlyPositive(T value, const std::string& name)
	{
		validateReal(value, name);
		if (value <= 0) {
            HUIRA_THROW_ERROR("Provided " + name + " is negative or zero: " + std::to_string(value));
		}
	}
	
	template <IsVec T>
	void validateReal(const T& vec, const std::string& name)
	{
		for (int i = 0; i < T::length(); ++i) {
			if (std::isinf(vec[i]) || std::isnan(vec[i])) {
                HUIRA_THROW_ERROR("Provided " + name + " contains INF or NaN: " + huira::vec_to_string(vec));
			}
		}
	}
	
	template <IsVec T>
	void validateStrictlyPositive(const T& vec, const std::string& name)
	{
		validateReal(vec, name);
		for (int i = 0; i < T::length(); ++i) {
			if (vec[i] <= 0) {
                HUIRA_THROW_ERROR("Provided " + name + " contains negative values: " + huira::vec_to_string(vec));
			}
		}
	}
	
	template <IsMat T>
	void validateReal(const T& mat, const std::string& name)
	{
		for (int col = 0; col < mat.length(); ++col) {
			for (int row = 0; row < mat[col].length(); ++row) {
				if (std::isinf(mat[col][row]) || std::isnan(mat[col][row])) {
					HUIRA_THROW_ERROR("Provided " + name + " contains INF or NaN: " + huira::mat_to_string(mat));
				}
			}
		}
	}
}
