#include "InventoryMovement.hpp"
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

InventoryMovement::InventoryMovement()
    : id(0),
      movementType(MovementType::TRANSFER),
      productId(0),
      batchId(0),
      fromCellId(0),
      toCellId(0),
      quantity(0),
      referenceId(0),
      status(MovementStatus::PLANNED),
      performedBy(0)
{
    
}

InventoryMovement::InventoryMovement(const Poco::JSON::Object& json)
{
    std::string typeStr = JsonUtils::getString(json, "movement_type", "transfer");
    movementType = stringToMovementType(typeStr);
    
    productId = JsonUtils::getInt(json, "product_id", 0);
    batchId = JsonUtils::getInt(json, "batch_id", 0);
    fromCellId = JsonUtils::getInt(json, "from_cell_id", 0);
    toCellId = JsonUtils::getInt(json, "to_cell_id", 0);
    quantity = JsonUtils::getInt(json, "quantity", 0);
    referenceId = JsonUtils::getInt(json, "reference_id", 0);
    referenceType = JsonUtils::getString(json, "reference_type", "");
    movementDate = JsonUtils::getString(json, "movement_date", "");
    performedBy = JsonUtils::getInt(json, "performed_by", 0);
    reason = JsonUtils::getString(json, "reason", "");
    
    std::string statusStr = JsonUtils::getString(json, "status", "planned");
    status = stringToMovementStatus(statusStr);
    
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
    
    if (json.has("from_cell_code"))
    {
        fromCellCode = JsonUtils::getString(json, "from_cell_code", "");
    }
    
    if (json.has("to_cell_code"))
    {
        toCellCode = JsonUtils::getString(json, "to_cell_code", "");
    }
    
    if (json.has("performed_by_name"))
    {
        performedByName = JsonUtils::getString(json, "performed_by_name", "");
    }
}

Poco::JSON::Object InventoryMovement::toJson() const
{
    Poco::JSON::Object json;
    
    if (id > 0)
    {
        json.set("id", id);
    }
    
    json.set("movement_type", movementTypeToString(movementType));
    json.set("product_id", productId);
    json.set("batch_id", batchId);
    
    if (fromCellId > 0)
    {
        json.set("from_cell_id", fromCellId);
    }
    
    if (toCellId > 0)
    {
        json.set("to_cell_id", toCellId);
    }
    
    json.set("quantity", quantity);
    
    if (referenceId > 0)
    {
        json.set("reference_id", referenceId);
    }
    
    if (!referenceType.empty())
    {
        json.set("reference_type", referenceType);
    }
    
    json.set("movement_date", movementDate);
    json.set("performed_by", performedBy);
    
    if (!reason.empty())
    {
        json.set("reason", reason);
    }
    
    json.set("status", movementStatusToString(status));
    
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
    
    if (!fromCellCode.empty())
    {
        json.set("from_cell_code", fromCellCode);
    }
    
    if (!toCellCode.empty())
    {
        json.set("to_cell_code", toCellCode);
    }
    
    if (!performedByName.empty())
    {
        json.set("performed_by_name", performedByName);
    }
    
    json.set("is_transfer", isTransfer());
    json.set("is_receipt", isReceipt());
    json.set("is_shipment", isShipment());
    json.set("is_adjustment", isAdjustment());
    json.set("can_be_cancelled", canBeCancelled());
    json.set("is_completed", isCompleted());
    
    return json;
}

InventoryMovement InventoryMovement::fromJson(const Poco::JSON::Object& json)
{
    return InventoryMovement(json);
}

bool InventoryMovement::validate() const
{
    if (productId <= 0 || batchId <= 0)
    {
        return false;
    }
    
    if (quantity < MIN_QUANTITY)
    {
        return false;
    }
    
    if (performedBy <= 0)
    {
        return false;
    }
    
    if (movementType != MovementType::RECEIPT && fromCellId <= 0)
    {
        return false;
    }
    
    if (movementType != MovementType::SHIPMENT && toCellId <= 0)
    {
        return false;
    }
    
    if (referenceType.length() > MAX_REFERENCE_TYPE_LENGTH)
    {
        return false;
    }
    
    if (!movementDate.empty() && !Validator::isValidDateTime(movementDate))
    {
        return false;
    }
    
    return true;
}

bool InventoryMovement::isTransfer() const
{
    return movementType == MovementType::TRANSFER;
}

bool InventoryMovement::isReceipt() const
{
    return movementType == MovementType::RECEIPT;
}

bool InventoryMovement::isShipment() const
{
    return movementType == MovementType::SHIPMENT;
}

bool InventoryMovement::isAdjustment() const
{
    return movementType == MovementType::ADJUSTMENT;
}

bool InventoryMovement::canBeCancelled() const
{
    return status == MovementStatus::PLANNED || status == MovementStatus::IN_PROGRESS;
}

bool InventoryMovement::isCompleted() const
{
    return status == MovementStatus::COMPLETED;
}

std::string InventoryMovement::movementTypeToString(MovementType type)
{
    switch (type)
    {
        case MovementType::RECEIPT: return "receipt";
        case MovementType::SHIPMENT: return "shipment";
        case MovementType::TRANSFER: return "transfer";
        case MovementType::ADJUSTMENT: return "adjustment";
        case MovementType::COUNT: return "count";
        default: return "transfer";
    }
}

MovementType InventoryMovement::stringToMovementType(const std::string& typeStr)
{
    if (typeStr == "receipt") return MovementType::RECEIPT;
    if (typeStr == "shipment") return MovementType::SHIPMENT;
    if (typeStr == "adjustment") return MovementType::ADJUSTMENT;
    if (typeStr == "count") return MovementType::COUNT;
    return MovementType::TRANSFER;
}

std::string InventoryMovement::movementStatusToString(MovementStatus status)
{
    switch (status)
    {
        case MovementStatus::PLANNED: return "planned";
        case MovementStatus::IN_PROGRESS: return "in_progress";
        case MovementStatus::COMPLETED: return "completed";
        case MovementStatus::CANCELLED: return "cancelled";
        default: return "planned";
    }
}

MovementStatus InventoryMovement::stringToMovementStatus(const std::string& statusStr)
{
    if (statusStr == "in_progress") return MovementStatus::IN_PROGRESS;
    if (statusStr == "completed") return MovementStatus::COMPLETED;
    if (statusStr == "cancelled") return MovementStatus::CANCELLED;
    return MovementStatus::PLANNED;
}

} // namespace database::models
