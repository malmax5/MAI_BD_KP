#pragma once

#include <string>
#include <Poco/JSON/Object.h>

namespace warehouse_backend::database::models
{

enum class CellStatus
{
    EMPTY,
    PARTIALLY_OCCUPIED,
    FULL,
    BLOCKED
};

enum class TemperatureZone
{
    NORMAL,
    COOL,
    FROZEN
};

class WarehouseCell
{
public:
    long long id;
    std::string cellCode;
    std::string zone;
    std::string rack;
    std::string shelf;
    std::string position;
    double maxVolume;
    double maxWeight;
    double currentOccupancy;
    CellStatus status;
    TemperatureZone temperatureZone;
    std::string lastInventoryDate;

    WarehouseCell();
    explicit WarehouseCell(const Poco::JSON::Object& json);

    Poco::JSON::Object toJson() const;
    static WarehouseCell fromJson(const Poco::JSON::Object& json);

    bool validate() const;
    
    bool canStore(double volume, double weight) const;
    double getAvailableVolume() const;
    double getAvailableWeight() const;
    void updateOccupancy(double volume, double weight);
    
    static std::string statusToString(CellStatus status);
    static CellStatus stringToStatus(const std::string& statusStr);
    
    static std::string temperatureZoneToString(TemperatureZone zone);
    static TemperatureZone stringToTemperatureZone(const std::string& zoneStr);
    
    static constexpr double MIN_VOLUME = 0.01;
    static constexpr double MIN_WEIGHT = 0.01;
    static constexpr double MAX_OCCUPANCY = 100.0;
};

} // namespace database::models