#pragma once

#include <string>
#include <Poco/JSON/Object.h>

namespace warehouse_backend::database::models
{

enum class PickingStatus
{
    NOT_STARTED,
    IN_PROGRESS,
    PICKED,
    PACKED
};

class OrderItem
{
public:
    long long id;
    long long orderId;
    long long productId;
    long long batchId;
    int quantityOrdered;
    int quantityShipped;
    double unitPrice;
    double discountPercent;
    double lineTotal;
    PickingStatus pickingStatus;
    long long pickedBy;
    std::string pickedAt;

    std::string productName;
    std::string productSku;
    std::string batchNumber;
    std::string pickedByName;

    OrderItem();
    explicit OrderItem(const Poco::JSON::Object& json);

    Poco::JSON::Object toJson() const;
    static OrderItem fromJson(const Poco::JSON::Object& json);

    bool validate() const;
    
    void calculateLineTotal();
    bool canBePicked() const;
    bool isFullyShipped() const;
    double getDiscountedPrice() const;
    int getRemainingToShip() const;
    
    static std::string pickingStatusToString(PickingStatus status);
    static PickingStatus stringToPickingStatus(const std::string& statusStr);
    
    static constexpr int MIN_QUANTITY = 1;
    static constexpr double MIN_UNIT_PRICE = 0.0;
    static constexpr double MIN_DISCOUNT = 0.0;
    static constexpr double MAX_DISCOUNT = 100.0;
};

} // namespace database::models