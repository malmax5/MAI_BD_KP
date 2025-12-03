#include "ProductBatch.hpp"
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

ProductBatch::ProductBatch()
    : id(0),
      productId(0),
      supplierId(0),
      quantityReceived(0),
      quantityAvailable(0),
      unitCost(0.0),
      storageCellId(0),
      qualityStatus(QualityStatus::PENDING)
{

}

ProductBatch::ProductBatch(const Poco::JSON::Object& json)
{
    batchNumber = JsonUtils::getString(json, "batch_number", "");
    productId = JsonUtils::getInt(json, "product_id", 0);
    supplierId = JsonUtils::getInt(json, "supplier_id", 0);
    quantityReceived = JsonUtils::getInt(json, "quantity_received", 0);
    quantityAvailable = JsonUtils::getInt(json, "quantity_available", 0);
    unitCost = JsonUtils::getDouble(json, "unit_cost", 0.0);
    manufactureDate = JsonUtils::getString(json, "manufacture_date", "");
    expirationDate = JsonUtils::getString(json, "expiration_date", "");
    arrivalDate = JsonUtils::getString(json, "arrival_date", "");
    storageCellId = JsonUtils::getInt(json, "storage_cell_id", 0);
    
    std::string statusStr = JsonUtils::getString(json, "quality_status", "pending");
    qualityStatus = stringToQualityStatus(statusStr);
    
    invoiceNumber = JsonUtils::getString(json, "invoice_number", "");
    
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
    
    if (json.has("supplier_name"))
    {
        supplierName = JsonUtils::getString(json, "supplier_name", "");
    }
    
    if (json.has("storage_cell_code"))
    {
        storageCellCode = JsonUtils::getString(json, "storage_cell_code", "");
    }
}

Poco::JSON::Object ProductBatch::toJson() const
{
    Poco::JSON::Object json;
    
    if (id > 0)
    {
        json.set("id", id);
    }
    
    json.set("batch_number", batchNumber);
    json.set("product_id", productId);
    json.set("supplier_id", supplierId);
    json.set("quantity_received", quantityReceived);
    json.set("quantity_available", quantityAvailable);
    json.set("unit_cost", unitCost);
    
    if (!manufactureDate.empty())
    {
        json.set("manufacture_date", manufactureDate);
    }
    
    if (!expirationDate.empty())
    {
        json.set("expiration_date", expirationDate);
    }
    
    json.set("arrival_date", arrivalDate);
    json.set("storage_cell_id", storageCellId);
    json.set("quality_status", qualityStatusToString(qualityStatus));
    
    if (!invoiceNumber.empty())
    {
        json.set("invoice_number", invoiceNumber);
    }
    
    // Дополнительные поля
    if (!productName.empty())
    {
        json.set("product_name", productName);
    }
    
    if (!productSku.empty())
    {
        json.set("product_sku", productSku);
    }
    
    if (!supplierName.empty())
    {
        json.set("supplier_name", supplierName);
    }
    
    if (!storageCellCode.empty())
    {
        json.set("storage_cell_code", storageCellCode);
    }
    
    json.set("is_expired", isExpired());
    json.set("days_until_expiration", daysUntilExpiration());
    json.set("total_value", unitCost * quantityAvailable);
    json.set("needs_inspection", needsQualityInspection());
    
    return json;
}

ProductBatch ProductBatch::fromJson(const Poco::JSON::Object& json)
{
    return ProductBatch(json);
}

bool ProductBatch::validate() const
{
    if (batchNumber.empty() || batchNumber.length() > 100)
    {
        return false;
    }
    
    if (productId <= 0 || supplierId <= 0 || storageCellId <= 0)
    {
        return false;
    }
    
    if (quantityReceived < MIN_QUANTITY || quantityAvailable < MIN_QUANTITY)
    {
        return false;
    }
    
    if (quantityAvailable > quantityReceived)
    {
        return false;
    }
    
    if (unitCost < MIN_UNIT_COST)
    {
        return false;
    }
    
    if (!manufactureDate.empty() && !Validator::isValidDate(manufactureDate))
    {
        return false;
    }
    
    if (!expirationDate.empty() && !Validator::isValidDate(expirationDate))
    {
        return false;
    }
    
    return true;
}

bool ProductBatch::isExpired() const
{
    if (expirationDate.empty())
    {
        return false;
    }
    
    try
    {
        Poco::DateTime expiration = DateUtils::parseDate(expirationDate);
        Poco::DateTime now = DateUtils::now();
        return now > expiration;
    }
    catch (...)
    {
        return false;
    }
}

int ProductBatch::daysUntilExpiration() const
{
    if (expirationDate.empty())
    {
        return INT_MAX;
    }
    
    try
    {
        Poco::DateTime expiration = DateUtils::parseDate(expirationDate);
        Poco::DateTime now = DateUtils::now();
        return DateUtils::daysBetween(now, expiration);
    }
    catch (...)
    {
        return INT_MAX;
    }
}

bool ProductBatch::canBeUsed(int quantity) const
{
    return qualityStatus == QualityStatus::APPROVED && 
           !isExpired() && 
           quantityAvailable >= quantity;
}

void ProductBatch::useQuantity(int quantity)
{
    if (quantity > 0 && quantity <= quantityAvailable)
    {
        quantityAvailable -= quantity;
    }
}

bool ProductBatch::needsQualityInspection() const
{
    return qualityStatus == QualityStatus::PENDING;
}

std::string ProductBatch::qualityStatusToString(QualityStatus status)
{
    switch (status)
    {
        case QualityStatus::PENDING: return "pending";
        case QualityStatus::APPROVED: return "approved";
        case QualityStatus::REJECTED: return "rejected";
        case QualityStatus::QUARANTINE: return "quarantine";
        default: return "pending";
    }
}

QualityStatus ProductBatch::stringToQualityStatus(const std::string& statusStr)
{
    if (statusStr == "approved") return QualityStatus::APPROVED;
    if (statusStr == "rejected") return QualityStatus::REJECTED;
    if (statusStr == "quarantine") return QualityStatus::QUARANTINE;
    return QualityStatus::PENDING;
}

} // namespace database::models
