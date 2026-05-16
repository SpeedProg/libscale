#pragma once
#include <gtkmm/box.h>
#include <gtkmm/label.h>
#include "libscale/types.hpp"

namespace libscale {

/**
 * @brief Custom GTK4 list item widget representing a single row in the measurement history table.
 * Adheres to SRP by managing the specific text formatting and grid alignment for a single Measurement.
 */
class HistoryRow : public Gtk::Box {
   public:
    /**
     * @brief Constructs a new HistoryRow layout with labels for weight, fat, muscle, etc.
     */
    HistoryRow();
    
    /**
     * @brief Binds a computed measurement structure and formats the column labels with physical units.
     * @param measurement Measurement record to format and bind.
     */
    void set_measurement(const Measurement& measurement);

   private:
    Gtk::Label* m_date_label;
    Gtk::Label* m_weight_label;
    Gtk::Label* m_bmi_label;
    Gtk::Label* m_fat_label;
    Gtk::Label* m_water_label;
    Gtk::Label* m_muscle_label;

    static auto format_val(double val, const std::string& unit) -> std::string;
};

}  // namespace libscale
