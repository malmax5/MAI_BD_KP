#pragma once

#include "BaseController.hpp"
#include "../services/InventoryService.hpp"
#include "../services/AuthService.hpp"
#include "../services/ProductService.hpp"
#include "../services/ProductBatchService.hpp"
#include <Poco/URI.h>
#include <memory>

namespace warehouse_backend::controllers
{

class ProductBatchController : public BaseController
{
public:
    ProductBatchController();
    virtual ~ProductBatchController() = default;
    
    void handleRequest(Poco::Net::HTTPServerRequest& request, 
                      Poco::Net::HTTPServerResponse& response) override;
    
    void handleGetBatches(Poco::Net::HTTPServerRequest& request, 
                         Poco::Net::HTTPServerResponse& response);
    
    void handleGetBatchById(Poco::Net::HTTPServerRequest& request, 
                           Poco::Net::HTTPServerResponse& response);
    
    void handleGetBatchByNumber(Poco::Net::HTTPServerRequest& request, 
                               Poco::Net::HTTPServerResponse& response);
    
    void handleCreateBatch(Poco::Net::HTTPServerRequest& request, 
                          Poco::Net::HTTPServerResponse& response);
    
    void handleUpdateBatch(Poco::Net::HTTPServerRequest& request, 
                          Poco::Net::HTTPServerResponse& response);
    
    void handleDeleteBatch(Poco::Net::HTTPServerRequest& request, 
                          Poco::Net::HTTPServerResponse& response);
    
    void handleUpdateBatchQuality(Poco::Net::HTTPServerRequest& request, 
                                Poco::Net::HTTPServerResponse& response);
    
    void handleGetBatchesByProduct(Poco::Net::HTTPServerRequest& request, 
                                  Poco::Net::HTTPServerResponse& response);
    
    void handleGetBatchesBySupplier(Poco::Net::HTTPServerRequest& request, 
                                   Poco::Net::HTTPServerResponse& response);
    
    void handleGetExpiringBatches(Poco::Net::HTTPServerRequest& request, 
                                 Poco::Net::HTTPServerResponse& response);
    
    void handleGetBatchesNeedingInspection(Poco::Net::HTTPServerRequest& request, 
                                          Poco::Net::HTTPServerResponse& response);
    
    void handleGetBatchStatistics(Poco::Net::HTTPServerRequest& request, 
                                Poco::Net::HTTPServerResponse& response);
    
    void handleGetExpirationReport(Poco::Net::HTTPServerRequest& request, 
                                  Poco::Net::HTTPServerResponse& response);
    
    void handleGetQualityStatusReport(Poco::Net::HTTPServerRequest& request, 
                                     Poco::Net::HTTPServerResponse& response);
    
    void handleCheckBatchAvailability(Poco::Net::HTTPServerRequest& request, 
                                     Poco::Net::HTTPServerResponse& response);
    
    bool validateRequest(Poco::Net::HTTPServerRequest& request, 
                        Poco::Net::HTTPServerResponse& response,
                        std::string& errorMessage) override;
    
    bool authorizeRequest(Poco::Net::HTTPServerRequest& request, 
                         Poco::Net::HTTPServerResponse& response,
                         std::string& errorMessage) override;
    
    bool validateBatchAccess(Poco::Net::HTTPServerRequest& request, 
                            long long batchId,
                            std::string& errorMessage);
    
    long long getCurrentUserId(Poco::Net::HTTPServerRequest& request);
    database::models::UserRole getCurrentUserRole(Poco::Net::HTTPServerRequest& request);

    std::map<std::string, std::string> getFilterParameters(Poco::Net::HTTPServerRequest& request);
    int getQueryParameterInt(const Poco::URI& uri, const std::string& name, int defaultValue = 0);
    
private:
    std::unique_ptr<services::InventoryService> inventoryService;
    std::unique_ptr<services::ProductBatchService> batchService;
    std::unique_ptr<services::AuthService> authService;
    std::unique_ptr<services::ProductService> productService;
    
    bool validateCreateBatchData(const Poco::JSON::Object::Ptr& json, 
                                std::vector<std::string>& errors);
    
    bool validateUpdateBatchData(const Poco::JSON::Object::Ptr& json, 
                                std::vector<std::string>& errors);
    
    bool validateQualityUpdateData(const Poco::JSON::Object::Ptr& json, 
                                  std::vector<std::string>& errors);
    
    bool validateSearchParameters(const std::map<std::string, std::string>& filters,
                                 std::vector<std::string>& errors);
    
    void logBatchEvent(long long userId, 
                      const std::string& action,
                      long long batchId,
                      const std::string& ipAddress,
                      const std::string& userAgent,
                      bool success,
                      const std::string& details = "");
    
    std::unique_ptr<database::models::ProductBatch> extractBatchFromJson(const Poco::JSON::Object::Ptr& json);
    
    bool canCreateBatch(Poco::Net::HTTPServerRequest& request, 
                       const database::models::ProductBatch& batchData,
                       std::string& errorMessage);
    
    bool canUpdateBatch(Poco::Net::HTTPServerRequest& request, 
                       long long batchId,
                       const database::models::ProductBatch& batchData,
                       std::string& errorMessage);
    
    bool canDeleteBatch(Poco::Net::HTTPServerRequest& request, 
                       long long batchId,
                       std::string& errorMessage);
    
    bool canUpdateBatchQuality(Poco::Net::HTTPServerRequest& request, 
                              long long batchId,
                              std::string& errorMessage);
    
    Poco::JSON::Object buildPaginationResponse(int page, int pageSize, int totalItems, 
                                              const Poco::JSON::Array& data);
};

} // namespace controllers
