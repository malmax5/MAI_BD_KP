// warehouse_backend/src/services/ShipmentService.hpp
#pragma once

#include "../database/repositories/ShipmentRepository.hpp"
#include "../database/repositories/CustomerOrderRepository.hpp"
#include "../database/repositories/OrderItemRepository.hpp"
#include "../database/repositories/ProductBatchRepository.hpp"
#include "../database/repositories/AuditLogRepository.hpp"
#include "../config/ConfigManager.hpp"
#include "../utils/DateUtils.hpp"
#include "../utils/JsonUtils.hpp"
#include <memory>
#include <string>
#include <vector>
#include <Poco/JSON/Object.h>
#include <Poco/JSON/Array.h>

namespace warehouse_backend::services
{

struct ShipmentCreationResult
{
    bool success;
    std::string message;
    long long shipmentId;
    std::string shipmentNumber;
    std::unique_ptr<database::models::Shipment> shipment;
    Poco::JSON::Object details;
    
    ShipmentCreationResult();
    Poco::JSON::Object toJson() const;
};

struct ShipmentUpdateResult
{
    bool success;
    std::string message;
    std::unique_ptr<database::models::Shipment> updatedShipment;
    std::vector<std::string> changes;
    
    Poco::JSON::Object toJson() const;
};

class ShipmentService
{
public:
    ShipmentService();
    ~ShipmentService() = default;
    
    ShipmentCreationResult createShipment(long long orderId,
                                         const std::string& carrier,
                                         const std::string& shippingMethod,
                                         double shippingCost,
                                         const std::string& estimatedArrival,
                                         long long createdBy = 0);
    
    ShipmentUpdateResult updateShipmentStatus(long long shipmentId,
                                            const std::string& newStatus,
                                            long long updatedBy = 0,
                                            const std::string& notes = "");
    
    ShipmentUpdateResult updateTrackingInfo(long long shipmentId,
                                          const std::string& trackingNumber,
                                          const std::string& carrier,
                                          long long updatedBy = 0);
    
    ShipmentUpdateResult addShipmentNotes(long long shipmentId,
                                         const std::string& notes,
                                         long long updatedBy = 0);
    
    bool cancelShipment(long long shipmentId,
                       long long cancelledBy = 0,
                       const std::string& reason = "");
    
    bool markShipmentAsDelivered(long long shipmentId,
                                const std::string& actualArrival = "",
                                long long deliveredBy = 0);
    
    Poco::JSON::Object getShipmentDetails(long long shipmentId);
    Poco::JSON::Object getShipmentWithOrderDetails(long long shipmentId);
    
    Poco::JSON::Array findShipmentsByOrder(long long orderId);
    Poco::JSON::Array findShipmentsByStatus(const std::string& status,
                                           int page = 1,
                                           int pageSize = 20);
    
    Poco::JSON::Array findShipmentsByCarrier(const std::string& carrier,
                                            int page = 1,
                                            int pageSize = 20);
    
    Poco::JSON::Array findShipmentsByDateRange(const std::string& startDate,
                                              const std::string& endDate,
                                              int page = 1,
                                              int pageSize = 20);
    
    Poco::JSON::Array getDelayedShipments(int daysThreshold = 3);
    Poco::JSON::Array getShipmentsDueToday();
    Poco::JSON::Array getInTransitShipments();
    
    Poco::JSON::Object getShipmentStatistics();
    Poco::JSON::Array getCarrierPerformanceReport();
    Poco::JSON::Array getShippingCostAnalysis(const std::string& startDate,
                                             const std::string& endDate);
    
    bool validateShipmentForOrder(long long orderId, std::string& errorMessage);
    bool canShipOrder(long long orderId, std::string& errorMessage);
    
    double calculateShippingCost(long long orderId,
                                const std::string& shippingMethod,
                                double weight = 0.0);
    
    std::string shipmentToJsonString(const database::models::Shipment& shipment) const;

private:
    std::unique_ptr<database::repositories::ShipmentRepository> shipmentRepository;
    std::unique_ptr<database::repositories::CustomerOrderRepository> orderRepository;
    std::unique_ptr<database::repositories::OrderItemRepository> itemRepository;
    std::unique_ptr<database::repositories::ProductBatchRepository> batchRepository;
    std::unique_ptr<database::repositories::AuditLogRepository> auditRepository;
    
    std::string generateShipmentNumber();
    double calculateShipmentWeight(long long orderId);
    void updateOrderStatusOnShipment(long long orderId, long long shipmentId);
    void logShipmentAudit(const std::string& action,
                         long long shipmentId,
                         long long changedBy,
                         const std::string& details = "");
};

} // namespace services