namespace huira::units {
    /**
     * @brief Get the value in SI units
     *
     * @tparam Dim The dimensionality of the quantity
     * @tparam Scale The scale type of the quantity
     * @return double The value converted to SI units
     *
     * Converts the internal value to the SI base unit for this dimension
     * by applying the appropriate scale conversion.
     */
    template<IsDimensionality Dim, IsRatioOrTag Scale>
    double Quantity<Dim, Scale>::get_si_value() const
    {
        return this->to_si(value_);
    }

    /**
     * @brief Get the raw value in the current scale
     *
     * @tparam Dim The dimensionality of the quantity
     * @tparam Scale The scale type of the quantity
     * @return double The value in the current unit scale
     *
     * Returns the stored value without any unit conversion.
     */
    template<IsDimensionality Dim, IsRatioOrTag Scale>
    double Quantity<Dim, Scale>::value() const
    {
        return value_;
    }

    /**
     * @brief Get the raw value (alias for value())
     *
     * @tparam Dim The dimensionality of the quantity
     * @tparam Scale The scale type of the quantity
     * @return double The value in the current unit scale
     *
     * Returns the stored value without any unit conversion.
     * This is an alias for value() provided for compatibility.
     */
    template<IsDimensionality Dim, IsRatioOrTag Scale>
    double Quantity<Dim, Scale>::raw_value() const
    {
        return value_;
    }

    /**
     * @brief Convert quantity to string representation
     *
     * @tparam Dim The dimensionality of the quantity
     * @tparam Scale The scale type of the quantity
     * @return std::string String representation showing value and SI unit
     *
     * Formats the quantity as a string with the SI value followed by
     * the SI unit string for this dimension.
     */
    template<IsDimensionality Dim, IsRatioOrTag Scale>
    std::string Quantity<Dim, Scale>::to_string() const
    {
        std::string output = std::to_string(get_si_value());
        output += " " + Dim::to_si_string();
        return output;
    }

    // ========================================================================
    // Dimensionless specialization member function definitions
    // ========================================================================

    /**
     * @brief Get the value in SI units (dimensionless specialization)
     *
     * @tparam Scale The scale type of the dimensionless quantity
     * @return double The value converted to SI units
     *
     * For dimensionless quantities, converts the internal value using
     * the scale factor.
     */
    template<IsRatioOrTag Scale>
    double Quantity<Dimensionless, Scale>::get_si_value() const
    {
        return this->to_si(value_);
    }

    /**
     * @brief Get the raw value in the current scale (dimensionless specialization)
     *
     * @tparam Scale The scale type of the dimensionless quantity
     * @return double The value in the current unit scale
     *
     * Returns the stored value without any unit conversion.
     */
    template<IsRatioOrTag Scale>
    double Quantity<Dimensionless, Scale>::value() const
    {
        return value_;
    }

    /**
     * @brief Get the raw value (dimensionless specialization)
     *
     * @tparam Scale The scale type of the dimensionless quantity
     * @return double The value in the current unit scale
     *
     * Returns the stored value without any unit conversion.
     * This is an alias for value() provided for compatibility.
     */
    template<IsRatioOrTag Scale>
    double Quantity<Dimensionless, Scale>::raw_value() const
    {
        return value_;
    }

    /**
     * @brief Convert dimensionless quantity to string representation
     *
     * @tparam Scale The scale type of the dimensionless quantity
     * @return std::string String representation of the numeric value
     *
     * Formats the dimensionless quantity as a string showing only
     * the numeric value in SI units (no unit suffix).
     */
    template<IsRatioOrTag Scale>
    std::string Quantity<Dimensionless, Scale>::to_string() const
    {
        return std::to_string(get_si_value());
    }

}
