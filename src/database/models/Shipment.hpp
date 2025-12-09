#pragma once

#include <string>
#include <Poco/JSON/Object.h>
#include <Poco/Nullable.h>

namespace warehouse_backend::database::models
{

enum class ShipmentStatus
{
    PREPARING,
    IN_TRANSIT,
    DELIVERED,
    DELAYED,
    RETURNED
};

class Shipment
{
public:
    Poco::Int64 id;
    std::string shipmentNumber;
    Poco::Int64 orderId;
    std::string carrier;
    Poco::Nullable<std::string> trackingNumber;
    Poco::Nullable<std::string> shippingMethod;
    Poco::Nullable<double> shippingCost;
    std::string shipmentDate;
    Poco::Nullable<std::string> estimatedArrival;
    Poco::Nullable<std::string> actualArrival;
    ShipmentStatus status;
    Poco::Nullable<std::string> notes;
    Poco::Nullable<double> weightTotal;
    Poco::Nullable<std::string> dimensionsTotal;

    std::string orderNumber;
    std::string customerName;

    Shipment();
    explicit Shipment(const Poco::JSON::Object& json);

    Poco::JSON::Object toJson() const;
    static Shipment fromJson(const Poco::JSON::Object& json);

    bool validate() const;
    
    bool isDelivered() const;
    bool isInTransit() const;
    bool isDelayed() const;
    int daysInTransit() const;
    bool needsTrackingUpdate() const;
    
    static std::string statusToString(ShipmentStatus status);
    static ShipmentStatus stringToStatus(const std::string& statusStr);
    
    static constexpr int MAX_CARRIER_LENGTH = 100;
    static constexpr double MIN_SHIPPING_COST = 0.0;
    static constexpr double MIN_WEIGHT = 0.0;
};

} // namespace database::models
