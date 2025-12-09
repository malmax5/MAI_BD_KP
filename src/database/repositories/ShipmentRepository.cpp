#include "ShipmentRepository.hpp"
#include "../models/Shipment.hpp"
#include <Poco/Data/Session.h>
#include <Poco/Data/RecordSet.h>
#include <Poco/Data/Statement.h>
#include <Poco/Data/Row.h>
#include <Poco/DateTime.h>
#include <Poco/DateTimeFormat.h>
#include <Poco/DateTimeFormatter.h>
#include <Poco/DateTimeParser.h>
#include <Poco/Dynamic/Var.h>
#include "../../utils/DateUtils.hpp"
#include "../../utils/JsonUtils.hpp"

using namespace Poco::Data;
using namespace Poco::Data::Keywords;
using namespace Poco;

using namespace warehouse_backend::utils;

namespace warehouse_backend::database::repositories
{

const std::string ShipmentRepository::TABLE_NAME = "shipments";
const std::vector<std::string> ShipmentRepository::SEARCH_FIELDS = {
    "shipment_number", "carrier", "tracking_number", "shipping_method", "notes"
};

ShipmentRepository::ShipmentRepository() : BaseRepository<models::Shipment>()
{
}

std::unique_ptr<models::Shipment> ShipmentRepository::findById(long long id)
{
    auto connection = acquireConnection();
    
    try
    {
        Poco::Int64 pocoId = static_cast<Poco::Int64>(id);
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT s.id, s.shipment_number, s.order_id, s.carrier, "
                  "s.tracking_number, s.shipping_method, s.shipping_cost, "
                  "s.shipment_date, s.estimated_arrival, s.actual_arrival, "
                  "s.status, s.notes, s.weight_total, s.dimensions_total, "
                  "co.order_number, co.customer_name "
                  "FROM " << TABLE_NAME << " s "
                  "JOIN customer_orders co ON co.id = s.order_id "
                  "WHERE s.id = $1",
            use(pocoId),
            now;
        
        Poco::Data::RecordSet rs(select);
        
        if (rs.rowCount() > 0)
        {
            auto shipment = std::make_unique<models::Shipment>();
            shipment->id = rs.value("id", 0).convert<long long>();
            shipment->shipmentNumber = rs.value("shipment_number").convert<std::string>();
            shipment->orderId = rs.value("order_id", 0).convert<long long>();
            shipment->carrier = rs.value("carrier").convert<std::string>();
            shipment->trackingNumber = rs.value("tracking_number").convert<std::string>();
            shipment->shippingMethod = rs.value("shipping_method").convert<std::string>();
            shipment->shippingCost = rs.value("shipping_cost", 0.0).convert<double>();
            shipment->shipmentDate = rs.value("shipment_date").convert<std::string>();
            shipment->estimatedArrival = rs.value("estimated_arrival").isEmpty() ? "" : rs.value("estimated_arrival").convert<std::string>();
            shipment->actualArrival = rs.value("actual_arrival").isEmpty() ? "" : rs.value("actual_arrival").convert<std::string>();
            
            std::string statusStr = rs.value("status").convert<std::string>();
            shipment->status = models::Shipment::stringToStatus(statusStr);
            
            shipment->notes = rs.value("notes").convert<std::string>();
            shipment->weightTotal = rs.value("weight_total", 0.0).convert<double>();
            shipment->dimensionsTotal = rs.value("dimensions_total").convert<std::string>();
            shipment->orderNumber = rs.value("order_number").convert<std::string>();
            shipment->customerName = rs.value("customer_name").convert<std::string>();
            
            return shipment;
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findById: " + e.displayText());
    }
    
    return nullptr;
}

std::vector<std::unique_ptr<models::Shipment>> ShipmentRepository::findAll()
{
    std::vector<std::unique_ptr<models::Shipment>> shipments;
    auto connection = acquireConnection();
    
    try
    {
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT s.id, s.shipment_number, s.order_id, s.carrier, "
                  "s.tracking_number, s.shipping_method, s.shipping_cost, "
                  "s.shipment_date, s.estimated_arrival, s.actual_arrival, "
                  "s.status, s.notes, s.weight_total, s.dimensions_total, "
                  "co.order_number, co.customer_name "
                  "FROM " << TABLE_NAME << " s "
                  "JOIN customer_orders co ON co.id = s.order_id "
                  "ORDER BY s.shipment_date DESC, s.id",
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            auto shipment = std::make_unique<models::Shipment>();
            shipment->id = rs.value("id", 0).convert<long long>();
            shipment->shipmentNumber = rs.value("shipment_number").convert<std::string>();
            shipment->orderId = rs.value("order_id", 0).convert<long long>();
            shipment->carrier = rs.value("carrier").convert<std::string>();
            shipment->trackingNumber = rs.value("tracking_number").convert<std::string>();
            shipment->shippingMethod = rs.value("shipping_method").convert<std::string>();
            shipment->shippingCost = rs.value("shipping_cost", 0.0).convert<double>();
            shipment->shipmentDate = rs.value("shipment_date").convert<std::string>();
            shipment->estimatedArrival = rs.value("estimated_arrival").isEmpty() ? "" : rs.value("estimated_arrival").convert<std::string>();
            shipment->actualArrival = rs.value("actual_arrival").isEmpty() ? "" : rs.value("actual_arrival").convert<std::string>();
            
            std::string statusStr = rs.value("status").convert<std::string>();
            shipment->status = models::Shipment::stringToStatus(statusStr);
            
            shipment->notes = rs.value("notes").convert<std::string>();
            shipment->weightTotal = rs.value("weight_total", 0.0).convert<double>();
            shipment->dimensionsTotal = rs.value("dimensions_total").convert<std::string>();
            shipment->orderNumber = rs.value("order_number").convert<std::string>();
            shipment->customerName = rs.value("customer_name").convert<std::string>();
            
            shipments.push_back(std::move(shipment));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findAll: " + e.displayText());
    }
    
    return shipments;
}

std::vector<std::unique_ptr<models::Shipment>> ShipmentRepository::findPaginated(int page, int pageSize)
{
    std::vector<std::unique_ptr<models::Shipment>> shipments;
    auto connection = acquireConnection();
    
    if (page < 1) page = 1;
    int offset = (page - 1) * pageSize;
    
    try
    {
        int usePageSize = pageSize;
        int useOffset = offset;
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT s.id, s.shipment_number, s.order_id, s.carrier, "
                  "s.tracking_number, s.shipping_method, s.shipping_cost, "
                  "s.shipment_date, s.estimated_arrival, s.actual_arrival, "
                  "s.status, s.notes, s.weight_total, s.dimensions_total, "
                  "co.order_number, co.customer_name "
                  "FROM " << TABLE_NAME << " s "
                  "JOIN customer_orders co ON co.id = s.order_id "
                  "ORDER BY s.shipment_date DESC, s.id LIMIT $1 OFFSET $2",
            use(usePageSize),
            use(useOffset),
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            auto shipment = std::make_unique<models::Shipment>();
            shipment->id = rs.value("id", 0).convert<long long>();
            shipment->shipmentNumber = rs.value("shipment_number").convert<std::string>();
            shipment->orderId = rs.value("order_id", 0).convert<long long>();
            shipment->carrier = rs.value("carrier").convert<std::string>();
            shipment->trackingNumber = rs.value("tracking_number").convert<std::string>();
            shipment->shippingMethod = rs.value("shipping_method").convert<std::string>();
            shipment->shippingCost = rs.value("shipping_cost", 0.0).convert<double>();
            shipment->shipmentDate = rs.value("shipment_date").convert<std::string>();
            shipment->estimatedArrival = rs.value("estimated_arrival").isEmpty() ? "" : rs.value("estimated_arrival").convert<std::string>();
            shipment->actualArrival = rs.value("actual_arrival").isEmpty() ? "" : rs.value("actual_arrival").convert<std::string>();
            
            std::string statusStr = rs.value("status").convert<std::string>();
            shipment->status = models::Shipment::stringToStatus(statusStr);
            
            shipment->notes = rs.value("notes").convert<std::string>();
            shipment->weightTotal = rs.value("weight_total", 0.0).convert<double>();
            shipment->dimensionsTotal = rs.value("dimensions_total").convert<std::string>();
            shipment->orderNumber = rs.value("order_number").convert<std::string>();
            shipment->customerName = rs.value("customer_name").convert<std::string>();
            
            shipments.push_back(std::move(shipment));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findPaginated: " + e.displayText());
    }
    
    return shipments;
}

long long ShipmentRepository::create(const models::Shipment& shipment)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        std::string statusStr = models::Shipment::statusToString(shipment.status);

        models::Shipment shipmentCopy = shipment;
        
        Poco::Data::Statement insert(connection->getSession());
        Poco::Int64 newId = 0;
        
        insert << "INSERT INTO " << TABLE_NAME << " "
                  "(shipment_number, order_id, carrier, tracking_number, shipping_method, "
                  "shipping_cost, shipment_date, estimated_arrival, actual_arrival, "
                  "status, notes, weight_total, dimensions_total) "
                  "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10::shipment_status, $11, $12, $13) "
                  "RETURNING id",
            use(shipmentCopy.shipmentNumber),
            use(shipmentCopy.orderId),
            use(shipmentCopy.carrier),
            use(shipmentCopy.trackingNumber),
            use(shipmentCopy.shippingMethod),
            use(shipmentCopy.shippingCost),
            use(shipmentCopy.shipmentDate),
            use(shipmentCopy.estimatedArrival),
            use(shipmentCopy.actualArrival),
            use(statusStr),
            use(shipmentCopy.notes),
            use(shipmentCopy.weightTotal),
            use(shipmentCopy.dimensionsTotal),
            into(newId),
            now;
        
        commitTransaction(*connection);
        return static_cast<long long>(newId);
    }
    catch (const Poco::Exception& e)
    {
        rollbackTransaction(*connection);
        throw std::runtime_error("Database error in create: " + e.displayText());
    }
}

bool ShipmentRepository::update(long long id, const models::Shipment& shipment)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        std::string statusStr = models::Shipment::statusToString(shipment.status);
        
        models::Shipment shipmentCopy = shipment;

        Poco::Int64 idCopy = id;
        
        Poco::Data::Statement update(connection->getSession());
        update << "UPDATE " << TABLE_NAME << " SET "
                  "shipment_number = $1, order_id = $2, carrier = $3, "
                  "tracking_number = $4, shipping_method = $5, shipping_cost = $6, "
                  "shipment_date = $7, estimated_arrival = $8, actual_arrival = $9, "
                  "status = $10::shipment_status, notes = $11, "
                  "weight_total = $12, dimensions_total = $13 "
                  "WHERE id = $14",
            use(shipmentCopy.shipmentNumber),
            use(shipmentCopy.orderId),
            use(shipmentCopy.carrier),
            use(shipmentCopy.trackingNumber),
            use(shipmentCopy.shippingMethod),
            use(shipmentCopy.shippingCost),
            use(shipmentCopy.shipmentDate),
            use(shipmentCopy.estimatedArrival),
            use(shipmentCopy.actualArrival),
            use(statusStr),
            use(shipmentCopy.notes),
            use(shipmentCopy.weightTotal),
            use(shipmentCopy.dimensionsTotal),
            use(idCopy);
        
        int rowsAffected = update.execute();
        
        commitTransaction(*connection);
        return rowsAffected > 0;
    }
    catch (const Poco::Exception& e)
    {
        rollbackTransaction(*connection);
        throw std::runtime_error("Database error in update: " + e.displayText());
    }
}

bool ShipmentRepository::remove(long long id)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        long long idCopy = id;
        
        Poco::Data::Statement del(connection->getSession());
        del << "DELETE FROM " << TABLE_NAME << " WHERE id = $1",
            use(idCopy);
        
        int rowsAffected = del.execute();
        
        commitTransaction(*connection);
        return rowsAffected > 0;
    }
    catch (const Poco::Exception& e)
    {
        rollbackTransaction(*connection);
        throw std::runtime_error("Database error in remove: " + e.displayText());
    }
}

bool ShipmentRepository::softDelete(long long id)
{
    return remove(id);
}

int ShipmentRepository::count()
{
    auto connection = acquireConnection();
    
    try
    {
        Poco::Data::Statement countStmt(connection->getSession());
        countStmt << "SELECT COUNT(*) FROM " << TABLE_NAME,
            now;
        
        Poco::Data::RecordSet rs(countStmt);
        if (rs.rowCount() > 0)
        {
            return rs.value(0, 0).convert<int>();
        }
        
        return 0;
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in count: " + e.displayText());
    }
}

Poco::JSON::Array ShipmentRepository::findAllAsJson()
{
    auto shipments = findAll();
    Poco::JSON::Array jsonArray;
    
    for (const auto& shipment : shipments)
    {
        if (shipment)
        {
            jsonArray.add(shipment->toJson());
        }
    }
    
    return jsonArray;
}

Poco::JSON::Object ShipmentRepository::findByIdAsJson(long long id)
{
    auto shipment = findById(id);
    if (shipment)
    {
        return shipment->toJson();
    }
    
    return Poco::JSON::Object();
}

std::vector<std::unique_ptr<models::Shipment>> ShipmentRepository::findByField(
    const std::string& fieldName, const std::string& fieldValue)
{
    std::vector<std::unique_ptr<models::Shipment>> shipments;
    auto connection = acquireConnection();
    
    try
    {
        std::string sql = "SELECT s.id, s.shipment_number, s.order_id, s.carrier, "
                          "s.tracking_number, s.shipping_method, s.shipping_cost, "
                          "s.shipment_date, s.estimated_arrival, s.actual_arrival, "
                          "s.status, s.notes, s.weight_total, s.dimensions_total, "
                          "co.order_number, co.customer_name "
                          "FROM " + TABLE_NAME + " s "
                          "JOIN customer_orders co ON co.id = s.order_id "
                          "WHERE s." + fieldName + " = $1 "
                          "ORDER BY s.shipment_date DESC";
        
        std::string fieldValueCopy = fieldValue;
        Poco::Data::Statement select(connection->getSession());
        select << sql,
            use(fieldValueCopy),
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            auto shipment = std::make_unique<models::Shipment>();
            shipment->id = rs.value("id", 0).convert<long long>();
            shipment->shipmentNumber = rs.value("shipment_number").convert<std::string>();
            shipment->orderId = rs.value("order_id", 0).convert<long long>();
            shipment->carrier = rs.value("carrier").convert<std::string>();
            shipment->trackingNumber = rs.value("tracking_number").convert<std::string>();
            shipment->shippingMethod = rs.value("shipping_method").convert<std::string>();
            shipment->shippingCost = rs.value("shipping_cost", 0.0).convert<double>();
            shipment->shipmentDate = rs.value("shipment_date").convert<std::string>();
            shipment->estimatedArrival = rs.value("estimated_arrival").isEmpty() ? "" : rs.value("estimated_arrival").convert<std::string>();
            shipment->actualArrival = rs.value("actual_arrival").isEmpty() ? "" : rs.value("actual_arrival").convert<std::string>();
            
            std::string statusStr = rs.value("status").convert<std::string>();
            shipment->status = models::Shipment::stringToStatus(statusStr);
            
            shipment->notes = rs.value("notes").convert<std::string>();
            shipment->weightTotal = rs.value("weight_total", 0.0).convert<double>();
            shipment->dimensionsTotal = rs.value("dimensions_total").convert<std::string>();
            shipment->orderNumber = rs.value("order_number").convert<std::string>();
            shipment->customerName = rs.value("customer_name").convert<std::string>();
            
            shipments.push_back(std::move(shipment));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findByField: " + e.displayText());
    }
    
    return shipments;
}

std::vector<std::unique_ptr<models::Shipment>> ShipmentRepository::search(
    const std::string& query, const std::vector<std::string>& fields)
{
    std::vector<std::unique_ptr<models::Shipment>> shipments;
    auto connection = acquireConnection();
    
    try
    {
        std::string searchQuery = buildSearchQuery(query, SEARCH_FIELDS);
        std::string sql = "SELECT s.id, s.shipment_number, s.order_id, s.carrier, "
                          "s.tracking_number, s.shipping_method, s.shipping_cost, "
                          "s.shipment_date, s.estimated_arrival, s.actual_arrival, "
                          "s.status, s.notes, s.weight_total, s.dimensions_total, "
                          "co.order_number, co.customer_name "
                          "FROM " + TABLE_NAME + " s "
                          "JOIN customer_orders co ON co.id = s.order_id "
                          "WHERE " + searchQuery + " "
                          "ORDER BY s.shipment_date DESC";
        
        Poco::Data::Statement select(connection->getSession());
        select << sql,
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            auto shipment = std::make_unique<models::Shipment>();
            shipment->id = rs.value("id", 0).convert<long long>();
            shipment->shipmentNumber = rs.value("shipment_number").convert<std::string>();
            shipment->orderId = rs.value("order_id", 0).convert<long long>();
            shipment->carrier = rs.value("carrier").convert<std::string>();
            shipment->trackingNumber = rs.value("tracking_number").convert<std::string>();
            shipment->shippingMethod = rs.value("shipping_method").convert<std::string>();
            shipment->shippingCost = rs.value("shipping_cost", 0.0).convert<double>();
            shipment->shipmentDate = rs.value("shipment_date").convert<std::string>();
            shipment->estimatedArrival = rs.value("estimated_arrival").isEmpty() ? "" : rs.value("estimated_arrival").convert<std::string>();
            shipment->actualArrival = rs.value("actual_arrival").isEmpty() ? "" : rs.value("actual_arrival").convert<std::string>();
            
            std::string statusStr = rs.value("status").convert<std::string>();
            shipment->status = models::Shipment::stringToStatus(statusStr);
            
            shipment->notes = rs.value("notes").convert<std::string>();
            shipment->weightTotal = rs.value("weight_total", 0.0).convert<double>();
            shipment->dimensionsTotal = rs.value("dimensions_total").convert<std::string>();
            shipment->orderNumber = rs.value("order_number").convert<std::string>();
            shipment->customerName = rs.value("customer_name").convert<std::string>();
            
            shipments.push_back(std::move(shipment));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in search: " + e.displayText());
    }
    
    return shipments;
}

std::unique_ptr<models::Shipment> ShipmentRepository::findByShipmentNumber(const std::string& shipmentNumber)
{
    auto result = findByField("shipment_number", shipmentNumber);
    if (!result.empty())
    {
        return std::move(result[0]);
    }
    return nullptr;
}

std::vector<std::unique_ptr<models::Shipment>> ShipmentRepository::findByOrderId(long long orderId)
{
    return findByField("order_id", std::to_string(orderId));
}

std::vector<std::unique_ptr<models::Shipment>> ShipmentRepository::findByCarrier(const std::string& carrier)
{
    return findByField("carrier", carrier);
}

std::vector<std::unique_ptr<models::Shipment>> ShipmentRepository::findByTrackingNumber(const std::string& trackingNumber)
{
    return findByField("tracking_number", trackingNumber);
}

std::vector<std::unique_ptr<models::Shipment>> ShipmentRepository::findByStatus(models::ShipmentStatus status)
{
    auto connection = acquireConnection();
    std::vector<std::unique_ptr<models::Shipment>> shipments;
    
    try
    {
        std::string statusStr = models::Shipment::statusToString(status);
        std::string statusStrCopy = statusStr;
        
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT s.id, s.shipment_number, s.order_id, s.carrier, "
                  "s.tracking_number, s.shipping_method, s.shipping_cost, "
                  "s.shipment_date, s.estimated_arrival, s.actual_arrival, "
                  "s.status, s.notes, s.weight_total, s.dimensions_total, "
                  "co.order_number, co.customer_name "
                  "FROM " << TABLE_NAME << " s "
                  "JOIN customer_orders co ON co.id = s.order_id "
                  "WHERE s.status = $1 "
                  "ORDER BY s.shipment_date DESC",
            use(statusStrCopy),
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            auto shipment = std::make_unique<models::Shipment>();
            shipment->id = rs.value("id", 0).convert<long long>();
            shipment->shipmentNumber = rs.value("shipment_number").convert<std::string>();
            shipment->orderId = rs.value("order_id", 0).convert<long long>();
            shipment->carrier = rs.value("carrier").convert<std::string>();
            shipment->trackingNumber = rs.value("tracking_number").convert<std::string>();
            shipment->shippingMethod = rs.value("shipping_method").convert<std::string>();
            shipment->shippingCost = rs.value("shipping_cost", 0.0).convert<double>();
            shipment->shipmentDate = rs.value("shipment_date").convert<std::string>();
            shipment->estimatedArrival = rs.value("estimated_arrival").isEmpty() ? "" : rs.value("estimated_arrival").convert<std::string>();
            shipment->actualArrival = rs.value("actual_arrival").isEmpty() ? "" : rs.value("actual_arrival").convert<std::string>();
            
            shipment->status = status;
            shipment->notes = rs.value("notes").convert<std::string>();
            shipment->weightTotal = rs.value("weight_total", 0.0).convert<double>();
            shipment->dimensionsTotal = rs.value("dimensions_total").convert<std::string>();
            shipment->orderNumber = rs.value("order_number").convert<std::string>();
            shipment->customerName = rs.value("customer_name").convert<std::string>();
            
            shipments.push_back(std::move(shipment));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findByStatus: " + e.displayText());
    }
    
    return shipments;
}

std::vector<std::unique_ptr<models::Shipment>> ShipmentRepository::findByShippingMethod(const std::string& method)
{
    return findByField("shipping_method", method);
}

std::vector<std::unique_ptr<models::Shipment>> ShipmentRepository::findByDateRange(const std::string& startDate, const std::string& endDate)
{
    auto connection = acquireConnection();
    std::vector<std::unique_ptr<models::Shipment>> shipments;
    
    try
    {
        std::string startDateCopy = startDate;
        std::string endDateCopy = endDate;
        
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT s.id, s.shipment_number, s.order_id, s.carrier, "
                  "s.tracking_number, s.shipping_method, s.shipping_cost, "
                  "s.shipment_date, s.estimated_arrival, s.actual_arrival, "
                  "s.status, s.notes, s.weight_total, s.dimensions_total, "
                  "co.order_number, co.customer_name "
                  "FROM " << TABLE_NAME << " s "
                  "JOIN customer_orders co ON co.id = s.order_id "
                  "WHERE s.shipment_date >= $1 AND s.shipment_date <= $2 "
                  "ORDER BY s.shipment_date DESC",
            use(startDateCopy),
            use(endDateCopy),
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            auto shipment = std::make_unique<models::Shipment>();
            shipment->id = rs.value("id", 0).convert<long long>();
            shipment->shipmentNumber = rs.value("shipment_number").convert<std::string>();
            shipment->orderId = rs.value("order_id", 0).convert<long long>();
            shipment->carrier = rs.value("carrier").convert<std::string>();
            shipment->trackingNumber = rs.value("tracking_number").convert<std::string>();
            shipment->shippingMethod = rs.value("shipping_method").convert<std::string>();
            shipment->shippingCost = rs.value("shipping_cost", 0.0).convert<double>();
            shipment->shipmentDate = rs.value("shipment_date").convert<std::string>();
            shipment->estimatedArrival = rs.value("estimated_arrival").isEmpty() ? "" : rs.value("estimated_arrival").convert<std::string>();
            shipment->actualArrival = rs.value("actual_arrival").isEmpty() ? "" : rs.value("actual_arrival").convert<std::string>();
            
            std::string statusStr = rs.value("status").convert<std::string>();
            shipment->status = models::Shipment::stringToStatus(statusStr);
            
            shipment->notes = rs.value("notes").convert<std::string>();
            shipment->weightTotal = rs.value("weight_total", 0.0).convert<double>();
            shipment->dimensionsTotal = rs.value("dimensions_total").convert<std::string>();
            shipment->orderNumber = rs.value("order_number").convert<std::string>();
            shipment->customerName = rs.value("customer_name").convert<std::string>();
            
            shipments.push_back(std::move(shipment));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findByDateRange: " + e.displayText());
    }
    
    return shipments;
}

std::vector<std::unique_ptr<models::Shipment>> ShipmentRepository::findByEstimatedArrivalDate(const std::string& date)
{
    return findByField("estimated_arrival", date);
}

std::vector<std::unique_ptr<models::Shipment>> ShipmentRepository::findPreparingShipments()
{
    return findByStatus(models::ShipmentStatus::PREPARING);
}

std::vector<std::unique_ptr<models::Shipment>> ShipmentRepository::findInTransitShipments()
{
    return findByStatus(models::ShipmentStatus::IN_TRANSIT);
}

std::vector<std::unique_ptr<models::Shipment>> ShipmentRepository::findDelayedShipments()
{
    return findByStatus(models::ShipmentStatus::DELAYED);
}

std::vector<std::unique_ptr<models::Shipment>> ShipmentRepository::findCompletedShipments()
{
    auto connection = acquireConnection();
    std::vector<std::unique_ptr<models::Shipment>> shipments;
    
    try
    {
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT s.id, s.shipment_number, s.order_id, s.carrier, "
                  "s.tracking_number, s.shipping_method, s.shipping_cost, "
                  "s.shipment_date, s.estimated_arrival, s.actual_arrival, "
                  "s.status, s.notes, s.weight_total, s.dimensions_total, "
                  "co.order_number, co.customer_name "
                  "FROM " << TABLE_NAME << " s "
                  "JOIN customer_orders co ON co.id = s.order_id "
                  "WHERE s.status IN ('delivered', 'returned') "
                  "ORDER BY s.actual_arrival DESC",
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            auto shipment = std::make_unique<models::Shipment>();
            shipment->id = rs.value("id", 0).convert<long long>();
            shipment->shipmentNumber = rs.value("shipment_number").convert<std::string>();
            shipment->orderId = rs.value("order_id", 0).convert<long long>();
            shipment->carrier = rs.value("carrier").convert<std::string>();
            shipment->trackingNumber = rs.value("tracking_number").convert<std::string>();
            shipment->shippingMethod = rs.value("shipping_method").convert<std::string>();
            shipment->shippingCost = rs.value("shipping_cost", 0.0).convert<double>();
            shipment->shipmentDate = rs.value("shipment_date").convert<std::string>();
            shipment->estimatedArrival = rs.value("estimated_arrival").isEmpty() ? "" : rs.value("estimated_arrival").convert<std::string>();
            shipment->actualArrival = rs.value("actual_arrival").isEmpty() ? "" : rs.value("actual_arrival").convert<std::string>();
            
            std::string statusStr = rs.value("status").convert<std::string>();
            shipment->status = models::Shipment::stringToStatus(statusStr);
            
            shipment->notes = rs.value("notes").convert<std::string>();
            shipment->weightTotal = rs.value("weight_total", 0.0).convert<double>();
            shipment->dimensionsTotal = rs.value("dimensions_total").convert<std::string>();
            shipment->orderNumber = rs.value("order_number").convert<std::string>();
            shipment->customerName = rs.value("customer_name").convert<std::string>();
            
            shipments.push_back(std::move(shipment));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findCompletedShipments: " + e.displayText());
    }
    
    return shipments;
}

std::vector<std::unique_ptr<models::Shipment>> ShipmentRepository::findShipmentsDueToday()
{
    auto connection = acquireConnection();
    std::vector<std::unique_ptr<models::Shipment>> shipments;
    
    try
    {
        std::string today = DateUtils::formatDate(DateUtils::now());
        
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT s.id, s.shipment_number, s.order_id, s.carrier, "
                  "s.tracking_number, s.shipping_method, s.shipping_cost, "
                  "s.shipment_date, s.estimated_arrival, s.actual_arrival, "
                  "s.status, s.notes, s.weight_total, s.dimensions_total, "
                  "co.order_number, co.customer_name "
                  "FROM " << TABLE_NAME << " s "
                  "JOIN customer_orders co ON co.id = s.order_id "
                  "WHERE s.estimated_arrival = $1 "
                  "AND s.status IN ('preparing', 'in_transit') "
                  "ORDER BY s.estimated_arrival",
            use(today),
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            auto shipment = std::make_unique<models::Shipment>();
            shipment->id = rs.value("id", 0).convert<long long>();
            shipment->shipmentNumber = rs.value("shipment_number").convert<std::string>();
            shipment->orderId = rs.value("order_id", 0).convert<long long>();
            shipment->carrier = rs.value("carrier").convert<std::string>();
            shipment->trackingNumber = rs.value("tracking_number").convert<std::string>();
            shipment->shippingMethod = rs.value("shipping_method").convert<std::string>();
            shipment->shippingCost = rs.value("shipping_cost", 0.0).convert<double>();
            shipment->shipmentDate = rs.value("shipment_date").convert<std::string>();
            shipment->estimatedArrival = rs.value("estimated_arrival").isEmpty() ? "" : rs.value("estimated_arrival").convert<std::string>();
            shipment->actualArrival = rs.value("actual_arrival").isEmpty() ? "" : rs.value("actual_arrival").convert<std::string>();
            
            std::string statusStr = rs.value("status").convert<std::string>();
            shipment->status = models::Shipment::stringToStatus(statusStr);
            
            shipment->notes = rs.value("notes").convert<std::string>();
            shipment->weightTotal = rs.value("weight_total", 0.0).convert<double>();
            shipment->dimensionsTotal = rs.value("dimensions_total").convert<std::string>();
            shipment->orderNumber = rs.value("order_number").convert<std::string>();
            shipment->customerName = rs.value("customer_name").convert<std::string>();
            
            shipments.push_back(std::move(shipment));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findShipmentsDueToday: " + e.displayText());
    }
    
    return shipments;
}

bool ShipmentRepository::updateStatus(long long id, models::ShipmentStatus newStatus)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        std::string statusStr = models::Shipment::statusToString(newStatus);
        long long idCopy = id;
        std::string statusStrCopy = statusStr;
        
        Poco::Data::Statement update(connection->getSession());
        update << "UPDATE " << TABLE_NAME << " SET status = $1 WHERE id = $2",
            use(statusStrCopy),
            use(idCopy);
        
        int rowsAffected = update.execute();
        
        commitTransaction(*connection);
        return rowsAffected > 0;
    }
    catch (const Poco::Exception& e)
    {
        rollbackTransaction(*connection);
        throw std::runtime_error("Database error in updateStatus: " + e.displayText());
    }
}

bool ShipmentRepository::updateTrackingInfo(long long id, const std::string& trackingNumber, const std::string& carrier)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        long long idCopy = id;
        std::string trackingNumberCopy = trackingNumber;
        std::string carrierCopy = carrier;
        
        Poco::Data::Statement update(connection->getSession());
        update << "UPDATE " << TABLE_NAME << " SET "
                  "tracking_number = $1, carrier = $2 WHERE id = $3",
            use(trackingNumberCopy),
            use(carrierCopy),
            use(idCopy);
        
        int rowsAffected = update.execute();
        
        commitTransaction(*connection);
        return rowsAffected > 0;
    }
    catch (const Poco::Exception& e)
    {
        rollbackTransaction(*connection);
        throw std::runtime_error("Database error in updateTrackingInfo: " + e.displayText());
    }
}

bool ShipmentRepository::updateShippingInfo(long long id, const std::string& method, double shippingCost)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        long long idCopy = id;
        std::string methodCopy = method;
        double costCopy = shippingCost;
        
        Poco::Data::Statement update(connection->getSession());
        update << "UPDATE " << TABLE_NAME << " SET "
                  "shipping_method = $1, shipping_cost = $2 WHERE id = $3",
            use(methodCopy),
            use(costCopy),
            use(idCopy);
        
        int rowsAffected = update.execute();
        
        commitTransaction(*connection);
        return rowsAffected > 0;
    }
    catch (const Poco::Exception& e)
    {
        rollbackTransaction(*connection);
        throw std::runtime_error("Database error in updateShippingInfo: " + e.displayText());
    }
}

bool ShipmentRepository::updateArrivalDates(long long id, const std::string& estimatedArrival, const std::string& actualArrival)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        long long idCopy = id;
        std::string estimatedCopy = estimatedArrival;
        std::string actualCopy = actualArrival;
        
        Poco::Data::Statement update(connection->getSession());
        update << "UPDATE " << TABLE_NAME << " SET "
                  "estimated_arrival = $1, actual_arrival = $2 WHERE id = $3",
            use(estimatedCopy),
            use(actualCopy),
            use(idCopy);
        
        int rowsAffected = update.execute();
        
        commitTransaction(*connection);
        return rowsAffected > 0;
    }
    catch (const Poco::Exception& e)
    {
        rollbackTransaction(*connection);
        throw std::runtime_error("Database error in updateArrivalDates: " + e.displayText());
    }
}

bool ShipmentRepository::updateWeightAndDimensions(long long id, double weight, const std::string& dimensions)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        long long idCopy = id;
        double weightCopy = weight;
        std::string dimensionsCopy = dimensions;
        
        Poco::Data::Statement update(connection->getSession());
        update << "UPDATE " << TABLE_NAME << " SET "
                  "weight_total = $1, dimensions_total = $2 WHERE id = $3",
            use(weightCopy),
            use(dimensionsCopy),
            use(idCopy);
        
        int rowsAffected = update.execute();
        
        commitTransaction(*connection);
        return rowsAffected > 0;
    }
    catch (const Poco::Exception& e)
    {
        rollbackTransaction(*connection);
        throw std::runtime_error("Database error in updateWeightAndDimensions: " + e.displayText());
    }
}

bool ShipmentRepository::markAsInTransit(long long id)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        long long idCopy = id;
        std::string statusStr = "in_transit";
        std::string shipmentDate = DateUtils::formatDateTime(DateUtils::now());
        
        Poco::Data::Statement update(connection->getSession());
        update << "UPDATE " << TABLE_NAME << " SET "
                  "status = $1, shipment_date = $2 WHERE id = $3",
            use(statusStr),
            use(shipmentDate),
            use(idCopy);
        
        int rowsAffected = update.execute();
        
        commitTransaction(*connection);
        return rowsAffected > 0;
    }
    catch (const Poco::Exception& e)
    {
        rollbackTransaction(*connection);
        throw std::runtime_error("Database error in markAsInTransit: " + e.displayText());
    }
}

bool ShipmentRepository::markAsDelivered(long long id)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        long long idCopy = id;
        std::string statusStr = "delivered";
        std::string actualArrival = DateUtils::formatDateTime(DateUtils::now());
        
        Poco::Data::Statement update(connection->getSession());
        update << "UPDATE " << TABLE_NAME << " SET "
                  "status = $1, actual_arrival = $2 WHERE id = $3",
            use(statusStr),
            use(actualArrival),
            use(idCopy);
        
        int rowsAffected = update.execute();
        
        commitTransaction(*connection);
        return rowsAffected > 0;
    }
    catch (const Poco::Exception& e)
    {
        rollbackTransaction(*connection);
        throw std::runtime_error("Database error in markAsDelivered: " + e.displayText());
    }
}

bool ShipmentRepository::markAsDelayed(long long id)
{
    return updateStatus(id, models::ShipmentStatus::DELAYED);
}

bool ShipmentRepository::cancelShipment(long long id)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        long long idCopy = id;
        
        auto shipment = findById(id);
        if (!shipment)
        {
            throw std::runtime_error("Shipment not found");
        }
        
        if (shipment->status == models::ShipmentStatus::DELIVERED || 
            shipment->status == models::ShipmentStatus::RETURNED)
        {
            throw std::runtime_error("Cannot cancel a completed shipment");
        }
        
        Poco::Data::Statement del(connection->getSession());
        del << "DELETE FROM " << TABLE_NAME << " WHERE id = $1",
            use(idCopy);
        
        int rowsAffected = del.execute();
        
        commitTransaction(*connection);
        return rowsAffected > 0;
    }
    catch (const Poco::Exception& e)
    {
        rollbackTransaction(*connection);
        throw std::runtime_error("Database error in cancelShipment: " + e.displayText());
    }
}

int ShipmentRepository::countByStatus(models::ShipmentStatus status)
{
    auto connection = acquireConnection();
    
    try
    {
        std::string statusStr = models::Shipment::statusToString(status);
        std::string statusStrCopy = statusStr;
        
        Poco::Data::Statement countStmt(connection->getSession());
        countStmt << "SELECT COUNT(*) FROM " << TABLE_NAME << " WHERE status = $1",
            use(statusStrCopy),
            now;
        
        Poco::Data::RecordSet rs(countStmt);
        if (rs.rowCount() > 0)
        {
            return rs.value(0, 0).convert<int>();
        }
        
        return 0;
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in countByStatus: " + e.displayText());
    }
}

int ShipmentRepository::countByCarrier(const std::string& carrier)
{
    auto connection = acquireConnection();
    
    try
    {
        std::string carrierCopy = carrier;
        
        Poco::Data::Statement countStmt(connection->getSession());
        countStmt << "SELECT COUNT(*) FROM " << TABLE_NAME << " WHERE carrier = $1",
            use(carrierCopy),
            now;
        
        Poco::Data::RecordSet rs(countStmt);
        if (rs.rowCount() > 0)
        {
            return rs.value(0, 0).convert<int>();
        }
        
        return 0;
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in countByCarrier: " + e.displayText());
    }
}

int ShipmentRepository::countByOrder(long long orderId)
{
    auto connection = acquireConnection();
    
    try
    {
        long long orderIdCopy = orderId;
        
        Poco::Data::Statement countStmt(connection->getSession());
        countStmt << "SELECT COUNT(*) FROM " << TABLE_NAME << " WHERE order_id = $1",
            use(orderIdCopy),
            now;
        
        Poco::Data::RecordSet rs(countStmt);
        if (rs.rowCount() > 0)
        {
            return rs.value(0, 0).convert<int>();
        }
        
        return 0;
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in countByOrder: " + e.displayText());
    }
}

double ShipmentRepository::getTotalShippingCost(const std::string& startDate, const std::string& endDate)
{
    auto connection = acquireConnection();
    
    try
    {
        std::string startDateCopy = startDate;
        std::string endDateCopy = endDate;
        
        Poco::Data::Statement sumStmt(connection->getSession());
        sumStmt << "SELECT COALESCE(SUM(shipping_cost), 0) FROM " << TABLE_NAME 
                << " WHERE shipment_date >= $1 AND shipment_date <= $2",
            use(startDateCopy),
            use(endDateCopy),
            now;
        
        Poco::Data::RecordSet rs(sumStmt);
        if (rs.rowCount() > 0)
        {
            return rs.value(0, 0.0).convert<double>();
        }
        
        return 0.0;
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in getTotalShippingCost: " + e.displayText());
    }
}

double ShipmentRepository::getAverageShippingCost()
{
    auto connection = acquireConnection();
    
    try
    {
        Poco::Data::Statement avgStmt(connection->getSession());
        avgStmt << "SELECT COALESCE(AVG(shipping_cost), 0) FROM " << TABLE_NAME,
            now;
        
        Poco::Data::RecordSet rs(avgStmt);
        if (rs.rowCount() > 0)
        {
            return rs.value(0, 0.0).convert<double>();
        }
        
        return 0.0;
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in getAverageShippingCost: " + e.displayText());
    }
}

int ShipmentRepository::getAverageTransitDays()
{
    auto connection = acquireConnection();
    
    try
    {
        Poco::Data::Statement avgStmt(connection->getSession());
        avgStmt << "SELECT COALESCE(AVG(EXTRACT(DAY FROM (actual_arrival::date - shipment_date::date))), 0) "
                << "FROM " << TABLE_NAME << " "
                << "WHERE status = 'delivered' AND actual_arrival IS NOT NULL",
            now;
        
        Poco::Data::RecordSet rs(avgStmt);
        if (rs.rowCount() > 0)
        {
            return rs.value(0, 0.0).convert<int>();
        }
        
        return 0;
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in getAverageTransitDays: " + e.displayText());
    }
}

std::unique_ptr<models::Shipment> ShipmentRepository::getShipmentWithOrderDetails(long long id)
{
    return findById(id);
}

Poco::JSON::Array ShipmentRepository::getShipmentStatistics()
{
    auto connection = acquireConnection();
    Poco::JSON::Array jsonArray;
    
    try
    {
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT status, COUNT(*) as count, "
                  "COALESCE(SUM(shipping_cost), 0) as total_cost, "
                  "COALESCE(AVG(shipping_cost), 0) as avg_cost "
                  "FROM " << TABLE_NAME << " "
                  "GROUP BY status "
                  "ORDER BY count DESC",
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            Poco::JSON::Object stats;
            stats.set("status", rs.value("status").convert<std::string>());
            stats.set("count", rs.value("count", 0).convert<int>());
            stats.set("total_cost", rs.value("total_cost", 0.0).convert<double>());
            stats.set("avg_cost", rs.value("avg_cost", 0.0).convert<double>());
            
            jsonArray.add(stats);
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in getShipmentStatistics: " + e.displayText());
    }
    
    return jsonArray;
}

Poco::JSON::Array ShipmentRepository::getCarrierPerformanceReport()
{
    auto connection = acquireConnection();
    Poco::JSON::Array jsonArray;
    
    try
    {
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT carrier, "
                  "COUNT(*) as total_shipments, "
                  "SUM(CASE WHEN status = 'delivered' THEN 1 ELSE 0 END) as delivered, "
                  "SUM(CASE WHEN status = 'delayed' THEN 1 ELSE 0 END) as delayed, "
                  "SUM(CASE WHEN status = 'returned' THEN 1 ELSE 0 END) as returned, "
                  "COALESCE(SUM(shipping_cost), 0) as total_cost, "
                  "COALESCE(AVG(shipping_cost), 0) as avg_cost, "
                  "COALESCE(AVG("
                  "  CASE WHEN actual_arrival IS NOT NULL AND shipment_date IS NOT NULL "
                  "  THEN (actual_arrival::date - shipment_date::date) "
                  "  ELSE NULL END"
                  "), 0) as avg_transit_days "
                  "FROM " << TABLE_NAME << " "
                  "GROUP BY carrier "
                  "ORDER BY total_shipments DESC",
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            Poco::JSON::Object report;
            report.set("carrier", rs.value("carrier").convert<std::string>());
            report.set("total_shipments", rs.value("total_shipments", 0).convert<int>());
            report.set("delivered", rs.value("delivered", 0).convert<int>());
            report.set("delayed", rs.value("delayed", 0).convert<int>());
            report.set("returned", rs.value("returned", 0).convert<int>());
            report.set("total_cost", rs.value("total_cost", 0.0).convert<double>());
            report.set("avg_cost", rs.value("avg_cost", 0.0).convert<double>());
            report.set("avg_transit_days", rs.value("avg_transit_days", 0).convert<int>());
            
            int delivered = rs.value("delivered", 0).convert<int>();
            int total = rs.value("total_shipments", 0).convert<int>();
            double deliveryRate = total > 0 ? (static_cast<double>(delivered) / total) * 100 : 0;
            report.set("delivery_rate", deliveryRate);
            
            jsonArray.add(report);
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in getCarrierPerformanceReport: " + e.displayText());
    }
    
    return jsonArray;
}

Poco::JSON::Array ShipmentRepository::getShippingCostAnalysis(const std::string& startDate, const std::string& endDate)
{
    auto connection = acquireConnection();
    Poco::JSON::Array jsonArray;
    
    try
    {
        std::string startDateCopy = startDate;
        std::string endDateCopy = endDate;
        
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT shipping_method, carrier, "
                  "COUNT(*) as shipment_count, "
                  "COALESCE(SUM(shipping_cost), 0) as total_cost, "
                  "COALESCE(AVG(shipping_cost), 0) as avg_cost, "
                  "COALESCE(MIN(shipping_cost), 0) as min_cost, "
                  "COALESCE(MAX(shipping_cost), 0) as max_cost "
                  "FROM " << TABLE_NAME << " "
                  "WHERE shipment_date >= $1 AND shipment_date <= $2 "
                  "GROUP BY shipping_method, carrier "
                  "ORDER BY total_cost DESC",
            use(startDateCopy),
            use(endDateCopy),
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            Poco::JSON::Object analysis;
            analysis.set("shipping_method", rs.value("shipping_method").convert<std::string>());
            analysis.set("carrier", rs.value("carrier").convert<std::string>());
            analysis.set("shipment_count", rs.value("shipment_count", 0).convert<int>());
            analysis.set("total_cost", rs.value("total_cost", 0.0).convert<double>());
            analysis.set("avg_cost", rs.value("avg_cost", 0.0).convert<double>());
            analysis.set("min_cost", rs.value("min_cost", 0.0).convert<double>());
            analysis.set("max_cost", rs.value("max_cost", 0.0).convert<double>());
            
            jsonArray.add(analysis);
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in getShippingCostAnalysis: " + e.displayText());
    }
    
    return jsonArray;
}

bool ShipmentRepository::shipmentNumberExists(const std::string& shipmentNumber)
{
    auto connection = acquireConnection();
    
    try
    {
        std::string shipmentNumberCopy = shipmentNumber;
        
        Poco::Data::Statement check(connection->getSession());
        check << "SELECT COUNT(*) FROM " << TABLE_NAME << " WHERE shipment_number = $1",
            use(shipmentNumberCopy),
            now;
        
        Poco::Data::RecordSet rs(check);
        if (rs.rowCount() > 0)
        {
            return rs.value(0, 0).convert<int>() > 0;
        }
        
        return false;
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in shipmentNumberExists: " + e.displayText());
    }
}

std::vector<std::pair<long long, std::string>> ShipmentRepository::getActiveShipmentNumbers()
{
    auto connection = acquireConnection();
    std::vector<std::pair<long long, std::string>> result;
    
    try
    {
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT id, shipment_number FROM " << TABLE_NAME 
                << " WHERE status IN ('preparing', 'in_transit') "
                << "ORDER BY shipment_number",
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            result.emplace_back(
                rs.value("id", 0).convert<long long>(),
                rs.value("shipment_number").convert<std::string>()
            );
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in getActiveShipmentNumbers: " + e.displayText());
    }
    
    return result;
}

std::vector<std::string> ShipmentRepository::getUniqueCarriers()
{
    auto connection = acquireConnection();
    std::vector<std::string> carriers;
    
    try
    {
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT DISTINCT carrier FROM " << TABLE_NAME 
                << " ORDER BY carrier",
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            carriers.push_back(rs.value("carrier").convert<std::string>());
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in getUniqueCarriers: " + e.displayText());
    }
    
    return carriers;
}

models::Shipment ShipmentRepository::mapRowToShipment(Poco::Data::Row& row) const
{
    models::Shipment shipment;
    shipment.id = row.get(0).convert<long long>();
    shipment.shipmentNumber = row.get(1).convert<std::string>();
    shipment.orderId = row.get(2).convert<long long>();
    shipment.carrier = row.get(3).convert<std::string>();
    shipment.trackingNumber = row.get(4).convert<std::string>();
    shipment.shippingMethod = row.get(5).convert<std::string>();
    shipment.shippingCost = row.get(6).convert<double>();
    shipment.shipmentDate = row.get(7).convert<std::string>();
    shipment.estimatedArrival = row.get(8).convert<std::string>();
    shipment.actualArrival = row.get(9).convert<std::string>();
    
    std::string statusStr = row.get(10).convert<std::string>();
    shipment.status = models::Shipment::stringToStatus(statusStr);
    
    shipment.notes = row.get(11).convert<std::string>();
    shipment.weightTotal = row.get(12).convert<double>();
    shipment.dimensionsTotal = row.get(13).convert<std::string>();
    
    return shipment;
}

std::string ShipmentRepository::generateShipmentNumber()
{
    auto connection = acquireConnection();
    
    try
    {
        Poco::Data::Statement countStmt(connection->getSession());
        countStmt << "SELECT COUNT(*) FROM " << TABLE_NAME 
                  << " WHERE shipment_date::date = CURRENT_DATE",
            now;
        
        Poco::Data::RecordSet rs(countStmt);
        int count = rs.rowCount() > 0 ? rs.value(0, 0).convert<int>() : 0;
        
        std::string today = DateUtils::formatDate(DateUtils::now(), "YYYYMMDD");
        return "SHIP-" + today + "-" + std::to_string(count + 1).substr(0, 4);
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in generateShipmentNumber: " + e.displayText());
    }
}

} // namespace database::repositories
