#pragma once

#include <string>
#include <Poco/JSON/Object.h>

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
    long long id;
    std::string shipmentNumber;
    long long orderId;
    std::string carrier;
    std::string trackingNumber;
    std::string shippingMethod;
    double shippingCost;
    std::string shipmentDate;
    std::string estimatedArrival;
    std::string actualArrival;
    ShipmentStatus status;
    std::string notes;
    double weightTotal;
    std::string dimensionsTotal;

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