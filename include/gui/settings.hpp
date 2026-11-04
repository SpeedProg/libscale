#pragma once
#include <filesystem>
#include <string>

namespace comfort_select {

/// Default window width in pixels.
static constexpr int default_window_width = 1000;
/// Default window height in pixels.
static constexpr int default_window_height = 700;
/// Conversion factor from Kilograms to Pounds.
static constexpr double kg_to_lbs_factor = 2.20462;
/// Conversion factor from Centimeters to Inches.
static constexpr double cm_per_inch = 2.54;
/// Number of inches in a foot.
static constexpr int inches_per_foot = 12;

/**
 * @brief Available physical unit systems for display formatting.
 */
enum class UnitSystem : uint8_t {
    /// Metric units (Kg, Cm).
    Metric,
    /// Imperial units (Lbs, Ft/In).
    Imperial
};

/**
 * @brief Abstract settings manager interface managing application state persistence (unit system,
 * window sizes).
 */
class ISettingsManager {
   public:
    /**
     * @brief Virtual destructor.
     */
    virtual ~ISettingsManager() = default;

    /**
     * @brief Default constructor.
     */
    ISettingsManager() = default;
    ISettingsManager(const ISettingsManager&) = delete;
    auto operator=(const ISettingsManager&) -> ISettingsManager& = delete;
    ISettingsManager(ISettingsManager&&) = delete;
    auto operator=(ISettingsManager&&) -> ISettingsManager& = delete;

    /**
     * @brief Loads application configuration from a JSON/text file.
     * @param config_path Path to the configuration file.
     */
    virtual void load(const std::filesystem::path& config_path) = 0;

    /**
     * @brief Saves the current application settings state to the configuration path.
     */
    virtual void save() = 0;

    /**
     * @brief Retrieves the active physical unit system display preference.
     * @return Unit system enum.
     */
    [[nodiscard]] virtual auto get_unit_system() const -> UnitSystem = 0;

    /**
     * @brief Sets the physical unit system display preference.
     * @param units Metric or Imperial preference.
     */
    virtual void set_unit_system(UnitSystem units) = 0;

    /**
     * @brief Retrieves the ID of the last active user profile selected.
     * @return Database ID of the user.
     */
    [[nodiscard]] virtual auto get_last_user_id() const -> int = 0;

    /**
     * @brief Stores the ID of the last active user profile selected.
     * @param user_id Database ID of the user.
     */
    virtual void set_last_user_id(int user_id) = 0;

    /**
     * @brief Gets stored window width.
     * @return Stored width in pixels.
     */
    [[nodiscard]] virtual auto get_window_width() const -> int = 0;

    /**
     * @brief Gets stored window height.
     * @return Stored height in pixels.
     */
    [[nodiscard]] virtual auto get_window_height() const -> int = 0;

    /**
     * @brief Checks if the window was previously maximized.
     * @return True if maximized, false otherwise.
     */
    [[nodiscard]] virtual auto get_is_maximized() const -> bool = 0;

    /**
     * @brief Encapsulates GTK window dimension states.
     */
    struct WindowState {
        /// Stored width in pixels.
        int width;
        /// Stored height in pixels.
        int height;
        /// True if window was maximized.
        bool maximized;
    };

    /**
     * @brief Updates the stored GTK window dimensions state.
     * @param state Custom WindowState struct containing width, height, and maximized status.
     */
    virtual void set_window_state(WindowState state) = 0;
};

/**
 * @brief Concrete settings manager implementing local JSON file persistence.
 */
class JsonSettingsManager : public ISettingsManager {
   public:
    /**
     * @brief Default constructor.
     */
    JsonSettingsManager() = default;

    /**
     * @brief Destructor.
     */
    ~JsonSettingsManager() override = default;

    JsonSettingsManager(const JsonSettingsManager&) = delete;
    auto operator=(const JsonSettingsManager&) -> JsonSettingsManager& = delete;
    JsonSettingsManager(JsonSettingsManager&&) = delete;
    auto operator=(JsonSettingsManager&&) -> JsonSettingsManager& = delete;

    void load(const std::filesystem::path& config_path) override;
    void save() override;

    [[nodiscard]] auto get_unit_system() const -> UnitSystem override {
        return m_units;
    }
    void set_unit_system(UnitSystem units) override {
        m_units = units;
    }

    [[nodiscard]] auto get_last_user_id() const -> int override {
        return m_last_user_id;
    }
    void set_last_user_id(int user_id) override {
        m_last_user_id = user_id;
    }

    [[nodiscard]] auto get_window_width() const -> int override {
        return m_window_width;
    }
    [[nodiscard]] auto get_window_height() const -> int override {
        return m_window_height;
    }
    [[nodiscard]] auto get_is_maximized() const -> bool override {
        return m_is_maximized;
    }
    void set_window_state(WindowState state) override {
        m_window_width = state.width;
        m_window_height = state.height;
        m_is_maximized = state.maximized;
    }

   private:
    std::filesystem::path m_path;
    UnitSystem m_units = UnitSystem::Metric;
    int m_last_user_id = 0;
    int m_window_width = default_window_width;
    int m_window_height = default_window_height;
    bool m_is_maximized = false;
};

}  // namespace comfort_select
