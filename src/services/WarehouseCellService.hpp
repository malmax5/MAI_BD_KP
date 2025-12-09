#pragma once

#include "../database/repositories/WarehouseCellRepository.hpp"
#include "../database/repositories/ProductBatchRepository.hpp"
#include "../database/repositories/AuditLogRepository.hpp"
#include "../config/ConfigManager.hpp"
#include "../utils/Validator.hpp"
#include "../utils/JsonUtils.hpp"
#include "../utils/DateUtils.hpp"
#include <memory>
#include <string>
#include <Poco/JSON/Object.h>
#include <Poco/JSON/Array.h>

namespace warehouse_backend::services
{

struct WarehouseCellServiceResult
{
    bool success;
    std::string message;
    long long cellId;
    std::unique_ptr<database::models::WarehouseCell> cell;
    Poco::JSON::Object data;
    
    WarehouseCellServiceResult();
    Poco::JSON::Object toJson() const;
};

class WarehouseCellService
{
public:
    WarehouseCellService();
    ~WarehouseCellService() = default;
    
    WarehouseCellServiceResult createCell(const database::models::WarehouseCell& cell, long long createdBy = 0);
    WarehouseCellServiceResult getCellById(long long cellId);
    WarehouseCellServiceResult getCellByCode(const std::string& cellCode);
    WarehouseCellServiceResult updateCell(long long cellId, const database::models::WarehouseCell& updatedCell, long long updatedBy = 0);
    WarehouseCellServiceResult blockCell(long long cellId, long long blockedBy = 0);
    WarehouseCellServiceResult unblockCell(long long cellId, long long unblockedBy = 0);
    
    Poco::JSON::Array getAvailableCells(double requiredVolume, double requiredWeight);
    Poco::JSON::Array getCellsByZone(const std::string& zone);
    Poco::JSON::Array getCellsByStatus(const std::string& status);
    Poco::JSON::Array getCellBatches(long long cellId);
    
    bool validateCellData(const database::models::WarehouseCell& cell, std::string& errorMessage);
    bool canStoreInCell(long long cellId, double volume, double weight);
    WarehouseCellServiceResult findBestCellForStorage(double volume, double weight, const std::string& temperatureZone = "");
    
private:
    std::string warehouseCellToJsonString(const database::models::WarehouseCell& warehouseCell) const;

    std::unique_ptr<database::repositories::WarehouseCellRepository> cellRepository;
    std::unique_ptr<database::repositories::ProductBatchRepository> batchRepository;
    std::unique_ptr<database::repositories::AuditLogRepository> auditRepository;
};

} // namespace services