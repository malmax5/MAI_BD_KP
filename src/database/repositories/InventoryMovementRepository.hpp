#pragma once

#include "BaseRepository.hpp"
#include "../models/InventoryMovement.hpp"
#include <memory>
#include <string>
#include <vector>
#include <Poco/JSON/Object.h>
#include <Poco/JSON/Array.h>

namespace warehouse_backend::database::repositories
{

class InventoryMovementRepository : public BaseRepository<models::InventoryMovement>
{
public:
    InventoryMovementRepository();
    ~InventoryMovementRepository() override = default;
    
    std::unique_ptr<models::InventoryMovement> findById(long long id) override;
    std::vector<std::unique_ptr<models::InventoryMovement>> findAll() override;
    std::vector<std::unique_ptr<models::InventoryMovement>> findPaginated(int page, int pageSize) override;
    long long create(const models::InventoryMovement& movement) override;
    bool update(long long id, const models::InventoryMovement& movement) override;
    bool remove(long long id) override;
    bool softDelete(long long id) override;
    int count() override;
    
    Poco::JSON::Array findAllAsJson() override;
    Poco::JSON::Object findByIdAsJson(long long id) override;
    
    std::vector<std::unique_ptr<models::InventoryMovement>> findByField(const std::string& fieldName, 
                                                                        const std::string& fieldValue) override;
    std::vector<std::unique_ptr<models::InventoryMovement>> search(const std::string& query, 
                                                                   const std::vector<std::string>& fields) override;
    
    std::vector<std::unique_ptr<models::InventoryMovement>> findByProductId(long long productId);
    std::vector<std::unique_ptr<models::InventoryMovement>> findByBatchId(long long batchId);
    std::vector<std::unique_ptr<models::InventoryMovement>> findByMovementType(models::MovementType type);
    std::vector<std::unique_ptr<models::InventoryMovement>> findByStatus(models::MovementStatus status);
    std::vector<std::unique_ptr<models::InventoryMovement>> findByPerformedBy(long long userId);
    std::vector<std::unique_ptr<models::InventoryMovement>> findByFromCell(long long cellId);
    std::vector<std::unique_ptr<models::InventoryMovement>> findByToCell(long long cellId);
    std::vector<std::unique_ptr<models::InventoryMovement>> findByDateRange(const std::string& startDate, const std::string& endDate);
    std::vector<std::unique_ptr<models::InventoryMovement>> findByReference(long long referenceId, const std::string& referenceType);
    
    std::vector<std::unique_ptr<models::InventoryMovement>> findPlannedMovements();
    std::vector<std::unique_ptr<models::InventoryMovement>> findInProgressMovements();
    std::vector<std::unique_ptr<models::InventoryMovement>> findCompletedMovements();
    std::vector<std::unique_ptr<models::InventoryMovement>> findReceiptMovements();
    std::vector<std::unique_ptr<models::InventoryMovement>> findShipmentMovements();
    std::vector<std::unique_ptr<models::InventoryMovement>> findTransferMovements();
    std::vector<std::unique_ptr<models::InventoryMovement>> findAdjustmentMovements();
    
    bool updateStatus(long long id, models::MovementStatus newStatus);
    bool updateQuantity(long long id, int newQuantity);
    bool updateCells(long long id, long long fromCellId, long long toCellId);
    bool updateReason(long long id, const std::string& reason);
    bool updatePerformedBy(long long id, long long userId);
    
    bool markAsInProgress(long long id);
    bool markAsCompleted(long long id);
    bool markAsCancelled(long long id);
    
    int countByMovementType(models::MovementType type);
    int countByStatus(models::MovementStatus status);
    int countByProduct(long long productId);
    int countByUser(long long userId);
    
    int getTotalQuantityMoved(models::MovementType type, const std::string& startDate, const std::string& endDate);
    double getTotalValueMoved(models::MovementType type, const std::string& startDate, const std::string& endDate);
    
    Poco::JSON::Array getMovementStatistics();
    Poco::JSON::Array getMovementReport(const std::string& startDate, const std::string& endDate);
    Poco::JSON::Array getProductMovementHistory(long long productId);
    Poco::JSON::Array getCellMovementHistory(long long cellId);
    
    bool createTransferMovement(long long productId, long long batchId, 
                               long long fromCellId, long long toCellId, 
                               int quantity, long long performedBy, 
                               const std::string& reason = "");
    bool createAdjustmentMovement(long long productId, long long batchId, 
                                 long long cellId, int quantity, 
                                 long long performedBy, const std::string& reason = "");
    
    std::vector<std::pair<long long, std::string>> getRecentMovements(int limit = 10);
    
private:
    models::InventoryMovement mapRowToMovement(Poco::Data::Row& row) const;
    
    static const std::string TABLE_NAME;
    static const std::vector<std::string> SEARCH_FIELDS;
};

} // namespace database::repositories
