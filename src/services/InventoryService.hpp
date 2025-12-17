#pragma once

#include "../database/repositories/ProductBatchRepository.hpp"
#include "../database/repositories/WarehouseCellRepository.hpp"
#include "../database/repositories/InventoryMovementRepository.hpp"
#include "../database/repositories/ProductRepository.hpp"
#include "../database/repositories/AuditLogRepository.hpp"
#include "../config/ConfigManager.hpp"
#include "../utils/DateUtils.hpp"
#include <memory>
#include <string>
#include <vector>
#include <Poco/JSON/Object.h>
#include <Poco/JSON/Array.h>

namespace warehouse_backend::services
{

struct InventoryMovementResult
{
    bool success;
    std::string message;
    long long movementId;
    std::unique_ptr<database::models::InventoryMovement> movement;
    Poco::JSON::Object details;
    
    InventoryMovementResult();
    Poco::JSON::Object toJson() const;
};

struct StockCheckResult
{
    bool available;
    int quantityAvailable;
    int quantityNeeded;
    std::vector<long long> availableBatchIds;
    std::string message;
    
    Poco::JSON::Object toJson() const;
};

class InventoryService
{
public:
    InventoryService();
    ~InventoryService() = default;
    
    InventoryMovementResult receiveProduct(long long productId,
                                          long long supplierId,
                                          int quantity,
                                          double unitCost,
                                          const std::string& batchNumber,
                                          const std::string& expirationDate = "",
                                          long long storageCellId = 0,
                                          long long receivedBy = 0);
    
    InventoryMovementResult transferProduct(long long batchId,
                                           long long fromCellId,
                                           long long toCellId,
                                           int quantity,
                                           long long performedBy = 0,
                                           const std::string& reason = "");
    
    InventoryMovementResult adjustInventory(long long productId,
                                           long long batchId,
                                           long long cellId,
                                           int quantityAdjustment,
                                           long long performedBy = 0,
                                           const std::string& reason = "");
    
    StockCheckResult checkStockAvailability(long long productId, int quantity);
    StockCheckResult checkBatchAvailability(long long batchId, int quantity);
    
    Poco::JSON::Array getMovements(int page = 1, int pageSize = 20, 
                                   const std::map<std::string, std::string>& filters = {});
    Poco::JSON::Object getProductStockInfo(long long productId);
    Poco::JSON::Object getBatchStockInfo(long long batchId);
    Poco::JSON::Object getCellStockInfo(long long cellId);
    
    Poco::JSON::Array findExpiringProducts(int daysThreshold = 30);
    Poco::JSON::Array findLowStockProducts(int threshold = 10);
    Poco::JSON::Array findProductsNeedingQualityInspection();
    
    bool updateBatchQualityStatus(long long batchId,
                                 const std::string& qualityStatus,
                                 long long updatedBy = 0);
    
    Poco::JSON::Array getInventoryMovementHistory(long long productId,
                                                 const std::string& startDate = "",
                                                 const std::string& endDate = "");
    
    Poco::JSON::Array getCellMovementHistory(long long cellId,
                                            const std::string& startDate = "",
                                            const std::string& endDate = "");
    
    Poco::JSON::Object getInventoryStatistics();
    Poco::JSON::Array getStockValueReport();
    Poco::JSON::Array getExpirationReport();
    Poco::JSON::Array getQualityStatusReport();
    
    Poco::JSON::Object findBestStorageCell(double volume,
                                          double weight,
                                          const std::string& temperatureZone = "");
    
    bool reserveStock(long long batchId, int quantity, long long reservedBy = 0);
    bool releaseStock(long long batchId, int quantity, long long releasedBy = 0);
    
private:
    std::unique_ptr<database::repositories::ProductBatchRepository> batchRepository;
    std::unique_ptr<database::repositories::WarehouseCellRepository> cellRepository;
    std::unique_ptr<database::repositories::InventoryMovementRepository> movementRepository;
    std::unique_ptr<database::repositories::ProductRepository> productRepository;
    std::unique_ptr<database::repositories::AuditLogRepository> auditRepository;
    
    std::string generateBatchNumber(long long productId);
    void updateCellOccupancy(long long cellId);
    void logInventoryAudit(const std::string& action,
                          long long recordId,
                          long long changedBy,
                          const std::string& details = "");
};

} // namespace services
