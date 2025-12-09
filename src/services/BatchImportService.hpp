#pragma once

#include "../database/repositories/ProductRepository.hpp"
#include "../database/repositories/SupplierRepository.hpp"
#include "../database/repositories/ProductBatchRepository.hpp"
#include "../config/ConfigManager.hpp"
#include "../utils/Validator.hpp"
#include <memory>
#include <string>
#include <Poco/JSON/Object.h>
#include <Poco/JSON/Array.h>

namespace warehouse_backend::services
{

struct ImportResult
{
    bool success;
    std::string message;
    int importedCount;
    int failedCount;
    Poco::JSON::Array failedItems;
    
    ImportResult();
    Poco::JSON::Object toJson() const;
};

class BatchImportService
{
public:
    BatchImportService();
    ~BatchImportService() = default;
    
    ImportResult importProducts(const Poco::JSON::Array& productsData, long long importedBy = 0);
    ImportResult importSuppliers(const Poco::JSON::Array& suppliersData, long long importedBy = 0);
    ImportResult importBatches(const Poco::JSON::Array& batchesData, long long importedBy = 0);
    
private:
    std::unique_ptr<database::repositories::ProductRepository> productRepository;
    std::unique_ptr<database::repositories::SupplierRepository> supplierRepository;
    std::unique_ptr<database::repositories::ProductBatchRepository> batchRepository;
};

} // namespace services