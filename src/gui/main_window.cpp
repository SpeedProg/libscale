#include "gui/main_window.hpp"

#include <gdkmm/display.h>
#include <giomm/liststore.h>
#include <glibmm/refptr.h>
#include <glibmm/ustring.h>
#include <gtk/gtk.h>
#include <gtkmm/applicationwindow.h>
#include <gtkmm/box.h>
#include <gtkmm/builder.h>
#include <gtkmm/button.h>
#include <gtkmm/cssprovider.h>
#include <gtkmm/label.h>
#include <gtkmm/listbox.h>
#include <gtkmm/listboxrow.h>
#include <gtkmm/listitem.h>
#include <gtkmm/listview.h>
#include <gtkmm/noselection.h>
#include <gtkmm/object.h>
#include <gtkmm/scale.h>
#include <gtkmm/signallistitemfactory.h>
#include <gtkmm/stylecontext.h>
#include <sigc++/functors/mem_fun.h>

#include <algorithm>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <format>
#include <iostream>
#include <iterator>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "gui/graph.hpp"
#include "gui/history_row.hpp"
#include "libscale/types.hpp"
#include "gui/settings.hpp"
#include "libscale/manager.hpp"
#include "libscale/worker.hpp"

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
        "../share/comfort_select_dashboard",
        "share/comfort_select_dashboard",
        "/usr/share/comfort_select_dashboard",
        "/usr/local/share/comfort_select_dashboard",
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

MainWindow::MainWindow(BaseObjectType* cobject, const Glib::RefPtr<Gtk::Builder>& builder,
                       HealthManager& manager, ISettingsManager& settings_manager)
    : Gtk::ApplicationWindow(cobject),
      m_manager(manager),
      m_settings(settings_manager),
      m_builder(builder),
      m_main_graph(Gtk::make_managed<TrendGraph>()) {
    // Get widgets from builder
    m_sync_button = builder->get_widget<Gtk::Button>("sync_button");
    m_status_label = builder->get_widget<Gtk::Label>("status_label");
    m_user_list = builder->get_widget<Gtk::ListBox>("user_list");
    m_graph_container = builder->get_widget<Gtk::Box>("graph_container");
    m_profile_name = builder->get_widget<Gtk::Label>("profile_name");
    m_profile_height = builder->get_widget<Gtk::Label>("profile_height");
    m_profile_gender = builder->get_widget<Gtk::Label>("profile_gender");
    m_profile_activity = builder->get_widget<Gtk::Label>("profile_activity");
    m_profile_birthday = builder->get_widget<Gtk::Label>("profile_birthday");
    m_profile_slot = builder->get_widget<Gtk::Label>("profile_slot");
    m_profile_device_id = builder->get_widget<Gtk::Label>("profile_device_id");

    m_card_weight = builder->get_widget<Gtk::Button>("card_weight");
    m_card_bmi = builder->get_widget<Gtk::Button>("card_bmi");
    m_card_fat = builder->get_widget<Gtk::Button>("card_fat");
    m_card_water = builder->get_widget<Gtk::Button>("card_water");
    m_card_muscle = builder->get_widget<Gtk::Button>("card_muscle");

    m_val_weight = builder->get_widget<Gtk::Label>("val_weight");
    m_val_bmi = builder->get_widget<Gtk::Label>("val_bmi");
    m_val_fat = builder->get_widget<Gtk::Label>("val_fat");
    m_val_water = builder->get_widget<Gtk::Label>("val_water");
    m_val_muscle = builder->get_widget<Gtk::Label>("val_muscle");

    m_spark_weight_box = builder->get_widget<Gtk::Box>("spark_weight");
    m_spark_bmi_box = builder->get_widget<Gtk::Box>("spark_bmi");
    m_spark_fat_box = builder->get_widget<Gtk::Box>("spark_fat");
    m_spark_water_box = builder->get_widget<Gtk::Box>("spark_water");
    m_spark_muscle_box = builder->get_widget<Gtk::Box>("spark_muscle");

    m_history_list = builder->get_widget<Gtk::ListView>("history_list");

    m_main_graph->set_expand(true);
    m_main_graph->set_size_request(default_graph_width, default_graph_height);
    if (m_graph_container != nullptr) {
        m_graph_container->append(*m_main_graph);
    }

    auto create_spark = [&](Gtk::Box* container, MetricType type) {
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

    // Signals
    m_sync_button->signal_clicked().connect(sigc::mem_fun(*this, &MainWindow::on_sync_clicked));
    m_user_list->signal_row_selected().connect(sigc::mem_fun(*this, &MainWindow::on_user_selected));

    m_card_weight->signal_clicked().connect([this]() { on_metric_clicked(MetricType::Weight); });
    m_card_bmi->signal_clicked().connect([this]() { on_metric_clicked(MetricType::BMI); });
    m_card_fat->signal_clicked().connect([this]() { on_metric_clicked(MetricType::BodyFat); });
    m_card_water->signal_clicked().connect([this]() { on_metric_clicked(MetricType::Water); });
    m_card_muscle->signal_clicked().connect([this]() { on_metric_clicked(MetricType::Muscle); });

    m_range_30d = builder->get_widget<Gtk::Button>("range_30d");
    m_range_90d = builder->get_widget<Gtk::Button>("range_90d");
    m_range_1y = builder->get_widget<Gtk::Button>("range_1y");
    m_range_all = builder->get_widget<Gtk::Button>("range_all");
    m_range_start_slider = builder->get_widget<Gtk::Scale>("range_start_slider");
    m_range_end_slider = builder->get_widget<Gtk::Scale>("range_end_slider");
    m_range_start_label = builder->get_widget<Gtk::Label>("range_start_label");
    m_range_end_label = builder->get_widget<Gtk::Label>("range_end_label");

    m_range_30d->signal_clicked().connect([this]() {
        m_active_preset = range_preset_30d;
        m_view_model.set_range_preset(range_preset_30d);
    });
    m_range_90d->signal_clicked().connect([this]() {
        m_active_preset = range_preset_90d;
        m_view_model.set_range_preset(range_preset_90d);
    });
    m_range_1y->signal_clicked().connect([this]() {
        m_active_preset = range_preset_1y;
        m_view_model.set_range_preset(range_preset_1y);
    });
    m_range_all->signal_clicked().connect([this]() {
        m_active_preset = 0;
        m_view_model.set_range_preset(0);
    });

    auto on_slider_changed = [this]() {
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
    m_sync_complete_dispatcher.connect([this]() { on_sync_complete(m_last_sync_result); });

    setup_history_list();
    refresh_user_list();

    // Load CSS
    auto css_provider = Gtk::CssProvider::create();
    auto css_path = find_resource("style.css");
    if (!css_path.empty()) {
        try {
            css_provider->load_from_path(css_path);
            Gtk::StyleContext::add_provider_for_display(Gdk::Display::get_default(), css_provider,
                                                        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        } catch (const std::exception& e) {
            std::cerr << "Failed to load CSS: " << e.what() << "\n";
        }
    }

    set_default_size(m_settings.get_window_width(), m_settings.get_window_height());
    set_icon_name("comfort-select-dashboard");
    if (m_settings.get_is_maximized()) {
        maximize();
    }
}

MainWindow::~MainWindow() {
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

    auto update_label = [](Gtk::Label* label, double val, const std::string& unit) {
        label->set_text(std::format("{:.1f}{}", val, unit));
    };

    if (filtered.empty()) {
        m_val_weight->set_text("--.- kg");
        m_val_bmi->set_text("--.-");
        m_val_fat->set_text("--.- %");
        m_val_water->set_text("--.- %");
        m_val_muscle->set_text("--.- %");
        m_range_start_label->set_text("--");
        m_range_end_label->set_text("--");
    } else {
        const auto& latest = filtered.back();
        update_label(m_val_weight, latest.weight.value, " kg");
        update_label(m_val_bmi, latest.bmi, "");
        update_label(m_val_fat, latest.body_fat.value, " %");
        update_label(m_val_water, latest.water.value, " %");
        update_label(m_val_muscle, latest.muscle_mass.value, " %");
        m_range_start_label->set_text(std::format("{:%F}", filtered.front().date));
        m_range_end_label->set_text(std::format("{:%F}", filtered.back().date));
    }

    m_main_graph->set_data(filtered);
    m_spark_weight->set_data(filtered);
    m_spark_bmi->set_data(filtered);
    m_spark_fat->set_data(filtered);
    m_spark_water->set_data(filtered);
    m_spark_muscle->set_data(filtered);

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
    m_manager.start_sync([this](SyncResult result) {
        m_last_sync_result = std::move(result);
        m_sync_complete_dispatcher.emit();
    });
}

void MainWindow::on_sync_complete(const SyncResult& result) {
    m_sync_button->set_sensitive(true);
    if (result.success) {
        m_status_label->set_text("Sync complete");
        if (!result.new_users.empty()) {
            refresh_user_list();
        }
        if (m_current_user) {
            load_user_data(m_current_user->id);
        }
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
    m_main_graph->set_metric(type);
    m_card_weight->remove_css_class("selected");
    m_card_bmi->remove_css_class("selected");
    m_card_fat->remove_css_class("selected");
    m_card_water->remove_css_class("selected");
    m_card_muscle->remove_css_class("selected");

    switch (type) {
        case MetricType::Weight:
            m_card_weight->add_css_class("selected");
            break;
        case MetricType::BMI:
            m_card_bmi->add_css_class("selected");
            break;
        case MetricType::BodyFat:
            m_card_fat->add_css_class("selected");
            break;
        case MetricType::Water:
            m_card_water->add_css_class("selected");
            break;
        case MetricType::Muscle:
            m_card_muscle->add_css_class("selected");
            break;
    }
}

void MainWindow::setup_history_list() {
    m_history_model = Gio::ListStore<MeasurementObject>::create();
    m_history_list->set_model(Gtk::NoSelection::create(m_history_model));
    auto factory = Gtk::SignalListItemFactory::create();
    factory->signal_setup().connect(&MainWindow::on_setup_list_item);
    factory->signal_bind().connect(&MainWindow::on_bind_list_item);
    m_history_list->set_factory(factory);
}

void MainWindow::on_setup_list_item(const Glib::RefPtr<Gtk::ListItem>& list_item) {
    list_item->set_child(*Gtk::make_managed<HistoryRow>());
}

void MainWindow::on_bind_list_item(const Glib::RefPtr<Gtk::ListItem>& list_item) {
    auto row_obj = std::dynamic_pointer_cast<MeasurementObject>(list_item->get_item());
    auto* row_widget = dynamic_cast<HistoryRow*>(list_item->get_child());
    if (row_obj && (row_widget != nullptr)) {
        row_widget->set_measurement(row_obj->get_measurement());
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
        return;
    }

    auto user_it = find_current_user_iterator();
    if (user_it != m_users.end()) {
        m_current_user = *user_it;
    }

    const auto& user = *m_current_user;
    m_profile_name->set_text(user.name);
    m_profile_height->set_text(std::format("{} cm", user.height.value));

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
}

auto MainWindow::find_current_user_iterator() const -> std::vector<UserProfile>::const_iterator {
    if (!m_current_user) {
        return m_users.end();
    }
    return std::ranges::find_if(m_users, [this](const UserProfile& user_profile) {
        return user_profile.id == m_current_user->id;
    });
}

}  // namespace libscale
