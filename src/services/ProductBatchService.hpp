#pragma once

#include <memory>
#include <string>
#include <vector>
#include <map>
#include <Poco/JSON/Object.h>
#include <Poco/JSON/Array.h>
#include "../database/models/ProductBatch.hpp"
#include "../database/repositories/ProductBatchRepository.hpp"
#include "../database/repositories/ProductRepository.hpp"
#include "../database/repositories/SupplierRepository.hpp"
#include "../database/repositories/WarehouseCellRepository.hpp"
#include "../database/repositories/AuditLogRepository.hpp"
#include "../utils/DateUtils.hpp"

namespace warehouse_backend::services
{

struct ProductBatchCreationResult
{
    bool success;
    std::string message;
    long long batchId;
    std::string batchNumber;
    std::shared_ptr<database::models::ProductBatch> batch;
    Poco::JSON::Object batchDetails;
    
    ProductBatchCreationResult()
        : success(false), batchId(0), batch(nullptr)
    {
    }
    
    Poco::JSON::Object toJson() const;
};

struct ProductBatchUpdateResult
{
    bool success;
    std::string message;
    std::shared_ptr<database::models::ProductBatch> updatedBatch;
    std::vector<std::string> changes;
    
    ProductBatchUpdateResult()
        : success(false), updatedBatch(nullptr)
    {
    }
    
    Poco::JSON::Object toJson() const;
};

struct BatchQualityInspectionResult
{
    bool success;
    std::string message;
    std::string previousStatus;
    std::string newStatus;
    std::shared_ptr<database::models::ProductBatch> inspectedBatch;
    std::string inspectionNotes;
    
    BatchQualityInspectionResult()
        : success(false), inspectedBatch(nullptr)
    {
    }
    
    Poco::JSON::Object toJson() const;
};

struct BatchTransferResult
{
    bool success;
    std::string message;
    std::shared_ptr<database::models::ProductBatch> transferredBatch;
    long long fromCellId;
    long long toCellId;
    std::string fromCellCode;
    std::string toCellCode;
    
    BatchTransferResult()
        : success(false), transferredBatch(nullptr), fromCellId(0), toCellId(0)
    {
    }
    
    Poco::JSON::Object toJson() const;
};

class ProductBatchService
{
public:
    ProductBatchService();
    
    ProductBatchCreationResult createProductBatch(const database::models::ProductBatch& batch, long long createdBy);
    ProductBatchUpdateResult updateProductBatch(long long batchId, const database::models::ProductBatch& updatedBatch, long long updatedBy);
    bool deleteProductBatch(long long batchId, long long deletedBy, const std::string& reason = "");
    
    std::shared_ptr<database::models::ProductBatch> getBatchById(long long batchId);
    std::shared_ptr<database::models::ProductBatch> getBatchByNumber(const std::string& batchNumber);
    Poco::JSON::Array getBatchesByProduct(long long productId, int page = 1, int pageSize = 20);
    Poco::JSON::Array getBatchesBySupplier(long long supplierId, int page = 1, int pageSize = 20);
    Poco::JSON::Array getBatchesByStorageCell(long long cellId, int page = 1, int pageSize = 20);
    
    BatchQualityInspectionResult inspectBatch(long long batchId, const std::string& newStatus, 
                                              long long inspectedBy, const std::string& notes = "");
    Poco::JSON::Array getBatchesNeedingInspection(int page = 1, int pageSize = 20);
    
    ProductBatchUpdateResult useBatchQuantity(long long batchId, int quantity, long long usedBy, 
                                              const std::string& reason = "");
    ProductBatchUpdateResult returnBatchQuantity(long long batchId, int quantity, long long returnedBy,
                                                 const std::string& reason = "");
    ProductBatchUpdateResult adjustBatchQuantity(long long batchId, int newQuantity, long long adjustedBy,
                                                 const std::string& reason = "");
    
    BatchTransferResult transferBatch(long long batchId, long long newCellId, long long transferredBy,
                                      const std::string& reason = "");
    Poco::JSON::Object findBestCellForBatch(long long batchId);
    
    Poco::JSON::Object getBatchStatistics();
    Poco::JSON::Array getExpiringBatchesReport(int daysThreshold = 30);
    Poco::JSON::Array getExpiredBatchesReport();
    Poco::JSON::Array getQualityStatusReport();
    Poco::JSON::Array getSupplierBatchReport(long long supplierId);
    Poco::JSON::Array getProductStockSummary(long long productId = 0);
    Poco::JSON::Array getStockValueAnalysis();
    Poco::JSON::Array getBatchAgingReport();
    
    Poco::JSON::Array searchBatches(const std::string& query, int page = 1, int pageSize = 20);
    Poco::JSON::Array findAvailableBatchesForProduct(long long productId, int requiredQuantity);
    Poco::JSON::Array findBatchesForOrder(long long productId, int quantity);
    
    Poco::JSON::Array importBatches(const Poco::JSON::Array& batchesJson, long long importedBy);
    bool exportBatchesToJson(const std::string& filePath, const std::vector<long long>& batchIds);
    
    bool validateBatchForCreation(const database::models::ProductBatch& batch, std::string& errorMessage);
    bool canBatchBeUsed(long long batchId, int quantity);
    bool isBatchExpired(long long batchId);
    int getDaysUntilExpiration(long long batchId);
    double calculateBatchValue(long long batchId);
    Poco::JSON::Object getBatchDetails(long long batchId);
    
private:
    std::unique_ptr<database::repositories::ProductBatchRepository> batchRepository;
    std::unique_ptr<database::repositories::ProductRepository> productRepository;
    std::unique_ptr<database::repositories::SupplierRepository> supplierRepository;
    std::unique_ptr<database::repositories::WarehouseCellRepository> cellRepository;
    std::unique_ptr<database::repositories::AuditLogRepository> auditRepository;
    
    std::string generateBatchNumber(const std::string& productSku = "");
    bool checkCellCapacity(long long cellId, long long productId, int quantity);
    void logBatchAudit(const std::string& action, long long batchId, long long userId, 
                       const std::string& details, const std::string& ipAddress = "127.0.0.1");
    void enrichBatchWithDetails(database::models::ProductBatch& batch);
    Poco::JSON::Object convertBatchToJson(const database::models::ProductBatch& batch, bool includeDetails = false);
};

} // namespace services
