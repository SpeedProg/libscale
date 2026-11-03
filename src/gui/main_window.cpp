#include "gui/main_window.hpp"

#include <gdkmm/display.h>
#include <giomm/liststore.h>
#include <glibmm/main.h>
#include <glibmm/refptr.h>
#include <glibmm/ustring.h>
#include <gtk/gtk.h>
#include <gtkmm/adjustment.h>
#include <gtkmm/applicationwindow.h>
#include <gtkmm/box.h>
#include <gtkmm/builder.h>
#include <gtkmm/button.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/comboboxtext.h>
#include <gtkmm/cssprovider.h>
#include <gtkmm/dialog.h>
#include <gtkmm/entry.h>
#include <gtkmm/enums.h>
#include <gtkmm/filechooser.h>
#include <gtkmm/filechooserdialog.h>
#include <gtkmm/filefilter.h>
#include <gtkmm/label.h>
#include <gtkmm/listbox.h>
#include <gtkmm/listboxrow.h>
#include <gtkmm/listitem.h>
#include <gtkmm/listview.h>
#include <gtkmm/messagedialog.h>
#include <gtkmm/noselection.h>
#include <gtkmm/object.h>
#include <gtkmm/scale.h>
#include <gtkmm/settings.h>
#include <gtkmm/signallistitemfactory.h>
#include <gtkmm/spinbutton.h>
#include <gtkmm/stylecontext.h>
#include <sigc++/functors/mem_fun.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <iterator>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "gui/graph.hpp"
#include "gui/history_row.hpp"
#include "gui/settings.hpp"  //
#include "libscale/exporter.hpp"
#include "libscale/manager.hpp"
#include "libscale/types.hpp"
#include "libscale/worker.hpp"

// NOLINTBEGIN(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access): Subscripting is used
// for Gtk::ListBox row indexes which are pre-validated.
namespace libscale {

static constexpr int default_graph_width = 400;
static constexpr int default_graph_height = 200;
static constexpr double slider_max = 1.0;  // Normalized scale for ViewModel

static constexpr int range_preset_30d = 30;
static constexpr int range_preset_90d = 90;
static constexpr int range_preset_1y = 365;

namespace {
auto find_resource(const std::filesystem::path& rel_path) -> std::string {
    const std::vector<std::filesystem::path> search_roots = {
        ".",
        "..",
        "resources",
        "../resources",
        "../share/de.speedprog.comfort-select-dashboard",
        "share/de.speedprog.comfort-select-dashboard",
        "/usr/share/de.speedprog.comfort-select-dashboard",
        "/usr/local/share/de.speedprog.comfort-select-dashboard",
        "src/gui",
        "../src/gui",
        "../../src/gui",
        "ui",
        "../ui",
        "../../ui"};

    for (const auto& root : search_roots) {
        auto full_path = root / rel_path;
        if (std::filesystem::exists(full_path)) {
            return std::filesystem::absolute(full_path).string();
        }
    }
    return "";
}
}  // namespace

MainWindow::MainWindow(
    BaseObjectType* cobject,
    const Glib::RefPtr<Gtk::Builder>& builder,  // NOLINT(readability-function-cognitive-complexity)
    HealthManager& manager, ISettingsManager& settings_manager)
    : Gtk::ApplicationWindow(cobject),
      m_manager(manager),
      m_settings(settings_manager),
      m_builder(builder),
      m_btn_edit_waist(builder->get_widget<Gtk::Button>("btn_edit_waist")),
      m_card_bmi(builder->get_widget<Gtk::Button>("card_bmi")),
      m_card_fat(builder->get_widget<Gtk::Button>("card_fat")),
      m_card_muscle(builder->get_widget<Gtk::Button>("card_muscle")),
      m_card_water(builder->get_widget<Gtk::Button>("card_water")),
      m_card_weight(builder->get_widget<Gtk::Button>("card_weight")),
      m_graph_container(builder->get_widget<Gtk::Box>("graph_container")),
      m_profile_activity(builder->get_widget<Gtk::Label>("profile_activity")),
      m_profile_birthday(builder->get_widget<Gtk::Label>("profile_birthday")),
      m_profile_device_id(builder->get_widget<Gtk::Label>("profile_device_id")),
      m_profile_gender(builder->get_widget<Gtk::Label>("profile_gender")),
      m_profile_height(builder->get_widget<Gtk::Label>("profile_height")),
      m_profile_name(builder->get_widget<Gtk::Label>("profile_name")),
      m_profile_slot(builder->get_widget<Gtk::Label>("profile_slot")),
      m_profile_waist(builder->get_widget<Gtk::Label>("profile_waist")),
      m_spark_bmi_box(builder->get_widget<Gtk::Box>("spark_bmi")),
      m_spark_fat_box(builder->get_widget<Gtk::Box>("spark_fat")),
      m_spark_muscle_box(builder->get_widget<Gtk::Box>("spark_muscle")),
      m_spark_water_box(builder->get_widget<Gtk::Box>("spark_water")),
      m_spark_weight_box(builder->get_widget<Gtk::Box>("spark_weight")),
      m_status_label(builder->get_widget<Gtk::Label>("status_label")),
      m_sync_button(builder->get_widget<Gtk::Button>("sync_button")),
      m_user_list(builder->get_widget<Gtk::ListBox>("user_list")),
      m_val_bmi(builder->get_widget<Gtk::Label>("val_bmi")),
      m_val_fat(builder->get_widget<Gtk::Label>("val_fat")),
      m_val_muscle(builder->get_widget<Gtk::Label>("val_muscle")),
      m_val_water(builder->get_widget<Gtk::Label>("val_water")),
      m_val_weight(builder->get_widget<Gtk::Label>("val_weight")),
      m_main_graph(Gtk::make_managed<TrendGraph>()),
      m_history_list(builder->get_widget<Gtk::ListView>("history_list")) {
    // Get widgets from builder
    m_range_30d = builder->get_widget<Gtk::Button>("range_30d");
    m_range_90d = builder->get_widget<Gtk::Button>("range_90d");
    m_range_1y = builder->get_widget<Gtk::Button>("range_1y");
    m_range_all = builder->get_widget<Gtk::Button>("range_all");
    m_range_start_slider = builder->get_widget<Gtk::Scale>("range_start_slider");
    m_range_end_slider = builder->get_widget<Gtk::Scale>("range_end_slider");
    m_range_start_label = builder->get_widget<Gtk::Label>("range_start_label");
    m_range_end_label = builder->get_widget<Gtk::Label>("range_end_label");
    m_formula_v2_toggle = builder->get_widget<Gtk::CheckButton>("formula_v2_toggle");
    m_multi_metric_toggle = builder->get_widget<Gtk::CheckButton>("multi_metric_toggle");
    m_export_csv_button = builder->get_widget<Gtk::Button>("export_csv_button");
    m_export_json_button = builder->get_widget<Gtk::Button>("export_json_button");
    m_unit_metric_radio = builder->get_widget<Gtk::CheckButton>("unit_metric_radio");
    m_unit_imperial_radio = builder->get_widget<Gtk::CheckButton>("unit_imperial_radio");
    m_devices_list = builder->get_widget<Gtk::ListBox>("devices_list");
    m_device_details_box = builder->get_widget<Gtk::Box>("device_details_box");
    m_device_name_entry = builder->get_widget<Gtk::Entry>("device_name_entry");
    m_device_key_label = builder->get_widget<Gtk::Label>("device_key_label");
    m_device_pathway_combo = builder->get_widget<Gtk::ComboBoxText>("device_pathway_combo");
    m_device_save_button = builder->get_widget<Gtk::Button>("device_save_button");

    m_device_pathway_combo->append("FootToFoot", "Foot to Foot");
    m_device_pathway_combo->append("HandToFoot", "Hand to Foot");
    m_device_details_box->set_sensitive(false);
    m_device_save_button->set_sensitive(false);

    init_sparklines();
    init_event_handlers();

    UnitSystem const units = m_settings.get_unit_system();
    m_main_graph->set_unit_system(units);
    m_spark_weight->set_unit_system(units);
    m_spark_bmi->set_unit_system(units);
    m_spark_fat->set_unit_system(units);
    m_spark_water->set_unit_system(units);
    m_spark_muscle->set_unit_system(units);

    if (units == UnitSystem::Imperial) {
        m_unit_imperial_radio->set_active(true);
    } else {
        m_unit_metric_radio->set_active(true);
    }

    setup_history_list();
    refresh_user_list();
    refresh_device_list();

    set_name("main_window");

    auto settings = Gtk::Settings::get_default();
    if (settings) {
        settings->property_gtk_application_prefer_dark_theme().set_value(true);
    }

    // Load CSS
    auto css_provider = Gtk::CssProvider::create();
    auto css_path = find_resource("style.css");
    if (css_path.empty()) {
        spdlog::warn("Could not locate style.css in any search path; continuing without styling.");
    } else {
        try {
            css_provider->load_from_path(css_path);
            Gtk::StyleContext::add_provider_for_display(Gdk::Display::get_default(), css_provider,
                                                        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        } catch (const std::exception& e) {
            spdlog::error("Failed to load CSS from {}: {}", css_path, e.what());
        }
    }

    set_default_size(m_settings.get_window_width(), m_settings.get_window_height());
    set_icon_name("de.speedprog.comfort-select-dashboard");
    if (m_settings.get_is_maximized()) {
        maximize();
    }
    check_missing_waist_circumferences();
}

MainWindow::~MainWindow() {
    m_manager.stop_sync();
    int win_width = 0;
    int win_height = 0;
    get_default_size(win_width, win_height);
    m_settings.set_window_state(
        {.width = win_width, .height = win_height, .maximized = is_maximized()});
    m_settings.save();
}

auto MainWindow::create(HealthManager& manager, ISettingsManager& settings_manager) -> MainWindow* {
    auto ui_path = find_resource("main_window.ui");
    if (ui_path.empty()) {
        throw std::runtime_error("Could not find main_window.ui in any of the search paths.");
    }

    auto builder = Gtk::Builder::create_from_file(ui_path);
    return Gtk::Builder::get_widget_derived<MainWindow>(builder, "main_window", manager,
                                                        settings_manager);
}

void MainWindow::on_view_model_changed() {
    const auto& filtered = m_view_model.get_filtered_data();

    auto update_label = [](Gtk::Label* label, double val, const std::string& unit) -> void {
        label->set_text(std::format("{:.1f}{}", val, unit));
    };

    UnitSystem const units = m_settings.get_unit_system();

    bool const use_v2 = m_formula_v2_toggle->get_active();

    if (filtered.empty()) {
        m_val_weight->set_text(units == UnitSystem::Imperial ? "--.- lbs" : "--.- kg");
        m_val_bmi->set_text("--.-");
        m_val_fat->set_text("--.- %");
        m_val_water->set_text("--.- %");
        m_val_muscle->set_text("--.- %");
        m_range_start_label->set_text("--");
        m_range_end_label->set_text("--");
    } else {
        const auto& latest = filtered.back();
        if (units == UnitSystem::Imperial) {
            update_label(m_val_weight, latest.weight.value * kg_to_lbs_factor, " lbs");
        } else {
            update_label(m_val_weight, latest.weight.value, " kg");
        }
        update_label(m_val_bmi, latest.bmi, "");

        double body_fat = latest.body_fat.value;
        double water = latest.water.value;
        double muscle = latest.muscle_mass.value;
        if (use_v2) {
            if (latest.body_fat_v2.value > 0.0) {
                body_fat = latest.body_fat_v2.value;
            }
            if (latest.water_v2.value > 0.0) {
                water = latest.water_v2.value;
            }
            if (latest.muscle_mass_v2.value > 0.0) {
                muscle = latest.muscle_mass_v2.value;
            }
        }

        update_label(m_val_fat, body_fat, " %");
        update_label(m_val_water, water, " %");
        update_label(m_val_muscle, muscle, " %");
        m_range_start_label->set_text(std::format("{:%F}", filtered.front().date));
        m_range_end_label->set_text(std::format("{:%F}", filtered.back().date));
    }

    m_main_graph->set_use_v2(use_v2);
    m_spark_weight->set_use_v2(use_v2);
    m_spark_bmi->set_use_v2(use_v2);
    m_spark_fat->set_use_v2(use_v2);
    m_spark_water->set_use_v2(use_v2);
    m_spark_muscle->set_use_v2(use_v2);

    m_main_graph->set_metrics(m_selected_metrics);
    m_main_graph->set_data(filtered);
    m_spark_weight->set_data(filtered);
    m_spark_bmi->set_data(filtered);
    m_spark_fat->set_data(filtered);
    m_spark_water->set_data(filtered);
    m_spark_muscle->set_data(filtered);

    auto update_card_selection = [this](Gtk::Button* card, MetricType type,
                                        const std::string& type_name) -> void {
        card->remove_css_class("selected");
        card->remove_css_class(type_name);

        bool const is_selected =
            std::ranges::find(m_selected_metrics, type) != m_selected_metrics.end();
        if (is_selected) {
            card->add_css_class("selected");
            card->add_css_class(type_name);
        }
    };

    update_card_selection(m_card_weight, MetricType::Weight, "weight");
    update_card_selection(m_card_bmi, MetricType::BMI, "bmi");
    update_card_selection(m_card_fat, MetricType::BodyFat, "fat");
    update_card_selection(m_card_water, MetricType::Water, "water");
    update_card_selection(m_card_muscle, MetricType::Muscle, "muscle");

    m_history_model->remove_all();
    auto history = filtered;
    std::ranges::reverse(history);
    for (const auto& measurement : history) {
        m_history_model->append(MeasurementObject::create(measurement));
    }

    update_range_button_selection(m_active_preset);
}

void MainWindow::update_range_button_selection(int days) {
    m_range_30d->remove_css_class("selected");
    m_range_90d->remove_css_class("selected");
    m_range_1y->remove_css_class("selected");
    m_range_all->remove_css_class("selected");

    if (days == range_preset_30d) {
        m_range_30d->add_css_class("selected");
    } else if (days == range_preset_90d) {
        m_range_90d->add_css_class("selected");
    } else if (days == range_preset_1y) {
        m_range_1y->add_css_class("selected");
    } else if (days == 0) {
        m_range_all->add_css_class("selected");
    }
}

void MainWindow::on_sync_clicked() {
    m_status_label->set_text("Syncing...");
    m_sync_button->set_sensitive(false);
    m_manager.start_sync([this](const SyncResult& result) -> void {
        m_last_sync_result = result;
        m_sync_complete_dispatcher.emit();
    });
}

void MainWindow::on_sync_complete(const SyncResult& result) {
    m_sync_button->set_sensitive(true);
    if (result.success) {
        m_status_label->set_text("Sync complete");

        // Find which users in result.new_users are STILL unmapped after process_sync_results
        m_pending_reconciliation_users.clear();
        const auto all_local_users = m_manager.get_users();
        for (const auto& scale_user : result.new_users) {
            bool const is_mapped =
                std::ranges::any_of(all_local_users, [&](const UserProfile& local_user) -> bool {
                    return local_user.device_user_id == scale_user.device_user_id;
                });
            if (!is_mapped) {
                m_pending_reconciliation_users.push_back(scale_user);
            }
        }

        prompt_next_user_reconciliation();
    } else {
        m_status_label->set_text("Error: " + result.error_message);
    }
}

void MainWindow::on_user_selected(Gtk::ListBoxRow* row) {
    if (row == nullptr) {
        m_current_user = std::nullopt;
        update_profile_view();
        return;
    }
    int const index = row->get_index();
    if (index >= 0 && std::cmp_less(index, m_users.size())) {
        m_current_user = m_users[static_cast<size_t>(index)];
        load_user_data(m_current_user->id);
        update_profile_view();
    }
}

void MainWindow::refresh_user_list() {
    while (auto* row = m_user_list->get_row_at_index(0)) {
        m_user_list->remove(*row);
    }
    m_users = m_manager.get_users();
    for (const auto& user : m_users) {
        m_user_list->append(*Gtk::make_managed<Gtk::Label>(user.name));
    }

    if (!m_users.empty()) {
        int index_to_select = 0;
        if (m_current_user) {
            auto user_it = find_current_user_iterator();
            if (user_it != m_users.end()) {
                index_to_select = static_cast<int>(std::distance(m_users.cbegin(), user_it));
            }
        }
        auto* row = m_user_list->get_row_at_index(index_to_select);
        if (row != nullptr) {
            m_user_list->select_row(*row);
        }
    } else {
        m_current_user = std::nullopt;
        update_profile_view();
    }
}

void MainWindow::load_user_data(int user_id) {
    m_view_model.set_data(m_manager.get_measurements(user_id));
}

void MainWindow::on_metric_clicked(MetricType type) {
    if ((m_multi_metric_toggle != nullptr) && m_multi_metric_toggle->get_active()) {
        auto const metric_it = std::ranges::find(m_selected_metrics, type);
        if (metric_it != m_selected_metrics.end()) {
            if (m_selected_metrics.size() > 1) {
                m_selected_metrics.erase(metric_it);
            }
        } else {
            m_selected_metrics.push_back(type);
        }
    } else {
        m_selected_metrics = {type};
    }
    on_view_model_changed();
}

void MainWindow::setup_history_list() {
    m_history_model = Gio::ListStore<MeasurementObject>::create();
    m_history_list->set_model(Gtk::NoSelection::create(m_history_model));
    auto factory = Gtk::SignalListItemFactory::create();
    factory->signal_setup().connect(&MainWindow::on_setup_list_item);
    factory->signal_bind().connect(sigc::mem_fun(*this, &MainWindow::on_bind_list_item));
    m_history_list->set_factory(factory);
}

void MainWindow::on_setup_list_item(const Glib::RefPtr<Gtk::ListItem>& list_item) {
    list_item->set_child(*Gtk::make_managed<HistoryRow>());
}

void MainWindow::on_bind_list_item(const Glib::RefPtr<Gtk::ListItem>& list_item) {
    auto row_obj = std::dynamic_pointer_cast<MeasurementObject>(list_item->get_item());
    auto* row_widget = dynamic_cast<HistoryRow*>(list_item->get_child());
    if (row_obj && (row_widget != nullptr)) {
        bool const use_v2 = m_formula_v2_toggle->get_active();
        row_widget->set_measurement(row_obj->get_measurement(), m_settings.get_unit_system(),
                                    use_v2);
    }
}

void MainWindow::on_device_arrived() {
    m_status_label->set_text("Scale detected!");
    if (m_sync_button->get_sensitive()) {
        on_sync_clicked();
    }
}

void MainWindow::notify_device_arrived() {
    m_hotplug_dispatcher.emit();
}

void MainWindow::update_profile_view() {
    if (!m_current_user) {
        m_profile_name->set_text("--");
        m_profile_height->set_text("--");
        m_profile_gender->set_text("--");
        m_profile_activity->set_text("--");
        m_profile_birthday->set_text("--");
        m_profile_slot->set_text("--");
        m_profile_device_id->set_text("--");
        m_profile_waist->set_text("--");
        return;
    }

    auto user_it = find_current_user_iterator();
    if (user_it != m_users.end()) {
        m_current_user = *user_it;
    }

    const auto& user = *m_current_user;
    m_profile_name->set_text(user.name);

    UnitSystem const units = m_settings.get_unit_system();
    if (units == UnitSystem::Imperial) {
        double const total_inches = std::round(user.height.value / cm_per_inch);
        const int feet = static_cast<int>(total_inches) / inches_per_foot;
        const int inches = static_cast<int>(total_inches) % inches_per_foot;
        m_profile_height->set_text(std::format("{}' {}\"", feet, inches));
    } else {
        m_profile_height->set_text(std::format("{} cm", user.height.value));
    }

    std::string const gender_str = (user.gender == Gender::Male) ? "Male" : "Female";
    m_profile_gender->set_text(gender_str);

    std::string activity_str;
    switch (user.activity_level) {
        case ActivityLevel::Scarcely:
            activity_str = "Scarcely Active";
            break;
        case ActivityLevel::Light:
            activity_str = "Lightly Active";
            break;
        case ActivityLevel::HomeGarden:
            activity_str = "Moderately Active";
            break;
        case ActivityLevel::Sports:
            activity_str = "Very Active";
            break;
        case ActivityLevel::HighLevel:
            activity_str = "Extremely Active";
            break;
    }
    m_profile_activity->set_text(activity_str);

    if (user.birthday.ok()) {
        m_profile_birthday->set_text(std::format("{:%F}", user.birthday));
    } else {
        m_profile_birthday->set_text("--");
    }

    m_profile_slot->set_text(std::to_string(user.slot_id));
    m_profile_device_id->set_text(user.device_user_id);

    if (user.waist_circumference && user.waist_circumference->value > 0) {
        if (units == UnitSystem::Imperial) {
            double const inches = std::round(user.waist_circumference->value / cm_per_inch);
            m_profile_waist->set_text(std::format("{:.0f} in", inches));
        } else {
            m_profile_waist->set_text(std::format("{} cm", user.waist_circumference->value));
        }
    } else {
        m_profile_waist->set_text("--");
    }
}

auto MainWindow::find_current_user_iterator() const -> std::vector<UserProfile>::const_iterator {
    if (!m_current_user) {
        return m_users.end();
    }
    return std::ranges::find_if(m_users, [this](const UserProfile& user_profile) -> bool {
        return user_profile.id == m_current_user->id;
    });
}

void MainWindow::on_export_csv_clicked() {
    const auto& data = m_view_model.get_filtered_data();
    if (data.empty()) {
        show_info_dialog("No Data to Export",
                         "There are no measurements in the current view to export.");
        return;
    }

    if (!m_csv_export_dialog) {
        m_csv_export_dialog = std::make_unique<Gtk::FileChooserDialog>(
            *this, "Export CSV File", Gtk::FileChooser::Action::SAVE);
        m_csv_export_dialog->set_modal(true);
        m_csv_export_dialog->add_button("_Cancel", Gtk::ResponseType::CANCEL);
        m_csv_export_dialog->add_button("_Save", Gtk::ResponseType::ACCEPT);

        auto filter_csv = Gtk::FileFilter::create();
        filter_csv->set_name("CSV files (*.csv)");
        filter_csv->add_pattern("*.csv");
        m_csv_export_dialog->add_filter(filter_csv);
        m_csv_export_dialog->set_current_name("export.csv");

        m_csv_export_dialog->signal_response().connect([this](int response_id) -> void {
            if (response_id == Gtk::ResponseType::ACCEPT) {
                auto file = m_csv_export_dialog->get_file();
                if (file) {
                    std::string path = file->get_path();
                    if (!path.ends_with(".csv")) {
                        path += ".csv";
                    }
                    try {
                        std::ofstream out(path);
                        if (!out) {
                            throw std::runtime_error("Could not open file for writing.");
                        }
                        const auto& data = m_view_model.get_filtered_data();
                        out << libscale::DataExporter::to_csv(data);
                        m_status_label->set_text("Exported CSV to " + path);
                    } catch (const std::exception& e) {
                        show_error_dialog("Export Failed",
                                          std::format("Failed to save CSV file:\n{}", e.what()));
                    }
                }
            }
            m_csv_export_dialog->hide();
        });
    }
    m_csv_export_dialog->show();
}

void MainWindow::on_export_json_clicked() {
    const auto& data = m_view_model.get_filtered_data();
    if (data.empty()) {
        show_info_dialog("No Data to Export",
                         "There are no measurements in the current view to export.");
        return;
    }

    if (!m_json_export_dialog) {
        m_json_export_dialog = std::make_unique<Gtk::FileChooserDialog>(
            *this, "Export JSON File", Gtk::FileChooser::Action::SAVE);
        m_json_export_dialog->set_modal(true);
        m_json_export_dialog->add_button("_Cancel", Gtk::ResponseType::CANCEL);
        m_json_export_dialog->add_button("_Save", Gtk::ResponseType::ACCEPT);

        auto filter_json = Gtk::FileFilter::create();
        filter_json->set_name("JSON files (*.json)");
        filter_json->add_pattern("*.json");
        m_json_export_dialog->add_filter(filter_json);
        m_json_export_dialog->set_current_name("export.json");

        m_json_export_dialog->signal_response().connect([this](int response_id) -> void {
            if (response_id == Gtk::ResponseType::ACCEPT) {
                auto file = m_json_export_dialog->get_file();
                if (file) {
                    std::string path = file->get_path();
                    if (!path.ends_with(".json")) {
                        path += ".json";
                    }
                    try {
                        std::ofstream out(path);
                        if (!out) {
                            throw std::runtime_error("Could not open file for writing.");
                        }
                        const auto& data = m_view_model.get_filtered_data();
                        out << libscale::DataExporter::to_json(data);
                        m_status_label->set_text("Exported JSON to " + path);
                    } catch (const std::exception& e) {
                        show_error_dialog("Export Failed",
                                          std::format("Failed to save JSON file:\n{}", e.what()));
                    }
                }
            }
            m_json_export_dialog->hide();
        });
    }
    m_json_export_dialog->show();
}

void MainWindow::show_info_dialog(
    const std::string& title,      // NOLINT(bugprone-easily-swappable-parameters)
    const std::string& message) {  // NOLINT(bugprone-easily-swappable-parameters)
    m_info_dialog = std::make_unique<Gtk::MessageDialog>(
        *this, title, false, Gtk::MessageType::INFO, Gtk::ButtonsType::OK, true);
    m_info_dialog->set_secondary_text(message);
    m_info_dialog->signal_response().connect([this](int) -> void { m_info_dialog->hide(); });
    m_info_dialog->show();
}

void MainWindow::show_error_dialog(
    const std::string& title,      // NOLINT(bugprone-easily-swappable-parameters)
    const std::string& message) {  // NOLINT(bugprone-easily-swappable-parameters)
    m_error_dialog = std::make_unique<Gtk::MessageDialog>(
        *this, title, false, Gtk::MessageType::ERROR, Gtk::ButtonsType::OK, true);
    m_error_dialog->set_secondary_text(message);
    m_error_dialog->signal_response().connect([this](int) -> void { m_error_dialog->hide(); });
    m_error_dialog->show();
}

void MainWindow::check_missing_waist_circumferences() {
    m_users_needing_waist.clear();
    for (const auto& user : m_users) {
        if (!user.waist_circumference || user.waist_circumference->value <= 0) {
            m_users_needing_waist.push_back(user);
        }
    }
    prompt_next_missing_waist();
}

void MainWindow::prompt_next_missing_waist() {
    if (m_users_needing_waist.empty()) {
        return;
    }
    UserProfile user = m_users_needing_waist.front();
    m_users_needing_waist.erase(m_users_needing_waist.begin());

    auto* dialog = Gtk::make_managed<Gtk::Dialog>();
    dialog->set_title(std::format("Enter Waist Circumference for {}", user.name));
    dialog->set_transient_for(*this);
    dialog->set_modal(true);

    static constexpr int dialog_box_spacing = 10;
    static constexpr int dialog_box_margin = 15;
    auto* content_area = dialog->get_content_area();
    auto* box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, dialog_box_spacing);
    box->set_margin(dialog_box_margin);

    auto* label = Gtk::make_managed<Gtk::Label>(
        std::format("Please enter the waist circumference for {}:", user.name));

    UnitSystem const units = m_settings.get_unit_system();
    static constexpr double default_lower_cm = 30.0;
    static constexpr double default_upper_cm = 250.0;
    static constexpr double default_def_val_cm = 80.0;
    static constexpr double default_lower_in = 12.0;
    static constexpr double default_upper_in = 100.0;
    static constexpr double default_def_val_in = 32.0;
    static constexpr double step_increment = 1.0;
    static constexpr double page_increment = 5.0;
    static constexpr int box_spacing = 5;

    double lower = default_lower_cm;
    double upper = default_upper_cm;
    double def_val = default_def_val_cm;
    std::string unit_label = " cm";
    if (units == UnitSystem::Imperial) {
        lower = default_lower_in;
        upper = default_upper_in;
        def_val = default_def_val_in;
        unit_label = " inches";
    }

    auto* spin_box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, box_spacing);
    auto* spin_button = Gtk::make_managed<Gtk::SpinButton>(
        Gtk::Adjustment::create(def_val, lower, upper, step_increment, page_increment, 0.0),
        step_increment, 0);
    auto* suffix = Gtk::make_managed<Gtk::Label>(unit_label);
    spin_box->append(*spin_button);
    spin_box->append(*suffix);

    box->append(*label);
    box->append(*spin_box);
    content_area->append(*box);

    dialog->add_button("Cancel", Gtk::ResponseType::CANCEL);
    dialog->add_button("Save", Gtk::ResponseType::ACCEPT);

    dialog->signal_response().connect(
        [this, dialog, user, spin_button, units](int response_id) mutable -> void {
            if (response_id == Gtk::ResponseType::ACCEPT) {
                double val = spin_button->get_value();
                if (units == UnitSystem::Imperial) {
                    val = std::round(val * cm_per_inch);  // Convert to cm
                }
                user.waist_circumference = units::Cm{static_cast<int>(val)};
                m_manager.update_user(user);
                refresh_user_list();

                prompt_recalculate_history(user, [this]() -> void { prompt_next_missing_waist(); });
            } else {
                prompt_next_missing_waist();
            }
            dialog->hide();
        });
    dialog->show();
}

void MainWindow::prompt_recalculate_history(const UserProfile& user,
                                            const std::function<void()>& on_finished) {
    auto* dialog = Gtk::make_managed<Gtk::MessageDialog>(
        *this, "Recalculate Historical Measurements?", false, Gtk::MessageType::QUESTION,
        Gtk::ButtonsType::YES_NO, true);
    dialog->set_secondary_text(
        std::format("Would you like to recalculate all past measurements for {} using the updated "
                    "waist circumference?\n\nIf Yes, existing RFM and V2 body composition metrics "
                    "will be updated. If No, this change will only affect future measurements.",
                    user.name));

    dialog->signal_response().connect([this, dialog, user, on_finished](int response_id) -> void {
        if (response_id == Gtk::ResponseType::YES) {
            recalculate_user_history(user.id);
        }
        dialog->hide();
        if (on_finished) {
            on_finished();
        }
    });
    dialog->show();
}

void MainWindow::prompt_waist_change(const UserProfile& user) {
    auto* dialog = Gtk::make_managed<Gtk::Dialog>();
    dialog->set_title(std::format("Edit Waist Circumference for {}", user.name));
    dialog->set_transient_for(*this);
    dialog->set_modal(true);

    static constexpr int dialog_box_spacing = 10;
    static constexpr int dialog_box_margin = 15;
    auto* content_area = dialog->get_content_area();
    auto* box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, dialog_box_spacing);
    box->set_margin(dialog_box_margin);

    auto* label = Gtk::make_managed<Gtk::Label>(
        std::format("Enter new waist circumference for {}:", user.name));

    UnitSystem const units = m_settings.get_unit_system();
    static constexpr double default_lower_cm = 30.0;
    static constexpr double default_upper_cm = 250.0;
    static constexpr double default_def_val_cm = 80.0;
    static constexpr double default_lower_in = 12.0;
    static constexpr double default_upper_in = 100.0;
    static constexpr double default_def_val_in = 32.0;
    static constexpr double step_increment = 1.0;
    static constexpr double page_increment = 5.0;
    static constexpr int box_spacing = 5;

    double lower = default_lower_cm;
    double upper = default_upper_cm;
    double def_val = default_def_val_cm;
    std::string unit_label = " cm";

    if (user.waist_circumference && user.waist_circumference->value > 0) {
        if (units == UnitSystem::Imperial) {
            def_val = std::round(user.waist_circumference->value / cm_per_inch);
        } else {
            def_val = user.waist_circumference->value;
        }
    }

    if (units == UnitSystem::Imperial) {
        lower = default_lower_in;
        upper = default_upper_in;
        unit_label = " inches";
    }

    auto* spin_box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, box_spacing);
    auto* spin_button = Gtk::make_managed<Gtk::SpinButton>(
        Gtk::Adjustment::create(def_val, lower, upper, step_increment, page_increment, 0.0),
        step_increment, 0);
    auto* suffix = Gtk::make_managed<Gtk::Label>(unit_label);
    spin_box->append(*spin_button);
    spin_box->append(*suffix);

    box->append(*label);
    box->append(*spin_box);
    content_area->append(*box);

    dialog->add_button("Cancel", Gtk::ResponseType::CANCEL);
    dialog->add_button("Save", Gtk::ResponseType::ACCEPT);

    dialog->signal_response().connect(
        [this, dialog, user_copy = user, spin_button, units](int response_id) mutable -> void {
            if (response_id == Gtk::ResponseType::ACCEPT) {
                double val = spin_button->get_value();
                if (units == UnitSystem::Imperial) {
                    val = std::round(val * cm_per_inch);  // Convert to cm
                }
                user_copy.waist_circumference = units::Cm{static_cast<int>(val)};
                m_manager.update_user(user_copy);
                refresh_user_list();

                prompt_recalculate_history(user_copy, nullptr);
            }
            dialog->hide();
        });
    dialog->show();
}

void MainWindow::recalculate_user_history(int user_id) {
    m_manager.recalculate_measurements(user_id);
    load_user_data(user_id);
}

void MainWindow::prompt_next_user_reconciliation() {
    if (m_pending_reconciliation_users.empty()) {
        refresh_user_list();
        check_missing_waist_circumferences();
        if (m_current_user) {
            load_user_data(m_current_user->id);
        }
        return;
    }

    UserProfile const scale_user = m_pending_reconciliation_users.front();
    m_pending_reconciliation_users.erase(m_pending_reconciliation_users.begin());

    auto dialog = std::make_unique<Gtk::Dialog>();
    auto* dialog_raw = dialog.get();
    dialog_raw->set_title(std::format("Unmapped Scale Profile: {}", scale_user.name));
    dialog_raw->set_transient_for(*this);
    dialog_raw->set_modal(true);

    static constexpr int dialog_box_spacing = 10;
    static constexpr int dialog_box_margin = 15;
    auto* content_area = dialog_raw->get_content_area();
    auto* box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, dialog_box_spacing);
    box->set_margin(dialog_box_margin);

    auto* label = Gtk::make_managed<Gtk::Label>(
        std::format("A user profile named '{}' (Slot {}) was found on the scale but is not linked "
                    "to any local profile.\n"
                    "Select an existing local profile to link, or create a new one.",
                    scale_user.name, scale_user.slot_id));
    box->append(*label);

    auto* dropdown = Gtk::make_managed<Gtk::ComboBoxText>();
    dropdown->append("0", std::format("Create new profile: {}", scale_user.name));
    dropdown->set_active_id("0");

    auto all_local_users = m_manager.get_users();
    for (const auto& local_user : all_local_users) {
        if (local_user.device_user_id.empty()) {
            dropdown->append(std::to_string(local_user.id), local_user.name);
        }
    }
    box->append(*dropdown);
    content_area->append(*box);

    dialog_raw->add_button("Cancel", Gtk::ResponseType::CANCEL);
    dialog_raw->add_button("Link", Gtk::ResponseType::ACCEPT);

    dialog_raw->signal_response().connect(
        [this, dialog_raw, scale_user, dropdown](int response_id) mutable -> void {
            if (response_id == Gtk::ResponseType::ACCEPT) {
                std::string const selected_id_str = dropdown->get_active_id();
                std::optional<int> user_id = std::nullopt;
                if (selected_id_str != "0") {
                    user_id = std::stoi(selected_id_str);
                }
                m_manager.associate_scale_user(scale_user, user_id, m_last_sync_result);
            }
            dialog_raw->hide();
            Glib::signal_idle().connect_once([this]() -> void {
                m_active_dialog.reset();
                prompt_next_user_reconciliation();
            });
        });

    m_active_dialog = std::move(dialog);
    m_active_dialog->show();
}

void MainWindow::refresh_device_list() {
    while (auto* row = m_devices_list->get_row_at_index(0)) {
        m_devices_list->remove(*row);
    }

    m_devices = m_manager.get_devices();
    for (const auto& device_item : m_devices) {
        m_devices_list->append(*Gtk::make_managed<Gtk::Label>(device_item.name));
    }

    if (!m_devices.empty()) {
        int index_to_select = 0;
        if (m_selected_device) {
            auto found_device =
                std::ranges::find_if(m_devices, [this](const Device& device_item) -> bool {
                    return device_item.id == m_selected_device->id;
                });
            if (found_device != m_devices.end()) {
                index_to_select = static_cast<int>(std::distance(m_devices.begin(), found_device));
            }
        }
        auto* row = m_devices_list->get_row_at_index(index_to_select);
        if (row != nullptr) {
            m_devices_list->select_row(*row);
        }
    } else {
        m_selected_device = std::nullopt;
        m_device_details_box->set_sensitive(false);
        m_device_save_button->set_sensitive(false);
    }
}

void MainWindow::on_device_selected(Gtk::ListBoxRow* row) {
    if (row == nullptr) {
        m_selected_device = std::nullopt;
        m_device_details_box->set_sensitive(false);
        m_device_save_button->set_sensitive(false);
        return;
    }
    int const index = row->get_index();
    if (index >= 0 && std::cmp_less(index, m_devices.size())) {
        m_selected_device = m_devices[static_cast<size_t>(index)];

        m_device_name_entry->set_text(m_selected_device->name);
        m_device_key_label->set_text(m_selected_device->device_key);

        if (m_selected_device->pathway == ImpedancePathway::FootToFoot) {
            m_device_pathway_combo->set_active_id("FootToFoot");
        } else {
            m_device_pathway_combo->set_active_id("HandToFoot");
        }

        m_device_details_box->set_sensitive(true);
        on_device_name_changed();
    }
}

void MainWindow::on_device_name_changed() {
    if (!m_selected_device) {
        m_device_save_button->set_sensitive(false);
        return;
    }
    std::string const name = m_device_name_entry->get_text();
    bool const is_empty_or_whitespace = std::ranges::all_of(name, [](char character_item) -> bool {
        return character_item == ' ' || character_item == '\t' || character_item == '\n' ||
               character_item == '\v' || character_item == '\f' || character_item == '\r';
    });
    m_device_save_button->set_sensitive(!is_empty_or_whitespace);
}

void MainWindow::on_device_save_clicked() {
    if (!m_selected_device) {
        return;
    }

    std::string const name = m_device_name_entry->get_text();
    bool const is_empty_or_whitespace = std::ranges::all_of(name, [](char character_item) -> bool {
        return character_item == ' ' || character_item == '\t' || character_item == '\n' ||
               character_item == '\v' || character_item == '\f' || character_item == '\r';
    });
    if (is_empty_or_whitespace) {
        show_error_dialog("Invalid Name",
                          "Device name cannot be empty or consist only of whitespace.");
        return;
    }

    ImpedancePathway pathway = ImpedancePathway::HandToFoot;
    if (m_device_pathway_combo->get_active_id() == "FootToFoot") {
        pathway = ImpedancePathway::FootToFoot;
    }

    bool const pathway_changed = (pathway != m_selected_device->pathway);
    int const device_id = m_selected_device->id;

    Device updated_device = *m_selected_device;
    updated_device.name = name;
    updated_device.pathway = pathway;

    m_manager.update_device(updated_device);
    m_selected_device = updated_device;

    refresh_device_list();

    if (pathway_changed) {
        auto* dialog = Gtk::make_managed<Gtk::MessageDialog>(
            *this, "Recalculate Historical Measurements?", false, Gtk::MessageType::QUESTION,
            Gtk::ButtonsType::YES_NO, true);
        dialog->set_secondary_text(
            std::format("The electrode pathway for '{}' has been modified.\n\n"
                        "Would you like to recalculate the body composition history for all users "
                        "who have measurements recorded from this device?",
                        updated_device.name));

        dialog->signal_response().connect([this, dialog, device_id](int response_id) -> void {
            if (response_id == Gtk::ResponseType::YES) {
                m_manager.recalculate_device_measurements(device_id);
                if (m_current_user) {
                    load_user_data(m_current_user->id);
                    update_profile_view();
                }
            }
            dialog->hide();
        });
        dialog->show();
    } else {
        if (m_current_user) {
            update_profile_view();
        }
    }
}

void MainWindow::init_sparklines() {
    m_main_graph->set_expand(true);
    m_main_graph->set_size_request(default_graph_width, default_graph_height);
    if (m_graph_container != nullptr) {
        m_graph_container->append(*m_main_graph);
    }

    auto create_spark = [&](Gtk::Box* container, MetricType type) -> auto {
        auto* spark = Gtk::make_managed<TrendGraph>();
        spark->set_compact_mode(true);
        spark->set_metric(type);
        spark->set_expand(true);
        if (container != nullptr) {
            container->set_can_target(false);
            container->append(*spark);
        }
        return spark;
    };

    m_spark_weight = create_spark(m_spark_weight_box, MetricType::Weight);
    m_spark_bmi = create_spark(m_spark_bmi_box, MetricType::BMI);
    m_spark_fat = create_spark(m_spark_fat_box, MetricType::BodyFat);
    m_spark_water = create_spark(m_spark_water_box, MetricType::Water);
    m_spark_muscle = create_spark(m_spark_muscle_box, MetricType::Muscle);
}

void MainWindow::init_event_handlers() {
    m_sync_button->signal_clicked().connect(sigc::mem_fun(*this, &MainWindow::on_sync_clicked));
    m_user_list->signal_row_selected().connect(sigc::mem_fun(*this, &MainWindow::on_user_selected));
    m_btn_edit_waist->signal_clicked().connect([this]() -> void {
        if (m_current_user) {
            prompt_waist_change(*m_current_user);
        }
    });

    m_card_weight->signal_clicked().connect(
        [this]() -> void { on_metric_clicked(MetricType::Weight); });
    m_card_bmi->signal_clicked().connect([this]() -> void { on_metric_clicked(MetricType::BMI); });
    m_card_fat->signal_clicked().connect(
        [this]() -> void { on_metric_clicked(MetricType::BodyFat); });
    m_card_water->signal_clicked().connect(
        [this]() -> void { on_metric_clicked(MetricType::Water); });
    m_card_muscle->signal_clicked().connect(
        [this]() -> void { on_metric_clicked(MetricType::Muscle); });

    m_formula_v2_toggle->signal_toggled().connect([this]() -> void { on_view_model_changed(); });

    m_multi_metric_toggle->signal_toggled().connect([this]() -> void {
        if (!m_multi_metric_toggle->get_active()) {
            if (m_selected_metrics.empty()) {
                m_selected_metrics = {MetricType::Weight};
            } else {
                m_selected_metrics = {m_selected_metrics.front()};
            }
        }
        on_view_model_changed();
    });

    m_range_30d->signal_clicked().connect([this]() -> void {
        m_active_preset = range_preset_30d;
        m_view_model.set_range_preset(range_preset_30d);
    });
    m_range_90d->signal_clicked().connect([this]() -> void {
        m_active_preset = range_preset_90d;
        m_view_model.set_range_preset(range_preset_90d);
    });
    m_range_1y->signal_clicked().connect([this]() -> void {
        m_active_preset = range_preset_1y;
        m_view_model.set_range_preset(range_preset_1y);
    });
    m_range_all->signal_clicked().connect([this]() -> void {
        m_active_preset = 0;
        m_view_model.set_range_preset(0);
    });

    if (m_export_csv_button != nullptr) {
        m_export_csv_button->signal_clicked().connect(
            sigc::mem_fun(*this, &MainWindow::on_export_csv_clicked));
    }
    if (m_export_json_button != nullptr) {
        m_export_json_button->signal_clicked().connect(
            sigc::mem_fun(*this, &MainWindow::on_export_json_clicked));
    }

    auto on_slider_changed = [this]() -> void {
        if (!m_updating_from_model) {
            m_active_preset = -1;
            m_view_model.set_range_percentages(m_range_start_slider->get_value(),
                                               m_range_end_slider->get_value());
        }
    };
    m_range_start_slider->signal_value_changed().connect(on_slider_changed);
    m_range_end_slider->signal_value_changed().connect(on_slider_changed);

    m_view_model.signal_data_changed().connect(
        sigc::mem_fun(*this, &MainWindow::on_view_model_changed));

    m_hotplug_dispatcher.connect(sigc::mem_fun(*this, &MainWindow::on_device_arrived));
    m_sync_complete_dispatcher.connect([this]() -> void { on_sync_complete(m_last_sync_result); });

    auto on_unit_changed = [this]() -> void {
        UnitSystem const units_val =
            m_unit_imperial_radio->get_active() ? UnitSystem::Imperial : UnitSystem::Metric;
        if (units_val != m_settings.get_unit_system()) {
            m_settings.set_unit_system(units_val);
            m_settings.save();

            m_main_graph->set_unit_system(units_val);
            m_spark_weight->set_unit_system(units_val);
            m_spark_bmi->set_unit_system(units_val);
            m_spark_fat->set_unit_system(units_val);
            m_spark_water->set_unit_system(units_val);
            m_spark_muscle->set_unit_system(units_val);

            on_view_model_changed();
            update_profile_view();
        }
    };
    m_unit_metric_radio->signal_toggled().connect(on_unit_changed);
    m_unit_imperial_radio->signal_toggled().connect(on_unit_changed);

    m_devices_list->signal_row_selected().connect(
        sigc::mem_fun(*this, &MainWindow::on_device_selected));
    m_device_save_button->signal_clicked().connect(
        sigc::mem_fun(*this, &MainWindow::on_device_save_clicked));
    m_device_name_entry->signal_changed().connect(
        sigc::mem_fun(*this, &MainWindow::on_device_name_changed));
}

}  // namespace libscale
// NOLINTEND(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
