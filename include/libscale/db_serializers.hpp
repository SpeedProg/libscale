#pragma once
#include <chrono>
#include <format>
#include <charconv>
#include <sqlite_orm/sqlite_orm.h>
#include "types.hpp"

/**
 * @brief Custom SQLite-ORM serialization converters namespace.
 */
namespace sqlite_orm {

// --- Cm Mappings ---

/**
 * @brief SQLite-ORM type printer specialization for strong unit Cm.
 * Tells SQLite to store Cm as an INTEGER.
 */
template<>
struct type_printer<libscale::units::Cm> : integer_printer {};

/**
 * @brief SQLite-ORM field printer specialization for strong unit Cm.
 */
template<>
struct field_printer<libscale::units::Cm> {
    /**
     * @brief Formats a Cm instance as a string.
     * @param val Cm unit.
     * @return Formatted string.
     */
    auto operator()(const libscale::units::Cm& val) const -> std::string { return std::to_string(val.value); }
};

/**
 * @brief SQLite-ORM statement binder specialization for strong unit Cm.
 */
template<>
struct statement_binder<libscale::units::Cm> {
    /**
     * @brief Binds a Cm value to an SQLite statement parameter.
     * @param stmt Statement pointer.
     * @param index Parameter index.
     * @param value Cm unit to bind.
     * @return SQLite status code.
     */
    static auto bind(sqlite3_stmt* stmt, int index, const libscale::units::Cm& value) -> int {
        return sqlite3_bind_int(stmt, index, value.value);
    }
};

/**
 * @brief SQLite-ORM row extractor specialization for strong unit Cm.
 */
template<>
struct row_extractor<libscale::units::Cm> {
    /**
     * @brief Extracts a Cm value from a statement column.
     * @param stmt Statement pointer.
     * @param columnIndex Column index.
     * @return Extracted Cm unit.
     */
    static auto extract(sqlite3_stmt* stmt, int columnIndex) -> libscale::units::Cm {
        return libscale::units::Cm{sqlite3_column_int(stmt, columnIndex)};
    }
    /**
     * @brief Extracts a Cm value from a SQLite value object.
     * @param value SQLite value.
     * @return Extracted Cm unit.
     */
    static auto extract(sqlite3_value* value) -> libscale::units::Cm {
        return libscale::units::Cm{sqlite3_value_int(value)};
    }
};

// --- Kg Mappings ---

/**
 * @brief SQLite-ORM type printer specialization for strong unit Kg.
 * Tells SQLite to store Kg as a REAL (double).
 */
template<>
struct type_printer<libscale::units::Kg> : real_printer {};

/**
 * @brief SQLite-ORM field printer specialization for strong unit Kg.
 */
template<>
struct field_printer<libscale::units::Kg> {
    /**
     * @brief Formats a Kg instance as a string.
     * @param val Kg unit.
     * @return Formatted string.
     */
    auto operator()(const libscale::units::Kg& val) const -> std::string { return std::to_string(val.value); }
};

/**
 * @brief SQLite-ORM statement binder specialization for strong unit Kg.
 */
template<>
struct statement_binder<libscale::units::Kg> {
    /**
     * @brief Binds a Kg value to an SQLite statement parameter.
     * @param stmt Statement pointer.
     * @param index Parameter index.
     * @param value Kg unit to bind.
     * @return SQLite status code.
     */
    static auto bind(sqlite3_stmt* stmt, int index, const libscale::units::Kg& value) -> int {
        return sqlite3_bind_double(stmt, index, value.value);
    }
};

/**
 * @brief SQLite-ORM row extractor specialization for strong unit Kg.
 */
template<>
struct row_extractor<libscale::units::Kg> {
    /**
     * @brief Extracts a Kg value from a statement column.
     * @param stmt Statement pointer.
     * @param columnIndex Column index.
     * @return Extracted Kg unit.
     */
    static auto extract(sqlite3_stmt* stmt, int columnIndex) -> libscale::units::Kg {
        return libscale::units::Kg{sqlite3_column_double(stmt, columnIndex)};
    }
    /**
     * @brief Extracts a Kg value from a SQLite value object.
     * @param value SQLite value.
     * @return Extracted Kg unit.
     */
    static auto extract(sqlite3_value* value) -> libscale::units::Kg {
        return libscale::units::Kg{sqlite3_value_double(value)};
    }
};

// --- Years Mappings ---

/**
 * @brief SQLite-ORM type printer specialization for strong unit Years.
 * Tells SQLite to store Years as an INTEGER.
 */
template<>
struct type_printer<libscale::units::Years> : integer_printer {};

/**
 * @brief SQLite-ORM field printer specialization for strong unit Years.
 */
template<>
struct field_printer<libscale::units::Years> {
    /**
     * @brief Formats a Years instance as a string.
     * @param val Years unit.
     * @return Formatted string.
     */
    auto operator()(const libscale::units::Years& val) const -> std::string { return std::to_string(val.value); }
};

/**
 * @brief SQLite-ORM statement binder specialization for strong unit Years.
 */
template<>
struct statement_binder<libscale::units::Years> {
    /**
     * @brief Binds a Years value to an SQLite statement parameter.
     * @param stmt Statement pointer.
     * @param index Parameter index.
     * @param value Years unit to bind.
     * @return SQLite status code.
     */
    static auto bind(sqlite3_stmt* stmt, int index, const libscale::units::Years& value) -> int {
        return sqlite3_bind_int(stmt, index, value.value);
    }
};

/**
 * @brief SQLite-ORM row extractor specialization for strong unit Years.
 */
template<>
struct row_extractor<libscale::units::Years> {
    /**
     * @brief Extracts a Years value from a statement column.
     * @param stmt Statement pointer.
     * @param columnIndex Column index.
     * @return Extracted Years unit.
     */
    static auto extract(sqlite3_stmt* stmt, int columnIndex) -> libscale::units::Years {
        return libscale::units::Years{sqlite3_column_int(stmt, columnIndex)};
    }
    /**
     * @brief Extracts a Years value from a SQLite value object.
     * @param value SQLite value.
     * @return Extracted Years unit.
     */
    static auto extract(sqlite3_value* value) -> libscale::units::Years {
        return libscale::units::Years{sqlite3_value_int(value)};
    }
};

// --- Percent Mappings ---

/**
 * @brief SQLite-ORM type printer specialization for strong unit Percent.
 * Tells SQLite to store Percent as a REAL (double).
 */
template<>
struct type_printer<libscale::units::Percent> : real_printer {};

/**
 * @brief SQLite-ORM field printer specialization for strong unit Percent.
 */
template<>
struct field_printer<libscale::units::Percent> {
    /**
     * @brief Formats a Percent instance as a string.
     * @param val Percent unit.
     * @return Formatted string.
     */
    auto operator()(const libscale::units::Percent& val) const -> std::string { return std::to_string(val.value); }
};

/**
 * @brief SQLite-ORM statement binder specialization for strong unit Percent.
 */
template<>
struct statement_binder<libscale::units::Percent> {
    /**
     * @brief Binds a Percent value to an SQLite statement parameter.
     * @param stmt Statement pointer.
     * @param index Parameter index.
     * @param value Percent unit to bind.
     * @return SQLite status code.
     */
    static auto bind(sqlite3_stmt* stmt, int index, const libscale::units::Percent& value) -> int {
        return sqlite3_bind_double(stmt, index, value.value);
    }
};

/**
 * @brief SQLite-ORM row extractor specialization for strong unit Percent.
 */
template<>
struct row_extractor<libscale::units::Percent> {
    /**
     * @brief Extracts a Percent value from a statement column.
     * @param stmt Statement pointer.
     * @param columnIndex Column index.
     * @return Extracted Percent unit.
     */
    static auto extract(sqlite3_stmt* stmt, int columnIndex) -> libscale::units::Percent {
        return libscale::units::Percent{sqlite3_column_double(stmt, columnIndex)};
    }
    /**
     * @brief Extracts a Percent value from a SQLite value object.
     * @param value SQLite value.
     * @return Extracted Percent unit.
     */
    static auto extract(sqlite3_value* value) -> libscale::units::Percent {
        return libscale::units::Percent{sqlite3_value_double(value)};
    }
};

// --- Kcal Mappings ---

/**
 * @brief SQLite-ORM type printer specialization for strong unit Kcal.
 * Tells SQLite to store Kcal as an INTEGER.
 */
template<>
struct type_printer<libscale::units::Kcal> : integer_printer {};

/**
 * @brief SQLite-ORM field printer specialization for strong unit Kcal.
 */
template<>
struct field_printer<libscale::units::Kcal> {
    /**
     * @brief Formats a Kcal instance as a string.
     * @param val Kcal unit.
     * @return Formatted string.
     */
    auto operator()(const libscale::units::Kcal& val) const -> std::string { return std::to_string(val.value); }
};

/**
 * @brief SQLite-ORM statement binder specialization for strong unit Kcal.
 */
template<>
struct statement_binder<libscale::units::Kcal> {
    /**
     * @brief Binds a Kcal value to an SQLite statement parameter.
     * @param stmt Statement pointer.
     * @param index Parameter index.
     * @param value Kcal unit to bind.
     * @return SQLite status code.
     */
    static auto bind(sqlite3_stmt* stmt, int index, const libscale::units::Kcal& value) -> int {
        return sqlite3_bind_int(stmt, index, value.value);
    }
};

/**
 * @brief SQLite-ORM row extractor specialization for strong unit Kcal.
 */
template<>
struct row_extractor<libscale::units::Kcal> {
    /**
     * @brief Extracts a Kcal value from a statement column.
     * @param stmt Statement pointer.
     * @param columnIndex Column index.
     * @return Extracted Kcal unit.
     */
    static auto extract(sqlite3_stmt* stmt, int columnIndex) -> libscale::units::Kcal {
        return libscale::units::Kcal{sqlite3_column_int(stmt, columnIndex)};
    }
    /**
     * @brief Extracts a Kcal value from a SQLite value object.
     * @param value SQLite value.
     * @return Extracted Kcal unit.
     */
    static auto extract(sqlite3_value* value) -> libscale::units::Kcal {
        return libscale::units::Kcal{sqlite3_value_int(value)};
    }
};

// --- Gender Mappings ---

/**
 * @brief SQLite-ORM type printer specialization for enum Gender.
 * Tells SQLite to store Gender as an INTEGER.
 */
template<>
struct type_printer<libscale::Gender> : integer_printer {};

/**
 * @brief SQLite-ORM field printer specialization for enum Gender.
 */
template<>
struct field_printer<libscale::Gender> {
    /**
     * @brief Formats a Gender value as a string.
     * @param val Gender enum.
     * @return Formatted string.
     */
    auto operator()(const libscale::Gender& val) const -> std::string { return std::to_string(static_cast<int>(val)); }
};

/**
 * @brief SQLite-ORM statement binder specialization for enum Gender.
 */
template<>
struct statement_binder<libscale::Gender> {
    /**
     * @brief Binds a Gender value to an SQLite statement parameter.
     * @param stmt Statement pointer.
     * @param index Parameter index.
     * @param value Gender enum to bind.
     * @return SQLite status code.
     */
    static auto bind(sqlite3_stmt* stmt, int index, const libscale::Gender& value) -> int {
        return sqlite3_bind_int(stmt, index, static_cast<int>(value));
    }
};

/**
 * @brief SQLite-ORM row extractor specialization for enum Gender.
 */
template<>
struct row_extractor<libscale::Gender> {
    /**
     * @brief Extracts a Gender value from a statement column.
     * @param stmt Statement pointer.
     * @param columnIndex Column index.
     * @return Extracted Gender enum.
     */
    static auto extract(sqlite3_stmt* stmt, int columnIndex) -> libscale::Gender {
        return static_cast<libscale::Gender>(sqlite3_column_int(stmt, columnIndex));
    }
    /**
     * @brief Extracts a Gender value from a SQLite value object.
     * @param value SQLite value.
     * @return Extracted Gender enum.
     */
    static auto extract(sqlite3_value* value) -> libscale::Gender {
        return static_cast<libscale::Gender>(sqlite3_value_int(value));
    }
};

// --- ActivityLevel Mappings ---

/**
 * @brief SQLite-ORM type printer specialization for enum ActivityLevel.
 * Tells SQLite to store ActivityLevel as an INTEGER.
 */
template<>
struct type_printer<libscale::ActivityLevel> : integer_printer {};

/**
 * @brief SQLite-ORM field printer specialization for enum ActivityLevel.
 */
template<>
struct field_printer<libscale::ActivityLevel> {
    /**
     * @brief Formats an ActivityLevel value as a string.
     * @param val ActivityLevel enum.
     * @return Formatted string.
     */
    auto operator()(const libscale::ActivityLevel& val) const -> std::string { return std::to_string(static_cast<int>(val)); }
};

/**
 * @brief SQLite-ORM statement binder specialization for enum ActivityLevel.
 */
template<>
struct statement_binder<libscale::ActivityLevel> {
    /**
     * @brief Binds an ActivityLevel value to an SQLite statement parameter.
     * @param stmt Statement pointer.
     * @param index Parameter index.
     * @param value ActivityLevel enum to bind.
     * @return SQLite status code.
     */
    static auto bind(sqlite3_stmt* stmt, int index, const libscale::ActivityLevel& value) -> int {
        return sqlite3_bind_int(stmt, index, static_cast<int>(value));
    }
};

/**
 * @brief SQLite-ORM row extractor specialization for enum ActivityLevel.
 */
template<>
struct row_extractor<libscale::ActivityLevel> {
    /**
     * @brief Extracts an ActivityLevel value from a statement column.
     * @param stmt Statement pointer.
     * @param columnIndex Column index.
     * @return Extracted ActivityLevel enum.
     */
    static auto extract(sqlite3_stmt* stmt, int columnIndex) -> libscale::ActivityLevel {
        return static_cast<libscale::ActivityLevel>(sqlite3_column_int(stmt, columnIndex));
    }
    /**
     * @brief Extracts an ActivityLevel value from a SQLite value object.
     * @param value SQLite value.
     * @return Extracted ActivityLevel enum.
     */
    static auto extract(sqlite3_value* value) -> libscale::ActivityLevel {
        return static_cast<libscale::ActivityLevel>(sqlite3_value_int(value));
    }
};

// --- year_month_day Mappings ---

/**
 * @brief SQLite-ORM type printer specialization for year_month_day.
 * Tells SQLite to store year_month_day as TEXT.
 */
template<>
struct type_printer<std::chrono::year_month_day> : text_printer {};

/**
 * @brief SQLite-ORM field printer specialization for year_month_day.
 */
template<>
struct field_printer<std::chrono::year_month_day> {
    /**
     * @brief Formats a year_month_day instance as a string.
     * @param val Date.
     * @return Formatted string.
     */
    auto operator()(const std::chrono::year_month_day& val) const -> std::string { return std::format("'{:%F}'", val); }
};

/**
 * @brief SQLite-ORM statement binder specialization for year_month_day.
 */
template<>
struct statement_binder<std::chrono::year_month_day> {
    /**
     * @brief Binds a year_month_day value to an SQLite statement parameter as TEXT.
     * @param stmt Statement pointer.
     * @param index Parameter index.
     * @param value Date to bind.
     * @return SQLite status code.
     */
    static auto bind(sqlite3_stmt* stmt, int index, const std::chrono::year_month_day& value) -> int {
        std::string const str = std::format("{:%F}", value);
        return sqlite3_bind_text(stmt, index, str.c_str(), -1, SQLITE_TRANSIENT);
    }
};

/**
 * @brief SQLite-ORM row extractor specialization for year_month_day.
 */
template <>
struct row_extractor<std::chrono::year_month_day> {
    /**
     * @brief Extracts a year_month_day value from a statement column.
     * @param stmt Statement pointer.
     * @param columnIndex Column index.
     * @return Extracted date.
     */
    static auto extract(sqlite3_stmt* stmt, int columnIndex) -> std::chrono::year_month_day {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): SQLite C API stores text columns as unsigned char*, requiring cast to standard char* interface.
        const char* str = reinterpret_cast<const char*>(sqlite3_column_text(stmt, columnIndex));
        if (str == nullptr) {
            return {};
        }
        return deserialize(str);
    }
    /**
     * @brief Extracts a year_month_day value from a SQLite value object.
     * @param value SQLite value.
     * @return Extracted date.
     */
    static auto extract(sqlite3_value* value) -> std::chrono::year_month_day {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): SQLite C API stores text values as unsigned char*, requiring cast to standard char* interface.
        const char* str = reinterpret_cast<const char*>(sqlite3_value_text(value));
        if (str == nullptr) {
            return {};
        }
        return deserialize(str);
    }

   private:
    static constexpr std::size_t min_date_str_len = 10;  // "YYYY-MM-DD"
    [[nodiscard]] static auto deserialize(std::string_view date_str) -> std::chrono::year_month_day {
        if (date_str.length() < min_date_str_len) {
            return {};
        }
        int year_val = 0;
        int month_val = 0;
        int day_val = 0;
        const char* start = date_str.data();
        const char* end = date_str.data() + date_str.size();
        auto [p1, ec1] = std::from_chars(start, end, year_val);
        if (ec1 != std::errc{} || p1 == end || *p1 != '-') {
            return {};
        }
        auto [p2, ec2] = std::from_chars(p1 + 1, end, month_val);
        if (ec2 != std::errc{} || p2 == end || *p2 != '-') {
            return {};
        }
        auto [p3, ec3] = std::from_chars(p2 + 1, end, day_val);
        if (ec3 != std::errc{}) {
            return {};
        }
        return std::chrono::year(year_val) / month_val / day_val;
    }
};

} // namespace sqlite_orm
