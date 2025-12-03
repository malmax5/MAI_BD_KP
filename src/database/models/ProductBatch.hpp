#pragma once

#include <string>
#include <Poco/JSON/Object.h>

namespace warehouse_backend::database::models
{

enum class QualityStatus
{
    PENDING,
    APPROVED,
    REJECTED,
    QUARANTINE
};

class ProductBatch
{
public:
    long long id;
    std::string batchNumber;
    long long productId;
    long long supplierId;
    int quantityReceived;
    int quantityAvailable;
    double unitCost;
    std::string manufactureDate;
    std::string expirationDate;
    std::string arrivalDate;
    long long storageCellId;
    QualityStatus qualityStatus;
    std::string invoiceNumber;

    std::string productName;
    std::string productSku;
    std::string supplierName;
    std::string storageCellCode;

    ProductBatch();
    explicit ProductBatch(const Poco::JSON::Object& json);

    Poco::JSON::Object toJson() const;
    static ProductBatch fromJson(const Poco::JSON::Object& json);

    bool validate() const;
    
    bool isExpired() const;
    int daysUntilExpiration() const;
    bool canBeUsed(int quantity) const;
    void useQuantity(int quantity);
    bool needsQualityInspection() const;
    
    static std::string qualityStatusToString(QualityStatus status);
    static QualityStatus stringToQualityStatus(const std::string& statusStr);
    
    static constexpr int MIN_QUANTITY = 0;
    static constexpr double MIN_UNIT_COST = 0.0;
};

} // namespace database::models