#pragma once
#include <gtkmm/drawingarea.h>
#include <gtkmm/gestureclick.h>
#include <gtkmm/popover.h>
#include <gtkmm/label.h>
#include <vector>
#include <optional>

#include "libscale/types.hpp"
#include "gui/settings.hpp"

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
 * @brief Custom GTK4 DrawingArea widget for rendering a clean line chart representing health trends.
 * Features automated scaling, hover-over popovers for individual data points, and date coordinate mapping.
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
    void on_draw(const Cairo::RefPtr<Cairo::Context>& context, int width, int height); // NOLINT(bugprone-easily-swappable-parameters): GTKmm Cairo rendering signature requires consecutive width/height dimensions.

   private:
    auto get_value(const Measurement& measurement) const -> double;
    static auto get_time_value(std::chrono::year_month_day date) -> double;
    static auto format_time_value(double days) -> std::string;
    void on_click(int n_press, double x_input, double y_input);

    std::vector<Measurement> m_data;
    MetricType m_metric = MetricType::Weight;
    UnitSystem m_units = UnitSystem::Metric;
    bool m_compact = false;
    bool m_use_v2 = false;

    Glib::RefPtr<Gtk::GestureClick> m_click_gesture;
    Gtk::Popover m_popover;
    Gtk::Label m_popover_label;
};

}  // namespace libscale
