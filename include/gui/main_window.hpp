#pragma once
#include <gtkmm/applicationwindow.h>
#include <gtkmm/builder.h>
#include <gtkmm/button.h>
#include <gtkmm/entry.h>
#include <gtkmm/label.h>
#include <gtkmm/listbox.h>
#include <gtkmm/listview.h>
#include <gtkmm/listitem.h>
#include <gtkmm/box.h>
#include <gtkmm/scale.h>
#include <giomm/liststore.h>
#include <glibmm/dispatcher.h>
#include <optional>
#include <utility>
#include <vector>

#include "libscale/manager.hpp"
#include "gui/graph.hpp"
#include "gui/settings.hpp"
#include "gui/dashboard_view_model.hpp"

namespace libscale {

/**
 * @brief Glib::Object wrapper for raw Measurement structures.
 * Necessary for binding and populating standard Gtk::ListView dynamic models.
 */
class MeasurementObject : public Glib::Object {
   public:
    /**
     * @brief Factory method creating a shared pointer-managed Glib reference wrapper for a Measurement.
     * @param measurement Struct to wrap.
     * @return Reference-counted Glib wrapper instance.
     */
    static auto create(const Measurement& measurement) -> Glib::RefPtr<MeasurementObject> {
        // NOLINTNEXTLINE(cppcoreguidelines-owning-memory): Glib reference-counting architecture requires allocating with raw new to allow RefPtr to take reference-counted ownership.
        return Glib::make_refptr_for_instance<MeasurementObject>(new MeasurementObject(measurement));
    }

    /**
     * @brief Retrieves the underlying wrapped Measurement record.
     * @return Const reference to the struct.
     */
    auto get_measurement() const -> const Measurement& { return m_measurement; }

   protected:
    /**
     * @brief Protected constructor initializing the wrapped Measurement.
     * @param measurement Measurement.
     */
    explicit MeasurementObject(Measurement measurement) : m_measurement(std::move(measurement)) {}

   private:
    Measurement m_measurement;
};

/**
 * @brief The main graphical window of the Comfort Select Comfort Select application.
 * Manages GTK4 builders, maps layout widget signals, binds MVVM view-models, and orchestrates
 * background hotplug event dispatchers.
 */
class MainWindow : public Gtk::ApplicationWindow {
   public:
    /**
     * @brief Constructs the MainWindow.
     * @param cobject Underlying GTK application window instance pointer.
     * @param builder Gtk::Builder instance parsing the UI XML templates.
     * @param manager health manager orchestrator reference.
     * @param settings_manager persistence manager reference.
     */
    MainWindow(BaseObjectType* cobject, const Glib::RefPtr<Gtk::Builder>& builder,
               HealthManager& manager, ISettingsManager& settings_manager);

    /**
     * @brief Destructor. Disconnects hardware hotplug signals.
     */
    ~MainWindow() override;

    MainWindow(const MainWindow&) = delete;
    auto operator=(const MainWindow&) -> MainWindow& = delete;
    MainWindow(MainWindow&&) = delete;
    auto operator=(MainWindow&&) -> MainWindow& = delete;

    /**
     * @brief Factory method constructing and instantiating the MainWindow from GTK4 Builder XML.
     * @param manager health manager reference.
     * @param settings_manager persistence manager reference.
     * @return Pointer to the allocated MainWindow object.
     */
    static auto create(HealthManager& manager, ISettingsManager& settings_manager) -> MainWindow*;

    /**
     * @brief Triggers the internal hotplug dispatcher to notify the main window of device arrival.
     * Safe to call from any background polling thread.
     */
    void notify_device_arrived();

   private:
    void on_sync_clicked();
    void on_sync_complete(const SyncResult& result);
    void on_user_selected(Gtk::ListBoxRow* row);
    void on_add_user_clicked();
    void refresh_user_list();
    void load_user_data(int user_id);
    void on_device_arrived();
    void on_metric_clicked(MetricType type);
    
    // ViewModel integration
    void on_view_model_changed();
    void update_range_button_selection(int days);

    void setup_history_list();
    static void on_setup_list_item(const Glib::RefPtr<Gtk::ListItem>& list_item);
    static void on_bind_list_item(const Glib::RefPtr<Gtk::ListItem>& list_item);

    HealthManager& m_manager;
    ISettingsManager& m_settings;
    DashboardViewModel m_view_model;

    Glib::Dispatcher m_hotplug_dispatcher;
    Glib::Dispatcher m_sync_complete_dispatcher;
    SyncResult m_last_sync_result;

    Glib::RefPtr<Gtk::Builder> m_builder;

    Gtk::Button* m_sync_button = nullptr;
    Gtk::Label* m_status_label = nullptr;
    Gtk::ListBox* m_user_list = nullptr;
    Gtk::Box* m_graph_container = nullptr;
    Gtk::Entry* m_user_name_entry = nullptr;
    Gtk::Button* m_add_user_button = nullptr;

    // Metric Cards
    Gtk::Button* m_card_weight = nullptr;
    Gtk::Button* m_card_bmi = nullptr;
    Gtk::Button* m_card_fat = nullptr;
    Gtk::Button* m_card_water = nullptr;
    Gtk::Button* m_card_muscle = nullptr;

    Gtk::Label* m_val_weight = nullptr;
    Gtk::Label* m_val_bmi = nullptr;
    Gtk::Label* m_val_fat = nullptr;
    Gtk::Label* m_val_water = nullptr;
    Gtk::Label* m_val_muscle = nullptr;

    Gtk::Box* m_spark_weight_box = nullptr;
    Gtk::Box* m_spark_bmi_box = nullptr;
    Gtk::Box* m_spark_fat_box = nullptr;
    Gtk::Box* m_spark_water_box = nullptr;
    Gtk::Box* m_spark_muscle_box = nullptr;

    TrendGraph* m_main_graph = nullptr;
    TrendGraph* m_spark_weight = nullptr;
    TrendGraph* m_spark_bmi = nullptr;
    TrendGraph* m_spark_fat = nullptr;
    TrendGraph* m_spark_water = nullptr;
    TrendGraph* m_spark_muscle = nullptr;

    // History List
    Gtk::ListView* m_history_list = nullptr;
    Glib::RefPtr<Gio::ListStore<MeasurementObject>> m_history_model;

    std::vector<UserProfile> m_users;
    std::optional<UserProfile> m_current_user;

    // Date range filtering widgets
    Gtk::Button* m_range_30d = nullptr;
    Gtk::Button* m_range_90d = nullptr;
    Gtk::Button* m_range_1y = nullptr;
    Gtk::Button* m_range_all = nullptr;
    Gtk::Scale* m_range_start_slider = nullptr;
    Gtk::Scale* m_range_end_slider = nullptr;
    Gtk::Label* m_range_start_label = nullptr;
    Gtk::Label* m_range_end_label = nullptr;
    
    int m_active_preset = 0;
    bool m_updating_from_model = false;
};

}  // namespace libscale
