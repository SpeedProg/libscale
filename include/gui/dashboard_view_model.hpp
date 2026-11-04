#pragma once
#include <sigc++/sigc++.h>

#include <chrono>
#include <functional>
#include <vector>

#include "libscale/types.hpp"

namespace comfort_select {

using libscale::Measurement;

/**
 * @brief The MVVM View-Model coordinating inputs, presets, range sliders, and filtered datasets.
 * Decouples rendering widgets (View) from the health metrics business logics (Model).
 */
class DashboardViewModel {
   public:
    /**
     * @brief Default constructor.
     */
    DashboardViewModel() = default;

    /**
     * @brief Sets the raw base vector of measurements to display.
     * @param data Raw measurement vector.
     */
    void set_data(const std::vector<Measurement>& data);

    /**
     * @brief Filters the historical measurement dataset based on range slider percentage bounds.
     * @param start_pct Starting boundary percentage (0.0 to 1.0).
     * @param end_pct Ending boundary percentage (0.0 to 1.0).
     */
    void set_range_percentages(double start_pct, double end_pct);

    /**
     * @brief Filters the dataset using preset date range options.
     * @param days Number of historical days to retain (e.g. 7, 30, 90). Passing 0 keeps all data.
     */
    void set_range_preset(int days);

    /**
     * @brief Retrieves the active filtered measurement subset.
     * @return Const reference to the filtered measurement list.
     */
    [[nodiscard]] auto get_filtered_data() const -> const std::vector<Measurement>& {
        return m_filtered_data;
    }

    /**
     * @brief Retrieves the full unfiltered measurement dataset.
     * @return Const reference to the complete measurement list.
     */
    [[nodiscard]] auto get_all_data() const -> const std::vector<Measurement>& {
        return m_all_data;
    }

    /**
     * @brief Signal emitted whenever the filtered measurement subset undergoes any updates.
     * @return Reference to Glib signal slot.
     */
    auto signal_data_changed() -> sigc::signal<void()>& {
        return m_signal_data_changed;
    }

   private:
    void apply_filter();

    std::vector<Measurement> m_all_data;
    std::vector<Measurement> m_filtered_data;

    double m_start_pct = 0.0;
    double m_end_pct = 1.0;

    sigc::signal<void()> m_signal_data_changed;
};

}  // namespace comfort_select
