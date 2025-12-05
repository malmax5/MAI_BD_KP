#include "OrderItem.hpp"
#include "../../utils/JsonUtils.hpp"
#include "../../utils/Validator.hpp"
#include <Poco/JSON/Parser.h>
#include <Poco/JSON/Stringifier.h>
#include <sstream>
#include <iomanip>
#include <cmath>

namespace warehouse_backend::database::models
{

using namespace warehouse_backend::utils;

OrderItem::OrderItem()
    : id(0),
      orderId(0),
      productId(0),
      batchId(0),
      quantityOrdered(0),
      quantityShipped(0),
      unitPrice(0.0),
      discountPercent(0.0),
      lineTotal(0.0),
      pickingStatus(PickingStatus::NOT_STARTED),
      pickedBy(0)
{
    
}

OrderItem::OrderItem(const Poco::JSON::Object& json)
{
    orderId = JsonUtils::getInt(json, "order_id", 0);
    productId = JsonUtils::getInt(json, "product_id", 0);
    batchId = JsonUtils::getInt(json, "batch_id", 0);
    quantityOrdered = JsonUtils::getInt(json, "quantity_ordered", 0);
    quantityShipped = JsonUtils::getInt(json, "quantity_shipped", 0);
    unitPrice = JsonUtils::getDouble(json, "unit_price", 0.0);
    discountPercent = JsonUtils::getDouble(json, "discount_percent", 0.0);
    
    if (json.has("line_total"))
    {
        lineTotal = JsonUtils::getDouble(json, "line_total", 0.0);
    }
    else
    {
        calculateLineTotal();
    }
    
    std::string statusStr = JsonUtils::getString(json, "picking_status", "not_started");
    pickingStatus = stringToPickingStatus(statusStr);
    
    pickedBy = JsonUtils::getInt(json, "picked_by", 0);
    pickedAt = JsonUtils::getString(json, "picked_at", "");
    
    if (json.has("id"))
    {
        id = JsonUtils::getInt(json, "id", 0);
    }
    
    if (json.has("product_name"))
    {
        productName = JsonUtils::getString(json, "product_name", "");
    }
    
    if (json.has("product_sku"))
    {
        productSku = JsonUtils::getString(json, "product_sku", "");
    }
    
    if (json.has("batch_number"))
    {
        batchNumber = JsonUtils::getString(json, "batch_number", "");
    }
    
    if (json.has("picked_by_name"))
    {
        pickedByName = JsonUtils::getString(json, "picked_by_name", "");
    }
}

Poco::JSON::Object OrderItem::toJson() const
{
    Poco::JSON::Object json;
    
    if (id > 0)
    {
        json.set("id", id);
    }
    
    json.set("order_id", orderId);
    json.set("product_id", productId);
    json.set("batch_id", batchId);
    json.set("quantity_ordered", quantityOrdered);
    json.set("quantity_shipped", quantityShipped);
    json.set("unit_price", unitPrice);
    json.set("discount_percent", discountPercent);
    json.set("line_total", lineTotal);
    json.set("picking_status", pickingStatusToString(pickingStatus));
    
    if (pickedBy > 0)
    {
        json.set("picked_by", pickedBy);
    }
    
    if (!pickedAt.empty())
    {
        json.set("picked_at", pickedAt);
    }
    
    if (!productName.empty())
    {
        json.set("product_name", productName);
    }
    
    if (!productSku.empty())
    {
        json.set("product_sku", productSku);
    }
    
    if (!batchNumber.empty())
    {
        json.set("batch_number", batchNumber);
    }
    
    if (!pickedByName.empty())
    {
        json.set("picked_by_name", pickedByName);
    }
    
    json.set("discounted_price", getDiscountedPrice());
    json.set("is_fully_shipped", isFullyShipped());
    json.set("remaining_to_ship", getRemainingToShip());
    json.set("can_be_picked", canBePicked());
    
    return json;
}

OrderItem OrderItem::fromJson(const Poco::JSON::Object& json)
{
    return OrderItem(json);
}

bool OrderItem::validate() const
{
    if (orderId <= 0 || productId <= 0 || batchId <= 0)
    {
        return false;
    }
    
    if (quantityOrdered < MIN_QUANTITY)
    {
        return false;
    }
    
    if (quantityShipped < 0 || quantityShipped > quantityOrdered)
    {
        return false;
    }
    
    if (unitPrice < MIN_UNIT_PRICE)
    {
        return false;
    }
    
    if (discountPercent < MIN_DISCOUNT || discountPercent > MAX_DISCOUNT)
    {
        return false;
    }
    
    return true;
}

void OrderItem::calculateLineTotal()
{
    double discountedPrice = unitPrice * (1.0 - discountPercent / 100.0);
    lineTotal = discountedPrice * quantityOrdered;
}

double OrderItem::calculateLineTotal() const
{
    return unitPrice * (1.0 - discountPercent / 100.0) * quantityOrdered;
}

bool OrderItem::canBePicked() const
{
    return pickingStatus == PickingStatus::NOT_STARTED || 
           pickingStatus == PickingStatus::IN_PROGRESS;
}

bool OrderItem::isFullyShipped() const
{
    return quantityShipped == quantityOrdered;
}

double OrderItem::getDiscountedPrice() const
{
    return unitPrice * (1.0 - discountPercent / 100.0);
}

int OrderItem::getRemainingToShip() const
{
    return quantityOrdered - quantityShipped;
}

std::string OrderItem::pickingStatusToString(PickingStatus status)
{
    switch (status)
    {
        case PickingStatus::NOT_STARTED: return "not_started";
        case PickingStatus::IN_PROGRESS: return "in_progress";
        case PickingStatus::PICKED: return "picked";
        case PickingStatus::PACKED: return "packed";
        default: return "not_started";
    }
}

PickingStatus OrderItem::stringToPickingStatus(const std::string& statusStr)
{
    if (statusStr == "in_progress") return PickingStatus::IN_PROGRESS;
    if (statusStr == "picked") return PickingStatus::PICKED;
    if (statusStr == "packed") return PickingStatus::PACKED;
    return PickingStatus::NOT_STARTED;
}

} // namespace database::models
