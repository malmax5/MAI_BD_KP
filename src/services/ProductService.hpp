#pragma once

#include "../database/repositories/ProductRepository.hpp"
#include "../database/repositories/ProductBatchRepository.hpp"
#include "../database/repositories/CategoryRepository.hpp"
#include "../database/repositories/SupplierRepository.hpp"
#include "../database/repositories/AuditLogRepository.hpp"
#include "../config/ConfigManager.hpp"
#include "../utils/Validator.hpp"
#include <memory>
#include <string>
#include <vector>
#include "../utils/Validator.hpp"
#include "../utils/JsonUtils.hpp"
#include "../utils/DateUtils.hpp"
#include <Poco/JSON/Object.h>
#include <Poco/JSON/Array.h>

namespace warehouse_backend::services
{

struct ProductServiceResult
{
    bool success;
    std::string message;
    long long productId;
    std::unique_ptr<database::models::Product> product;
    Poco::JSON::Object data;
    
    ProductServiceResult();
    
    Poco::JSON::Object toJson() const;
};

class ProductService
{
public:
    ProductService();
    ~ProductService() = default;
    
    ProductServiceResult createProduct(const database::models::Product& product,
                                      long long createdBy = 0);
    
    ProductServiceResult getProductById(long long productId);
    ProductServiceResult getProductBySku(const std::string& sku);
    
    Poco::JSON::Array getProductsByCategory(long long categoryId, 
                                           int page = 1, 
                                           int pageSize = 20);
    
    Poco::JSON::Array searchProducts(const std::string& query,
                                    int page = 1,
                                    int pageSize = 20);
    
    ProductServiceResult updateProduct(long long productId,
                                      const database::models::Product& updatedProduct,
                                      long long updatedBy = 0);
    
    ProductServiceResult updateStockLevels(long long productId,
                                          int minStockLevel,
                                          int maxStockLevel,
                                          long long updatedBy = 0);
    
    ProductServiceResult updatePrice(long long productId,
                                    double newPrice,
                                    long long updatedBy = 0);
    
    ProductServiceResult deactivateProduct(long long productId,
                                          long long deactivatedBy = 0);
    
    ProductServiceResult activateProduct(long long productId,
                                        long long activatedBy = 0);
    
    Poco::JSON::Array getLowStockProducts(int threshold = 10);
    Poco::JSON::Array getProductsNeedingReorder();
    
    Poco::JSON::Object getProductStockSummary(long long productId);
    Poco::JSON::Object getProductValueAnalysis(long long productId);
    
    Poco::JSON::Array getProductStatistics();
    Poco::JSON::Array getCategoryProductReport(long long categoryId);
    Poco::JSON::Array getSupplierProductReport(long long supplierId);
    
    bool validateProductData(const database::models::Product& product,
                            std::string& errorMessage);
    
    bool isSkuAvailable(const std::string& sku, long long excludeProductId = 0);
    
    std::string productToJsonString(const database::models::Product& user) const;
    
private:
    std::unique_ptr<database::repositories::ProductRepository> productRepository;
    std::unique_ptr<database::repositories::ProductBatchRepository> batchRepository;
    std::unique_ptr<database::repositories::CategoryRepository> categoryRepository;
    std::unique_ptr<database::repositories::SupplierRepository> supplierRepository;
    std::unique_ptr<database::repositories::AuditLogRepository> auditRepository;
    
    void logProductAudit(const std::string& action,
                        long long productId,
                        long long changedBy,
                        const std::string& oldValues = "",
                        const std::string& newValues = "");
};

} // namespace services
