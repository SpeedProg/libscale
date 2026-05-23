#include "gui/graph.hpp"

#include <cairomm/context.h>
#include <cairomm/refptr.h>
#include <gdkmm/rectangle.h>
#include <gtkmm/gestureclick.h>
#include <sigc++/functors/mem_fun.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <format>
#include <functional>
#include <numbers>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "gui/settings.hpp"
#include "libscale/types.hpp"

namespace libscale {

namespace {
constexpr double bg_color_component = 0.08;
constexpr double grid_color_component = 0.2;
constexpr double line_color_r = 0.0;
constexpr double line_color_g = 0.7;
constexpr double line_color_b = 1.0;
constexpr double line_width = 3.0;
constexpr double padding_size = 50.0;
constexpr double point_radius = 4.0;
constexpr double multiplier_2 = 2.0;
constexpr double compact_padding = 5.0;
constexpr double max_distance = 1000000.0;
constexpr double compact_threshold = 10.0;
constexpr double full_threshold = 20.0;
constexpr int popover_label_margin = 10;
constexpr int grid_label_offset = 5;
constexpr int x_label_offset = 5;
constexpr double min_range_threshold = 0.1;
constexpr double default_range_val = 1.0;
constexpr double range_margin_factor = 0.1;
constexpr double compact_line_width = 2.0;
constexpr double label_color_component = 0.6;
constexpr double divisor_2 = 2.0;
constexpr int grid_line_count = 3;
constexpr double grid_line_width = 1.0;
constexpr double point_border_width = 1.0;

class CoordinateMapper {
   public:
    // Mapper constructor requires min/max bounding coordinate pairs.
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    CoordinateMapper(double min_v, double max_v, double min_t, double max_t, int width_px,
                     int height_px, bool compact)
        : m_range(max_v - min_v),
          m_width(width_px),
          m_height(height_px),
          m_padding(compact ? compact_padding : padding_size) {
        if (m_range < min_range_threshold) {
            m_range = default_range_val;
        }
        m_min_val = min_v - m_range * range_margin_factor;
        m_max_val = max_v + m_range * range_margin_factor;
        m_range = m_max_val - m_min_val;

        m_min_time = min_t;
        m_max_time = max_t;
        m_time_range = m_max_time - m_min_time;
        if (m_time_range <= 0.0) {
            m_time_range = 1.0;
        }

        m_graph_width = m_width - multiplier_2 * m_padding;
        m_graph_height = m_height - multiplier_2 * m_padding;
    }

    // Mapping function converts 2D (val, time) points to screen coordinates.
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    [[nodiscard]] auto map(double val, double time) const -> std::pair<double, double> {
        double const pos_x = m_padding + ((time - m_min_time) * m_graph_width / m_time_range);
        double const pos_y = m_height - m_padding - ((val - m_min_val) * m_graph_height / m_range);
        return {pos_x, pos_y};
    }

    [[nodiscard]] auto padding() const noexcept -> double {
        return m_padding;
    }
    [[nodiscard]] auto graph_height() const noexcept -> double {
        return m_graph_height;
    }
    [[nodiscard]] auto graph_width() const noexcept -> double {
        return m_graph_width;
    }
    [[nodiscard]] auto max_val() const noexcept -> double {
        return m_max_val;
    }
    [[nodiscard]] auto range() const noexcept -> double {
        return m_range;
    }
    [[nodiscard]] auto min_time() const noexcept -> double {
        return m_min_time;
    }
    [[nodiscard]] auto time_range() const noexcept -> double {
        return m_time_range;
    }

   private:
    double m_min_val = 0.0;
    double m_max_val = 0.0;
    double m_range = 0.0;
    double m_min_time = 0.0;
    double m_max_time = 0.0;
    double m_time_range = 0.0;
    double m_width = 0.0;
    double m_height = 0.0;
    double m_padding = 0.0;
    double m_graph_width = 0.0;
    double m_graph_height = 0.0;
};

// Extracted template factory completely removes type-erasure overhead while adhering strictly to
// DRY NOLINTNEXTLINE(bugprone-easily-swappable-parameters): Factory constructs 2D mapper with
// standard screen width/height dimensions.
template <typename ValProj, typename TimeProj>
[[nodiscard]] auto create_mapper(const std::vector<Measurement>& data, int width, int height,
                                 bool compact, ValProj val_proj, TimeProj time_proj)
    -> std::optional<CoordinateMapper> {
    if (data.empty()) {
        return std::nullopt;
    }

    // C++20 Algorithms with projections for highly optimal boundary calculation
    auto [min_it, max_it] = std::ranges::minmax_element(data, std::ranges::less{}, val_proj);
    double const min_time = time_proj(data.front().date);
    double const max_time = time_proj(data.back().date);

    return CoordinateMapper(val_proj(*min_it), val_proj(*max_it), min_time, max_time, width, height,
                            compact);
}

}  // namespace

TrendGraph::TrendGraph() {
    set_draw_func(sigc::mem_fun(*this, &TrendGraph::on_draw));

    m_click_gesture = Gtk::GestureClick::create();
    m_click_gesture->signal_pressed().connect(sigc::mem_fun(*this, &TrendGraph::on_click));
    add_controller(m_click_gesture);

    m_popover.set_parent(*this);
    m_popover.set_autohide(true);
    m_popover.set_child(m_popover_label);
    m_popover_label.set_margin(popover_label_margin);

    set_can_target(true);
}

void TrendGraph::set_data(std::vector<Measurement> data) {
    m_data = std::move(data);
    m_popover.popdown();
    queue_draw();
}

void TrendGraph::set_metric(MetricType metric) {
    m_metric = metric;
    queue_draw();
}

void TrendGraph::set_compact_mode(bool compact) {
    m_compact = compact;
    set_can_target(!compact);
    queue_draw();
}

void TrendGraph::set_unit_system(UnitSystem units) {  // NOLINT(misc-include-cleaner)
    m_units = units;
    queue_draw();
}

void TrendGraph::set_use_v2(bool use_v2) {
    m_use_v2 = use_v2;
    queue_draw();
}

auto TrendGraph::get_value(const Measurement& measurement) const -> double {
    switch (m_metric) {
        case MetricType::Weight:
            if (m_units == UnitSystem::Imperial) {
                return measurement.weight.value * kg_to_lbs_factor;
            }
            return measurement.weight.value;
        case MetricType::BMI:
            return measurement.bmi;
        case MetricType::BodyFat:
            if (m_use_v2 && measurement.body_fat_v2.value > 0.0) {
                return measurement.body_fat_v2.value;
            }
            return measurement.body_fat.value;
        case MetricType::Water:
            if (m_use_v2 && measurement.water_v2.value > 0.0) {
                return measurement.water_v2.value;
            }
            return measurement.water.value;
        case MetricType::Muscle:
            if (m_use_v2 && measurement.muscle_mass_v2.value > 0.0) {
                return measurement.muscle_mass_v2.value;
            }
            return measurement.muscle_mass.value;
        default:
            return 0.0;
    }
}

auto TrendGraph::get_time_value(std::chrono::year_month_day date) -> double {
    if (!date.ok()) {
        return 0.0;
    }
    return static_cast<double>(std::chrono::sys_days{date}.time_since_epoch().count());
}

auto TrendGraph::format_time_value(double days) -> std::string {
    auto date_point = std::chrono::sys_days{std::chrono::days{static_cast<long long>(days)}};
    return std::format("{:%F}", std::chrono::year_month_day{date_point});
}

void TrendGraph::on_draw(const Cairo::RefPtr<Cairo::Context>& context, int width, int height) {
    auto mapper_opt = create_mapper(
        m_data, width, height, m_compact, [this](const auto& meas) { return get_value(meas); },
        [](auto date) { return get_time_value(date); });

    if (!mapper_opt) {
        return;
    }
    const auto& mapper = *mapper_opt;

    // Draw background
    if (!m_compact) {
        context->set_source_rgb(bg_color_component, bg_color_component, bg_color_component);
        context->paint();

        // Grid
        context->set_source_rgb(grid_color_component, grid_color_component, grid_color_component);
        context->set_line_width(grid_line_width);

        for (int i = 0; i < grid_line_count; ++i) {
            double const y_pos =
                mapper.padding() + (static_cast<double>(i) * mapper.graph_height() / divisor_2);
            context->move_to(mapper.padding(), y_pos);
            context->line_to(width - mapper.padding(), y_pos);
            context->stroke();

            double val = mapper.max_val() - (static_cast<double>(i) * mapper.range() / divisor_2);
            auto layout = create_pango_layout(std::format("{:.1f}", val));
            int text_w = 0;
            int text_h = 0;
            layout->get_pixel_size(text_w, text_h);
            context->set_source_rgb(label_color_component, label_color_component,
                                    label_color_component);
            context->move_to(mapper.padding() - text_w - grid_label_offset,
                             y_pos - (text_h / divisor_2));
            layout->show_in_cairo_context(context);
        }

        static constexpr int num_x_segments = 4;
        for (int i = 0; i <= num_x_segments; ++i) {
            double const time_ratio = static_cast<double>(i) / num_x_segments;
            double const current_time = mapper.min_time() + (time_ratio * mapper.time_range());
            double const x_pos = mapper.padding() + (time_ratio * mapper.graph_width());

            context->set_source_rgb(grid_color_component, grid_color_component,
                                    grid_color_component);
            context->move_to(x_pos, mapper.padding());
            context->line_to(x_pos, height - mapper.padding());
            context->stroke();

            auto layout = create_pango_layout(format_time_value(current_time));
            int text_w = 0;
            int text_h = 0;
            layout->get_pixel_size(text_w, text_h);
            context->set_source_rgb(label_color_component, label_color_component,
                                    label_color_component);
            context->move_to(x_pos - (text_w / divisor_2),
                             height - mapper.padding() + x_label_offset);
            layout->show_in_cairo_context(context);
        }
    }

    // Trend line
    context->set_source_rgb(line_color_r, line_color_g, line_color_b);
    context->set_line_width(m_compact ? compact_line_width : line_width);
    context->set_line_join(Cairo::Context::LineJoin::ROUND);

    bool first = true;
    for (const auto& measurement : m_data) {
        auto [pos_x, pos_y] = mapper.map(get_value(measurement), get_time_value(measurement.date));
        if (first) {
            context->move_to(pos_x, pos_y);
            first = false;
        } else {
            context->line_to(pos_x, pos_y);
        }
    }
    context->stroke();

    if (!m_compact) {
        for (const auto& measurement : m_data) {
            auto [pos_x, pos_y] =
                mapper.map(get_value(measurement), get_time_value(measurement.date));
            context->set_source_rgb(line_color_r, line_color_g, line_color_b);
            context->arc(pos_x, pos_y, point_radius, 0, multiplier_2 * std::numbers::pi);
            context->fill();
            context->set_source_rgb(1.0, 1.0, 1.0);
            context->set_line_width(point_border_width);
            context->arc(pos_x, pos_y, point_radius, 0, multiplier_2 * std::numbers::pi);
            context->stroke();
        }
    }
}

void TrendGraph::on_click(int /*n_press*/, double x_input, double y_input) {
    auto mapper_opt = create_mapper(
        m_data, get_width(), get_height(), m_compact,
        [this](const auto& measurement) { return get_value(measurement); },
        [](auto date) { return get_time_value(date); });

    if (!mapper_opt) {
        m_popover.popdown();
        return;
    }
    const auto& mapper = *mapper_opt;

    std::optional<size_t> closest;
    double min_distance = max_distance;
    const double threshold = m_compact ? compact_threshold : full_threshold;

    for (size_t i = 0; i < m_data.size(); ++i) {
        auto [pos_x, pos_y] = mapper.map(get_value(m_data[i]), get_time_value(m_data[i].date));
        double const dist = std::hypot(x_input - pos_x, y_input - pos_y);
        if (dist < min_distance && dist < threshold) {
            min_distance = dist;
            closest = i;
        }
    }

    if (closest) {
        const auto& measurement = m_data[*closest];
        std::string_view unit;
        if (m_metric == MetricType::Weight) {
            unit = (m_units == UnitSystem::Imperial) ? " lbs" : " kg";
        } else if (m_metric == MetricType::BMI) {
            unit = "";
        } else {
            unit = "%";
        }

        m_popover_label.set_text(
            std::format("{:%F}\n{:.1f}{}", measurement.date, get_value(measurement), unit));

        auto [pos_x, pos_y] = mapper.map(get_value(measurement), get_time_value(measurement.date));
        Gdk::Rectangle const rect(static_cast<int>(pos_x), static_cast<int>(pos_y), 1, 1);
        m_popover.set_pointing_to(rect);
        m_popover.popup();
    } else {
        m_popover.popdown();
    }
}

}  // namespace libscale
