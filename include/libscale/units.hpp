#pragma once

// NOLINTBEGIN(misc-non-private-member-variables-in-classes): Physical units wrapper structs provide
// direct .value member access for performance and mathematical syntax clarity.

#include <compare>
#include <concepts>

/**
 * @brief Strong units and physical conversion types namespace.
 */
namespace libscale::units {

/**
 * @brief Base template for strong types to prevent mixing different physical units.
 * Provides standard arithmetic and relational operators to enable type-safe physical calculations.
 * @tparam UnderlyingType Underlying numeric type (e.g. double, int).
 * @tparam TagType Tag type used to uniquely identify the physical unit.
 */
template <typename UnderlyingType, typename TagType>
struct Unit {
    /// The underlying numeric value type representation.
    using value_type = UnderlyingType;
    /// The raw value of the strong unit.
    UnderlyingType value{};

    /**
     * @brief Default constructor. Initializes value to default.
     */
    Unit() = default;

    /**
     * @brief Explicit constructor initializing the strong unit with a raw value.
     * @param val Raw value.
     */
    explicit constexpr Unit(UnderlyingType val) : value(val) {}

    /**
     * @brief Default three-way comparison operator (spaceship operator).
     * Enables full relational comparison operations (<, <=, >, >=, ==, !=) on identical units.
     */
    auto operator<=>(const Unit&) const = default;

    /**
     * @brief Adds another unit of the same type.
     * @param other The unit to add.
     * @return A new Unit containing the summed values.
     */
    constexpr auto operator+(const Unit& other) const -> Unit {
        return Unit(value + other.value);
    }

    /**
     * @brief Subtracts another unit of the same type.
     * @param other The unit to subtract.
     * @return A new Unit containing the subtracted values.
     */
    constexpr auto operator-(const Unit& other) const -> Unit {
        return Unit(value - other.value);
    }

    /**
     * @brief Adds and assigns another unit of the same type.
     * @param other The unit to add.
     * @return Reference to this unit.
     */
    constexpr auto operator+=(const Unit& other) -> Unit& {
        value += other.value;
        return *this;
    }

    /**
     * @brief Subtracts and assigns another unit of the same type.
     * @param other The unit to subtract.
     * @return Reference to this unit.
     */
    constexpr auto operator-=(const Unit& other) -> Unit& {
        value -= other.value;
        return *this;
    }

    /**
     * @brief Multiplies the physical unit by a scalar multiplier.
     * @tparam ScalarType Scalar type convertible to the underlying type UnderlyingType.
     * @param scalar The scalar multiplier.
     * @return A new Unit containing the scaled value.
     */
    template <std::convertible_to<UnderlyingType> ScalarType>
    constexpr auto operator*(ScalarType scalar) const -> Unit {
        return Unit(value * static_cast<UnderlyingType>(scalar));
    }

    /**
     * @brief Divides the physical unit by a scalar divisor.
     * @tparam ScalarType Scalar type convertible to the underlying type UnderlyingType.
     * @param scalar The scalar divisor.
     * @return A new Unit containing the divided value.
     */
    template <std::convertible_to<UnderlyingType> ScalarType>
    constexpr auto operator/(ScalarType scalar) const -> Unit {
        return Unit(value / static_cast<UnderlyingType>(scalar));
    }
};

/// Tag identifying centimeters.
struct CmTag {};
/// Strong unit representing a length in centimeters.
struct Cm : Unit<int, CmTag> {
    using Unit::Unit;
};

/// Tag identifying kilograms.
struct KgTag {};
/// Strong unit representing a weight in kilograms.
struct Kg : Unit<double, KgTag> {
    using Unit::Unit;
};

/// Tag identifying years duration.
struct YearsTag {};
/// Strong unit representing duration or age in years.
struct Years : Unit<int, YearsTag> {
    using Unit::Unit;
};

/// Tag identifying a specific calendar year.
struct YearTag {};
/// Strong unit representing a calendar year (e.g. 2026).
struct Year : Unit<int, YearTag> {
    using Unit::Unit;
};

/// Tag identifying calendar months.
struct MonthTag {};
/// Strong unit representing a calendar month (1-12) or duration in months.
struct Month : Unit<int, MonthTag> {
    using Unit::Unit;
};

/// Tag identifying calendar days.
struct DayTag {};
/// Strong unit representing a calendar day (1-31) or duration in days.
struct Day : Unit<int, DayTag> {
    using Unit::Unit;
};

/// Tag identifying percentages.
struct PercentTag {};
/// Strong unit representing a percentage (0.0 to 100.0).
struct Percent : Unit<double, PercentTag> {
    using Unit::Unit;
};

/// Tag identifying kilocalories.
struct KcalTag {};
/// Strong unit representing energy requirements or consumption in kilocalories.
struct Kcal : Unit<int, KcalTag> {
    using Unit::Unit;
};

/// Tag identifying electrical resistance in ohms.
struct OhmTag {};
/// Strong unit representing electrical impedance or resistance in ohms.
struct Ohm : Unit<double, OhmTag> {
    using Unit::Unit;
};

}  // namespace libscale::units
// NOLINTEND(misc-non-private-member-variables-in-classes)
