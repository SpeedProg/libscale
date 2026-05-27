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
#include <limits>

#include "gui/settings.hpp"
#include "libscale/types.hpp"

// NOLINTBEGIN(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access): Subscripting is used on data vectors where indices are pre-validated.
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
constexpr double min_scale_range = 0.001;

constexpr double padding_three_scales_left = 95.0;
constexpr double far_left_axis_offset = 45.0;
constexpr double guide_line_color_component = 0.8;
constexpr double guide_line_alpha = 0.6;
constexpr double guide_line_dash_length = 4.0;

constexpr double weight_color_r = 0.0;
constexpr double weight_color_g = 0.67;
constexpr double weight_color_b = 1.0;

constexpr double bmi_color_r = 0.63;
constexpr double bmi_color_g = 0.13;
constexpr double bmi_color_b = 0.94;

constexpr double fat_color_r = 1.0;
constexpr double fat_color_g = 0.34;
constexpr double fat_color_b = 0.13;

constexpr double water_color_r = 0.13;
constexpr double water_color_g = 0.59;
constexpr double water_color_b = 0.95;

constexpr double muscle_color_r = 0.3;
constexpr double muscle_color_g = 0.69;
constexpr double muscle_color_b = 0.31;

constexpr double neutral_axis_color_component = 0.6;

constexpr double constant_range_lower_factor = 0.9;
constexpr double constant_range_upper_factor = 1.1;
}  // namespace

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
        m_min_val = min_v - (m_range * range_margin_factor);
        m_max_val = max_v + (m_range * range_margin_factor);
        m_range = m_max_val - m_min_val;

        m_min_time = min_t;
        m_max_time = max_t;
        m_time_range = m_max_time - m_min_time;
        if (m_time_range <= 0.0) {
            m_time_range = 1.0;
        }

        m_graph_width = m_width - (multiplier_2 * m_padding);
        m_graph_height = m_height - (multiplier_2 * m_padding);
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

namespace {

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

struct MetricStyle {
    double r;
    double g;
    double b;
    std::string_view name;
    std::string_view unit_metric;
    std::string_view unit_imperial;
    ScaleType scale;
};

[[nodiscard]] auto get_metric_style(MetricType metric) -> MetricStyle {
    switch (metric) {
        case MetricType::Weight:
            return {.r = weight_color_r,
                    .g = weight_color_g,
                    .b = weight_color_b,
                    .name = "Weight",
                    .unit_metric = " kg",
                    .unit_imperial = " lbs",
                    .scale = ScaleType::Weight};
        case MetricType::BMI:
            return {.r = bmi_color_r,
                    .g = bmi_color_g,
                    .b = bmi_color_b,
                    .name = "BMI",
                    .unit_metric = "",
                    .unit_imperial = "",
                    .scale = ScaleType::BMI};
        case MetricType::BodyFat:
            return {.r = fat_color_r,
                    .g = fat_color_g,
                    .b = fat_color_b,
                    .name = "Body Fat",
                    .unit_metric = " %",
                    .unit_imperial = " %",
                    .scale = ScaleType::Percentage};
        case MetricType::Water:
            return {.r = water_color_r,
                    .g = water_color_g,
                    .b = water_color_b,
                    .name = "Water",
                    .unit_metric = " %",
                    .unit_imperial = " %",
                    .scale = ScaleType::Percentage};
        case MetricType::Muscle:
            return {.r = muscle_color_r,
                    .g = muscle_color_g,
                    .b = muscle_color_b,
                    .name = "Muscle",
                    .unit_metric = " %",
                    .unit_imperial = " %",
                    .scale = ScaleType::Percentage};
    }
    return {.r = 1.0,
            .g = 1.0,
            .b = 1.0,
            .name = "",
            .unit_metric = "",
            .unit_imperial = "",
            .scale = ScaleType::Weight};
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

    m_popover.signal_closed().connect([this]() -> void {
        m_selected_index = std::nullopt;
        queue_draw();
    });

    set_can_target(true);
}

void TrendGraph::set_data(std::vector<Measurement> data) {
    m_data = std::move(data);
    m_popover.popdown();
    m_selected_index = std::nullopt;
    queue_draw();
}

void TrendGraph::set_metric(MetricType metric) {
    set_metrics({metric});
}

void TrendGraph::set_metrics(const std::vector<MetricType>& metrics) {
    m_metrics = metrics;
    m_popover.popdown();
    m_selected_index = std::nullopt;
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

auto TrendGraph::get_value(const Measurement& measurement, MetricType metric) const -> double {
    switch (metric) {
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

auto TrendGraph::get_paddings() const -> std::pair<double, double> {
    if (m_compact) {
        return {compact_padding, compact_padding};
    }

    if (m_metrics.size() <= 1) {
        return {padding_size, padding_size};
    }

    bool has_weight = false;
    bool has_bmi = false;
    bool has_percent = false;

    for (auto metric : m_metrics) {
        auto style = get_metric_style(metric);
        if (style.scale == ScaleType::Weight) {
            has_weight = true;
        } else if (style.scale == ScaleType::BMI) {
            has_bmi = true;
        } else if (style.scale == ScaleType::Percentage) {
            has_percent = true;
        }
    }

    int active_scales_count = 0;
    if (has_weight) {
        ++active_scales_count;
    }
    if (has_bmi) {
        ++active_scales_count;
    }
    if (has_percent) {
        ++active_scales_count;
    }

    if (active_scales_count == 3) {
        return {padding_three_scales_left, padding_size};
    }
    return {padding_size, padding_size};
}

void TrendGraph::on_draw(const Cairo::RefPtr<Cairo::Context>& context, int /*width*/, int /*height*/) {
    if (m_data.empty() || m_metrics.empty()) {
        return;
    }

    if (m_metrics.size() == 1 || m_compact) {
        draw_single_metric(context);
    } else {
        draw_multi_metric(context);
    }
}

void TrendGraph::draw_single_metric(const Cairo::RefPtr<Cairo::Context>& context) {
    MetricType const active_metric = m_metrics.front();
    auto mapper_opt = create_mapper(
        m_data, get_width(), get_height(), m_compact,
        [this, active_metric](const auto& meas) -> auto { return get_value(meas, active_metric); },
        [](auto date) -> auto { return get_time_value(date); });

    if (!mapper_opt) {
        return;
    }
    const auto& mapper = *mapper_opt;

    draw_single_background(context, mapper, active_metric);
    draw_single_line(context, mapper, active_metric);
}

void TrendGraph::draw_single_background(const Cairo::RefPtr<Cairo::Context>& context,
                                       const CoordinateMapper& mapper,
                                       MetricType metric) {
    if (m_compact) {
        return;
    }

    context->set_source_rgb(bg_color_component, bg_color_component, bg_color_component);
    context->paint();

    // Grid
    context->set_source_rgb(grid_color_component, grid_color_component, grid_color_component);
    context->set_line_width(grid_line_width);

    for (int i = 0; i < grid_line_count; ++i) {
        double const y_pos =
            mapper.padding() + (static_cast<double>(i) * mapper.graph_height() / divisor_2);
        context->move_to(mapper.padding(), y_pos);
        context->line_to(get_width() - mapper.padding(), y_pos);
        context->stroke();

        double const val = mapper.max_val() - (static_cast<double>(i) * mapper.range() / divisor_2);
        auto layout = create_pango_layout(std::format("{:.1f}", val));
        int text_w = 0;
        int text_h = 0;
        layout->get_pixel_size(text_w, text_h);
        
        auto style = get_metric_style(metric);
        context->set_source_rgb(style.r, style.g, style.b);
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
        context->line_to(x_pos, get_height() - mapper.padding());
        context->stroke();

        auto layout = create_pango_layout(format_time_value(current_time));
        int text_w = 0;
        int text_h = 0;
        layout->get_pixel_size(text_w, text_h);
        context->set_source_rgb(label_color_component, label_color_component,
                                label_color_component);
        context->move_to(x_pos - (text_w / divisor_2),
                         get_height() - mapper.padding() + x_label_offset);
        layout->show_in_cairo_context(context);
    }

    // Draw vertical selection line if popover is open
    if (m_selected_index && *m_selected_index < m_data.size()) {
        const auto& selected_measurement = m_data[*m_selected_index];
        double const selected_time = get_time_value(selected_measurement.date);
        double const selected_x = mapper.padding() + ((selected_time - mapper.min_time()) * mapper.graph_width() / mapper.time_range());

        context->set_source_rgba(guide_line_color_component, guide_line_color_component,
                                 guide_line_color_component, guide_line_alpha);
        context->set_line_width(grid_line_width);
        const std::vector<double> dashes = {guide_line_dash_length, guide_line_dash_length};
        context->set_dash(dashes, 0.0);
        context->move_to(selected_x, mapper.padding());
        context->line_to(selected_x, get_height() - mapper.padding());
        context->stroke();
        context->unset_dash();
    }
}

void TrendGraph::draw_single_line(const Cairo::RefPtr<Cairo::Context>& context,
                                 const CoordinateMapper& mapper, MetricType metric) {
    auto style = get_metric_style(metric);
    context->set_source_rgb(style.r, style.g, style.b);
    context->set_line_width(m_compact ? compact_line_width : line_width);
    context->set_line_join(Cairo::Context::LineJoin::ROUND);

    bool first = true;
    for (const auto& measurement : m_data) {
        auto [pos_x, pos_y] = mapper.map(get_value(measurement, metric), get_time_value(measurement.date));
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
                mapper.map(get_value(measurement, metric), get_time_value(measurement.date));
            context->set_source_rgb(style.r, style.g, style.b);
            context->arc(pos_x, pos_y, point_radius, 0, multiplier_2 * std::numbers::pi);
            context->fill();
            context->set_source_rgb(1.0, 1.0, 1.0);
            context->set_line_width(point_border_width);
            context->arc(pos_x, pos_y, point_radius, 0, multiplier_2 * std::numbers::pi);
            context->stroke();
        }
    }
}

void TrendGraph::draw_multi_metric(const Cairo::RefPtr<Cairo::Context>& context) {
    // Multi-metric overlay logic (Option B)
    bool has_weight = false;
    bool has_bmi = false;
    bool has_percent = false;

    for (auto metric : m_metrics) {
        auto style = get_metric_style(metric);
        if (style.scale == ScaleType::Weight) {
            has_weight = true;
        } else if (style.scale == ScaleType::BMI) {
            has_bmi = true;
        } else if (style.scale == ScaleType::Percentage) {
            has_percent = true;
        }
    }
    auto const [left_padding, right_padding] = get_paddings();

    double const min_time = get_time_value(m_data.front().date);
    double const max_time = get_time_value(m_data.back().date);
    double time_range = max_time - min_time;
    if (time_range <= 0.0) {
        time_range = 1.0;
    }

    MultiLayoutInfo layout_info;
    layout_info.grid_left = left_padding;
    layout_info.grid_right = static_cast<double>(get_width()) - right_padding;
    layout_info.grid_top = padding_size;
    layout_info.grid_bottom = static_cast<double>(get_height()) - padding_size;
    layout_info.graph_width = layout_info.grid_right - layout_info.grid_left;
    layout_info.graph_height = layout_info.grid_bottom - layout_info.grid_top;
    layout_info.min_time = min_time;
    layout_info.time_range = time_range;

    ScaleRange range_weight;
    ScaleRange range_bmi;
    ScaleRange range_percent;

    if (has_weight) {
        auto [min_w, max_w] = get_scale_min_max(ScaleType::Weight);
        range_weight.min_val = min_w;
        range_weight.max_val = max_w;
        range_weight.range = max_w - min_w;
        range_weight.active = true;
    }
    if (has_bmi) {
        auto [min_b, max_b] = get_scale_min_max(ScaleType::BMI);
        range_bmi.min_val = min_b;
        range_bmi.max_val = max_b;
        range_bmi.range = max_b - min_b;
        range_bmi.active = true;
    }
    if (has_percent) {
        auto [min_p, max_p] = get_scale_min_max(ScaleType::Percentage);
        range_percent.min_val = min_p;
        range_percent.max_val = max_p;
        range_percent.range = max_p - min_p;
        range_percent.active = true;
    }

    // Assign Axis position X coordinate (Option B)
    std::vector<ScaleRange*> active_scales;
    if (range_weight.active) {
        active_scales.push_back(&range_weight);
    }
    if (range_bmi.active) {
        active_scales.push_back(&range_bmi);
    }
    if (range_percent.active) {
        active_scales.push_back(&range_percent);
    }

    if (active_scales.size() == 3) {
        range_weight.axis_x = layout_info.grid_left - far_left_axis_offset;
        range_bmi.axis_x = layout_info.grid_left;
        range_percent.axis_x = layout_info.grid_right;
    } else if (active_scales.size() == 2) {
        active_scales[0]->axis_x = layout_info.grid_left;
        active_scales[1]->axis_x = layout_info.grid_right;
    } else if (active_scales.size() == 1) {
        active_scales[0]->axis_x = layout_info.grid_left;
    }

    MultiScaleRanges const scales{
        .weight = range_weight,
        .bmi = range_bmi,
        .percent = range_percent
    };

    draw_multi_grid_and_axes(context, layout_info, scales);
    draw_multi_curves(context, layout_info, scales);
}

auto TrendGraph::get_scale_min_max(ScaleType type) const -> std::pair<double, double> {
    double min_v = std::numeric_limits<double>::max();
    double max_v = std::numeric_limits<double>::lowest();
    bool has_any = false;
    for (auto metric : m_metrics) {
        if (get_metric_style(metric).scale == type) {
            has_any = true;
            for (const auto& measurement : m_data) {
                double const val = get_value(measurement, metric);
                min_v = std::min(val, min_v);
                max_v = std::max(val, max_v);
            }
        }
    }
    if (!has_any) {
        return {0.0, 0.0};
    }
    double const range_diff = max_v - min_v;
    if (range_diff < min_scale_range) {
        if (std::abs(min_v) < min_scale_range) {
            return {-1.0, 1.0};
        }
        return {min_v * constant_range_lower_factor, min_v * constant_range_upper_factor};
    }
    double const margin = range_diff * range_margin_factor;
    return {min_v - margin, max_v + margin};
}

void TrendGraph::draw_scale_axis(const Cairo::RefPtr<Cairo::Context>& context,
                                 const ScaleRange& scale_range, const MultiLayoutInfo& layout_info,
                                 double red, double green, double blue, std::string_view suffix) {
    if (!scale_range.active) {
        return;
    }

    // Draw vertical axis line
    context->set_source_rgb(red, green, blue);
    context->set_line_width(grid_line_width);
    context->move_to(scale_range.axis_x, layout_info.grid_top);
    context->line_to(scale_range.axis_x, layout_info.grid_bottom);
    context->stroke();

    // Draw labels
    for (int i = 0; i < grid_line_count; ++i) {
        double const y_pos = layout_info.grid_top + (static_cast<double>(i) * layout_info.graph_height / divisor_2);
        double const val = scale_range.max_val - (static_cast<double>(i) * scale_range.range / divisor_2);

        auto layout = create_pango_layout(std::format("{:.1f}{}", val, suffix));
        int text_w = 0;
        int text_h = 0;
        layout->get_pixel_size(text_w, text_h);

        double label_x = 0.0;
        if (scale_range.axis_x >= layout_info.grid_right - 1.0) {
            label_x = scale_range.axis_x + grid_label_offset;
        } else {
            label_x = scale_range.axis_x - static_cast<double>(text_w) - grid_label_offset;
        }

        context->move_to(label_x, y_pos - (static_cast<double>(text_h) / divisor_2));
        layout->show_in_cairo_context(context);
    }
}

void TrendGraph::draw_multi_grid_and_axes(const Cairo::RefPtr<Cairo::Context>& context,
                                          const MultiLayoutInfo& layout_info,
                                          const MultiScaleRanges& scales) {
    // Draw background
    context->set_source_rgb(bg_color_component, bg_color_component, bg_color_component);
    context->paint();

    // Horizontal Grid lines
    context->set_source_rgb(grid_color_component, grid_color_component, grid_color_component);
    context->set_line_width(grid_line_width);

    for (int i = 0; i < grid_line_count; ++i) {
        double const y_pos = layout_info.grid_top + (static_cast<double>(i) * layout_info.graph_height / divisor_2);
        context->move_to(layout_info.grid_left, y_pos);
        context->line_to(layout_info.grid_right, y_pos);
        context->stroke();
    }

    // Vertical Grid lines
    static constexpr int num_x_segments = 4;
    for (int i = 0; i <= num_x_segments; ++i) {
        double const time_ratio = static_cast<double>(i) / num_x_segments;
        double const current_time = layout_info.min_time + (time_ratio * layout_info.time_range);
        double const x_pos = layout_info.grid_left + (time_ratio * layout_info.graph_width);

        context->set_source_rgb(grid_color_component, grid_color_component,
                                grid_color_component);
        context->move_to(x_pos, layout_info.grid_top);
        context->line_to(x_pos, layout_info.grid_bottom);
        context->stroke();

        auto layout = create_pango_layout(format_time_value(current_time));
        int text_w = 0;
        int text_h = 0;
        layout->get_pixel_size(text_w, text_h);
        context->set_source_rgb(label_color_component, label_color_component,
                                label_color_component);
        context->move_to(x_pos - (static_cast<double>(text_w) / divisor_2),
                         layout_info.grid_bottom + x_label_offset);
        layout->show_in_cairo_context(context);
    }

    std::string_view const weight_suffix = (m_units == UnitSystem::Imperial) ? " lbs" : " kg";
    draw_scale_axis(context, scales.weight, layout_info, weight_color_r, weight_color_g, weight_color_b, weight_suffix);
    draw_scale_axis(context, scales.bmi, layout_info, bmi_color_r, bmi_color_g, bmi_color_b, "");

    int percentage_metrics_count = 0;
    MetricType single_percentage_metric = MetricType::BodyFat;
    for (auto metric : m_metrics) {
        if (get_metric_style(metric).scale == ScaleType::Percentage) {
            ++percentage_metrics_count;
            single_percentage_metric = metric;
        }
    }
    double percent_r = neutral_axis_color_component;
    double percent_g = neutral_axis_color_component;
    double percent_b = neutral_axis_color_component;
    if (percentage_metrics_count == 1) {
        auto style_percentage = get_metric_style(single_percentage_metric);
        percent_r = style_percentage.r;
        percent_g = style_percentage.g;
        percent_b = style_percentage.b;
    }
    draw_scale_axis(context, scales.percent, layout_info, percent_r, percent_g, percent_b, " %");
}

void TrendGraph::draw_multi_curves(const Cairo::RefPtr<Cairo::Context>& context,
                                   const MultiLayoutInfo& layout_info,
                                   const MultiScaleRanges& scales) {
    auto map_y = [&](double val, ScaleType type) -> double {
        const ScaleRange* scale_range = nullptr;
        if (type == ScaleType::Weight) {
            scale_range = &scales.weight;
        } else if (type == ScaleType::BMI) {
            scale_range = &scales.bmi;
        } else {
            scale_range = &scales.percent;
        }

        if (scale_range->range < min_scale_range) {
            return layout_info.grid_bottom;
        }
        return layout_info.grid_bottom - ((val - scale_range->min_val) * layout_info.graph_height / scale_range->range);
    };

    auto map_x = [&](double time) -> double {
        return layout_info.grid_left + ((time - layout_info.min_time) * layout_info.graph_width / layout_info.time_range);
    };

    // Draw vertical selection line if popover is open
    if (m_selected_index && *m_selected_index < m_data.size()) {
        const auto& selected_measurement = m_data[*m_selected_index];
        double const selected_time = get_time_value(selected_measurement.date);
        double const selected_x = map_x(selected_time);

        context->set_source_rgba(guide_line_color_component, guide_line_color_component,
                                 guide_line_color_component, guide_line_alpha);
        context->set_line_width(grid_line_width);
        const std::vector<double> dashes = {guide_line_dash_length, guide_line_dash_length};
        context->set_dash(dashes, 0.0);
        context->move_to(selected_x, layout_info.grid_top);
        context->line_to(selected_x, layout_info.grid_bottom);
        context->stroke();
        context->unset_dash();
    }

    // Draw curves
    for (auto metric : m_metrics) {
        auto style_info = get_metric_style(metric);
        context->set_source_rgb(style_info.r, style_info.g, style_info.b);
        context->set_line_width(line_width);
        context->set_line_join(Cairo::Context::LineJoin::ROUND);

        bool first = true;
        for (const auto& measurement : m_data) {
            double const val = get_value(measurement, metric);
            double const time_val = get_time_value(measurement.date);
            double const pos_x = map_x(time_val);
            double const pos_y = map_y(val, style_info.scale);

            if (first) {
                context->move_to(pos_x, pos_y);
                first = false;
            } else {
                context->line_to(pos_x, pos_y);
            }
        }
        context->stroke();

        // Draw points
        for (const auto& measurement : m_data) {
            double const val = get_value(measurement, metric);
            double const time_val = get_time_value(measurement.date);
            double const pos_x = map_x(time_val);
            double const pos_y = map_y(val, style_info.scale);

            context->set_source_rgb(style_info.r, style_info.g, style_info.b);
            context->arc(pos_x, pos_y, point_radius, 0, multiplier_2 * std::numbers::pi);
            context->fill();
            context->set_source_rgb(1.0, 1.0, 1.0);
            context->set_line_width(point_border_width);
            context->arc(pos_x, pos_y, point_radius, 0, multiplier_2 * std::numbers::pi);
            context->stroke();
        }
    }
}

void TrendGraph::on_click(int /*n_press*/, double x_input, double /*y_input*/) {
    if (m_data.empty() || m_metrics.empty()) {
        m_selected_index = std::nullopt;
        m_popover.popdown();
        return;
    }

    auto const [left_padding, right_padding] = get_paddings();

    double const min_time = get_time_value(m_data.front().date);
    double const max_time = get_time_value(m_data.back().date);
    double time_range = max_time - min_time;
    if (time_range <= 0.0) {
        time_range = 1.0;
    }

    double const grid_left = left_padding;
    double const grid_right = static_cast<double>(get_width()) - right_padding;
    double const graph_width = grid_right - grid_left;

    std::optional<size_t> closest;
    double min_distance = max_distance;
    double const threshold = m_compact ? compact_threshold : full_threshold;

    for (size_t i = 0; i < m_data.size(); ++i) {
        double const time_val = get_time_value(m_data[i].date);
        double const pos_x = grid_left + ((time_val - min_time) * graph_width / time_range);
        double const dist = std::abs(x_input - pos_x);
        if (dist < min_distance && dist < threshold) {
            min_distance = dist;
            closest = i;
        }
    }

    if (closest) {
        m_selected_index = closest;
        const auto& measurement = m_data[*closest];

        std::string popover_text = std::format("{:%F}", measurement.date);
        for (auto metric : m_metrics) {
            auto style_info = get_metric_style(metric);
            double const val = get_value(measurement, metric);
            std::string_view const unit = (m_units == UnitSystem::Imperial) ? style_info.unit_imperial : style_info.unit_metric;
            popover_text += std::format("\n{}: {:.1f}{}", style_info.name, val, unit);
        }

        m_popover_label.set_text(popover_text);

        double const time_val = get_time_value(measurement.date);
        double const pos_x = grid_left + ((time_val - min_time) * graph_width / time_range);
        double const pos_y = static_cast<double>(get_height()) / divisor_2;

        Gdk::Rectangle const rect(static_cast<int>(pos_x), static_cast<int>(pos_y), 1, 1);
        m_popover.set_pointing_to(rect);
        m_popover.popup();
        queue_draw();
    } else {
        m_selected_index = std::nullopt;
        m_popover.popdown();
        queue_draw();
    }
}

}  // namespace libscale
// NOLINTEND(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
