#pragma once

#include "BaseRepository.hpp"
#include "../models/Shipment.hpp"
#include <memory>
#include <string>
#include <vector>
#include <Poco/JSON/Object.h>
#include <Poco/JSON/Array.h>

namespace warehouse_backend::database::repositories
{

class ShipmentRepository : public BaseRepository<models::Shipment>
{
public:
    ShipmentRepository();
    ~ShipmentRepository() override = default;
    
    std::unique_ptr<models::Shipment> findById(long long id) override;
    std::vector<std::unique_ptr<models::Shipment>> findAll() override;
    std::vector<std::unique_ptr<models::Shipment>> findPaginated(int page, int pageSize) override;
    long long create(const models::Shipment& shipment) override;
    bool update(long long id, const models::Shipment& shipment) override;
    bool remove(long long id) override;
    bool softDelete(long long id) override;
    int count() override;
    
    Poco::JSON::Array findAllAsJson() override;
    Poco::JSON::Object findByIdAsJson(long long id) override;
    
    std::vector<std::unique_ptr<models::Shipment>> findByField(const std::string& fieldName, 
                                                               const std::string& fieldValue) override;
    std::vector<std::unique_ptr<models::Shipment>> search(const std::string& query, 
                                                          const std::vector<std::string>& fields) override;
    
    std::unique_ptr<models::Shipment> findByShipmentNumber(const std::string& shipmentNumber);
    std::vector<std::unique_ptr<models::Shipment>> findByOrderId(long long orderId);
    std::vector<std::unique_ptr<models::Shipment>> findByCarrier(const std::string& carrier);
    std::vector<std::unique_ptr<models::Shipment>> findByTrackingNumber(const std::string& trackingNumber);
    std::vector<std::unique_ptr<models::Shipment>> findByStatus(models::ShipmentStatus status);
    std::vector<std::unique_ptr<models::Shipment>> findByShippingMethod(const std::string& method);
    std::vector<std::unique_ptr<models::Shipment>> findByDateRange(const std::string& startDate, const std::string& endDate);
    std::vector<std::unique_ptr<models::Shipment>> findByEstimatedArrivalDate(const std::string& date);
    
    std::vector<std::unique_ptr<models::Shipment>> findPreparingShipments();
    std::vector<std::unique_ptr<models::Shipment>> findInTransitShipments();
    std::vector<std::unique_ptr<models::Shipment>> findDelayedShipments();
    std::vector<std::unique_ptr<models::Shipment>> findCompletedShipments();
    std::vector<std::unique_ptr<models::Shipment>> findShipmentsDueToday();
    
    bool updateStatus(long long id, models::ShipmentStatus newStatus);
    bool updateTrackingInfo(long long id, const std::string& trackingNumber, const std::string& carrier);
    bool updateShippingInfo(long long id, const std::string& method, double shippingCost);
    bool updateArrivalDates(long long id, const std::string& estimatedArrival, const std::string& actualArrival = "");
    bool updateWeightAndDimensions(long long id, double weight, const std::string& dimensions);
    
    bool markAsInTransit(long long id);
    bool markAsDelivered(long long id);
    bool markAsDelayed(long long id);
    bool cancelShipment(long long id);
    
    int countByStatus(models::ShipmentStatus status);
    int countByCarrier(const std::string& carrier);
    int countByOrder(long long orderId);
    
    double getTotalShippingCost(const std::string& startDate, const std::string& endDate);
    double getAverageShippingCost();
    int getAverageTransitDays();
    
    std::unique_ptr<models::Shipment> getShipmentWithOrderDetails(long long id);
    
    Poco::JSON::Array getShipmentStatistics();
    Poco::JSON::Array getCarrierPerformanceReport();
    Poco::JSON::Array getShippingCostAnalysis(const std::string& startDate, const std::string& endDate);
    
    bool shipmentNumberExists(const std::string& shipmentNumber);
    
    std::vector<std::pair<long long, std::string>> getActiveShipmentNumbers();
    std::vector<std::string> getUniqueCarriers();
    
private:
    models::Shipment mapRowToShipment(Poco::Data::Row& row) const;
    std::string generateShipmentNumber();
    
    static const std::string TABLE_NAME;
    static const std::vector<std::string> SEARCH_FIELDS;
};

} // namespace database::repositories
