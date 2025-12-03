#include "CustomerOrder.hpp"
#include "OrderItem.hpp"
#include "../../utils/JsonUtils.hpp"
#include "../../utils/Validator.hpp"
#include "../../utils/DateUtils.hpp"
#include <Poco/JSON/Parser.h>
#include <Poco/JSON/Stringifier.h>
#include <Poco/JSON/Array.h>
#include <sstream>
#include <iomanip>

namespace warehouse_backend::database::models
{

using namespace warehouse_backend::utils;

CustomerOrder::CustomerOrder()
    : id(0),
      status(OrderStatus::NEW),
      totalAmount(0.0),
      priority(OrderPriority::NORMAL),
      createdBy(0)
{
    
}

CustomerOrder::CustomerOrder(const Poco::JSON::Object& json)
{
    orderNumber = JsonUtils::getString(json, "order_number", "");
    customerName = JsonUtils::getString(json, "customer_name", "");
    customerEmail = JsonUtils::getString(json, "customer_email", "");
    customerPhone = JsonUtils::getString(json, "customer_phone", "");
    shippingAddress = JsonUtils::getString(json, "shipping_address", "");
    orderDate = JsonUtils::getString(json, "order_date", "");
    
    std::string statusStr = JsonUtils::getString(json, "status", "new");
    status = stringToStatus(statusStr);
    
    totalAmount = JsonUtils::getDouble(json, "total_amount", 0.0);
    
    std::string priorityStr = JsonUtils::getString(json, "priority", "normal");
    priority = stringToPriority(priorityStr);
    
    notes = JsonUtils::getString(json, "notes", "");
    estimatedDeliveryDate = JsonUtils::getString(json, "estimated_delivery_date", "");
    actualDeliveryDate = JsonUtils::getString(json, "actual_delivery_date", "");
    createdBy = JsonUtils::getInt(json, "created_by", 0);
    
    if (json.has("id"))
    {
        id = JsonUtils::getInt(json, "id", 0);
    }
    
    if (json.has("created_by_name"))
    {
        createdByName = JsonUtils::getString(json, "created_by_name", "");
    }
}

Poco::JSON::Object CustomerOrder::toJson() const
{
    Poco::JSON::Object json;
    
    if (id > 0)
    {
        json.set("id", id);
    }
    
    json.set("order_number", orderNumber);
    json.set("customer_name", customerName);
    
    if (!customerEmail.empty())
    {
        json.set("customer_email", customerEmail);
    }
    
    if (!customerPhone.empty())
    {
        json.set("customer_phone", customerPhone);
    }
    
    json.set("shipping_address", shippingAddress);
    json.set("order_date", orderDate);
    json.set("status", statusToString(status));
    json.set("total_amount", totalAmount);
    json.set("priority", priorityToString(priority));
    
    if (!notes.empty())
    {
        json.set("notes", notes);
    }
    
    if (!estimatedDeliveryDate.empty())
    {
        json.set("estimated_delivery_date", estimatedDeliveryDate);
    }
    
    if (!actualDeliveryDate.empty())
    {
        json.set("actual_delivery_date", actualDeliveryDate);
    }
    
    json.set("created_by", createdBy);
    
    if (!createdByName.empty())
    {
        json.set("created_by_name", createdByName);
    }
    
    json.set("can_be_cancelled", canBeCancelled());
    json.set("is_complete", isComplete());
    json.set("is_shippable", isShippable());
    
    return json;
}

CustomerOrder CustomerOrder::fromJson(const Poco::JSON::Object& json)
{
    return CustomerOrder(json);
}

Poco::JSON::Object CustomerOrder::toJsonWithItems() const
{
    Poco::JSON::Object json = toJson();
    
    if (!orderItems.empty())
    {
        Poco::JSON::Array itemsArray;
        for (const auto& item : orderItems)
        {
            if (item)
            {
                itemsArray.add(item->toJson());
            }
        }
        json.set("order_items", itemsArray);
    }
    
    return json;
}

bool CustomerOrder::validate() const
{
    if (orderNumber.empty() || orderNumber.length() > 50)
    {
        return false;
    }
    
    if (customerName.empty() || customerName.length() > MAX_CUSTOMER_NAME)
    {
        return false;
    }
    
    if (!customerEmail.empty() && !Validator::isValidEmail(customerEmail))
    {
        return false;
    }
    
    if (shippingAddress.empty())
    {
        return false;
    }
    
    if (totalAmount < MIN_TOTAL_AMOUNT)
    {
        return false;
    }
    
    if (createdBy <= 0)
    {
        return false;
    }
    
    if (!estimatedDeliveryDate.empty() && !Validator::isValidDate(estimatedDeliveryDate))
    {
        return false;
    }
    
    if (!actualDeliveryDate.empty() && !Validator::isValidDate(actualDeliveryDate))
    {
        return false;
    }
    
    return true;
}

bool CustomerOrder::canBeCancelled() const
{
    return status == OrderStatus::NEW || status == OrderStatus::PROCESSING;
}

bool CustomerOrder::isComplete() const
{
    return status == OrderStatus::DELIVERED || status == OrderStatus::CANCELLED;
}

bool CustomerOrder::isShippable() const
{
    return status == OrderStatus::PICKED || status == OrderStatus::PACKED;
}

void CustomerOrder::addOrderItem(const std::shared_ptr<OrderItem>& item)
{
    if (item)
    {
        orderItems.push_back(item);
    }
}

std::string CustomerOrder::statusToString(OrderStatus status)
{
    switch (status)
    {
        case OrderStatus::NEW: return "new";
        case OrderStatus::PROCESSING: return "processing";
        case OrderStatus::PICKED: return "picked";
        case OrderStatus::PACKED: return "packed";
        case OrderStatus::SHIPPED: return "shipped";
        case OrderStatus::DELIVERED: return "delivered";
        case OrderStatus::CANCELLED: return "cancelled";
        default: return "new";
    }
}

OrderStatus CustomerOrder::stringToStatus(const std::string& statusStr)
{
    if (statusStr == "processing") return OrderStatus::PROCESSING;
    if (statusStr == "picked") return OrderStatus::PICKED;
    if (statusStr == "packed") return OrderStatus::PACKED;
    if (statusStr == "shipped") return OrderStatus::SHIPPED;
    if (statusStr == "delivered") return OrderStatus::DELIVERED;
    if (statusStr == "cancelled") return OrderStatus::CANCELLED;
    return OrderStatus::NEW;
}

std::string CustomerOrder::priorityToString(OrderPriority priority)
{
    switch (priority)
    {
        case OrderPriority::LOW: return "low";
        case OrderPriority::HIGH: return "high";
        case OrderPriority::URGENT: return "urgent";
        default: return "normal";
    }
}

OrderPriority CustomerOrder::stringToPriority(const std::string& priorityStr)
{
    if (priorityStr == "low") return OrderPriority::LOW;
    if (priorityStr == "high") return OrderPriority::HIGH;
    if (priorityStr == "urgent") return OrderPriority::URGENT;
    return OrderPriority::NORMAL;
}

} // namespace database::models
