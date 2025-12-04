#pragma once

#include "BaseRepository.hpp"
#include "../models/Product.hpp"
#include <memory>
#include <string>
#include <vector>
#include <Poco/JSON/Object.h>
#include <Poco/JSON/Array.h>

namespace warehouse_backend::database::repositories
{

class ProductRepository : public BaseRepository<models::Product>
{
public:
    ProductRepository();
    ~ProductRepository() override = default;
    
    std::unique_ptr<models::Product> findById(long long id) override;
    std::vector<std::unique_ptr<models::Product>> findAll() override;
    std::vector<std::unique_ptr<models::Product>> findPaginated(int page, int pageSize) override;
    long long create(const models::Product& product) override;
    bool update(long long id, const models::Product& product) override;
    bool remove(long long id) override;
    bool softDelete(long long id) override;
    int count() override;
    
    Poco::JSON::Array findAllAsJson() override;
    Poco::JSON::Object findByIdAsJson(long long id) override;
    
    std::vector<std::unique_ptr<models::Product>> findByField(const std::string& fieldName, 
                                                              const std::string& fieldValue) override;
    std::vector<std::unique_ptr<models::Product>> search(const std::string& query, 
                                                         const std::vector<std::string>& fields) override;
    
    std::unique_ptr<models::Product> findBySku(const std::string& sku);
    std::vector<std::unique_ptr<models::Product>> findByCategory(long long categoryId);
    std::vector<std::unique_ptr<models::Product>> findBySupplier(long long supplierId);
    std::vector<std::unique_ptr<models::Product>> findActiveProducts();
    std::vector<std::unique_ptr<models::Product>> findInactiveProducts();
    std::vector<std::unique_ptr<models::Product>> findProductsWithLowStock();
    std::vector<std::unique_ptr<models::Product>> findProductsWithHighStock();
    std::vector<std::unique_ptr<models::Product>> findProductsWithoutStock();
    
    bool updateStockLevels(long long id, int minStockLevel, int maxStockLevel);
    bool updatePrice(long long id, double newPrice);
    bool updateStatus(long long id, bool isActive);
    bool updateCategory(long long id, long long newCategoryId);
    bool updateSupplier(long long id, long long newSupplierId);
    
    int countActiveProducts();
    int countProductsInCategory(long long categoryId);
    int countProductsBySupplier(long long supplierId);
    
    double getTotalStockValue();
    double getAveragePrice();
    
    bool skuExists(const std::string& sku);
    
    Poco::JSON::Array getProductStatistics();
    Poco::JSON::Array getStockReport();
    Poco::JSON::Array getPriceAnalysisReport();
    
    std::vector<std::pair<long long, std::string>> getProductNames();
    
private:
    models::Product mapRowToProduct(Poco::Data::Row& row) const;
    
    void calculateCurrentStock(models::Product& product);
    
    static const std::string TABLE_NAME;
    static const std::vector<std::string> SEARCH_FIELDS;
};

} // namespace database::repositories
