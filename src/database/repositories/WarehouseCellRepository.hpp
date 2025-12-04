#pragma once

#include "BaseRepository.hpp"
#include "../models/WarehouseCell.hpp"
#include "../models/ProductBatch.hpp"
#include <memory>
#include <string>
#include <vector>
#include <Poco/JSON/Object.h>
#include <Poco/JSON/Array.h>

namespace warehouse_backend::database::repositories
{

class WarehouseCellRepository : public BaseRepository<models::WarehouseCell>
{
public:
    WarehouseCellRepository();
    ~WarehouseCellRepository() override = default;
    
    std::unique_ptr<models::WarehouseCell> findById(long long id) override;
    std::vector<std::unique_ptr<models::WarehouseCell>> findAll() override;
    std::vector<std::unique_ptr<models::WarehouseCell>> findPaginated(int page, int pageSize) override;
    long long create(const models::WarehouseCell& warehouseCell) override;
    bool update(long long id, const models::WarehouseCell& warehouseCell) override;
    bool remove(long long id) override;
    bool softDelete(long long id) override;
    int count() override;
    
    Poco::JSON::Array findAllAsJson() override;
    Poco::JSON::Object findByIdAsJson(long long id) override;
    
    std::vector<std::unique_ptr<models::WarehouseCell>> findByField(const std::string& fieldName, 
                                                                    const std::string& fieldValue) override;
    std::vector<std::unique_ptr<models::WarehouseCell>> search(const std::string& query, 
                                                               const std::vector<std::string>& fields) override;
    
    std::unique_ptr<models::WarehouseCell> findByCellCode(const std::string& cellCode);
    std::vector<std::unique_ptr<models::WarehouseCell>> findByZone(const std::string& zone);
    std::vector<std::unique_ptr<models::WarehouseCell>> findByStatus(const std::string& status);
    std::vector<std::unique_ptr<models::WarehouseCell>> findByTemperatureZone(const std::string& temperatureZone);
    std::vector<std::unique_ptr<models::WarehouseCell>> findAvailableCells(double requiredVolume, double requiredWeight);
    std::vector<std::unique_ptr<models::WarehouseCell>> findFullCells();
    std::vector<std::unique_ptr<models::WarehouseCell>> findEmptyCells();
    std::vector<std::unique_ptr<models::WarehouseCell>> findBlockedCells();
    
    bool updateStatus(long long id, const std::string& status);
    bool updateOccupancy(long long id, double newOccupancy);
    bool updateTemperatureZone(long long id, const std::string& temperatureZone);
    bool updateMaxCapacity(long long id, double maxVolume, double maxWeight);
    bool updateLastInventoryDate(long long id, const std::string& date);
    bool blockCell(long long id);
    bool unblockCell(long long id);
    
    int countByStatus(const std::string& status);
    int countByTemperatureZone(const std::string& temperatureZone);
    int countEmptyCells();
    int countFullCells();
    int countBlockedCells();
    
    double getTotalMaxVolume();
    double getTotalMaxWeight();
    double getTotalUsedVolume();
    double getTotalUsedWeight();
    double getAverageOccupancy();
    
    Poco::JSON::Array getCellStatistics();
    Poco::JSON::Array getZoneStatistics();
    Poco::JSON::Array getOccupancyReport();
    Poco::JSON::Array getTemperatureZoneReport();
    
    std::vector<std::pair<long long, std::string>> getCellCodes();
    
    models::WarehouseCell findBestCellForStorage(double volume, double weight, const std::string& temperatureZone = "");
    std::vector<models::WarehouseCell> findCellsForBatch(const models::ProductBatch& batch);
    
private:
    models::WarehouseCell mapRowToWarehouseCell(Poco::Data::Row& row) const;
    
    static const std::string TABLE_NAME;
    static const std::vector<std::string> SEARCH_FIELDS;
};

} // namespace database::repositories
