#pragma once

#include "../database/repositories/SupplierRepository.hpp"
#include "../database/repositories/ProductRepository.hpp"
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

struct SupplierServiceResult
{
    bool success;
    std::string message;
    long long supplierId;
    std::unique_ptr<database::models::Supplier> supplier;
    Poco::JSON::Object data;
    
    SupplierServiceResult();
    Poco::JSON::Object toJson() const;
};

class SupplierService
{
public:
    SupplierService();
    ~SupplierService() = default;
    
    SupplierServiceResult createSupplier(const database::models::Supplier& supplier, long long createdBy = 0);
    SupplierServiceResult getSupplierById(long long supplierId);
    SupplierServiceResult getSupplierByTaxId(const std::string& taxId);
    SupplierServiceResult updateSupplier(long long supplierId, const database::models::Supplier& updatedSupplier, long long updatedBy = 0);
    SupplierServiceResult deactivateSupplier(long long supplierId, long long deactivatedBy = 0);
    SupplierServiceResult activateSupplier(long long supplierId, long long activatedBy = 0);
    
    Poco::JSON::Array getActiveSuppliers();
    Poco::JSON::Array getSupplierProducts(long long supplierId);
    Poco::JSON::Array getSupplierBatches(long long supplierId);
    
    bool validateSupplierData(const database::models::Supplier& supplier, std::string& errorMessage);
    bool isTaxIdAvailable(const std::string& taxId, long long excludeSupplierId = 0);
    
private:
    std::unique_ptr<database::models::Supplier> enrichSupplierWithDetails(
        std::unique_ptr<database::models::Supplier> supplier);

    void logAuditEvent(const std::string& tableName,
                       long long recordId,
                       database::models::AuditAction action,
                       const std::string& oldValues,
                       const std::string& newValues,
                       long long changedBy,
                       const std::string& description);

    std::string supplierToJsonString(const database::models::Supplier& supplier) const;

    std::unique_ptr<database::repositories::SupplierRepository> supplierRepository;
    std::unique_ptr<database::repositories::ProductRepository> productRepository;
    std::unique_ptr<database::repositories::ProductBatchRepository> batchRepository;
    std::unique_ptr<database::repositories::AuditLogRepository> auditRepository;
};

} // namespace services