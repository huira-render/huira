#pragma once

#include <cmath>
#include <string>

#include "huira/concepts/numeric_concepts.hpp"
#include "huira/concepts/spectral_concepts.hpp"
#include "huira/core/types.hpp"
#include "huira/util/logger.hpp"

namespace huira {
template <IsFloatingPoint T>
void validate_real(T value, const std::string& name)
{
    if (std::isinf(value) || std::isnan(value)) {
        HUIRA_THROW_ERROR("Provided " + name + " is INF or NaN: " + std::to_string(value));
    }
}

template <IsFloatingPoint T>
void validate_strictly_positive(T value, const std::string& name)
{
    validate_real(value, name);
    if (value <= 0) {
        HUIRA_THROW_ERROR("Provided " + name + " is negative or zero: " + std::to_string(value));
    }
}

template <IsFloatingPoint T>
void validate_non_negative(T value, const std::string& name)
{
    validate_real(value, name);
    if (value < 0) {
        HUIRA_THROW_ERROR("Provided " + name + " is negative: " + std::to_string(value));
    }
}

/// Throws unless the value is finite and from 0 to 1.
template <IsFloatingPoint T>
void validate_unit_interval(T value, const std::string& name)
{
    validate_real(value, name);
    if (value < 0 || value > 1) {
        HUIRA_THROW_ERROR("Provided " + name + " is not from 0 to 1: " + std::to_string(value));
    }
}

template <IsVec T>
void validate_real(const T& vec, const std::string& name)
{
    for (int i = 0; i < T::length(); ++i) {
        if (std::isinf(vec[i]) || std::isnan(vec[i])) {
            HUIRA_THROW_ERROR("Provided " + name +
                              " contains INF or NaN: " + huira::vec_to_string(vec));
        }
    }
}

template <IsVec T>
void validate_strictly_positive(const T& vec, const std::string& name)
{
    validate_real(vec, name);
    for (int i = 0; i < T::length(); ++i) {
        if (vec[i] <= 0) {
            HUIRA_THROW_ERROR("Provided " + name +
                              " contains negative or zero values: " + huira::vec_to_string(vec));
        }
    }
}

/// Throws unless every value of a spectrum is finite and non-negative, as light is.
template <IsSpectral TSpectral>
void validate_spectrum(const TSpectral& spectrum, const std::string& name)
{
    if (!spectrum.valid()) {
        HUIRA_THROW_ERROR("Provided " + name +
                          " must be finite and non-negative: " + spectrum.to_string());
    }
}

/// Throws unless every value of a spectrum is finite and from 0 to 1, as an albedo is.
template <IsSpectral TSpectral>
void validate_spectral_ratio(const TSpectral& spectrum, const std::string& name)
{
    if (!spectrum.valid_ratio()) {
        HUIRA_THROW_ERROR("Provided " + name +
                          " must be finite and from 0 to 1: " + spectrum.to_string());
    }
}

template <IsMat T>
void validate_real(const T& mat, const std::string& name)
{
    for (int col = 0; col < mat.length(); ++col) {
        for (int row = 0; row < mat[col].length(); ++row) {
            if (std::isinf(mat[col][row]) || std::isnan(mat[col][row])) {
                HUIRA_THROW_ERROR("Provided " + name +
                                  " contains INF or NaN: " + huira::mat_to_string(mat));
            }
        }
    }
}
} // namespace huira
