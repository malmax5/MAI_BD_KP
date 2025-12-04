#include "WarehouseCellRepository.hpp"
#include "../models/WarehouseCell.hpp"
#include <Poco/Data/Session.h>
#include <Poco/Data/RecordSet.h>
#include <Poco/Data/Statement.h>
#include <Poco/Data/Row.h>
#include <Poco/DateTime.h>
#include <Poco/DateTimeFormat.h>
#include <Poco/DateTimeFormatter.h>
#include <Poco/DateTimeParser.h>
#include <Poco/Dynamic/Var.h>
#include "../../utils/DateUtils.hpp"
#include "../../utils/JsonUtils.hpp"

using namespace Poco::Data;
using namespace Poco::Data::Keywords;
using namespace Poco;

using namespace warehouse_backend::utils;

namespace warehouse_backend::database::repositories
{

const std::string WarehouseCellRepository::TABLE_NAME = "warehouse_cells";
const std::vector<std::string> WarehouseCellRepository::SEARCH_FIELDS = {
    "cell_code", "zone", "rack", "shelf", "position"
};

WarehouseCellRepository::WarehouseCellRepository() : BaseRepository<models::WarehouseCell>()
{

}

std::unique_ptr<models::WarehouseCell> WarehouseCellRepository::findById(long long id)
{
    auto connection = acquireConnection();
    
    try
    {
        Poco::Int64 pocoId = static_cast<Poco::Int64>(id);
        Statement select(connection->getSession());
        select << "SELECT id, cell_code, zone, rack, shelf, position, "
                  "max_volume, max_weight, current_occupancy, status, "
                  "temperature_zone, last_inventory_date "
                  "FROM " << TABLE_NAME << " WHERE id = $1",
            use(pocoId),
            now;
        
        RecordSet rs(select);
        
        if (rs.rowCount() > 0)
        {
            return std::make_unique<models::WarehouseCell>(mapRowToWarehouseCell(rs.row(0)));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findById: " + e.displayText());
    }
    
    return nullptr;
}

std::vector<std::unique_ptr<models::WarehouseCell>> WarehouseCellRepository::findAll()
{
    std::vector<std::unique_ptr<models::WarehouseCell>> warehouseCells;
    auto connection = acquireConnection();
    
    try
    {
        Statement select(connection->getSession());
        select << "SELECT id, cell_code, zone, rack, shelf, position, "
                  "max_volume, max_weight, current_occupancy, status, "
                  "temperature_zone, last_inventory_date "
                  "FROM " << TABLE_NAME << " ORDER BY zone, rack, shelf, position",
            now;
        
        RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            warehouseCells.push_back(std::make_unique<models::WarehouseCell>(mapRowToWarehouseCell(rs.row(i))));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findAll: " + e.displayText());
    }
    
    return warehouseCells;
}

std::vector<std::unique_ptr<models::WarehouseCell>> WarehouseCellRepository::findPaginated(int page, int pageSize)
{
    std::vector<std::unique_ptr<models::WarehouseCell>> warehouseCells;
    auto connection = acquireConnection();
    
    if (page < 1) page = 1;
    int offset = (page - 1) * pageSize;
    
    try
    {
        int usePageSize = pageSize;
        int useOffset = offset;
        Statement select(connection->getSession());
        select << "SELECT id, cell_code, zone, rack, shelf, position, "
                  "max_volume, max_weight, current_occupancy, status, "
                  "temperature_zone, last_inventory_date "
                  "FROM " << TABLE_NAME << " ORDER BY zone, rack, shelf, position LIMIT $1 OFFSET $2",
            use(usePageSize),
            use(useOffset),
            now;
        
        RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            warehouseCells.push_back(std::make_unique<models::WarehouseCell>(mapRowToWarehouseCell(rs.row(i))));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findPaginated: " + e.displayText());
    }
    
    return warehouseCells;
}

long long WarehouseCellRepository::create(const models::WarehouseCell& warehouseCell)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        std::string cellCodeCopy = warehouseCell.cellCode;
        std::string zoneCopy = warehouseCell.zone;
        std::string rackCopy = warehouseCell.rack;
        std::string shelfCopy = warehouseCell.shelf;
        std::string positionCopy = warehouseCell.position;
        double maxVolumeCopy = warehouseCell.maxVolume;
        double maxWeightCopy = warehouseCell.maxWeight;
        double currentOccupancyCopy = warehouseCell.currentOccupancy;
        std::string statusStr = models::WarehouseCell::statusToString(warehouseCell.status);
        std::string temperatureZoneStr = models::WarehouseCell::temperatureZoneToString(warehouseCell.temperatureZone);
        std::string lastInventoryDateCopy = warehouseCell.lastInventoryDate;
        
        Poco::Int64 newId = 0;
        Statement insert(connection->getSession());
        
        if (lastInventoryDateCopy.empty())
        {
            insert << "INSERT INTO " << TABLE_NAME << " "
                      "(cell_code, zone, rack, shelf, position, max_volume, max_weight, "
                      "current_occupancy, status, temperature_zone) "
                      "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9::cell_status, $10::temperature_zone) "
                      "RETURNING id",
                use(cellCodeCopy),
                use(zoneCopy),
                use(rackCopy),
                use(shelfCopy),
                use(positionCopy),
                use(maxVolumeCopy),
                use(maxWeightCopy),
                use(currentOccupancyCopy),
                use(statusStr),
                use(temperatureZoneStr),
                into(newId),
                now;
        }
        else
        {
            insert << "INSERT INTO " << TABLE_NAME << " "
                      "(cell_code, zone, rack, shelf, position, max_volume, max_weight, "
                      "current_occupancy, status, temperature_zone, last_inventory_date) "
                      "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9::cell_status, $10::temperature_zone, $11) "
                      "RETURNING id",
                use(cellCodeCopy),
                use(zoneCopy),
                use(rackCopy),
                use(shelfCopy),
                use(positionCopy),
                use(maxVolumeCopy),
                use(maxWeightCopy),
                use(currentOccupancyCopy),
                use(statusStr),
                use(temperatureZoneStr),
                use(lastInventoryDateCopy),
                into(newId),
                now;
        }
        
        commitTransaction(*connection);
        return static_cast<long long>(newId);
    }
    catch (const Poco::Exception& e)
    {
        rollbackTransaction(*connection);
        throw std::runtime_error("Database error in create: " + e.displayText());
    }
}

bool WarehouseCellRepository::update(long long id, const models::WarehouseCell& warehouseCell)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        std::string cellCodeCopy = warehouseCell.cellCode;
        std::string zoneCopy = warehouseCell.zone;
        std::string rackCopy = warehouseCell.rack;
        std::string shelfCopy = warehouseCell.shelf;
        std::string positionCopy = warehouseCell.position;
        double maxVolumeCopy = warehouseCell.maxVolume;
        double maxWeightCopy = warehouseCell.maxWeight;
        double currentOccupancyCopy = warehouseCell.currentOccupancy;
        std::string statusStr = models::WarehouseCell::statusToString(warehouseCell.status);
        std::string temperatureZoneStr = models::WarehouseCell::temperatureZoneToString(warehouseCell.temperatureZone);
        std::string lastInventoryDateCopy = warehouseCell.lastInventoryDate;
        long long idCopy = id;
        
        Statement update(connection->getSession());
        
        if (lastInventoryDateCopy.empty())
        {
            update << "UPDATE " << TABLE_NAME << " SET "
                      "cell_code = $1, zone = $2, rack = $3, shelf = $4, position = $5, "
                      "max_volume = $6, max_weight = $7, current_occupancy = $8, "
                      "status = $9::cell_status, temperature_zone = $10::temperature_zone "
                      "WHERE id = $11",
                use(cellCodeCopy),
                use(zoneCopy),
                use(rackCopy),
                use(shelfCopy),
                use(positionCopy),
                use(maxVolumeCopy),
                use(maxWeightCopy),
                use(currentOccupancyCopy),
                use(statusStr),
                use(temperatureZoneStr),
                use(idCopy),
                now;
        }
        else
        {
            update << "UPDATE " << TABLE_NAME << " SET "
                      "cell_code = $1, zone = $2, rack = $3, shelf = $4, position = $5, "
                      "max_volume = $6, max_weight = $7, current_occupancy = $8, "
                      "status = $9::cell_status, temperature_zone = $10::temperature_zone, "
                      "last_inventory_date = $11 WHERE id = $12",
                use(cellCodeCopy),
                use(zoneCopy),
                use(rackCopy),
                use(shelfCopy),
                use(positionCopy),
                use(maxVolumeCopy),
                use(maxWeightCopy),
                use(currentOccupancyCopy),
                use(statusStr),
                use(temperatureZoneStr),
                use(lastInventoryDateCopy),
                use(idCopy),
                now;
        }
        
        int rowsAffected = update.execute();
        
        commitTransaction(*connection);
        return rowsAffected > 0;
    }
    catch (const Poco::Exception& e)
    {
        rollbackTransaction(*connection);
        throw std::runtime_error("Database error in update: " + e.displayText());
    }
}

bool WarehouseCellRepository::remove(long long id)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        long long idCopy = id;
        
        Statement checkBatches(connection->getSession());
        checkBatches << "SELECT COUNT(*) FROM product_batches WHERE storage_cell_id = $1",
            use(idCopy),
            now;
        
        RecordSet rs(checkBatches);
        int batchCount = 0;
        if (rs.rowCount() > 0)
        {
            batchCount = rs.value(0, 0).convert<int>();
        }
        
        if (batchCount > 0)
        {
            throw std::runtime_error("Cannot delete warehouse cell with associated product batches");
        }
        
        Statement del(connection->getSession());
        del << "DELETE FROM " << TABLE_NAME << " WHERE id = $1",
            use(idCopy);
        
        int rowsAffected = del.execute();
        
        commitTransaction(*connection);
        return rowsAffected > 0;
    }
    catch (const Poco::Exception& e)
    {
        rollbackTransaction(*connection);
        throw std::runtime_error("Database error in remove: " + e.displayText());
    }
}

bool WarehouseCellRepository::softDelete(long long id)
{
    return updateStatus(id, "blocked");
}

int WarehouseCellRepository::count()
{
    auto connection = acquireConnection();
    
    try
    {
        Statement countStmt(connection->getSession());
        countStmt << "SELECT COUNT(*) FROM " << TABLE_NAME,
            now;
        
        RecordSet rs(countStmt);
        if (rs.rowCount() > 0)
        {
            return rs.value(0, 0).convert<int>();
        }
        
        return 0;
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in count: " + e.displayText());
    }
}

Poco::JSON::Array WarehouseCellRepository::findAllAsJson()
{
    auto warehouseCells = findAll();
    Poco::JSON::Array jsonArray;
    
    for (const auto& cell : warehouseCells)
    {
        if (cell)
        {
            jsonArray.add(cell->toJson());
        }
    }
    
    return jsonArray;
}

Poco::JSON::Object WarehouseCellRepository::findByIdAsJson(long long id)
{
    auto cell = findById(id);
    if (cell)
    {
        return cell->toJson();
    }
    
    return Poco::JSON::Object();
}

std::vector<std::unique_ptr<models::WarehouseCell>> WarehouseCellRepository::findByField(
    const std::string& fieldName, const std::string& fieldValue)
{
    std::vector<std::unique_ptr<models::WarehouseCell>> warehouseCells;
    auto connection = acquireConnection();
    
    try
    {
        std::string sql = "SELECT id, cell_code, zone, rack, shelf, position, "
                          "max_volume, max_weight, current_occupancy, status, "
                          "temperature_zone, last_inventory_date "
                          "FROM " + TABLE_NAME + " WHERE " + fieldName + " = $1 "
                          "ORDER BY zone, rack, shelf, position";
        
        std::string fieldValueCopy = fieldValue;
        Statement select(connection->getSession());
        select << sql,
            use(fieldValueCopy),
            now;
        
        RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            warehouseCells.push_back(std::make_unique<models::WarehouseCell>(mapRowToWarehouseCell(rs.row(i))));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findByField: " + e.displayText());
    }
    
    return warehouseCells;
}

std::vector<std::unique_ptr<models::WarehouseCell>> WarehouseCellRepository::search(
    const std::string& query, const std::vector<std::string>& fields)
{
    std::vector<std::unique_ptr<models::WarehouseCell>> warehouseCells;
    auto connection = acquireConnection();
    
    try
    {
        std::string searchClause = buildSearchQuery(query, fields);
        std::string sql = "SELECT id, cell_code, zone, rack, shelf, position, "
                          "max_volume, max_weight, current_occupancy, status, "
                          "temperature_zone, last_inventory_date "
                          "FROM " + TABLE_NAME + " WHERE " + searchClause + " "
                          "ORDER BY zone, rack, shelf, position";
        
        Statement select(connection->getSession());
        select << sql,
            now;
        
        RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            warehouseCells.push_back(std::make_unique<models::WarehouseCell>(mapRowToWarehouseCell(rs.row(i))));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in search: " + e.displayText());
    }
    
    return warehouseCells;
}

std::unique_ptr<models::WarehouseCell> WarehouseCellRepository::findByCellCode(const std::string& cellCode)
{
    auto connection = acquireConnection();
    
    try
    {
        std::string cellCodeCopy = cellCode;
        Statement select(connection->getSession());
        select << "SELECT id, cell_code, zone, rack, shelf, position, "
                  "max_volume, max_weight, current_occupancy, status, "
                  "temperature_zone, last_inventory_date "
                  "FROM " << TABLE_NAME << " WHERE cell_code = $1",
            use(cellCodeCopy),
            now;
        
        RecordSet rs(select);
        
        if (rs.rowCount() > 0)
        {
            return std::make_unique<models::WarehouseCell>(mapRowToWarehouseCell(rs.row(0)));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findByCellCode: " + e.displayText());
    }
    
    return nullptr;
}

std::vector<std::unique_ptr<models::WarehouseCell>> WarehouseCellRepository::findByZone(const std::string& zone)
{
    auto connection = acquireConnection();
    std::vector<std::unique_ptr<models::WarehouseCell>> warehouseCells;
    
    try
    {
        std::string zoneCopy = zone;
        Statement select(connection->getSession());
        select << "SELECT id, cell_code, zone, rack, shelf, position, "
                  "max_volume, max_weight, current_occupancy, status, "
                  "temperature_zone, last_inventory_date "
                  "FROM " << TABLE_NAME << " WHERE zone = $1 "
                  "ORDER BY rack, shelf, position",
            use(zoneCopy),
            now;
        
        RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            warehouseCells.push_back(std::make_unique<models::WarehouseCell>(mapRowToWarehouseCell(rs.row(i))));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findByZone: " + e.displayText());
    }
    
    return warehouseCells;
}

std::vector<std::unique_ptr<models::WarehouseCell>> WarehouseCellRepository::findByStatus(const std::string& status)
{
    auto connection = acquireConnection();
    std::vector<std::unique_ptr<models::WarehouseCell>> warehouseCells;
    
    try
    {
        std::string statusCopy = status;
        Statement select(connection->getSession());
        select << "SELECT id, cell_code, zone, rack, shelf, position, "
                  "max_volume, max_weight, current_occupancy, status, "
                  "temperature_zone, last_inventory_date "
                  "FROM " << TABLE_NAME << " WHERE status = $1::cell_status "
                  "ORDER BY zone, rack, shelf, position",
            use(statusCopy),
            now;
        
        RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            warehouseCells.push_back(std::make_unique<models::WarehouseCell>(mapRowToWarehouseCell(rs.row(i))));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findByStatus: " + e.displayText());
    }
    
    return warehouseCells;
}

std::vector<std::unique_ptr<models::WarehouseCell>> WarehouseCellRepository::findByTemperatureZone(const std::string& temperatureZone)
{
    auto connection = acquireConnection();
    std::vector<std::unique_ptr<models::WarehouseCell>> warehouseCells;
    
    try
    {
        std::string temperatureZoneCopy = temperatureZone;
        Statement select(connection->getSession());
        select << "SELECT id, cell_code, zone, rack, shelf, position, "
                  "max_volume, max_weight, current_occupancy, status, "
                  "temperature_zone, last_inventory_date "
                  "FROM " << TABLE_NAME << " WHERE temperature_zone = $1::temperature_zone "
                  "ORDER BY zone, rack, shelf, position",
            use(temperatureZoneCopy),
            now;
        
        RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            warehouseCells.push_back(std::make_unique<models::WarehouseCell>(mapRowToWarehouseCell(rs.row(i))));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findByTemperatureZone: " + e.displayText());
    }
    
    return warehouseCells;
}

std::vector<std::unique_ptr<models::WarehouseCell>> WarehouseCellRepository::findAvailableCells(double requiredVolume, double requiredWeight)
{
    auto connection = acquireConnection();
    std::vector<std::unique_ptr<models::WarehouseCell>> warehouseCells;
    
    try
    {
        double requiredVolumeCopy = requiredVolume;
        double requiredWeightCopy = requiredWeight;
        
        Statement select(connection->getSession());
        select << "SELECT id, cell_code, zone, rack, shelf, position, "
                  "max_volume, max_weight, current_occupancy, status, "
                  "temperature_zone, last_inventory_date "
                  "FROM " << TABLE_NAME << " "
                  "WHERE status != 'blocked'::cell_status AND status != 'full'::cell_status "
                  "AND (max_volume - (max_volume * current_occupancy / 100.0)) >= $1 "
                  "AND (max_weight - (max_weight * current_occupancy / 100.0)) >= $2 "
                  "ORDER BY current_occupancy ASC",
            use(requiredVolumeCopy),
            use(requiredWeightCopy),
            now;
        
        RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            warehouseCells.push_back(std::make_unique<models::WarehouseCell>(mapRowToWarehouseCell(rs.row(i))));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findAvailableCells: " + e.displayText());
    }
    
    return warehouseCells;
}

std::vector<std::unique_ptr<models::WarehouseCell>> WarehouseCellRepository::findFullCells()
{
    return findByStatus("full");
}

std::vector<std::unique_ptr<models::WarehouseCell>> WarehouseCellRepository::findEmptyCells()
{
    return findByStatus("empty");
}

std::vector<std::unique_ptr<models::WarehouseCell>> WarehouseCellRepository::findBlockedCells()
{
    return findByStatus("blocked");
}

bool WarehouseCellRepository::updateStatus(long long id, const std::string& status)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        std::string statusCopy = status;
        long long idCopy = id;
        
        Statement update(connection->getSession());
        update << "UPDATE " << TABLE_NAME << " SET status = $1::cell_status WHERE id = $2",
            use(statusCopy),
            use(idCopy),
            now;
        
        int rowsAffected = update.execute();
        
        commitTransaction(*connection);
        return rowsAffected > 0;
    }
    catch (const Poco::Exception& e)
    {
        rollbackTransaction(*connection);
        throw std::runtime_error("Database error in updateStatus: " + e.displayText());
    }
}

bool WarehouseCellRepository::updateOccupancy(long long id, double newOccupancy)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        double newOccupancyCopy = newOccupancy;
        long long idCopy = id;
        
        std::string newStatus = "partially_occupied";
        if (newOccupancy >= 95.0)
        {
            newStatus = "full";
        }
        else if (newOccupancy < 20.0)
        {
            newStatus = "empty";
        }
        
        Statement update(connection->getSession());
        update << "UPDATE " << TABLE_NAME << " SET current_occupancy = $1, status = $2::cell_status WHERE id = $3",
            use(newOccupancyCopy),
            use(newStatus),
            use(idCopy),
            now;
        
        int rowsAffected = update.execute();
        
        commitTransaction(*connection);
        return rowsAffected > 0;
    }
    catch (const Poco::Exception& e)
    {
        rollbackTransaction(*connection);
        throw std::runtime_error("Database error in updateOccupancy: " + e.displayText());
    }
}

bool WarehouseCellRepository::updateTemperatureZone(long long id, const std::string& temperatureZone)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        std::string temperatureZoneCopy = temperatureZone;
        long long idCopy = id;
        
        Statement update(connection->getSession());
        update << "UPDATE " << TABLE_NAME << " SET temperature_zone = $1::temperature_zone WHERE id = $2",
            use(temperatureZoneCopy),
            use(idCopy),
            now;
        
        int rowsAffected = update.execute();
        
        commitTransaction(*connection);
        return rowsAffected > 0;
    }
    catch (const Poco::Exception& e)
    {
        rollbackTransaction(*connection);
        throw std::runtime_error("Database error in updateTemperatureZone: " + e.displayText());
    }
}

bool WarehouseCellRepository::updateMaxCapacity(long long id, double maxVolume, double maxWeight)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        double maxVolumeCopy = maxVolume;
        double maxWeightCopy = maxWeight;
        long long idCopy = id;
        
        Statement update(connection->getSession());
        update << "UPDATE " << TABLE_NAME << " SET max_volume = $1, max_weight = $2 WHERE id = $3",
            use(maxVolumeCopy),
            use(maxWeightCopy),
            use(idCopy),
            now;
        
        int rowsAffected = update.execute();
        
        commitTransaction(*connection);
        return rowsAffected > 0;
    }
    catch (const Poco::Exception& e)
    {
        rollbackTransaction(*connection);
        throw std::runtime_error("Database error in updateMaxCapacity: " + e.displayText());
    }
}

bool WarehouseCellRepository::updateLastInventoryDate(long long id, const std::string& date)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        std::string dateCopy = date;
        long long idCopy = id;
        
        Statement update(connection->getSession());
        update << "UPDATE " << TABLE_NAME << " SET last_inventory_date = $1 WHERE id = $2",
            use(dateCopy),
            use(idCopy),
            now;
        
        int rowsAffected = update.execute();
        
        commitTransaction(*connection);
        return rowsAffected > 0;
    }
    catch (const Poco::Exception& e)
    {
        rollbackTransaction(*connection);
        throw std::runtime_error("Database error in updateLastInventoryDate: " + e.displayText());
    }
}

bool WarehouseCellRepository::blockCell(long long id)
{
    return updateStatus(id, "blocked");
}

bool WarehouseCellRepository::unblockCell(long long id)
{
    auto cell = findById(id);
    if (!cell)
    {
        return false;
    }
    
    std::string newStatus = "empty";
    if (cell->currentOccupancy >= 95.0)
    {
        newStatus = "full";
    }
    else if (cell->currentOccupancy >= 20.0)
    {
        newStatus = "partially_occupied";
    }
    
    return updateStatus(id, newStatus);
}

int WarehouseCellRepository::countByStatus(const std::string& status)
{
    auto connection = acquireConnection();
    
    try
    {
        std::string statusCopy = status;
        Statement countStmt(connection->getSession());
        countStmt << "SELECT COUNT(*) FROM " << TABLE_NAME << " WHERE status = $1::cell_status",
            use(statusCopy),
            now;
        
        RecordSet rs(countStmt);
        if (rs.rowCount() > 0)
        {
            return rs.value(0, 0).convert<int>();
        }
        
        return 0;
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in countByStatus: " + e.displayText());
    }
}

int WarehouseCellRepository::countByTemperatureZone(const std::string& temperatureZone)
{
    auto connection = acquireConnection();
    
    try
    {
        std::string temperatureZoneCopy = temperatureZone;
        Statement countStmt(connection->getSession());
        countStmt << "SELECT COUNT(*) FROM " << TABLE_NAME << " WHERE temperature_zone = $1::temperature_zone",
            use(temperatureZoneCopy),
            now;
        
        RecordSet rs(countStmt);
        if (rs.rowCount() > 0)
        {
            return rs.value(0, 0).convert<int>();
        }
        
        return 0;
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in countByTemperatureZone: " + e.displayText());
    }
}

int WarehouseCellRepository::countEmptyCells()
{
    return countByStatus("empty");
}

int WarehouseCellRepository::countFullCells()
{
    return countByStatus("full");
}

int WarehouseCellRepository::countBlockedCells()
{
    return countByStatus("blocked");
}

double WarehouseCellRepository::getTotalMaxVolume()
{
    auto connection = acquireConnection();
    
    try
    {
        Statement select(connection->getSession());
        select << "SELECT COALESCE(SUM(max_volume), 0) FROM " << TABLE_NAME,
            now;
        
        RecordSet rs(select);
        if (rs.rowCount() > 0)
        {
            return rs.value(0, 0).convert<double>();
        }
        
        return 0.0;
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in getTotalMaxVolume: " + e.displayText());
    }
}

double WarehouseCellRepository::getTotalMaxWeight()
{
    auto connection = acquireConnection();
    
    try
    {
        Statement select(connection->getSession());
        select << "SELECT COALESCE(SUM(max_weight), 0) FROM " << TABLE_NAME,
            now;
        
        RecordSet rs(select);
        if (rs.rowCount() > 0)
        {
            return rs.value(0, 0).convert<double>();
        }
        
        return 0.0;
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in getTotalMaxWeight: " + e.displayText());
    }
}

double WarehouseCellRepository::getTotalUsedVolume()
{
    auto connection = acquireConnection();
    
    try
    {
        Statement select(connection->getSession());
        select << "SELECT COALESCE(SUM(max_volume * current_occupancy / 100.0), 0) FROM " << TABLE_NAME,
            now;
        
        RecordSet rs(select);
        if (rs.rowCount() > 0)
        {
            return rs.value(0, 0).convert<double>();
        }
        
        return 0.0;
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in getTotalUsedVolume: " + e.displayText());
    }
}

double WarehouseCellRepository::getTotalUsedWeight()
{
    auto connection = acquireConnection();
    
    try
    {
        Statement select(connection->getSession());
        select << "SELECT COALESCE(SUM(max_weight * current_occupancy / 100.0), 0) FROM " << TABLE_NAME,
            now;
        
        RecordSet rs(select);
        if (rs.rowCount() > 0)
        {
            return rs.value(0, 0).convert<double>();
        }
        
        return 0.0;
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in getTotalUsedWeight: " + e.displayText());
    }
}

double WarehouseCellRepository::getAverageOccupancy()
{
    auto connection = acquireConnection();
    
    try
    {
        Statement select(connection->getSession());
        select << "SELECT COALESCE(AVG(current_occupancy), 0) FROM " << TABLE_NAME,
            now;
        
        RecordSet rs(select);
        if (rs.rowCount() > 0)
        {
            return rs.value(0, 0).convert<double>();
        }
        
        return 0.0;
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in getAverageOccupancy: " + e.displayText());
    }
}

Poco::JSON::Array WarehouseCellRepository::getCellStatistics()
{
    auto connection = acquireConnection();
    Poco::JSON::Array result;
    
    try
    {
        Statement select(connection->getSession());
        select << "SELECT zone, "
                  "COUNT(*) as total_cells, "
                  "SUM(max_volume) as total_max_volume, "
                  "SUM(max_weight) as total_max_weight, "
                  "AVG(current_occupancy)::DECIMAL(5,2) as avg_occupancy, "
                  "COUNT(CASE WHEN status = 'empty' THEN 1 END) as empty_cells, "
                  "COUNT(CASE WHEN status = 'partially_occupied' THEN 1 END) as partial_cells, "
                  "COUNT(CASE WHEN status = 'full' THEN 1 END) as full_cells, "
                  "COUNT(CASE WHEN status = 'blocked' THEN 1 END) as blocked_cells "
                  "FROM " << TABLE_NAME << " "
                  "GROUP BY zone "
                  "ORDER BY zone",
            now;
        
        RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            Poco::JSON::Object stat;
            stat.set("zone", rs.value("zone").convert<std::string>());
            stat.set("total_cells", rs.value("total_cells").convert<int>());
            stat.set("total_max_volume", rs.value("total_max_volume").convert<double>());
            stat.set("total_max_weight", rs.value("total_max_weight").convert<double>());
            stat.set("avg_occupancy", rs.value("avg_occupancy").convert<double>());
            stat.set("empty_cells", rs.value("empty_cells").convert<int>());
            stat.set("partial_cells", rs.value("partial_cells").convert<int>());
            stat.set("full_cells", rs.value("full_cells").convert<int>());
            stat.set("blocked_cells", rs.value("blocked_cells").convert<int>());
            
            result.add(stat);
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in getCellStatistics: " + e.displayText());
    }
    
    return result;
}

Poco::JSON::Array WarehouseCellRepository::getZoneStatistics()
{
    auto connection = acquireConnection();
    Poco::JSON::Array result;
    
    try
    {
        Statement select(connection->getSession());
        select << "SELECT zone, temperature_zone, "
                  "COUNT(*) as cell_count, "
                  "SUM(max_volume) as total_volume, "
                  "SUM(max_weight) as total_weight, "
                  "AVG(current_occupancy)::DECIMAL(5,2) as avg_occupancy "
                  "FROM " << TABLE_NAME << " "
                  "GROUP BY zone, temperature_zone "
                  "ORDER BY zone, temperature_zone",
            now;
        
        RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            Poco::JSON::Object stat;
            stat.set("zone", rs.value("zone").convert<std::string>());
            stat.set("temperature_zone", rs.value("temperature_zone").convert<std::string>());
            stat.set("cell_count", rs.value("cell_count").convert<int>());
            stat.set("total_volume", rs.value("total_volume").convert<double>());
            stat.set("total_weight", rs.value("total_weight").convert<double>());
            stat.set("avg_occupancy", rs.value("avg_occupancy").convert<double>());
            
            result.add(stat);
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in getZoneStatistics: " + e.displayText());
    }
    
    return result;
}

Poco::JSON::Array WarehouseCellRepository::getOccupancyReport()
{
    auto connection = acquireConnection();
    Poco::JSON::Array result;
    
    try
    {
        Statement select(connection->getSession());
        select << "SELECT "
                  "CASE "
                  "  WHEN current_occupancy = 0 THEN 'Empty' "
                  "  WHEN current_occupancy < 50 THEN 'Low' "
                  "  WHEN current_occupancy < 80 THEN 'Medium' "
                  "  WHEN current_occupancy < 95 THEN 'High' "
                  "  ELSE 'Full' "
                  "END as occupancy_level, "
                  "COUNT(*) as cell_count, "
                  "SUM(max_volume) as total_volume, "
                  "SUM(max_weight) as total_weight, "
                  "AVG(current_occupancy)::DECIMAL(5,2) as avg_occupancy "
                  "FROM " << TABLE_NAME << " "
                  "WHERE status != 'blocked' "
                  "GROUP BY occupancy_level "
                  "ORDER BY occupancy_level",
            now;
        
        RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            Poco::JSON::Object stat;
            stat.set("occupancy_level", rs.value("occupancy_level").convert<std::string>());
            stat.set("cell_count", rs.value("cell_count").convert<int>());
            stat.set("total_volume", rs.value("total_volume").convert<double>());
            stat.set("total_weight", rs.value("total_weight").convert<double>());
            stat.set("avg_occupancy", rs.value("avg_occupancy").convert<double>());
            
            result.add(stat);
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in getOccupancyReport: " + e.displayText());
    }
    
    return result;
}

Poco::JSON::Array WarehouseCellRepository::getTemperatureZoneReport()
{
    auto connection = acquireConnection();
    Poco::JSON::Array result;
    
    try
    {
        Statement select(connection->getSession());
        select << "SELECT temperature_zone, "
                  "COUNT(*) as cell_count, "
                  "SUM(max_volume) as total_volume, "
                  "SUM(max_weight) as total_weight, "
                  "AVG(current_occupancy)::DECIMAL(5,2) as avg_occupancy, "
                  "COUNT(CASE WHEN status = 'empty' THEN 1 END) as empty_cells, "
                  "COUNT(CASE WHEN status = 'partially_occupied' THEN 1 END) as partial_cells, "
                  "COUNT(CASE WHEN status = 'full' THEN 1 END) as full_cells "
                  "FROM " << TABLE_NAME << " "
                  "WHERE status != 'blocked' "
                  "GROUP BY temperature_zone "
                  "ORDER BY temperature_zone",
            now;
        
        RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            Poco::JSON::Object stat;
            stat.set("temperature_zone", rs.value("temperature_zone").convert<std::string>());
            stat.set("cell_count", rs.value("cell_count").convert<int>());
            stat.set("total_volume", rs.value("total_volume").convert<double>());
            stat.set("total_weight", rs.value("total_weight").convert<double>());
            stat.set("avg_occupancy", rs.value("avg_occupancy").convert<double>());
            stat.set("empty_cells", rs.value("empty_cells").convert<int>());
            stat.set("partial_cells", rs.value("partial_cells").convert<int>());
            stat.set("full_cells", rs.value("full_cells").convert<int>());
            
            result.add(stat);
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in getTemperatureZoneReport: " + e.displayText());
    }
    
    return result;
}

std::vector<std::pair<long long, std::string>> WarehouseCellRepository::getCellCodes()
{
    auto connection = acquireConnection();
    std::vector<std::pair<long long, std::string>> result;
    
    try
    {
        Statement select(connection->getSession());
        select << "SELECT id, cell_code FROM " << TABLE_NAME << " ORDER BY cell_code",
            now;
        
        RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            result.emplace_back(
                rs.value("id", 0).convert<long long>(),
                rs.value("cell_code").convert<std::string>()
            );
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in getCellCodes: " + e.displayText());
    }
    
    return result;
}

models::WarehouseCell WarehouseCellRepository::findBestCellForStorage(double volume, double weight, const std::string& temperatureZone)
{
    auto connection = acquireConnection();
    
    try
    {
        double volumeCopy = volume;
        double weightCopy = weight;
        
        std::string sql = "SELECT id, cell_code, zone, rack, shelf, position, "
                          "max_volume, max_weight, current_occupancy, status, "
                          "temperature_zone, last_inventory_date "
                          "FROM " + TABLE_NAME + " "
                          "WHERE status != 'blocked'::cell_status AND status != 'full'::cell_status "
                          "AND (max_volume - (max_volume * current_occupancy / 100.0)) >= $1 "
                          "AND (max_weight - (max_weight * current_occupancy / 100.0)) >= $2 ";
        
        if (!temperatureZone.empty())
        {
            sql += "AND temperature_zone = $3::temperature_zone ";
        }
        
        sql += "ORDER BY current_occupancy ASC, max_volume DESC, max_weight DESC LIMIT 1";
        
        Statement select(connection->getSession());
        
        if (!temperatureZone.empty())
        {
            std::string temperatureZoneCopy = temperatureZone;
            select << sql,
                use(volumeCopy),
                use(weightCopy),
                use(temperatureZoneCopy),
                now;
        }
        else
        {
            select << sql,
                use(volumeCopy),
                use(weightCopy),
                now;
        }
        
        RecordSet rs(select);
        
        if (rs.rowCount() > 0)
        {
            return mapRowToWarehouseCell(rs.row(0));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findBestCellForStorage: " + e.displayText());
    }
    
    throw std::runtime_error("No suitable warehouse cell found");
}

std::vector<models::WarehouseCell> WarehouseCellRepository::findCellsForBatch(const models::ProductBatch& batch)
{
    auto connection = acquireConnection();
    std::vector<models::WarehouseCell> result;
    
    try
    {
        // Здесь можно добавить логику для поиска ячеек с учетом специфики партии
        // Например, учитывать температурную зону, срок годности и т.д.
        double requiredVolume = 1.0; // Заглушка, нужно рассчитать объем партии
        double requiredWeight = 1.0; // Заглушка, нужно рассчитать вес партии
        
        auto availableCells = findAvailableCells(requiredVolume, requiredWeight);
        
        for (auto& cellPtr : availableCells)
        {
            if (cellPtr)
            {
                result.push_back(*cellPtr);
            }
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findCellsForBatch: " + e.displayText());
    }
    
    return result;
}

models::WarehouseCell WarehouseCellRepository::mapRowToWarehouseCell(Poco::Data::Row& row) const
{
    models::WarehouseCell cell;
    cell.id = row.get(0).convert<long long>();
    cell.cellCode = row.get(1).convert<std::string>();
    cell.zone = row.get(2).convert<std::string>();
    cell.rack = row.get(3).convert<std::string>();
    cell.shelf = row.get(4).convert<std::string>();
    cell.position = row.get(5).convert<std::string>();
    cell.maxVolume = row.get(6).convert<double>();
    cell.maxWeight = row.get(7).convert<double>();
    cell.currentOccupancy = row.get(8).convert<double>();
    
    std::string statusStr = row.get(9).convert<std::string>();
    cell.status = models::WarehouseCell::stringToStatus(statusStr);
    
    std::string temperatureZoneStr = row.get(10).convert<std::string>();
    cell.temperatureZone = models::WarehouseCell::stringToTemperatureZone(temperatureZoneStr);
    
    if (row.fieldCount() > 11 && !row.get(11).isEmpty())
    {
        cell.lastInventoryDate = row.get(11).convert<std::string>();
    }
    
    return cell;
}

} // namespace database::repositories
