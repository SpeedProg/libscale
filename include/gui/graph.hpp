#pragma once
#include <gtkmm/drawingarea.h>
#include <gtkmm/gestureclick.h>
#include <gtkmm/label.h>
#include <gtkmm/popover.h>

#include <optional>
#include <vector>

#include "gui/settings.hpp"
#include "libscale/types.hpp"

namespace libscale {

/**
 * @brief Identifies which health metric is currently selected for rendering on the graph.
 */
enum class MetricType : uint8_t {
    /// Weight in kilograms.
    Weight,
    /// Body Mass Index.
    BMI,
    /// Body fat percentage.
    BodyFat,
    /// Body water percentage.
    Water,
    /// Muscle mass percentage.
    Muscle
};

/**
 * @brief Identifies the grouping category of metric scales.
 */
enum class ScaleType : uint8_t {
    /// Absolute weight values.
    Weight,
    /// Dimensionless Body Mass Index.
    BMI,
    /// Percentages (Fat, Water, Muscle).
    Percentage
};

class CoordinateMapper;

/**
 * @brief Custom GTK4 DrawingArea widget for rendering a clean line chart representing health
 * trends. Features automated scaling, hover-over popovers for individual data points, and date
 * coordinate mapping.
 */
class TrendGraph : public Gtk::DrawingArea {
   public:
    /**
     * @brief Constructs the TrendGraph widget and configures click gestures.
     */
    TrendGraph();

    /**
     * @brief Destructor.
     */
    ~TrendGraph() override = default;
    TrendGraph(const TrendGraph&) = delete;
    auto operator=(const TrendGraph&) -> TrendGraph& = delete;
    TrendGraph(TrendGraph&&) = delete;
    auto operator=(TrendGraph&&) -> TrendGraph& = delete;

    /**
     * @brief Supplies the dataset to render on the line graph.
     * @param data Vector of processed measurements.
     */
    void set_data(std::vector<Measurement> data);

    /**
     * @brief Switches the active metric plotted on the graph.
     * @param metric Selected metric type.
     */
    void set_metric(MetricType metric);

    /**
     * @brief Sets the list of active metrics to plot overlay lines.
     * @param metrics Vector of selected metric types.
     */
    void set_metrics(const std::vector<MetricType>& metrics);

    /**
     * @brief Toggles compact rendering mode (useful for small sidebar layouts).
     * @param compact True to enable compact rendering.
     */
    void set_compact_mode(bool compact);

    /**
     * @brief Sets the physical unit system display preference.
     * @param units Metric or Imperial preference.
     */
    void set_unit_system(UnitSystem units);

    /**
     * @brief Toggles whether to use Clinical V2 calculations for body fat, water, and muscle mass.
     * @param use_v2 True to render V2 metrics on the graph.
     */
    void set_use_v2(bool use_v2);

   protected:
    /**
     * @brief Custom draw callback executed by GTK4 to render the trend line, axes, grid, and dots.
     * @param context Cairo drawing context reference.
     * @param width Width of the drawing area.
     * @param height Height of the drawing area.
     */
    void on_draw(const Cairo::RefPtr<Cairo::Context>& context, int width,
                 int height);  // NOLINT(bugprone-easily-swappable-parameters): GTKmm Cairo
                               // rendering signature requires consecutive width/height dimensions.

   private:
    auto get_value(const Measurement& measurement, MetricType metric) const -> double;
    static auto get_time_value(std::chrono::sys_seconds date) -> double;
    static auto format_time_value(double seconds) -> std::string;
    void on_click(int n_press, double x_input, double y_input);

    /**
     * @brief Computes the dynamic left and right graph paddings based on active scale types.
     * @return Pair containing left padding and right padding in pixels.
     */
    auto get_paddings() const -> std::pair<double, double>;

    /**
     * @brief Renders the trend graph with a single active metric line and standard single Y-axis.
     * @param context Cairo drawing context.
     * @param width Width of the drawing area.
     * @param height Height of the drawing area.
     */
    void draw_single_metric(const Cairo::RefPtr<Cairo::Context>& context);

    /**
     * @brief Renders the trend graph with multiple metric lines and overlay Y-axes.
     * @param context Cairo drawing context.
     */
    void draw_multi_metric(const Cairo::RefPtr<Cairo::Context>& context);

    struct ScaleRange {
        double min_val = 0.0;
        double max_val = 0.0;
        double range = 0.0;
        bool active = false;
        double axis_x = 0.0;
    };

    struct MultiScaleRanges {
        ScaleRange weight;
        ScaleRange bmi;
        ScaleRange percent;
    };

    struct MultiLayoutInfo {
        double grid_left = 0.0;
        double grid_right = 0.0;
        double grid_top = 0.0;
        double grid_bottom = 0.0;
        double graph_width = 0.0;
        double graph_height = 0.0;
        double min_time = 0.0;
        double time_range = 0.0;
    };

    /**
     * @brief Renders the background and grid for a single metric graph view.
     */
    void draw_single_background(const Cairo::RefPtr<Cairo::Context>& context,
                                const CoordinateMapper& mapper, MetricType metric);

    /**
     * @brief Renders the trend line and coordinate points for a single metric.
     */
    void draw_single_line(const Cairo::RefPtr<Cairo::Context>& context,
                          const CoordinateMapper& mapper, MetricType metric);

    /**
     * @brief Computes the min and max limits for a given ScaleType in multi-metric mode.
     */
    [[nodiscard]] auto get_scale_min_max(ScaleType type) const -> std::pair<double, double>;

    /**
     * @brief Renders a single Y-axis line and labels at the designated offset.
     */
    void draw_scale_axis(const Cairo::RefPtr<Cairo::Context>& context,
                         const ScaleRange& scale_range, const MultiLayoutInfo& layout_info,
                         double red, double green, double blue,
                         std::string_view suffix);  // NOLINT(bugprone-easily-swappable-parameters)

    /**
     * @brief Renders the grid lines and Y-axes for the multi-metric view.
     */
    void draw_multi_grid_and_axes(const Cairo::RefPtr<Cairo::Context>& context,
                                  const MultiLayoutInfo& layout_info,
                                  const MultiScaleRanges& scales);

    /**
     * @brief Renders the data curves and points for active metrics in multi-metric mode.
     */
    void draw_multi_curves(const Cairo::RefPtr<Cairo::Context>& context,
                           const MultiLayoutInfo& layout_info, const MultiScaleRanges& scales);

    std::vector<Measurement> m_data;
    std::vector<MetricType> m_metrics = {MetricType::Weight};
    UnitSystem m_units = UnitSystem::Metric;
    bool m_compact = false;
    bool m_use_v2 = false;
    std::optional<size_t> m_selected_index;

    Glib::RefPtr<Gtk::GestureClick> m_click_gesture;
    Gtk::Popover m_popover;
    Gtk::Label m_popover_label;
};

}  // namespace libscale
