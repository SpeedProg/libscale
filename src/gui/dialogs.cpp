#include "gui/dialogs.hpp"

#include <gtkmm/adjustment.h>
#include <gtkmm/box.h>
#include <gtkmm/comboboxtext.h>
#include <gtkmm/filefilter.h>
#include <gtkmm/label.h>
#include <gtkmm/messagedialog.h>
#include <gtkmm/spinbutton.h>

#include <cmath>
#include <exception>
#include <format>
#include <fstream>
#include <stdexcept>
#include <utility>

#include "gui/settings.hpp"
#include "libscale/types.hpp"

namespace comfort_select {

void prompt_waist_circumference(
    Gtk::Window& parent, const std::string& user_name, UnitSystem units,
    std::optional<libscale::units::Cm> current_value,
    const std::function<void(std::optional<libscale::units::Cm>)>& on_response) {
    auto* dialog = Gtk::make_managed<Gtk::Dialog>();
    dialog->set_title(std::format("Enter Waist Circumference for {}", user_name));
    dialog->set_transient_for(parent);
    dialog->set_modal(true);

    static constexpr int dialog_box_spacing = 10;
    static constexpr int dialog_box_margin = 15;
    auto* content_area = dialog->get_content_area();
    auto* box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, dialog_box_spacing);
    box->set_margin(dialog_box_margin);

    auto* label = Gtk::make_managed<Gtk::Label>(
        std::format("Please enter the waist circumference for {}:", user_name));

    static constexpr double default_lower_cm = 30.0;
    static constexpr double default_upper_cm = 250.0;
    static constexpr double default_value_cm = 80.0;
    static constexpr double default_lower_in = 12.0;
    static constexpr double default_upper_in = 100.0;
    static constexpr double default_value_in = 32.0;
    static constexpr double step_increment = 1.0;
    static constexpr double page_increment = 5.0;
    static constexpr int box_spacing = 5;

    double lower = default_lower_cm;
    double upper = default_upper_cm;
    double value = default_value_cm;
    std::string unit_label = " cm";
    if (units == UnitSystem::Imperial) {
        lower = default_lower_in;
        upper = default_upper_in;
        value = default_value_in;
        unit_label = " inches";
    }
    if (current_value && current_value->value > 0) {
        value = (units == UnitSystem::Imperial)
                    ? std::round(static_cast<double>(current_value->value) / cm_per_inch)
                    : static_cast<double>(current_value->value);
    }

    auto* spin_box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, box_spacing);
    auto* spin_button = Gtk::make_managed<Gtk::SpinButton>(
        Gtk::Adjustment::create(value, lower, upper, step_increment, page_increment, 0.0),
        step_increment, 0);
    spin_box->append(*spin_button);
    auto* suffix = Gtk::make_managed<Gtk::Label>(unit_label);
    spin_box->append(*suffix);

    box->append(*label);
    box->append(*spin_box);
    content_area->append(*box);

    dialog->add_button("Cancel", Gtk::ResponseType::CANCEL);
    dialog->add_button("Save", Gtk::ResponseType::ACCEPT);

    dialog->signal_response().connect(
        [dialog, spin_button, units, on_response](int response_id) -> void {
            std::optional<libscale::units::Cm> result;
            if (response_id == Gtk::ResponseType::ACCEPT) {
                double val = spin_button->get_value();
                if (units == UnitSystem::Imperial) {
                    val = std::round(val * cm_per_inch);  // Convert to cm
                }
                result = libscale::units::Cm{static_cast<int>(val)};
            }
            dialog->hide();
            if (on_response) {
                on_response(result);
            }
        });
    dialog->show();
}

void confirm_action(Gtk::Window& parent,
                    const std::string& title,  // NOLINT(bugprone-easily-swappable-parameters)
                    const std::string& message,
                    const std::function<void(bool confirmed)>& on_response) {
    auto* dialog = Gtk::make_managed<Gtk::MessageDialog>(
        parent, title, false, Gtk::MessageType::QUESTION, Gtk::ButtonsType::YES_NO, true);
    dialog->set_secondary_text(message);

    dialog->signal_response().connect([dialog, on_response](int response_id) -> void {
        dialog->hide();
        if (on_response) {
            on_response(response_id == Gtk::ResponseType::YES);
        }
    });
    dialog->show();
}

auto create_scale_link_dialog(
    Gtk::Window& parent, const UserProfile& scale_user,
    const std::vector<UserProfile>& linkable_users,
    const std::function<void(bool accepted, std::optional<int> user_id)>& on_response)
    -> std::unique_ptr<Gtk::Dialog> {
    auto dialog = std::make_unique<Gtk::Dialog>();
    auto* dialog_raw = dialog.get();
    dialog_raw->set_title(std::format("Unmapped Scale Profile: {}", scale_user.name));
    dialog_raw->set_transient_for(parent);
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
    for (const auto& local_user : linkable_users) {
        dropdown->append(std::to_string(local_user.id), local_user.name);
    }
    box->append(*dropdown);
    content_area->append(*box);

    dialog_raw->add_button("Cancel", Gtk::ResponseType::CANCEL);
    dialog_raw->add_button("Link", Gtk::ResponseType::ACCEPT);

    dialog_raw->signal_response().connect(
        [dialog_raw, dropdown, on_response](int response_id) -> void {
            bool const accepted = response_id == Gtk::ResponseType::ACCEPT;
            std::optional<int> user_id;
            if (accepted) {
                std::string const selected_id = dropdown->get_active_id();
                if (selected_id != "0") {
                    user_id = std::stoi(selected_id);
                }
            }
            dialog_raw->hide();
            if (on_response) {
                on_response(accepted, user_id);
            }
        });
    return dialog;
}

namespace {

void write_exported_content(const Gtk::FileChooserDialog& dialog, const std::string& extension,
                            const std::function<std::string()>& serialize,
                            const std::function<void(const std::string& path)>& on_saved,
                            const std::function<void(const std::string& message)>& on_error) {
    auto const file = dialog.get_file();
    if (!file) {
        return;
    }

    std::string path = file->get_path();
    if (!path.ends_with("." + extension)) {
        path += "." + extension;
    }

    try {
        std::ofstream out(path);
        if (!out) {
            throw std::runtime_error("Could not open file for writing.");
        }
        out << serialize();
        if (on_saved) {
            on_saved(path);
        }
    } catch (const std::exception& e) {
        if (on_error) {
            on_error(e.what());
        }
    }
}

}  // namespace

void export_measurements(Gtk::Window& parent, std::unique_ptr<Gtk::FileChooserDialog>& dialog_slot,
                         const ExportDialogOptions& options,
                         const std::function<std::string()>& serialize,
                         const std::function<void(const std::string& path)>& on_saved,
                         const std::function<void(const std::string& message)>& on_error) {
    if (!dialog_slot) {
        dialog_slot = std::make_unique<Gtk::FileChooserDialog>(parent, options.title,
                                                               Gtk::FileChooser::Action::SAVE);
        dialog_slot->set_modal(true);
        dialog_slot->add_button("_Cancel", Gtk::ResponseType::CANCEL);
        dialog_slot->add_button("_Save", Gtk::ResponseType::ACCEPT);

        auto const filter = Gtk::FileFilter::create();
        filter->set_name(options.filter_label);
        filter->add_pattern("*." + options.extension);
        dialog_slot->add_filter(filter);
        dialog_slot->set_current_name(options.default_name);

        auto* dialog_raw = dialog_slot.get();
        auto const extension = options.extension;
        dialog_slot->signal_response().connect(
            [dialog_raw, extension, serialize, on_saved, on_error](int response_id) -> void {
                if (response_id == Gtk::ResponseType::ACCEPT) {
                    write_exported_content(*dialog_raw, extension, serialize, on_saved, on_error);
                }
                dialog_raw->hide();
            });
    }
    dialog_slot->show();
}

}  // namespace comfort_select
