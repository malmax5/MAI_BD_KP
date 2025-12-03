#include "WarehouseCell.hpp"
#include "../../utils/JsonUtils.hpp"
#include "../../utils/Validator.hpp"
#include "../../utils/DateUtils.hpp"
#include <Poco/JSON/Parser.h>
#include <Poco/JSON/Stringifier.h>
#include <sstream>
#include <iomanip>
#include <cmath>

namespace warehouse_backend::database::models
{

using namespace warehouse_backend::utils;

WarehouseCell::WarehouseCell()
    : id(0),
      maxVolume(0.0),
      maxWeight(0.0),
      currentOccupancy(0.0),
      status(CellStatus::EMPTY),
      temperatureZone(TemperatureZone::NORMAL)
{
    
}

WarehouseCell::WarehouseCell(const Poco::JSON::Object& json)
{
    cellCode = JsonUtils::getString(json, "cell_code", "");
    zone = JsonUtils::getString(json, "zone", "");
    rack = JsonUtils::getString(json, "rack", "");
    shelf = JsonUtils::getString(json, "shelf", "");
    position = JsonUtils::getString(json, "position", "");
    maxVolume = JsonUtils::getDouble(json, "max_volume", 0.0);
    maxWeight = JsonUtils::getDouble(json, "max_weight", 0.0);
    currentOccupancy = JsonUtils::getDouble(json, "current_occupancy", 0.0);
    
    std::string statusStr = JsonUtils::getString(json, "status", "empty");
    status = stringToStatus(statusStr);
    
    std::string zoneStr = JsonUtils::getString(json, "temperature_zone", "normal");
    temperatureZone = stringToTemperatureZone(zoneStr);
    
    lastInventoryDate = JsonUtils::getString(json, "last_inventory_date", "");
    
    if (json.has("id"))
    {
        id = JsonUtils::getInt(json, "id", 0);
    }
}

Poco::JSON::Object WarehouseCell::toJson() const
{
    Poco::JSON::Object json;
    
    if (id > 0)
    {
        json.set("id", id);
    }
    
    json.set("cell_code", cellCode);
    json.set("zone", zone);
    json.set("rack", rack);
    json.set("shelf", shelf);
    json.set("position", position);
    json.set("max_volume", maxVolume);
    json.set("max_weight", maxWeight);
    json.set("current_occupancy", currentOccupancy);
    json.set("status", statusToString(status));
    json.set("temperature_zone", temperatureZoneToString(temperatureZone));
    
    if (!lastInventoryDate.empty())
    {
        json.set("last_inventory_date", lastInventoryDate);
    }
    
    // Вычисляемые поля
    json.set("available_volume", getAvailableVolume());
    json.set("available_weight", getAvailableWeight());
    json.set("is_available", status != CellStatus::BLOCKED && status != CellStatus::FULL);
    
    return json;
}

WarehouseCell WarehouseCell::fromJson(const Poco::JSON::Object& json)
{
    return WarehouseCell(json);
}

bool WarehouseCell::validate() const
{
    if (cellCode.empty() || cellCode.length() > 50)
    {
        return false;
    }
    
    if (maxVolume < MIN_VOLUME || maxWeight < MIN_WEIGHT)
    {
        return false;
    }
    
    if (currentOccupancy < 0 || currentOccupancy > MAX_OCCUPANCY)
    {
        return false;
    }
    
    if (!lastInventoryDate.empty() && !Validator::isValidDate(lastInventoryDate))
    {
        return false;
    }
    
    return true;
}

bool WarehouseCell::canStore(double volume, double weight) const
{
    if (status == CellStatus::BLOCKED || status == CellStatus::FULL)
    {
        return false;
    }
    
    double availableVolume = getAvailableVolume();
    double availableWeight = getAvailableWeight();
    
    return volume <= availableVolume && weight <= availableWeight;
}

double WarehouseCell::getAvailableVolume() const
{
    double occupiedVolume = (currentOccupancy / 100.0) * maxVolume;
    return maxVolume - occupiedVolume;
}

double WarehouseCell::getAvailableWeight() const
{
    double occupiedWeight = (currentOccupancy / 100.0) * maxWeight;
    return maxWeight - occupiedWeight;
}

void WarehouseCell::updateOccupancy(double volume, double weight)
{
    double volumePercentage = (volume / maxVolume) * 100.0;
    double weightPercentage = (weight / maxWeight) * 100.0;
    
    // Используем максимальное значение из двух метрик
    double newOccupancy = std::max(volumePercentage, weightPercentage);
    
    if (newOccupancy < 10.0)
    {
        status = CellStatus::EMPTY;
    }
    else if (newOccupancy >= 95.0)
    {
        status = CellStatus::FULL;
    }
    else
    {
        status = CellStatus::PARTIALLY_OCCUPIED;
    }
    
    currentOccupancy = newOccupancy;
}

std::string WarehouseCell::statusToString(CellStatus status)
{
    switch (status)
    {
        case CellStatus::EMPTY: return "empty";
        case CellStatus::PARTIALLY_OCCUPIED: return "partially_occupied";
        case CellStatus::FULL: return "full";
        case CellStatus::BLOCKED: return "blocked";
        default: return "empty";
    }
}

CellStatus WarehouseCell::stringToStatus(const std::string& statusStr)
{
    if (statusStr == "partially_occupied") return CellStatus::PARTIALLY_OCCUPIED;
    if (statusStr == "full") return CellStatus::FULL;
    if (statusStr == "blocked") return CellStatus::BLOCKED;
    return CellStatus::EMPTY;
}

std::string WarehouseCell::temperatureZoneToString(TemperatureZone zone)
{
    switch (zone)
    {
        case TemperatureZone::COOL: return "cool";
        case TemperatureZone::FROZEN: return "frozen";
        default: return "normal";
    }
}

TemperatureZone WarehouseCell::stringToTemperatureZone(const std::string& zoneStr)
{
    if (zoneStr == "cool") return TemperatureZone::COOL;
    if (zoneStr == "frozen") return TemperatureZone::FROZEN;
    return TemperatureZone::NORMAL;
}

} // namespace database::models
