#pragma once

#include "BaseController.hpp"
#include "../services/ShipmentService.hpp"
#include "../services/AuthService.hpp"
#include <memory>

namespace warehouse_backend::controllers
{

class ShipmentController : public BaseController
{
public:
    ShipmentController();
    virtual ~ShipmentController() = default;
    
    void handleRequest(Poco::Net::HTTPServerRequest& request, 
                      Poco::Net::HTTPServerResponse& response) override;
    
    void handleGetShipments(Poco::Net::HTTPServerRequest& request, 
                           Poco::Net::HTTPServerResponse& response);
    
    void handleGetShipmentById(Poco::Net::HTTPServerRequest& request, 
                              Poco::Net::HTTPServerResponse& response);
    
    void handleCreateShipment(Poco::Net::HTTPServerRequest& request, 
                             Poco::Net::HTTPServerResponse& response);
    
    void handleUpdateShipment(Poco::Net::HTTPServerRequest& request, 
                             Poco::Net::HTTPServerResponse& response);
    
    void handleUpdateShipmentStatus(Poco::Net::HTTPServerRequest& request, 
                                   Poco::Net::HTTPServerResponse& response);
    
    void handleUpdateTrackingInfo(Poco::Net::HTTPServerRequest& request, 
                                 Poco::Net::HTTPServerResponse& response);
    
    void handleCancelShipment(Poco::Net::HTTPServerRequest& request, 
                             Poco::Net::HTTPServerResponse& response);
    
    void handleMarkAsDelivered(Poco::Net::HTTPServerRequest& request, 
                              Poco::Net::HTTPServerResponse& response);
    
    void handleGetShipmentsByOrder(Poco::Net::HTTPServerRequest& request, 
                                  Poco::Net::HTTPServerResponse& response);
    
    void handleGetShipmentsByStatus(Poco::Net::HTTPServerRequest& request, 
                                   Poco::Net::HTTPServerResponse& response);
    
    void handleGetShipmentsByCarrier(Poco::Net::HTTPServerRequest& request, 
                                    Poco::Net::HTTPServerResponse& response);
    
    void handleGetShipmentStatistics(Poco::Net::HTTPServerRequest& request, 
                                    Poco::Net::HTTPServerResponse& response);
    
    void handleGetCarrierPerformanceReport(Poco::Net::HTTPServerRequest& request, 
                                          Poco::Net::HTTPServerResponse& response);
    
    void handleGetShippingCostAnalysis(Poco::Net::HTTPServerRequest& request, 
                                      Poco::Net::HTTPServerResponse& response);
    
    void handleGetDelayedShipments(Poco::Net::HTTPServerRequest& request, 
                                  Poco::Net::HTTPServerResponse& response);
    
    void handleGetShipmentsDueToday(Poco::Net::HTTPServerRequest& request, 
                                   Poco::Net::HTTPServerResponse& response);
    
    bool validateRequest(Poco::Net::HTTPServerRequest& request, 
                        Poco::Net::HTTPServerResponse& response,
                        std::string& errorMessage) override;
    
    bool authorizeRequest(Poco::Net::HTTPServerRequest& request, 
                         Poco::Net::HTTPServerResponse& response,
                         std::string& errorMessage) override;
    
    long long getCurrentUserId(Poco::Net::HTTPServerRequest& request);
    database::models::UserRole getCurrentUserRole(Poco::Net::HTTPServerRequest& request);
    
    bool canViewShipments(Poco::Net::HTTPServerRequest& request, 
                         std::string& errorMessage);
    
    bool canCreateShipment(Poco::Net::HTTPServerRequest& request, 
                          long long orderId,
                          std::string& errorMessage);
    
    bool canUpdateShipment(Poco::Net::HTTPServerRequest& request, 
                          long long shipmentId,
                          std::string& errorMessage);
    
    bool canCancelShipment(Poco::Net::HTTPServerRequest& request, 
                          long long shipmentId,
                          std::string& errorMessage);
    
private:
    std::unique_ptr<services::ShipmentService> shipmentService;
    std::unique_ptr<services::AuthService> authService;
    
    bool validateCreateShipmentData(const Poco::JSON::Object::Ptr& json, 
                                  std::vector<std::string>& errors);
    
    bool validateUpdateShipmentData(const Poco::JSON::Object::Ptr& json, 
                                  std::vector<std::string>& errors);
    
    bool validateStatusUpdateData(const Poco::JSON::Object::Ptr& json, 
                                 std::vector<std::string>& errors);
    
    bool validateTrackingUpdateData(const Poco::JSON::Object::Ptr& json, 
                                   std::vector<std::string>& errors);
    
    bool validateSearchParameters(const std::map<std::string, std::string>& filters,
                                 std::vector<std::string>& errors);
    
    std::unique_ptr<database::models::Shipment> extractShipmentFromJson(
        const Poco::JSON::Object::Ptr& json);
    
    void logShipmentEvent(long long userId, 
                         const std::string& action,
                         long long shipmentId,
                         const std::string& ipAddress,
                         const std::string& userAgent,
                         bool success,
                         const std::string& details = "");
};

} // namespace controllers
