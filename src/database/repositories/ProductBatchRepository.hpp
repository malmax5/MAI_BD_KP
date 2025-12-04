#pragma once

#include "BaseRepository.hpp"
#include "../models/ProductBatch.hpp"
#include <memory>
#include <string>
#include <vector>
#include <Poco/JSON/Object.h>
#include <Poco/JSON/Array.h>

namespace warehouse_backend::database::repositories
{

class ProductBatchRepository : public BaseRepository<models::ProductBatch>
{
public:
    ProductBatchRepository();
    ~ProductBatchRepository() override = default;
    
    std::unique_ptr<models::ProductBatch> findById(long long id) override;
    std::vector<std::unique_ptr<models::ProductBatch>> findAll() override;
    std::vector<std::unique_ptr<models::ProductBatch>> findPaginated(int page, int pageSize) override;
    long long create(const models::ProductBatch& productBatch) override;
    bool update(long long id, const models::ProductBatch& productBatch) override;
    bool remove(long long id) override;
    bool softDelete(long long id) override;
    int count() override;
    
    Poco::JSON::Array findAllAsJson() override;
    Poco::JSON::Object findByIdAsJson(long long id) override;
    
    std::vector<std::unique_ptr<models::ProductBatch>> findByField(const std::string& fieldName, 
                                                                   const std::string& fieldValue) override;
    std::vector<std::unique_ptr<models::ProductBatch>> search(const std::string& query, 
                                                              const std::vector<std::string>& fields) override;
    
    std::unique_ptr<models::ProductBatch> findByBatchNumber(const std::string& batchNumber);
    std::vector<std::unique_ptr<models::ProductBatch>> findByProduct(long long productId);
    std::vector<std::unique_ptr<models::ProductBatch>> findBySupplier(long long supplierId);
    std::vector<std::unique_ptr<models::ProductBatch>> findByStorageCell(long long storageCellId);
    std::vector<std::unique_ptr<models::ProductBatch>> findByQualityStatus(const std::string& qualityStatus);
    std::vector<std::unique_ptr<models::ProductBatch>> findExpiringBatches(int daysThreshold);
    std::vector<std::unique_ptr<models::ProductBatch>> findExpiredBatches();
    std::vector<std::unique_ptr<models::ProductBatch>> findBatchesNeedingInspection();
    std::vector<std::unique_ptr<models::ProductBatch>> findAvailableBatches(long long productId, int quantity);
    std::vector<std::unique_ptr<models::ProductBatch>> findBatchesWithLowQuantity(int threshold);
    
    bool updateQualityStatus(long long id, const std::string& qualityStatus);
    bool updateQuantity(long long id, int quantityReceived, int quantityAvailable);
    bool updateStorageCell(long long id, long long storageCellId);
    bool updateExpirationDate(long long id, const std::string& expirationDate);
    bool updateInvoiceNumber(long long id, const std::string& invoiceNumber);
    bool useQuantity(long long id, int quantity);
    bool returnQuantity(long long id, int quantity);
    
    int countByProduct(long long productId);
    int countBySupplier(long long supplierId);
    int countByQualityStatus(const std::string& qualityStatus);
    int countExpiringBatches(int daysThreshold);
    int countExpiredBatches();
    
    double getTotalBatchValue();
    double getTotalBatchValueByProduct(long long productId);
    double getTotalBatchValueBySupplier(long long supplierId);
    int getTotalAvailableQuantity(long long productId);
    double getAverageUnitCost(long long productId);
    
    bool batchNumberExists(const std::string& batchNumber);
    
    Poco::JSON::Array getBatchStatistics();
    Poco::JSON::Array getExpirationReport();
    Poco::JSON::Array getQualityStatusReport();
    Poco::JSON::Array getSupplierBatchReport(long long supplierId);
    
    std::vector<std::pair<long long, std::string>> getBatchNumbers();
    std::vector<models::ProductBatch> findBatchesForOrderItem(long long productId, int quantity);
    std::map<long long, int> getProductStockSummary();
    
private:
    models::ProductBatch mapRowToProductBatch(Poco::Data::Row& row) const;
    void enrichProductBatchWithDetails(models::ProductBatch& batch);
    
    static const std::string TABLE_NAME;
    static const std::vector<std::string> SEARCH_FIELDS;
};

} // namespace database::repositories
