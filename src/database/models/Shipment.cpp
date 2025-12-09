#include "Shipment.hpp"
#include "../../utils/JsonUtils.hpp"
#include "../../utils/Validator.hpp"
#include "../../utils/DateUtils.hpp"
#include <Poco/JSON/Parser.h>
#include <Poco/JSON/Stringifier.h>
#include <sstream>
#include <iomanip>

namespace warehouse_backend::database::models
{

using namespace warehouse_backend::utils;

Shipment::Shipment()
    : id(0),
      orderId(0),
      shippingCost(0.0),
      status(ShipmentStatus::PREPARING),
      weightTotal(0.0)
{
    
}

Shipment::Shipment(const Poco::JSON::Object& json)
{
    shipmentNumber = JsonUtils::getString(json, "shipment_number", "");
    orderId = JsonUtils::getInt(json, "order_id", 0);
    carrier = JsonUtils::getString(json, "carrier", "");
    trackingNumber = JsonUtils::getString(json, "tracking_number", "");
    shippingMethod = JsonUtils::getString(json, "shipping_method", "");
    shippingCost = JsonUtils::getDouble(json, "shipping_cost", 0.0);
    shipmentDate = JsonUtils::getString(json, "shipment_date", "");
    estimatedArrival = JsonUtils::getString(json, "estimated_arrival", "");
    actualArrival = JsonUtils::getString(json, "actual_arrival", "");
    
    std::string statusStr = JsonUtils::getString(json, "status", "preparing");
    status = stringToStatus(statusStr);
    
    notes = JsonUtils::getString(json, "notes", "");
    weightTotal = JsonUtils::getDouble(json, "weight_total", 0.0);
    dimensionsTotal = JsonUtils::getString(json, "dimensions_total", "");
    
    if (json.has("id"))
    {
        id = JsonUtils::getInt(json, "id", 0);
    }
    
    if (json.has("order_number"))
    {
        orderNumber = JsonUtils::getString(json, "order_number", "");
    }
    
    if (json.has("customer_name"))
    {
        customerName = JsonUtils::getString(json, "customer_name", "");
    }
}

Poco::JSON::Object Shipment::toJson() const
{
    Poco::JSON::Object json;
    
    if (id > 0)
    {
        json.set("id", id);
    }
    
    json.set("shipment_number", shipmentNumber);
    json.set("order_id", orderId);
    json.set("carrier", carrier);
    
    if (!trackingNumber.isNull())
    {
        json.set("tracking_number", trackingNumber);
    }
    
    if (!shippingMethod.isNull())
    {
        json.set("shipping_method", shippingMethod);
    }
    
    json.set("shipping_cost", shippingCost);
    json.set("shipment_date", shipmentDate);
    
    if (!estimatedArrival.isNull())
    {
        json.set("estimated_arrival", estimatedArrival);
    }
    
    if (!actualArrival.isNull())
    {
        json.set("actual_arrival", actualArrival);
    }
    
    json.set("status", statusToString(status));
    
    if (!notes.isNull())
    {
        json.set("notes", notes);
    }
    
    json.set("weight_total", weightTotal);
    
    if (!dimensionsTotal.isNull())
    {
        json.set("dimensions_total", dimensionsTotal);
    }
    
    if (!orderNumber.empty())
    {
        json.set("order_number", orderNumber);
    }
    
    if (!customerName.empty())
    {
        json.set("customer_name", customerName);
    }
    
    json.set("is_delivered", isDelivered());
    json.set("is_in_transit", isInTransit());
    json.set("is_delayed", isDelayed());
    json.set("days_in_transit", daysInTransit());
    json.set("needs_tracking_update", needsTrackingUpdate());
    
    return json;
}

Shipment Shipment::fromJson(const Poco::JSON::Object& json)
{
    return Shipment(json);
}

bool Shipment::validate() const
{
    if (shipmentNumber.empty() || shipmentNumber.length() > 50)
    {
        return false;
    }
    
    if (orderId <= 0)
    {
        return false;
    }
    
    if (carrier.empty() || carrier.length() > MAX_CARRIER_LENGTH)
    {
        return false;
    }
    
    if (shippingCost.value() < MIN_SHIPPING_COST)
    {
        return false;
    }
    
    if (weightTotal.value() < MIN_WEIGHT)
    {
        return false;
    }
    
    if (!shipmentDate.empty() && !Validator::isValidDateTime(shipmentDate))
    {
        return false;
    }
    
    if (!estimatedArrival.isNull() && !Validator::isValidDate(estimatedArrival.value()))
    {
        return false;
    }
    
    if (!actualArrival.isNull() && !Validator::isValidDate(actualArrival.value()))
    {
        return false;
    }
    
    return true;
}

bool Shipment::isDelivered() const
{
    return status == ShipmentStatus::DELIVERED;
}

bool Shipment::isInTransit() const
{
    return status == ShipmentStatus::IN_TRANSIT;
}

bool Shipment::isDelayed() const
{
    return status == ShipmentStatus::DELAYED;
}

int Shipment::daysInTransit() const
{
    if (!isInTransit() || shipmentDate.empty())
    {
        return 0;
    }
    
    try
    {
        Poco::DateTime shipment = DateUtils::parseDateTime(shipmentDate);
        Poco::DateTime now = DateUtils::now();
        return DateUtils::daysBetween(shipment, now);
    }
    catch (...)
    {
        return 0;
    }
}

bool Shipment::needsTrackingUpdate() const
{
    if (isDelivered() || status == ShipmentStatus::RETURNED)
    {
        return false;
    }
    
    if (shipmentDate.empty())
    {
        return false;
    }
    
    try
    {
        Poco::DateTime shipment = DateUtils::parseDateTime(shipmentDate);
        Poco::DateTime now = DateUtils::now();
        int days = DateUtils::daysBetween(shipment, now);
        return days > 3 && isInTransit();
    }
    catch (...)
    {
        return false;
    }
}

std::string Shipment::statusToString(ShipmentStatus status)
{
    switch (status)
    {
        case ShipmentStatus::PREPARING: return "preparing";
        case ShipmentStatus::IN_TRANSIT: return "in_transit";
        case ShipmentStatus::DELIVERED: return "delivered";
        case ShipmentStatus::DELAYED: return "delayed";
        case ShipmentStatus::RETURNED: return "returned";
        default: return "preparing";
    }
}

ShipmentStatus Shipment::stringToStatus(const std::string& statusStr)
{
    if (statusStr == "in_transit") return ShipmentStatus::IN_TRANSIT;
    if (statusStr == "delivered") return ShipmentStatus::DELIVERED;
    if (statusStr == "delayed") return ShipmentStatus::DELAYED;
    if (statusStr == "returned") return ShipmentStatus::RETURNED;
    return ShipmentStatus::PREPARING;
}

} // namespace database::models
