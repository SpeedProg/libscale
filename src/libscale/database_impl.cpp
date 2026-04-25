// NOLINTBEGIN(misc-include-cleaner)

#include "libscale/database_impl.hpp"

#include <sqlite_orm/sqlite_orm.h>

#include <chrono>
#include <filesystem>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <vector>

#include "libscale/types.hpp"
#include "libscale/db_serializers.hpp"

namespace libscale {

inline auto make_sqlite_storage(const std::string& path) {
    return sqlite_orm::make_storage(
        path,
        sqlite_orm::make_table(
            "users",
            sqlite_orm::make_column("id", &UserProfile::id,
                                    sqlite_orm::primary_key().autoincrement()),
            sqlite_orm::make_column("name", &UserProfile::name),
            sqlite_orm::make_column("height_cm", &UserProfile::height),
            sqlite_orm::make_column("gender", &UserProfile::gender),
            sqlite_orm::make_column("activity_level", &UserProfile::activity_level),
            sqlite_orm::make_column("birthday", &UserProfile::birthday),
            sqlite_orm::make_column("slot_id", &UserProfile::slot_id),
            sqlite_orm::make_column("device_user_id", &UserProfile::device_user_id)),
        sqlite_orm::make_table(
            "measurements",
            sqlite_orm::make_column("id", &Measurement::id,
                                    sqlite_orm::primary_key().autoincrement()),
            sqlite_orm::make_column("user_id", &Measurement::user_id),
            sqlite_orm::make_column("date", &Measurement::date),
            sqlite_orm::make_column("weight_kg", &Measurement::weight),
            sqlite_orm::make_column("body_fat_pct", &Measurement::body_fat),
            sqlite_orm::make_column("water_pct", &Measurement::water),
            sqlite_orm::make_column("muscle_mass_pct", &Measurement::muscle_mass),
            sqlite_orm::make_column("bmi", &Measurement::bmi),
            sqlite_orm::make_column("age_at_measurement", &Measurement::age),
            sqlite_orm::make_column("energy_kcal", &Measurement::energy),
            sqlite_orm::make_column("formula_version", &Measurement::formula_version),
            sqlite_orm::unique(&Measurement::user_id, &Measurement::date)),
        sqlite_orm::make_table(
            "scale_raw_data",
            sqlite_orm::make_column("id", &ScaleRawData::id,
                                    sqlite_orm::primary_key().autoincrement()),
            sqlite_orm::make_column("measurement_id", &ScaleRawData::measurement_id),
            sqlite_orm::make_column("weight_raw", &ScaleRawData::weight_raw),
            sqlite_orm::make_column("imp5k", &ScaleRawData::imp5k),
            sqlite_orm::make_column("imp50k", &ScaleRawData::imp50k),
            sqlite_orm::foreign_key(&ScaleRawData::measurement_id).references(&Measurement::id)));
}

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
    m_impl->storage = std::make_unique<Storage>(make_sqlite_storage(db_path.string()));
    m_impl->storage->sync_schema();
    return true;
}

auto SqliteDatabaseManager::get_users() const -> std::vector<UserProfile> {
    std::shared_lock const lock(m_mutex);
    return get_storage(m_impl->storage.get()).get_all<UserProfile>();
}

void SqliteDatabaseManager::add_user(const UserProfile& user) {
    std::unique_lock const lock(m_mutex);
    get_storage(m_impl->storage.get()).insert(user);
}

void SqliteDatabaseManager::update_user(const UserProfile& user) {
    std::unique_lock const lock(m_mutex);
    get_storage(m_impl->storage.get()).update(user);
}

void SqliteDatabaseManager::delete_user(int user_id) {
    std::unique_lock const lock(m_mutex);
    get_storage(m_impl->storage.get()).remove<UserProfile>(user_id);
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

void SqliteDatabaseManager::add_measurement(const Measurement& measurement) {
    std::unique_lock const lock(m_mutex);
    using namespace sqlite_orm;
    auto& storage = get_storage(m_impl->storage.get());
    storage.transaction([&]() {
        int inserted_id = measurement.id;
        if (inserted_id == 0) {
            inserted_id = storage.insert(measurement);
        } else {
            storage.replace(measurement);
        }
        if (measurement.scale_raw) {
            auto raw_data = *measurement.scale_raw;
            raw_data.measurement_id = inserted_id;
            storage.remove_all<ScaleRawData>(where(c(&ScaleRawData::measurement_id) == inserted_id));
            storage.insert(raw_data);
        }
        return true;
    });
}

void SqliteDatabaseManager::add_scale_raw_data(const ScaleRawData& raw_data) {
    std::unique_lock const lock(m_mutex);
    get_storage(m_impl->storage.get()).replace(raw_data);
}

auto SqliteDatabaseManager::get_scale_raw_data(int measurement_id) const -> std::optional<ScaleRawData> {
    std::shared_lock const lock(m_mutex);
    using namespace sqlite_orm;
    auto raw_list = get_storage(m_impl->storage.get())
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
    storage.transaction([&]() {
        storage.remove_all<ScaleRawData>(where(c(&ScaleRawData::measurement_id) == measurement_id));
        storage.remove<Measurement>(measurement_id);
        return true;
    });
}

}  // namespace libscale

// NOLINTEND(misc-include-cleaner)
