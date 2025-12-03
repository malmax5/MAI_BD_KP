#pragma once

#include <string>
#include <Poco/JSON/Object.h>

namespace warehouse_backend::database::models
{

class Product
{
public:
    long long id;
    std::string sku;
    std::string name;
    std::string description;
    long long categoryId;
    long long supplierId;
    double unitPrice;
    double weight;
    std::string dimensions;
    int minStockLevel;
    int maxStockLevel;
    bool isActive;
    std::string createdAt;

    std::string categoryName;
    std::string supplierName;
    int currentStock;

    Product();
    explicit Product(const Poco::JSON::Object& json);

    Poco::JSON::Object toJson() const;
    static Product fromJson(const Poco::JSON::Object& json);

    bool validate() const;
    
    bool needsReorder() const;
    double getTotalWeight(int quantity) const;
    
    static constexpr double MIN_PRICE = 0.0;
    static constexpr double MIN_WEIGHT = 0.0;
    static constexpr int MIN_STOCK_LEVEL = 0;
    static constexpr int MAX_NAME_LENGTH = 200;
    static constexpr int MAX_SKU_LENGTH = 50;
};

} // namespace database::models