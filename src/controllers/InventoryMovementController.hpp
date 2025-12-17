#pragma once

#include "BaseController.hpp"
#include "../services/InventoryService.hpp"
#include "../services/AuthService.hpp"
#include <memory>

namespace warehouse_backend::controllers
{

class InventoryMovementController : public BaseController
{
public:
    InventoryMovementController();
    virtual ~InventoryMovementController() = default;
    
    void handleRequest(Poco::Net::HTTPServerRequest& request, 
                      Poco::Net::HTTPServerResponse& response) override;
    
    void handleGetMovements(Poco::Net::HTTPServerRequest& request, 
                           Poco::Net::HTTPServerResponse& response);
    
    void handleGetMovementById(Poco::Net::HTTPServerRequest& request, 
                              Poco::Net::HTTPServerResponse& response);
    
    void handleCreateReceiptMovement(Poco::Net::HTTPServerRequest& request, 
                                    Poco::Net::HTTPServerResponse& response);
    
    void handleCreateTransferMovement(Poco::Net::HTTPServerRequest& request, 
                                     Poco::Net::HTTPServerResponse& response);
    
    void handleCreateAdjustmentMovement(Poco::Net::HTTPServerRequest& request, 
                                       Poco::Net::HTTPServerResponse& response);
    
    void handleUpdateMovementStatus(Poco::Net::HTTPServerRequest& request, 
                                   Poco::Net::HTTPServerResponse& response);
    
    void handleCancelMovement(Poco::Net::HTTPServerRequest& request, 
                            Poco::Net::HTTPServerResponse& response);
    
    void handleGetProductMovements(Poco::Net::HTTPServerRequest& request, 
                                  Poco::Net::HTTPServerResponse& response);
    
    void handleGetCellMovements(Poco::Net::HTTPServerRequest& request, 
                               Poco::Net::HTTPServerResponse& response);
    
    void handleGetMovementStatistics(Poco::Net::HTTPServerRequest& request, 
                                   Poco::Net::HTTPServerResponse& response);
    
    void handleGetMovementReport(Poco::Net::HTTPServerRequest& request, 
                               Poco::Net::HTTPServerResponse& response);
    
    void handleCheckStockAvailability(Poco::Net::HTTPServerRequest& request, 
                                     Poco::Net::HTTPServerResponse& response);
    
    void handleGetExpiringProducts(Poco::Net::HTTPServerRequest& request, 
                                  Poco::Net::HTTPServerResponse& response);
    
    void handleGetLowStockProducts(Poco::Net::HTTPServerRequest& request, 
                                  Poco::Net::HTTPServerResponse& response);
    
    void handleGetInventoryStatistics(Poco::Net::HTTPServerRequest& request, 
                                     Poco::Net::HTTPServerResponse& response);
    
    bool validateRequest(Poco::Net::HTTPServerRequest& request, 
                        Poco::Net::HTTPServerResponse& response,
                        std::string& errorMessage) override;
    
    bool authorizeRequest(Poco::Net::HTTPServerRequest& request, 
                         Poco::Net::HTTPServerResponse& response,
                         std::string& errorMessage) override;
    
    long long getCurrentUserId(Poco::Net::HTTPServerRequest& request);
    database::models::UserRole getCurrentUserRole(Poco::Net::HTTPServerRequest& request);
    
    bool canViewMovements(Poco::Net::HTTPServerRequest& request, 
                         std::string& errorMessage);
    
    bool canCreateMovement(Poco::Net::HTTPServerRequest& request, 
                          const std::string& movementType,
                          std::string& errorMessage);
    
    bool canUpdateMovement(Poco::Net::HTTPServerRequest& request, 
                          long long movementId,
                          const std::string& newStatus,
                          std::string& errorMessage);
    
    bool canCancelMovement(Poco::Net::HTTPServerRequest& request, 
                          long long movementId,
                          std::string& errorMessage);
    
private:
    std::unique_ptr<services::InventoryService> inventoryService;
    std::unique_ptr<services::AuthService> authService;
    
    bool validateReceiptData(const Poco::JSON::Object::Ptr& json, 
                            std::vector<std::string>& errors);
    
    bool validateTransferData(const Poco::JSON::Object::Ptr& json, 
                             std::vector<std::string>& errors);
    
    bool validateAdjustmentData(const Poco::JSON::Object::Ptr& json, 
                               std::vector<std::string>& errors);
    
    bool validateStatusUpdateData(const Poco::JSON::Object::Ptr& json, 
                                 std::vector<std::string>& errors);
    
    bool validateSearchParameters(const std::map<std::string, std::string>& filters,
                                 std::vector<std::string>& errors);
    
    std::unique_ptr<database::models::InventoryMovement> extractMovementFromJson(
        const Poco::JSON::Object::Ptr& json);
    
    void logMovementEvent(long long userId, 
                         const std::string& action,
                         long long movementId,
                         const std::string& ipAddress,
                         const std::string& userAgent,
                         bool success,
                         const std::string& details = "");
};

} // namespace controllers
