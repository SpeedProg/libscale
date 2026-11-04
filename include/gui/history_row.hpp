#pragma once
#include <gtkmm/box.h>
#include <gtkmm/label.h>

#include "gui/settings.hpp"
#include "libscale/types.hpp"

namespace comfort_select {

using libscale::Measurement;

/**
 * @brief Custom GTK4 list item widget representing a single row in the measurement history table.
 * Adheres to SRP by managing the specific text formatting and grid alignment for a single
 * Measurement.
 */
class HistoryRow : public Gtk::Box {
   public:
    /**
     * @brief Constructs a new HistoryRow layout with labels for weight, fat, muscle, etc.
     */
    HistoryRow();

    /**
     * @brief Binds a computed measurement structure and formats the column labels with physical
     * units.
     * @param measurement Measurement record to format and bind.
     * @param units Metric or Imperial preference.
     */
    void set_measurement(const Measurement& measurement, UnitSystem units, bool use_v2 = false);

   private:
    Gtk::Label* m_date_label;
    Gtk::Label* m_weight_label;
    Gtk::Label* m_bmi_label;
    Gtk::Label* m_fat_label;
    Gtk::Label* m_water_label;
    Gtk::Label* m_muscle_label;

    static auto format_val(double val, const std::string& unit) -> std::string;
};

}  // namespace comfort_select
