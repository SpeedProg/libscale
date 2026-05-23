#include "gui/history_row.hpp"

#include <gtkmm/box.h>
#include <gtkmm/enums.h>
#include <gtkmm/label.h>
#include <gtkmm/object.h>

#include <format>

#include "gui/settings.hpp"
#include "libscale/types.hpp"

namespace libscale {

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

    auto create_label = [this](int width) {
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

void HistoryRow::set_measurement(const Measurement& meas, UnitSystem units,
                                 bool use_v2) {  // NOLINT(misc-include-cleaner)
    m_date_label->set_text(std::format("{:%F}", meas.date));
    if (units == UnitSystem::Imperial) {
        m_weight_label->set_text(format_val(meas.weight.value * kg_to_lbs_factor, " lbs"));
    } else {
        m_weight_label->set_text(format_val(meas.weight.value, " kg"));
    }
    m_bmi_label->set_text(format_val(meas.bmi, ""));

    double body_fat = meas.body_fat.value;
    double water = meas.water.value;
    double muscle = meas.muscle_mass.value;

    if (use_v2) {
        if (meas.body_fat_v2.value > 0.0) {
            body_fat = meas.body_fat_v2.value;
        }
        if (meas.water_v2.value > 0.0) {
            water = meas.water_v2.value;
        }
        if (meas.muscle_mass_v2.value > 0.0) {
            muscle = meas.muscle_mass_v2.value;
        }
    }

    m_fat_label->set_text(format_val(body_fat, "%"));
    m_water_label->set_text(format_val(water, "%"));
    m_muscle_label->set_text(format_val(muscle, "%"));
}

auto HistoryRow::format_val(double val, const std::string& unit) -> std::string {
    return std::format("{:.1f}{}", val, unit);
}

}  // namespace libscale
