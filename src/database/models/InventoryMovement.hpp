#pragma once

#include <string>
#include <Poco/JSON/Object.h>
#include <Poco/Nullable.h>

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
    Poco::Int64 id;
    MovementType movementType;
    Poco::Int64 productId;
    Poco::Int64 batchId;
    Poco::Nullable<Poco::Int64> fromCellId;
    Poco::Nullable<Poco::Int64> toCellId;
    int quantity;
    Poco::Nullable<Poco::Int64> referenceId;
    Poco::Nullable<std::string> referenceType;
    std::string movementDate;
    Poco::Int64 performedBy;
    Poco::Nullable<std::string> reason;
    MovementStatus status;

    std::string productName;
    std::string productSku;
    std::string batchNumber;
    Poco::Nullable<std::string> fromCellCode;
    Poco::Nullable<std::string> toCellCode;
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
