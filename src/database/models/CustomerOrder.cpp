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
    shippingAddress = JsonUtils::getString(json, "shipping_address", "");
    orderDate = JsonUtils::getString(json, "order_date", "");

    std::string statusStr = JsonUtils::getString(json, "status", "new");
    status = stringToStatus(statusStr);

    totalAmount = JsonUtils::getDouble(json, "total_amount", 0.0);

    std::string priorityStr = JsonUtils::getString(json, "priority", "normal");
    priority = stringToPriority(priorityStr);

    createdBy = static_cast<Poco::Int64>(JsonUtils::getInt(json, "created_by", 0));

    createdByName = JsonUtils::getString(json, "created_by_name", "");

    if (json.has("customer_email") && !json.isNull("customer_email"))
        customerEmail = JsonUtils::getString(json, "customer_email", "");

    if (json.has("customer_phone") && !json.isNull("customer_phone"))
        customerPhone = JsonUtils::getString(json, "customer_phone", "");

    if (json.has("notes") && !json.isNull("notes"))
        notes = JsonUtils::getString(json, "notes", "");

    if (json.has("estimated_delivery_date") && !json.isNull("estimated_delivery_date"))
        estimatedDeliveryDate = JsonUtils::getString(json, "estimated_delivery_date", "");

    if (json.has("actual_delivery_date") && !json.isNull("actual_delivery_date"))
        actualDeliveryDate = JsonUtils::getString(json, "actual_delivery_date", "");

    if (json.has("id") && !json.isNull("id"))
        id = static_cast<Poco::Int64>(JsonUtils::getInt(json, "id", 0));
    else
        id = 0;
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
    
    if (!customerEmail.isNull())
    {
        json.set("customer_email", customerEmail.value());
    }
    
    if (!customerPhone.isNull())
    {
        json.set("customer_phone", customerPhone.value());
    }
    
    json.set("shipping_address", shippingAddress);
    json.set("order_date", orderDate);
    json.set("status", statusToString(status));
    json.set("total_amount", totalAmount);
    json.set("priority", priorityToString(priority));
    
    if (!notes.isNull())
    {
        json.set("notes", notes.value());
    }
    
    if (!estimatedDeliveryDate.isNull())
    {
        json.set("estimated_delivery_date", estimatedDeliveryDate.value());
    }
    
    if (!actualDeliveryDate.isNull())
    {
        json.set("actual_delivery_date", actualDeliveryDate.value());
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
    
    if (!customerEmail.isNull() && !Validator::isValidEmail(customerEmail.value()))
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
    
    if (!estimatedDeliveryDate.isNull() && !Validator::isValidDate(estimatedDeliveryDate.value()))
    {
        return false;
    }
    
    if (!actualDeliveryDate.isNull() && !Validator::isValidDate(actualDeliveryDate.value()))
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
