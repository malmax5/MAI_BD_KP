#pragma once

#include <string>
#include <Poco/JSON/Object.h>

namespace warehouse_backend::database::models
{

enum class MovementType
{
    RECEIPT,
    SHIPMENT,
    TRANSFER,
    ADJUSTMENT,
    COUNT
};

enum class MovementStatus
{
    PLANNED,
    IN_PROGRESS,
    COMPLETED,
    CANCELLED
};

class InventoryMovement
{
public:
    long long id;
    MovementType movementType;
    long long productId;
    long long batchId;
    long long fromCellId;
    long long toCellId;
    int quantity;
    long long referenceId;
    std::string referenceType;
    std::string movementDate;
    long long performedBy;
    std::string reason;
    MovementStatus status;

    std::string productName;
    std::string productSku;
    std::string batchNumber;
    std::string fromCellCode;
    std::string toCellCode;
    std::string performedByName;

    InventoryMovement();
    explicit InventoryMovement(const Poco::JSON::Object& json);

    Poco::JSON::Object toJson() const;
    static InventoryMovement fromJson(const Poco::JSON::Object& json);

    bool validate() const;
    
    bool isTransfer() const;
    bool isReceipt() const;
    bool isShipment() const;
    bool isAdjustment() const;
    bool canBeCancelled() const;
    bool isCompleted() const;
    
    static std::string movementTypeToString(MovementType type);
    static MovementType stringToMovementType(const std::string& typeStr);
    
    static std::string movementStatusToString(MovementStatus status);
    static MovementStatus stringToMovementStatus(const std::string& statusStr);
    
    static constexpr int MIN_QUANTITY = 1;
    static constexpr int MAX_REFERENCE_TYPE_LENGTH = 50;
};

} // namespace database::models