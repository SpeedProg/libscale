#include "libscale/database_impl.hpp"

#include <sqlite3.h>
#include <sqlite_orm/sqlite_orm.h>

#include <chrono>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "libscale/db_serializers.hpp"  // NOLINT(misc-include-cleaner): METAPROGRAMMING: Registering sqlite_orm custom serializers via template specializations.
#include "libscale/types.hpp"

namespace libscale {

namespace {
auto make_sqlite_storage(const std::string& path) {
    return sqlite_orm::make_storage(
        path,
        sqlite_orm::make_table(
            "users",
            sqlite_orm::make_column("id", &::libscale::UserProfile::id,
                                    sqlite_orm::primary_key().autoincrement()),
            sqlite_orm::make_column("name", &::libscale::UserProfile::name),
            sqlite_orm::make_column("height_cm", &::libscale::UserProfile::height),
            sqlite_orm::make_column("gender", &::libscale::UserProfile::gender),
            sqlite_orm::make_column("activity_level", &::libscale::UserProfile::activity_level),
            sqlite_orm::make_column("birthday", &::libscale::UserProfile::birthday),
            sqlite_orm::make_column("waist_circumference_cm",
                                    &::libscale::UserProfile::waist_circumference)),
        sqlite_orm::make_table(
            "devices",
            sqlite_orm::make_column("id", &::libscale::Device::id,
                                    sqlite_orm::primary_key().autoincrement()),
            sqlite_orm::make_column("name", &::libscale::Device::name),
            sqlite_orm::make_column("device_key", &::libscale::Device::device_key),
            sqlite_orm::make_column("pathway", &::libscale::Device::pathway),
            sqlite_orm::unique(&::libscale::Device::device_key)),
        sqlite_orm::make_table(
            "device_user_mappings",
            sqlite_orm::make_column("id", &::libscale::DeviceUserMapping::id,
                                    sqlite_orm::primary_key().autoincrement()),
            sqlite_orm::make_column("user_id", &::libscale::DeviceUserMapping::user_id),
            sqlite_orm::make_column("device_id", &::libscale::DeviceUserMapping::device_id),
            sqlite_orm::make_column("slot_id", &::libscale::DeviceUserMapping::slot_id),
            sqlite_orm::make_column("device_user_id",
                                    &::libscale::DeviceUserMapping::device_user_id),
            sqlite_orm::foreign_key(&::libscale::DeviceUserMapping::user_id)
                .references(&::libscale::UserProfile::id),
            sqlite_orm::foreign_key(&::libscale::DeviceUserMapping::device_id)
                .references(&::libscale::Device::id),
            sqlite_orm::unique(&::libscale::DeviceUserMapping::device_id,
                               &::libscale::DeviceUserMapping::slot_id),
            sqlite_orm::unique(&::libscale::DeviceUserMapping::user_id,
                               &::libscale::DeviceUserMapping::device_id)),
        sqlite_orm::make_table(
            "measurements",
            sqlite_orm::make_column("id", &::libscale::Measurement::id,
                                    sqlite_orm::primary_key().autoincrement()),
            sqlite_orm::make_column("user_id", &::libscale::Measurement::user_id),
            sqlite_orm::make_column("device_id", &::libscale::Measurement::device_id),
            sqlite_orm::make_column("date", &::libscale::Measurement::date),
            sqlite_orm::make_column("weight_kg", &::libscale::Measurement::weight),
            sqlite_orm::make_column("body_fat_pct", &::libscale::Measurement::body_fat),
            sqlite_orm::make_column("water_pct", &::libscale::Measurement::water),
            sqlite_orm::make_column("muscle_mass_pct", &::libscale::Measurement::muscle_mass),
            sqlite_orm::make_column("bmi", &::libscale::Measurement::bmi),
            sqlite_orm::make_column("age_at_measurement", &::libscale::Measurement::age),
            sqlite_orm::make_column("energy_kcal", &::libscale::Measurement::energy),
            sqlite_orm::make_column("waist_circumference_cm",
                                    &::libscale::Measurement::waist_circumference),
            sqlite_orm::make_column("body_fat_v2_pct", &::libscale::Measurement::body_fat_v2),
            sqlite_orm::make_column("water_v2_pct", &::libscale::Measurement::water_v2),
            sqlite_orm::make_column("muscle_mass_v2_pct", &::libscale::Measurement::muscle_mass_v2),
            sqlite_orm::make_column("energy_v2_kcal", &::libscale::Measurement::energy_v2),
            sqlite_orm::make_column("ecw_v2_pct", &::libscale::Measurement::ecw_v2),
            sqlite_orm::make_column("rfm", &::libscale::Measurement::rfm),
            sqlite_orm::make_column("formula_version", &::libscale::Measurement::formula_version),
            sqlite_orm::foreign_key(&::libscale::Measurement::device_id)
                .references(&::libscale::Device::id),
            sqlite_orm::unique(&::libscale::Measurement::user_id, &::libscale::Measurement::date)),
        sqlite_orm::make_table(
            "scale_raw_data",
            sqlite_orm::make_column("id", &::libscale::ScaleRawData::id,
                                    sqlite_orm::primary_key().autoincrement()),
            sqlite_orm::make_column("measurement_id", &::libscale::ScaleRawData::measurement_id),
            sqlite_orm::make_column("weight_raw", &::libscale::ScaleRawData::weight_raw),
            sqlite_orm::make_column("imp5k", &::libscale::ScaleRawData::imp5k),
            sqlite_orm::make_column("imp50k", &::libscale::ScaleRawData::imp50k),
            sqlite_orm::foreign_key(&::libscale::ScaleRawData::measurement_id)
                .references(&::libscale::Measurement::id)));
}
}  // namespace

using Storage = decltype(make_sqlite_storage(""));

namespace {
auto get_storage(Storage* storage) -> Storage& {
    if (storage == nullptr) {
        throw std::runtime_error("Database not initialized. Call initialize() first.");
    }
    return *storage;
}
}  // namespace

struct SqliteDatabaseManager::Impl {
    std::unique_ptr<Storage> storage;
};

SqliteDatabaseManager::SqliteDatabaseManager() : m_impl(std::make_unique<Impl>()) {}
SqliteDatabaseManager::~SqliteDatabaseManager() = default;

auto SqliteDatabaseManager::initialize(const std::filesystem::path& db_path) -> bool {
    std::unique_lock const lock(m_mutex);

    // Ensure parent directory exists
    try {
        if (auto parent = db_path.parent_path(); !parent.empty()) {
            std::filesystem::create_directories(parent);
        }
    } catch (const std::exception& ex) {
        std::cerr << "Failed to create database parent directory: " << ex.what() << "\n";
        return false;
    }

    bool const existed = std::filesystem::exists(db_path);
    std::filesystem::path backup_path = db_path;
    backup_path += ".tmp_backup";

    if (existed) {
        try {
            std::filesystem::copy_file(db_path, backup_path,
                                       std::filesystem::copy_options::overwrite_existing);
        } catch (const std::exception& ex) {
            std::cerr << "Failed to create database backup: " << ex.what() << "\n";
            return false;
        }
    }

    try {
        auto new_storage = std::make_unique<Storage>(make_sqlite_storage(db_path.string()));
        new_storage->on_open = [](sqlite3* database) -> void {
            sqlite3_exec(database, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);
        };
        new_storage->sync_schema();

        // Ensure owner-only permissions (chmod 600)
        if (std::filesystem::exists(db_path)) {
            std::filesystem::permissions(
                db_path, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write);
        }

        m_impl->storage = std::move(new_storage);

        // Success: Clean up backup if it exists
        if (existed) {
            std::filesystem::remove(backup_path);
        }
    } catch (const std::exception& ex) {
        std::cerr << "Database schema synchronization failed: " << ex.what() << "\n";

        // Rollback: Close storage first to release locks
        m_impl->storage.reset();

        if (existed) {
            try {
                std::filesystem::copy_file(backup_path, db_path,
                                           std::filesystem::copy_options::overwrite_existing);
                std::filesystem::remove(backup_path);
            } catch (const std::exception& rollback_ex) {
                std::cerr << "Database rollback failed: " << rollback_ex.what() << "\n";
            }
        } else {
            std::error_code err_code;
            std::filesystem::remove(db_path, err_code);
        }
        return false;
    }
    return true;
}

auto SqliteDatabaseManager::get_users() const -> std::vector<UserProfile> {
    std::shared_lock const lock(m_mutex);
    return get_storage(m_impl->storage.get()).get_all<UserProfile>();
}

auto SqliteDatabaseManager::add_user(const UserProfile& user) -> int {
    std::unique_lock const lock(m_mutex);
    return get_storage(m_impl->storage.get()).insert(user);
}

void SqliteDatabaseManager::update_user(const UserProfile& user) {
    std::unique_lock const lock(m_mutex);
    get_storage(m_impl->storage.get()).update(user);
}

void SqliteDatabaseManager::delete_user(int user_id) {
    std::unique_lock const lock(m_mutex);
    using namespace sqlite_orm;
    auto& storage = get_storage(m_impl->storage.get());
    storage.transaction([&]() -> bool {
        auto measurements =
            storage.get_all<Measurement>(where(c(&Measurement::user_id) == user_id));
        for (const auto& meas : measurements) {
            storage.remove_all<ScaleRawData>(where(c(&ScaleRawData::measurement_id) == meas.id));
        }
        storage.remove_all<Measurement>(where(c(&Measurement::user_id) == user_id));
        storage.remove_all<DeviceUserMapping>(where(c(&DeviceUserMapping::user_id) == user_id));
        storage.remove<UserProfile>(user_id);
        return true;
    });
}

auto SqliteDatabaseManager::get_measurements(int user_id) const -> std::vector<Measurement> {
    std::shared_lock const lock(m_mutex);
    using namespace sqlite_orm;
    return get_storage(m_impl->storage.get())
        .get_all<Measurement>(where(c(&Measurement::user_id) == user_id),
                              order_by(&Measurement::date));
}

auto SqliteDatabaseManager::get_measurements_range(int user_id, std::chrono::year_month_day start,
                                                   std::chrono::year_month_day end) const
    -> std::vector<Measurement> {
    std::shared_lock const lock(m_mutex);
    using namespace sqlite_orm;
    return get_storage(m_impl->storage.get())
        .get_all<Measurement>(where(c(&Measurement::user_id) == user_id &&
                                    c(&Measurement::date) >= start && c(&Measurement::date) <= end),
                              order_by(&Measurement::date));
}

namespace {
auto compare_measurements(const Measurement& existing_meas, const Measurement& incoming_meas,
                          Storage& storage)
    -> bool {  // NOLINT(bugprone-easily-swappable-parameters)
    using namespace sqlite_orm;
    auto existing_raw_a =
        storage.get_all<ScaleRawData>(where(c(&ScaleRawData::measurement_id) == existing_meas.id));

    bool const metrics_identical =
        existing_meas.user_id == incoming_meas.user_id &&
        existing_meas.device_id == incoming_meas.device_id &&
        existing_meas.weight.value == incoming_meas.weight.value &&
        existing_meas.body_fat.value == incoming_meas.body_fat.value &&
        existing_meas.water.value == incoming_meas.water.value &&
        existing_meas.muscle_mass.value == incoming_meas.muscle_mass.value &&
        existing_meas.bmi == incoming_meas.bmi &&
        existing_meas.age.value == incoming_meas.age.value &&
        existing_meas.energy.value == incoming_meas.energy.value &&
        existing_meas.waist_circumference == incoming_meas.waist_circumference &&
        existing_meas.body_fat_v2.value == incoming_meas.body_fat_v2.value &&
        existing_meas.water_v2.value == incoming_meas.water_v2.value &&
        existing_meas.muscle_mass_v2.value == incoming_meas.muscle_mass_v2.value &&
        existing_meas.energy_v2.value == incoming_meas.energy_v2.value &&
        existing_meas.ecw_v2.value == incoming_meas.ecw_v2.value &&
        existing_meas.rfm == incoming_meas.rfm &&
        existing_meas.formula_version == incoming_meas.formula_version;

    bool raw_identical = false;
    if (!incoming_meas.scale_raw) {
        raw_identical = existing_raw_a.empty();
    } else if (!existing_raw_a.empty()) {
        const auto& ext_r = existing_raw_a.front();
        const auto& new_r = *incoming_meas.scale_raw;
        raw_identical = ext_r.weight_raw == new_r.weight_raw && ext_r.imp5k == new_r.imp5k &&
                        ext_r.imp50k == new_r.imp50k;
    }
    return metrics_identical && raw_identical;
}
}  // namespace

void SqliteDatabaseManager::add_measurement(const Measurement& measurement) {
    std::unique_lock const lock(m_mutex);
    using namespace sqlite_orm;
    auto& storage = get_storage(m_impl->storage.get());
    storage.transaction([&]() -> bool {
        int inserted_id = measurement.id;
        if (inserted_id == 0) {
            std::chrono::sys_seconds adjusted_date = measurement.date;
            bool found_slot = false;

            while (!found_slot) {
                auto existing = storage.get_all<Measurement>(
                    where(c(&Measurement::user_id) == measurement.user_id &&
                          c(&Measurement::date) == adjusted_date));
                if (existing.empty()) {
                    Measurement new_meas = measurement;
                    new_meas.date = adjusted_date;
                    inserted_id = storage.insert(new_meas);
                    found_slot = true;
                } else {
                    if (compare_measurements(existing.front(), measurement, storage)) {
                        return true;  // Skip redundant write
                    }
                    adjusted_date += std::chrono::seconds(1);
                }
            }
        } else {
            storage.replace(measurement);
        }

        if (measurement.scale_raw) {
            auto raw_data = *measurement.scale_raw;
            raw_data.measurement_id = inserted_id;
            storage.remove_all<ScaleRawData>(
                where(c(&ScaleRawData::measurement_id) == inserted_id));
            storage.insert(raw_data);
        }
        return true;
    });
}

void SqliteDatabaseManager::add_scale_raw_data(const ScaleRawData& raw_data) {
    std::unique_lock const lock(m_mutex);
    get_storage(m_impl->storage.get()).replace(raw_data);
}

auto SqliteDatabaseManager::get_scale_raw_data(int measurement_id) const
    -> std::optional<ScaleRawData> {
    std::shared_lock const lock(m_mutex);
    using namespace sqlite_orm;
    auto raw_list =
        get_storage(m_impl->storage.get())
            .get_all<ScaleRawData>(where(c(&ScaleRawData::measurement_id) == measurement_id));
    if (raw_list.empty()) {
        return std::nullopt;
    }
    return raw_list.front();
}

void SqliteDatabaseManager::delete_measurement(int measurement_id) {
    std::unique_lock const lock(m_mutex);
    using namespace sqlite_orm;
    auto& storage = get_storage(m_impl->storage.get());
    storage.transaction([&]() -> bool {
        storage.remove_all<ScaleRawData>(where(c(&ScaleRawData::measurement_id) == measurement_id));
        storage.remove<Measurement>(measurement_id);
        return true;
    });
}

auto SqliteDatabaseManager::get_devices() const -> std::vector<Device> {
    std::shared_lock const lock(m_mutex);
    return get_storage(m_impl->storage.get()).get_all<Device>();
}

auto SqliteDatabaseManager::add_device(const Device& device) -> int {
    std::unique_lock const lock(m_mutex);
    return get_storage(m_impl->storage.get()).insert(device);
}

void SqliteDatabaseManager::update_device(const Device& device) {
    std::unique_lock const lock(m_mutex);
    get_storage(m_impl->storage.get()).update(device);
}

auto SqliteDatabaseManager::get_device(int device_id) const -> std::optional<Device> {
    std::shared_lock const lock(m_mutex);
    auto dev_ptr = get_storage(m_impl->storage.get()).get_no_throw<Device>(device_id);
    if (dev_ptr) {
        return *dev_ptr;
    }
    return std::nullopt;
}

auto SqliteDatabaseManager::get_device_by_key(const std::string& device_key) const
    -> std::optional<Device> {
    std::shared_lock const lock(m_mutex);
    using namespace sqlite_orm;
    auto list = get_storage(m_impl->storage.get())
                    .get_all<Device>(where(c(&Device::device_key) == device_key));
    if (list.empty()) {
        return std::nullopt;
    }
    return list.front();
}

auto SqliteDatabaseManager::get_device_user_mappings(int user_id) const
    -> std::vector<DeviceUserMapping> {
    std::shared_lock const lock(m_mutex);
    using namespace sqlite_orm;
    return get_storage(m_impl->storage.get())
        .get_all<DeviceUserMapping>(where(c(&DeviceUserMapping::user_id) == user_id));
}

auto SqliteDatabaseManager::add_device_user_mapping(const DeviceUserMapping& mapping) -> int {
    std::unique_lock const lock(m_mutex);
    return get_storage(m_impl->storage.get()).insert(mapping);
}

auto SqliteDatabaseManager::get_device_user_mapping(int device_id,
                                                    const std::string& device_user_id) const
    -> std::optional<DeviceUserMapping> {
    std::shared_lock const lock(m_mutex);
    using namespace sqlite_orm;
    auto list = get_storage(m_impl->storage.get())
                    .get_all<DeviceUserMapping>(
                        where(c(&DeviceUserMapping::device_id) == device_id &&
                              c(&DeviceUserMapping::device_user_id) == device_user_id));
    if (list.empty()) {
        return std::nullopt;
    }
    return list.front();
}

}  // namespace libscale
