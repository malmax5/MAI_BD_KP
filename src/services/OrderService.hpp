#pragma once

#include "../database/repositories/CustomerOrderRepository.hpp"
#include "../database/repositories/OrderItemRepository.hpp"
#include "../database/repositories/OrderPaymentRepository.hpp"
#include "../database/repositories/ProductRepository.hpp"
#include "../database/repositories/ProductBatchRepository.hpp"
#include "../database/repositories/AuditLogRepository.hpp"
#include "../config/ConfigManager.hpp"
#include "../utils/DateUtils.hpp"
#include <memory>
#include <string>
#include <vector>
#include <Poco/JSON/Object.h>
#include <Poco/JSON/Array.h>

namespace warehouse_backend::services
{

struct OrderCreationResult
{
    bool success;
    std::string message;
    long long orderId;
    std::string orderNumber;
    std::unique_ptr<database::models::CustomerOrder> order;
    Poco::JSON::Object orderDetails;
    
    OrderCreationResult();
    Poco::JSON::Object toJson() const;
};

struct OrderUpdateResult
{
    bool success;
    std::string message;
    std::unique_ptr<database::models::CustomerOrder> updatedOrder;
    std::vector<std::string> changes;
    
    Poco::JSON::Object toJson() const;
};

class OrderService
{
public:
    OrderService();
    ~OrderService() = default;
    
    OrderCreationResult createOrder(const database::models::CustomerOrder& order,
                                   const std::vector<database::models::OrderItem>& items,
                                   long long createdBy = 0);
    
    OrderUpdateResult updateOrderStatus(long long orderId,
                                       const std::string& newStatus,
                                       long long updatedBy = 0,
                                       const std::string& notes = "");
    
    OrderUpdateResult updateOrderPriority(long long orderId,
                                         const std::string& newPriority,
                                         long long updatedBy = 0);
    
    OrderUpdateResult addOrderItem(long long orderId,
                                  const database::models::OrderItem& item,
                                  long long addedBy = 0);
    
    OrderUpdateResult updateOrderItem(long long orderId,
                                     long long itemId,
                                     const database::models::OrderItem& updatedItem,
                                     long long updatedBy = 0);
    
    OrderUpdateResult removeOrderItem(long long orderId,
                                     long long itemId,
                                     long long removedBy = 0);
    
    Poco::JSON::Object getOrderDetails(long long orderId);
    Poco::JSON::Object getOrderWithItems(long long orderId);
    Poco::JSON::Object getOrderWithItemsAndShipments(long long orderId);
    
    Poco::JSON::Array findOrdersByCustomer(const std::string& customerEmail,
                                          int page = 1,
                                          int pageSize = 20);
    
    Poco::JSON::Array findOrdersByStatus(const std::string& status,
                                        int page = 1,
                                        int pageSize = 20);
    
    Poco::JSON::Array findOrdersByDateRange(const std::string& startDate,
                                           const std::string& endDate,
                                           int page = 1,
                                           int pageSize = 20);
    
    Poco::JSON::Array getPendingOrders(int page = 1, int pageSize = 20);
    Poco::JSON::Array getUrgentOrders(int page = 1, int pageSize = 20);
    
    bool cancelOrder(long long orderId, long long cancelledBy = 0, const std::string& reason = "");
    bool markOrderAsShipped(long long orderId, long long shippedBy = 0);
    bool markOrderAsDelivered(long long orderId, const std::string& actualDeliveryDate = "");
    
    Poco::JSON::Object getOrderStatistics();
    Poco::JSON::Array getRevenueReport(const std::string& startDate,
                                      const std::string& endDate);
    Poco::JSON::Array getCustomerOrderHistory(const std::string& customerEmail);
    
    double calculateOrderTotal(const std::vector<database::models::OrderItem>& items);
    bool validateOrderItems(const std::vector<database::models::OrderItem>& items,
                           std::string& errorMessage);
    
private:
    std::unique_ptr<database::repositories::CustomerOrderRepository> orderRepository;
    std::unique_ptr<database::repositories::OrderItemRepository> itemRepository;
    std::unique_ptr<database::repositories::OrderPaymentRepository> paymentRepository;
    std::unique_ptr<database::repositories::ProductRepository> productRepository;
    std::unique_ptr<database::repositories::ProductBatchRepository> batchRepository;
    std::unique_ptr<database::repositories::AuditLogRepository> auditRepository;
    
    std::string generateOrderNumber();
    void updateOrderTotal(long long orderId);
    void logOrderAudit(const std::string& action,
                      long long orderId,
                      long long changedBy,
                      const std::string& details = "");
};

} // namespace services