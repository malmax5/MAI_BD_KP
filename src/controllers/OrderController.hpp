#pragma once

#include "BaseController.hpp"
#include "../services/OrderService.hpp"
#include "../services/AuthService.hpp"
#include <memory>

namespace warehouse_backend::controllers
{

class OrderController : public BaseController
{
public:
    OrderController();
    virtual ~OrderController() = default;
    
    void handleRequest(Poco::Net::HTTPServerRequest& request, 
                      Poco::Net::HTTPServerResponse& response) override;
    
    void handleGetOrders(Poco::Net::HTTPServerRequest& request, 
                        Poco::Net::HTTPServerResponse& response);
    
    void handleGetOrderById(Poco::Net::HTTPServerRequest& request, 
                           Poco::Net::HTTPServerResponse& response);
    
    void handleCreateOrder(Poco::Net::HTTPServerRequest& request, 
                          Poco::Net::HTTPServerResponse& response);
    
    void handleUpdateOrder(Poco::Net::HTTPServerRequest& request, 
                          Poco::Net::HTTPServerResponse& response);
    
    void handleDeleteOrder(Poco::Net::HTTPServerRequest& request, 
                          Poco::Net::HTTPServerResponse& response);
    
    void handleCancelOrder(Poco::Net::HTTPServerRequest& request, 
                          Poco::Net::HTTPServerResponse& response);
    
    void handleUpdateOrderStatus(Poco::Net::HTTPServerRequest& request, 
                                Poco::Net::HTTPServerResponse& response);
    
    void handleUpdateOrderPriority(Poco::Net::HTTPServerRequest& request, 
                                  Poco::Net::HTTPServerResponse& response);
    
    void handleGetOrderItems(Poco::Net::HTTPServerRequest& request, 
                            Poco::Net::HTTPServerResponse& response);
    
    void handleAddOrderItem(Poco::Net::HTTPServerRequest& request, 
                           Poco::Net::HTTPServerResponse& response);
    
    void handleUpdateOrderItem(Poco::Net::HTTPServerRequest& request, 
                              Poco::Net::HTTPServerResponse& response);
    
    void handleRemoveOrderItem(Poco::Net::HTTPServerRequest& request, 
                              Poco::Net::HTTPServerResponse& response);
    
    void handleSearchOrders(Poco::Net::HTTPServerRequest& request, 
                           Poco::Net::HTTPServerResponse& response);
    
    void handleGetOrdersByCustomer(Poco::Net::HTTPServerRequest& request, 
                                  Poco::Net::HTTPServerResponse& response);
    
    void handleGetOrdersByStatus(Poco::Net::HTTPServerRequest& request, 
                                Poco::Net::HTTPServerResponse& response);
    
    void handleGetPendingOrders(Poco::Net::HTTPServerRequest& request, 
                               Poco::Net::HTTPServerResponse& response);
    
    void handleGetUrgentOrders(Poco::Net::HTTPServerRequest& request, 
                              Poco::Net::HTTPServerResponse& response);
    
    void handleGetOrderStatistics(Poco::Net::HTTPServerRequest& request, 
                                 Poco::Net::HTTPServerResponse& response);
    
    void handleGetRevenueReport(Poco::Net::HTTPServerRequest& request, 
                               Poco::Net::HTTPServerResponse& response);
    
    void handleGetCustomerOrderHistory(Poco::Net::HTTPServerRequest& request, 
                                      Poco::Net::HTTPServerResponse& response);
    
    void handleMarkOrderAsShipped(Poco::Net::HTTPServerRequest& request, 
                                 Poco::Net::HTTPServerResponse& response);
    
    void handleMarkOrderAsDelivered(Poco::Net::HTTPServerRequest& request, 
                                   Poco::Net::HTTPServerResponse& response);
    
    bool validateRequest(Poco::Net::HTTPServerRequest& request, 
                        Poco::Net::HTTPServerResponse& response,
                        std::string& errorMessage) override;
    
    bool authorizeRequest(Poco::Net::HTTPServerRequest& request, 
                         Poco::Net::HTTPServerResponse& response,
                         std::string& errorMessage) override;
    
    bool validateOrderAccess(Poco::Net::HTTPServerRequest& request, 
                            long long orderId,
                            std::string& errorMessage);
    
    long long getCurrentUserId(Poco::Net::HTTPServerRequest& request);
    database::models::UserRole getCurrentUserRole(Poco::Net::HTTPServerRequest& request);
    
private:
    std::unique_ptr<services::OrderService> orderService;
    std::unique_ptr<services::AuthService> authService;
    
    bool validateCreateOrderData(const Poco::JSON::Object::Ptr& json, 
                                std::vector<std::string>& errors);
    
    bool validateUpdateOrderData(const Poco::JSON::Object::Ptr& json, 
                                std::vector<std::string>& errors);
    
    bool validateUpdateStatusData(const Poco::JSON::Object::Ptr& json, 
                                 std::vector<std::string>& errors);
    
    bool validateUpdatePriorityData(const Poco::JSON::Object::Ptr& json, 
                                   std::vector<std::string>& errors);
    
    bool validateAddOrderItemData(const Poco::JSON::Object::Ptr& json, 
                                 std::vector<std::string>& errors);
    
    bool validateUpdateOrderItemData(const Poco::JSON::Object::Ptr& json, 
                                    std::vector<std::string>& errors);
    
    bool validateSearchParameters(const std::map<std::string, std::string>& filters,
                                 std::vector<std::string>& errors);
    
    void logOrderEvent(long long userId, 
                      const std::string& action,
                      long long orderId,
                      const std::string& ipAddress,
                      const std::string& userAgent,
                      bool success,
                      const std::string& details = "");
    
    std::unique_ptr<database::models::CustomerOrder> extractOrderFromJson(const Poco::JSON::Object::Ptr& json);
    std::unique_ptr<database::models::OrderItem> extractOrderItemFromJson(const Poco::JSON::Object::Ptr& json);
    
    bool canCreateOrder(Poco::Net::HTTPServerRequest& request, 
                       const database::models::CustomerOrder& orderData,
                       std::string& errorMessage);
    
    bool canUpdateOrder(Poco::Net::HTTPServerRequest& request, 
                       long long orderId,
                       const database::models::CustomerOrder& orderData,
                       std::string& errorMessage);
    
    bool canDeleteOrder(Poco::Net::HTTPServerRequest& request, 
                       long long orderId,
                       std::string& errorMessage);
    
    bool canCancelOrder(Poco::Net::HTTPServerRequest& request, 
                       long long orderId,
                       std::string& errorMessage);
    
    bool canUpdateOrderStatus(Poco::Net::HTTPServerRequest& request, 
                             long long orderId,
                             const std::string& newStatus,
                             std::string& errorMessage);
    
    bool canUpdateOrderPriority(Poco::Net::HTTPServerRequest& request, 
                               long long orderId,
                               const std::string& newPriority,
                               std::string& errorMessage);
    
    bool canAddOrderItem(Poco::Net::HTTPServerRequest& request, 
                        long long orderId,
                        std::string& errorMessage);
    
    bool canUpdateOrderItem(Poco::Net::HTTPServerRequest& request, 
                           long long orderId,
                           long long itemId,
                           std::string& errorMessage);
    
    bool canRemoveOrderItem(Poco::Net::HTTPServerRequest& request, 
                           long long orderId,
                           long long itemId,
                           std::string& errorMessage);
    
    bool canViewOrder(Poco::Net::HTTPServerRequest& request, 
                     long long orderId,
                     std::string& errorMessage);
    
    Poco::JSON::Object buildPaginationResponse(int page, int pageSize, int totalItems, 
                                              const Poco::JSON::Array& data);
};

} // namespace controllers
