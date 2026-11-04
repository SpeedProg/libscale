#pragma once
#include <gtkmm/dialog.h>
#include <gtkmm/filechooserdialog.h>
#include <gtkmm/window.h>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "gui/settings.hpp"
#include "libscale/types.hpp"

namespace comfort_select {

using libscale::UserProfile;

/**
 * @brief Prompts the user for a waist circumference in the active unit system.
 * @param parent Window the dialog is transient for.
 * @param user_name Display name of the affected user profile.
 * @param units Active unit system, deciding the spin button range and unit label.
 * @param current_value Existing waist circumference, used as the initial value when present.
 * @param on_response Callback invoked after the dialog closes with the edited value in
 * centimeters, or std::nullopt when the dialog was cancelled.
 */
void prompt_waist_circumference(
    Gtk::Window& parent, const std::string& user_name, UnitSystem units,
    std::optional<libscale::units::Cm> current_value,
    const std::function<void(std::optional<libscale::units::Cm>)>& on_response);

/**
 * @brief Shows a modal Yes/No question dialog.
 * @param parent Window the dialog is transient for.
 * @param title Dialog title.
 * @param message Question text displayed as secondary text.
 * @param on_response Callback invoked with true when Yes was chosen, false otherwise.
 */
void confirm_action(Gtk::Window& parent, const std::string& title, const std::string& message,
                    const std::function<void(bool confirmed)>& on_response);

/**
 * @brief Builds the dialog offering to link an unmapped scale profile to a local profile.
 * The caller owns the returned dialog for as long as it is displayed.
 * @param parent Window the dialog is transient for.
 * @param scale_user The unmapped profile discovered on the scale.
 * @param linkable_users Local profiles that can still be linked (no mapping to this device).
 * @param on_response Callback invoked after the dialog closes. When accepted, user_id is the
 * selected local profile ID, or std::nullopt to create a new profile. When cancelled, accepted
 * is false and user_id is std::nullopt.
 * @return The configured dialog, not yet shown.
 */
auto create_scale_link_dialog(
    Gtk::Window& parent, const UserProfile& scale_user,
    const std::vector<UserProfile>& linkable_users,
    const std::function<void(bool accepted, std::optional<int> user_id)>& on_response)
    -> std::unique_ptr<Gtk::Dialog>;

/**
 * @brief Shows a save dialog that writes serialized measurement data to the chosen file.
 * The caller owns the dialog slot so it is created once and reused on subsequent invocations.
 * @param parent Window the dialog is transient for.
 * @param dialog_slot Slot the dialog is lazily created in and kept alive by.
 * @param title Dialog title.
 * @param filter_label Human-readable file filter label.
 * @param default_name Pre-filled file name.
 * @param extension File extension (without dot) enforced on the selected path.
 * @param serialize Produces the file content at save time.
 * @param on_saved Callback invoked with the written path on success.
 * @param on_error Callback invoked with a failure message when the file cannot be written.
 */
void export_measurements(Gtk::Window& parent, std::unique_ptr<Gtk::FileChooserDialog>& dialog_slot,
                         const std::string& title, const std::string& filter_label,
                         const std::string& default_name, const std::string& extension,
                         const std::function<std::string()>& serialize,
                         const std::function<void(const std::string& path)>& on_saved,
                         const std::function<void(const std::string& message)>& on_error);

}  // namespace comfort_select
