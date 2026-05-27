#include "gui/dashboard_view_model.hpp"

#include <bits/chrono.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <iterator>
#include <vector>

#include "libscale/types.hpp"

namespace libscale {

void DashboardViewModel::set_data(const std::vector<Measurement>& data) {
    m_all_data = data;
    m_start_pct = 0.0;
    m_end_pct = 1.0;
    apply_filter();
}

void DashboardViewModel::set_range_percentages(double start_pct, double end_pct) {
    m_start_pct = std::clamp(start_pct, 0.0, 1.0);
    m_end_pct = std::clamp(end_pct, 0.0, 1.0);
    if (m_start_pct > m_end_pct) {
        std::swap(m_start_pct, m_end_pct);
    }
    apply_filter();
}

void DashboardViewModel::set_range_preset(int days) {
    if (m_all_data.empty()) {
        return;
    }

    if (days <= 0) {
        m_start_pct = 0.0;
        m_end_pct = 1.0;
    } else {
        auto const cutoff = std::chrono::floor<std::chrono::days>(  // NOLINT(misc-include-cleaner)
                                m_all_data.back().date) -
                            std::chrono::days{days};  // NOLINT(misc-include-cleaner)

        auto const cutoff_it = std::ranges::lower_bound(
            m_all_data, cutoff, {}, [](const Measurement& measurement) -> std::chrono::sys_days {
                return std::chrono::floor<std::chrono::days>(measurement.date);
            });
        auto const start_idx = static_cast<size_t>(std::distance(m_all_data.begin(), cutoff_it));

        m_start_pct = static_cast<double>(start_idx) /
                      static_cast<double>(std::max<size_t>(1, m_all_data.size() - 1));
        m_end_pct = 1.0;
    }
    apply_filter();
}

void DashboardViewModel::apply_filter() {
    if (m_all_data.empty()) {
        m_filtered_data.clear();
    } else {
        size_t const total = m_all_data.size();
        auto start_idx = static_cast<size_t>(m_start_pct * static_cast<double>(total - 1));
        auto end_idx = static_cast<size_t>(m_end_pct * static_cast<double>(total - 1));

        if (end_idx >= total) {
            end_idx = total - 1;
        }
        start_idx = std::min(start_idx, end_idx);

        m_filtered_data.assign(m_all_data.begin() + static_cast<long>(start_idx),
                               m_all_data.begin() + static_cast<long>(end_idx) + 1);
    }
    m_signal_data_changed.emit();
}

}  // namespace libscale
