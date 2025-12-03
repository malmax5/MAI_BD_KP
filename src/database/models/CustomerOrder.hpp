#pragma once

#include <string>
#include <vector>
#include <memory>
#include <Poco/JSON/Object.h>

namespace warehouse_backend::database::models
{

enum class OrderStatus
{
    NEW,
    PROCESSING,
    PICKED,
    PACKED,
    SHIPPED,
    DELIVERED,
    CANCELLED
};

enum class OrderPriority
{
    LOW,
    NORMAL,
    HIGH,
    URGENT
};

class CustomerOrder
{
public:
    long long id;
    std::string orderNumber;
    std::string customerName;
    std::string customerEmail;
    std::string customerPhone;
    std::string shippingAddress;
    std::string orderDate;
    OrderStatus status;
    double totalAmount;
    OrderPriority priority;
    std::string notes;
    std::string estimatedDeliveryDate;
    std::string actualDeliveryDate;
    long long createdBy;

    std::string createdByName;
    std::vector<std::shared_ptr<class OrderItem>> orderItems;

    CustomerOrder();
    explicit CustomerOrder(const Poco::JSON::Object& json);

    Poco::JSON::Object toJson() const;
    static CustomerOrder fromJson(const Poco::JSON::Object& json);
    Poco::JSON::Object toJsonWithItems() const;
    
    bool validate() const;
    
    bool canBeCancelled() const;
    bool isComplete() const;
    bool isShippable() const;
    void addOrderItem(const std::shared_ptr<class OrderItem>& item);
    
    static std::string statusToString(OrderStatus status);
    static OrderStatus stringToStatus(const std::string& statusStr);
    
    static std::string priorityToString(OrderPriority priority);
    static OrderPriority stringToPriority(const std::string& priorityStr);
    
    static constexpr int MAX_CUSTOMER_NAME = 150;
    static constexpr double MIN_TOTAL_AMOUNT = 0.0;
};

} // namespace database::models