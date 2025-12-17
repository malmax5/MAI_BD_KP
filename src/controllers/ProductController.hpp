#pragma once

#include "BaseController.hpp"
#include "../services/ProductService.hpp"
#include "../services/AuthService.hpp"
#include "../services/CategoryService.hpp"
#include "../services/SupplierService.hpp"
#include <memory>

namespace warehouse_backend::controllers
{

class ProductController : public BaseController
{
public:
    ProductController();
    virtual ~ProductController() = default;
    
    void handleRequest(Poco::Net::HTTPServerRequest& request, 
                      Poco::Net::HTTPServerResponse& response) override;
    
    void handleGetProducts(Poco::Net::HTTPServerRequest& request, 
                         Poco::Net::HTTPServerResponse& response);
    
    void handleGetProductById(Poco::Net::HTTPServerRequest& request, 
                            Poco::Net::HTTPServerResponse& response);
    
    void handleGetProductBySku(Poco::Net::HTTPServerRequest& request, 
                             Poco::Net::HTTPServerResponse& response);
    
    void handleCreateProduct(Poco::Net::HTTPServerRequest& request, 
                           Poco::Net::HTTPServerResponse& response);
    
    void handleUpdateProduct(Poco::Net::HTTPServerRequest& request, 
                           Poco::Net::HTTPServerResponse& response);
    
    void handleDeleteProduct(Poco::Net::HTTPServerRequest& request, 
                           Poco::Net::HTTPServerResponse& response);
    
    void handleSoftDeleteProduct(Poco::Net::HTTPServerRequest& request, 
                               Poco::Net::HTTPServerResponse& response);
    
    void handleActivateProduct(Poco::Net::HTTPServerRequest& request, 
                             Poco::Net::HTTPServerResponse& response);
    
    void handleDeactivateProduct(Poco::Net::HTTPServerRequest& request, 
                               Poco::Net::HTTPServerResponse& response);
    
    void handleUpdateStockLevels(Poco::Net::HTTPServerRequest& request, 
                                Poco::Net::HTTPServerResponse& response);
    
    void handleUpdatePrice(Poco::Net::HTTPServerRequest& request, 
                          Poco::Net::HTTPServerResponse& response);
    
    void handleSearchProducts(Poco::Net::HTTPServerRequest& request, 
                            Poco::Net::HTTPServerResponse& response);
    
    void handleGetProductsByCategory(Poco::Net::HTTPServerRequest& request, 
                                    Poco::Net::HTTPServerResponse& response);
    
    void handleGetProductsBySupplier(Poco::Net::HTTPServerRequest& request, 
                                    Poco::Net::HTTPServerResponse& response);
    
    void handleGetLowStockProducts(Poco::Net::HTTPServerRequest& request, 
                                  Poco::Net::HTTPServerResponse& response);
    
    void handleGetProductsNeedingReorder(Poco::Net::HTTPServerRequest& request, 
                                        Poco::Net::HTTPServerResponse& response);
    
    void handleGetProductStockSummary(Poco::Net::HTTPServerRequest& request, 
                                     Poco::Net::HTTPServerResponse& response);
    
    void handleGetProductStatistics(Poco::Net::HTTPServerRequest& request, 
                                   Poco::Net::HTTPServerResponse& response);
    
    void handleGetStockReport(Poco::Net::HTTPServerRequest& request, 
                             Poco::Net::HTTPServerResponse& response);
    
    void handleImportProducts(Poco::Net::HTTPServerRequest& request, 
                            Poco::Net::HTTPServerResponse& response);
    
    void handleExportProducts(Poco::Net::HTTPServerRequest& request, 
                            Poco::Net::HTTPServerResponse& response);
    
    bool validateRequest(Poco::Net::HTTPServerRequest& request, 
                        Poco::Net::HTTPServerResponse& response,
                        std::string& errorMessage) override;
    
    bool authorizeRequest(Poco::Net::HTTPServerRequest& request, 
                         Poco::Net::HTTPServerResponse& response,
                         std::string& errorMessage) override;
    
    bool validateProductAccess(Poco::Net::HTTPServerRequest& request, 
                              long long targetProductId,
                              std::string& errorMessage);
    
    long long getCurrentUserId(Poco::Net::HTTPServerRequest& request);
    database::models::UserRole getCurrentUserRole(Poco::Net::HTTPServerRequest& request);
    
private:
    std::unique_ptr<services::ProductService> productService;
    std::unique_ptr<services::AuthService> authService;
    std::unique_ptr<services::CategoryService> categoryService;
    std::unique_ptr<services::SupplierService> supplierService;
    
    bool validateCreateProductData(const Poco::JSON::Object::Ptr& json, 
                                 std::vector<std::string>& errors);
    
    bool validateUpdateProductData(const Poco::JSON::Object::Ptr& json, 
                                 std::vector<std::string>& errors);
    
    bool validateStockLevelsData(const Poco::JSON::Object::Ptr& json, 
                                std::vector<std::string>& errors);
    
    bool validatePriceUpdateData(const Poco::JSON::Object::Ptr& json, 
                               std::vector<std::string>& errors);
    
    bool validateSearchParameters(const std::map<std::string, std::string>& filters,
                                 std::vector<std::string>& errors);
    
    void logProductEvent(long long userId, 
                        const std::string& action,
                        long long targetProductId,
                        const std::string& ipAddress,
                        const std::string& userAgent,
                        bool success,
                        const std::string& details = "");
    
    std::unique_ptr<database::models::Product> extractProductFromJson(const Poco::JSON::Object::Ptr& json);
    
    bool canCreateProduct(Poco::Net::HTTPServerRequest& request, 
                         const database::models::Product& productData,
                         std::string& errorMessage);
    
    bool canUpdateProduct(Poco::Net::HTTPServerRequest& request, 
                         long long targetProductId,
                         const database::models::Product& productData,
                         std::string& errorMessage);
    
    bool canDeleteProduct(Poco::Net::HTTPServerRequest& request, 
                         long long targetProductId,
                         std::string& errorMessage);
    
    bool canViewProduct(Poco::Net::HTTPServerRequest& request, 
                       long long targetProductId,
                       std::string& errorMessage);
    
    Poco::JSON::Object buildPaginationResponse(int page, int pageSize, int totalItems, 
                                              const Poco::JSON::Array& data);
    
    long long extractProductIdFromPath(const std::string& uri);
    long long extractCategoryIdFromQuery(const std::string& uri);
    long long extractSupplierIdFromQuery(const std::string& uri);
};

} // namespace controllers
