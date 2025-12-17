#pragma once

#include "BaseController.hpp"
#include "../services/SupplierService.hpp"
#include "../services/AuthService.hpp"
#include <memory>

namespace warehouse_backend::controllers
{

class SupplierController : public BaseController
{
public:
    SupplierController();
    virtual ~SupplierController() = default;
    
    void handleRequest(Poco::Net::HTTPServerRequest& request, 
                      Poco::Net::HTTPServerResponse& response) override;
    
    void handleGetSuppliers(Poco::Net::HTTPServerRequest& request, 
                           Poco::Net::HTTPServerResponse& response);
    
    void handleGetSupplierById(Poco::Net::HTTPServerRequest& request, 
                              Poco::Net::HTTPServerResponse& response);
    
    void handleCreateSupplier(Poco::Net::HTTPServerRequest& request, 
                             Poco::Net::HTTPServerResponse& response);
    
    void handleUpdateSupplier(Poco::Net::HTTPServerRequest& request, 
                             Poco::Net::HTTPServerResponse& response);
    
    void handleDeleteSupplier(Poco::Net::HTTPServerRequest& request, 
                             Poco::Net::HTTPServerResponse& response);
    
    void handleGetActiveSuppliers(Poco::Net::HTTPServerRequest& request, 
                                 Poco::Net::HTTPServerResponse& response);
    
    void handleGetSupplierProducts(Poco::Net::HTTPServerRequest& request, 
                                  Poco::Net::HTTPServerResponse& response);
    
    void handleGetSupplierBatches(Poco::Net::HTTPServerRequest& request, 
                                 Poco::Net::HTTPServerResponse& response);
    
    void handleSearchSuppliers(Poco::Net::HTTPServerRequest& request, 
                              Poco::Net::HTTPServerResponse& response);
    
    void handleActivateSupplier(Poco::Net::HTTPServerRequest& request, 
                               Poco::Net::HTTPServerResponse& response);
    
    void handleDeactivateSupplier(Poco::Net::HTTPServerRequest& request, 
                                 Poco::Net::HTTPServerResponse& response);
    
    void handleGetSupplierByTaxId(Poco::Net::HTTPServerRequest& request, 
                                 Poco::Net::HTTPServerResponse& response);
    
    void handleGetSupplierStatistics(Poco::Net::HTTPServerRequest& request, 
                                    Poco::Net::HTTPServerResponse& response);
    
    bool validateRequest(Poco::Net::HTTPServerRequest& request, 
                        Poco::Net::HTTPServerResponse& response,
                        std::string& errorMessage) override;
    
    bool authorizeRequest(Poco::Net::HTTPServerRequest& request, 
                         Poco::Net::HTTPServerResponse& response,
                         std::string& errorMessage) override;
    
    bool validateSupplierAccess(Poco::Net::HTTPServerRequest& request, 
                               long long supplierId,
                               std::string& errorMessage);
    
    long long getCurrentUserId(Poco::Net::HTTPServerRequest& request);
    database::models::UserRole getCurrentUserRole(Poco::Net::HTTPServerRequest& request);
    
private:
    std::unique_ptr<services::SupplierService> supplierService;
    std::unique_ptr<services::AuthService> authService;
    
    bool validateCreateSupplierData(const Poco::JSON::Object::Ptr& json, 
                                   std::vector<std::string>& errors);
    
    bool validateUpdateSupplierData(const Poco::JSON::Object::Ptr& json, 
                                   std::vector<std::string>& errors);
    
    bool validateSearchParameters(const std::map<std::string, std::string>& filters,
                                 std::vector<std::string>& errors);
    
    void logSupplierEvent(long long userId, 
                         const std::string& action,
                         long long supplierId,
                         const std::string& ipAddress,
                         const std::string& userAgent,
                         bool success,
                         const std::string& details = "");
    
    std::unique_ptr<database::models::Supplier> extractSupplierFromJson(const Poco::JSON::Object::Ptr& json);
    
    bool canCreateSupplier(Poco::Net::HTTPServerRequest& request, 
                          const database::models::Supplier& supplierData,
                          std::string& errorMessage);
    
    bool canUpdateSupplier(Poco::Net::HTTPServerRequest& request, 
                          long long supplierId,
                          const database::models::Supplier& supplierData,
                          std::string& errorMessage);
    
    bool canDeleteSupplier(Poco::Net::HTTPServerRequest& request, 
                          long long supplierId,
                          std::string& errorMessage);
    
    bool canActivateSupplier(Poco::Net::HTTPServerRequest& request, 
                            long long supplierId,
                            std::string& errorMessage);
    
    bool canDeactivateSupplier(Poco::Net::HTTPServerRequest& request, 
                              long long supplierId,
                              std::string& errorMessage);
    
    Poco::JSON::Object buildPaginationResponse(int page, int pageSize, int totalItems, 
                                              const Poco::JSON::Array& data);
};

} // namespace controllers
