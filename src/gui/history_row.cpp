#include "gui/history_row.hpp"

#include <gtkmm/box.h>
#include <gtkmm/enums.h>
#include <gtkmm/label.h>
#include <gtkmm/object.h>

#include <format>

#include "gui/metrics.hpp"
#include "gui/settings.hpp"
#include "libscale/types.hpp"

namespace comfort_select {

static constexpr int list_item_spacing = 10;
static constexpr int list_item_margin = 5;
static constexpr int col_width_date = 150;
static constexpr int col_width_weight = 80;
static constexpr int col_width_bmi = 60;
static constexpr int col_width_fat = 60;
static constexpr int col_width_water = 60;
static constexpr int col_width_muscle = 60;

HistoryRow::HistoryRow() : Gtk::Box(Gtk::Orientation::HORIZONTAL, list_item_spacing) {
    set_margin(list_item_margin);

    auto create_label = [this](int width) -> Gtk::Label* {
        auto* label = Gtk::make_managed<Gtk::Label>();
        label->set_size_request(width, -1);
        label->set_xalign(0.0);
        append(*label);
        return label;
    };

    m_date_label = create_label(col_width_date);
    m_weight_label = create_label(col_width_weight);
    m_bmi_label = create_label(col_width_bmi);
    m_fat_label = create_label(col_width_fat);
    m_water_label = create_label(col_width_water);
    m_muscle_label = create_label(col_width_muscle);
}

void HistoryRow::set_measurement(const Measurement& meas, UnitSystem units, bool use_v2) {
    m_date_label->set_text(std::format("{:%F}", meas.date));
    if (units == UnitSystem::Imperial) {
        m_weight_label->set_text(format_val(meas.weight.value * kg_to_lbs_factor, " lbs"));
    } else {
        m_weight_label->set_text(format_val(meas.weight.value, " kg"));
    }
    m_bmi_label->set_text(format_val(meas.bmi, ""));

    m_fat_label->set_text(format_val(effective_body_fat(meas, use_v2).value, "%"));
    m_water_label->set_text(format_val(effective_water(meas, use_v2).value, "%"));
    m_muscle_label->set_text(format_val(effective_muscle_mass(meas, use_v2).value, "%"));
}

auto HistoryRow::format_val(double val, const std::string& unit) -> std::string {
    return std::format("{:.1f}{}", val, unit);
}

}  // namespace comfort_select
