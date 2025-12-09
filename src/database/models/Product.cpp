#include "Product.hpp"
#include "../../utils/JsonUtils.hpp"
#include "../../utils/Validator.hpp"
#include <Poco/JSON/Parser.h>
#include <Poco/JSON/Stringifier.h>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace warehouse_backend::database::models
{

using namespace warehouse_backend::utils;

Product::Product()
    : id(0),
      categoryId(0),
      supplierId(0),
      unitPrice(0.0),
      weight(0.0),
      minStockLevel(0),
      maxStockLevel(1000),
      isActive(true),
      currentStock(0)
{

}

Product::Product(const Poco::JSON::Object& json)
{
    sku = JsonUtils::getString(json, "sku", "");
    name = JsonUtils::getString(json, "name", "");
    description = JsonUtils::getString(json, "description", "");
    categoryId = JsonUtils::getInt(json, "category_id", 0);
    supplierId = JsonUtils::getInt(json, "supplier_id", 0);
    unitPrice = JsonUtils::getDouble(json, "unit_price", 0.0);
    weight = JsonUtils::getDouble(json, "weight", 0.0);
    dimensions = JsonUtils::getString(json, "dimensions", "");
    minStockLevel = JsonUtils::getInt(json, "min_stock_level", 0);
    maxStockLevel = JsonUtils::getInt(json, "max_stock_level", 1000);
    isActive = JsonUtils::getBool(json, "is_active", true);
    createdAt = JsonUtils::getString(json, "created_at", "");
    currentStock = JsonUtils::getInt(json, "current_stock", 0);
    
    if (json.has("id"))
    {
        id = JsonUtils::getInt(json, "id", 0);
    }
    
    if (json.has("category_name"))
    {
        categoryName = JsonUtils::getString(json, "category_name", "");
    }
    
    if (json.has("supplier_name"))
    {
        supplierName = JsonUtils::getString(json, "supplier_name", "");
    }
}

Poco::JSON::Object Product::toJson() const
{
    Poco::JSON::Object json;
    
    if (id > 0)
    {
        json.set("id", id);
    }
    
    json.set("sku", sku);
    json.set("name", name);
    
    if (!description.isNull())
    {
        json.set("description", description);
    }
    
    json.set("category_id", categoryId);
    json.set("supplier_id", supplierId);
    json.set("unit_price", unitPrice);
    
    if (weight > 0)
    {
        json.set("weight", weight);
    }
    
    if (!dimensions.isNull())
    {
        json.set("dimensions", dimensions);
    }
    
    json.set("min_stock_level", minStockLevel);
    json.set("max_stock_level", maxStockLevel);
    json.set("is_active", isActive);
    json.set("created_at", createdAt);
    
    json.set("current_stock", currentStock);
    
    if (!categoryName.empty())
    {
        json.set("category_name", categoryName);
    }
    
    if (!supplierName.empty())
    {
        json.set("supplier_name", supplierName);
    }
    
    json.set("needs_reorder", needsReorder());
    json.set("available_stock", maxStockLevel - currentStock);
    
    return json;
}

Product Product::fromJson(const Poco::JSON::Object& json)
{
    return Product(json);
}

bool Product::validate() const
{
    if (sku.empty() || sku.length() > MAX_SKU_LENGTH)
    {
        return false;
    }
    
    if (name.empty() || name.length() > MAX_NAME_LENGTH)
    {
        return false;
    }
    
    if (unitPrice < MIN_PRICE)
    {
        return false;
    }
    
    if (weight < MIN_WEIGHT)
    {
        return false;
    }
    
    if (minStockLevel < MIN_STOCK_LEVEL || maxStockLevel < minStockLevel)
    {
        return false;
    }
    
    if (categoryId <= 0 || supplierId <= 0)
    {
        return false;
    }
    
    return true;
}

bool Product::needsReorder() const
{
    return currentStock <= minStockLevel;
}

double Product::getTotalWeight(int quantity) const
{
    return weight * quantity;
}

} // namespace database::models
