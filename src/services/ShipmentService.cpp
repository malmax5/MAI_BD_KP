#include "ShipmentService.hpp"
#include "../database/ConnectionPool.hpp"
#include <Poco/JSON/Parser.h>
#include <Poco/JSON/Stringifier.h>
#include <Poco/DateTimeFormatter.h>
#include <Poco/DateTimeFormat.h>
#include <algorithm>
#include <sstream>

namespace warehouse_backend::services
{

ShipmentCreationResult::ShipmentCreationResult()
    : success(false), shipmentId(0), shipment(nullptr)
{
    
}

Poco::JSON::Object ShipmentCreationResult::toJson() const
{
    Poco::JSON::Object result;
    result.set("success", success);
    result.set("message", message);
    result.set("shipmentId", static_cast<Poco::Int64>(shipmentId));
    result.set("shipmentNumber", shipmentNumber);
    
    if (shipment)
    {
        result.set("shipment", shipment->toJson());
    }
    
    if (!details.size())
    {
        result.set("details", details);
    }
    
    return result;
}

Poco::JSON::Object ShipmentUpdateResult::toJson() const
{
    Poco::JSON::Object result;
    result.set("success", success);
    result.set("message", message);
    
    if (updatedShipment)
    {
        result.set("shipment", updatedShipment->toJson());
    }
    
    Poco::JSON::Array changesArray;
    for (const auto& change : changes)
    {
        changesArray.add(change);
    }
    result.set("changes", changesArray);
    
    return result;
}

ShipmentService::ShipmentService()
    : shipmentRepository(std::make_unique<database::repositories::ShipmentRepository>()),
      orderRepository(std::make_unique<database::repositories::CustomerOrderRepository>()),
      itemRepository(std::make_unique<database::repositories::OrderItemRepository>()),
      batchRepository(std::make_unique<database::repositories::ProductBatchRepository>()),
      auditRepository(std::make_unique<database::repositories::AuditLogRepository>())
{
}

ShipmentCreationResult ShipmentService::createShipment(long long orderId,
                                                     const std::string& carrier,
                                                     const std::string& shippingMethod,
                                                     double shippingCost,
                                                     const std::string& estimatedArrival,
                                                     long long createdBy)
{
    ShipmentCreationResult result;
    
    try
    {
        auto order = orderRepository->findById(orderId);
        if (!order)
        {
            result.success = false;
            result.message = "Order not found with ID: " + std::to_string(orderId);
            return result;
        }
        
        std::string errorMessage;
        if (!canShipOrder(orderId, errorMessage))
        {
            result.success = false;
            result.message = "Cannot create shipment: " + errorMessage;
            return result;
        }
        
        double weight = calculateShipmentWeight(orderId);
        
        database::models::Shipment shipment;
        shipment.orderId = orderId;
        shipment.carrier = carrier;
        shipment.shippingMethod = shippingMethod;
        shipment.shippingCost = shippingCost;
        shipment.weightTotal = weight;
        shipment.estimatedArrival = estimatedArrival;
        shipment.status = database::models::ShipmentStatus::PREPARING;
        shipment.shipmentDate = utils::DateUtils::formatDateTime(utils::DateUtils::now());
        
        long long shipmentId = shipmentRepository->create(shipment);
        
        if (shipmentId > 0)
        {
            result.success = true;
            result.shipmentId = shipmentId;
            result.message = "Shipment created successfully";
            
            result.shipment = shipmentRepository->findById(shipmentId);
            if (result.shipment)
            {
                result.shipmentNumber = result.shipment->shipmentNumber;
                
                logShipmentAudit("CREATE", shipmentId, createdBy,
                                "Shipment created for order " + std::to_string(orderId));
                
                updateOrderStatusOnShipment(orderId, shipmentId);
                
                result.details.set("orderId", static_cast<Poco::Int64>(orderId));
                result.details.set("carrier", carrier);
                result.details.set("shippingMethod", shippingMethod);
                result.details.set("shippingCost", shippingCost);
                result.details.set("estimatedArrival", estimatedArrival);
                result.details.set("weight", weight);
            }
        }
        else
        {
            result.success = false;
            result.message = "Failed to create shipment";
        }
    }
    catch (const std::exception& e)
    {
        result.success = false;
        result.message = "Error creating shipment: " + std::string(e.what());
    }
    
    return result;
}

ShipmentUpdateResult ShipmentService::updateShipmentStatus(long long shipmentId,
                                                          const std::string& newStatus,
                                                          long long updatedBy,
                                                          const std::string& notes)
{
    ShipmentUpdateResult result;
    
    try
    {
        auto currentShipment = shipmentRepository->findById(shipmentId);
        if (!currentShipment)
        {
            result.success = false;
            result.message = "Shipment not found with ID: " + std::to_string(shipmentId);
            return result;
        }
        
        auto newStatusEnum = database::models::Shipment::stringToStatus(newStatus);
        
        std::string oldStatus = database::models::Shipment::statusToString(currentShipment->status);
        
        bool updateSuccess = shipmentRepository->updateStatus(shipmentId, newStatusEnum);
        
        if (updateSuccess)
        {
            result.success = true;
            result.message = "Shipment status updated successfully";
            result.updatedShipment = shipmentRepository->findById(shipmentId);
            result.changes.push_back("Status changed from " + oldStatus + " to " + newStatus);
            
            std::string auditDetails = "Status updated: " + oldStatus + " -> " + newStatus;
            if (!notes.empty())
            {
                auditDetails += ". Notes: " + notes;
                database::models::Shipment updatedShipment = *currentShipment;
                updatedShipment.notes = notes;
                shipmentRepository->update(shipmentId, updatedShipment);
                result.changes.push_back("Notes updated");
            }
            
            logShipmentAudit("UPDATE_STATUS", shipmentId, updatedBy, auditDetails);
            
            if (newStatusEnum == database::models::ShipmentStatus::DELIVERED)
            {
                std::string actualArrival = utils::DateUtils::formatDateTime(utils::DateUtils::now());
                shipmentRepository->updateArrivalDates(shipmentId, currentShipment->estimatedArrival, actualArrival);
                result.changes.push_back("Actual arrival date set to " + actualArrival);
                
                orderRepository->markAsDelivered(currentShipment->orderId);
            }
        }
        else
        {
            result.success = false;
            result.message = "Failed to update shipment status";
        }
    }
    catch (const std::exception& e)
    {
        result.success = false;
        result.message = "Error updating shipment status: " + std::string(e.what());
    }
    
    return result;
}

ShipmentUpdateResult ShipmentService::updateTrackingInfo(long long shipmentId,
                                                        const std::string& trackingNumber,
                                                        const std::string& carrier,
                                                        long long updatedBy)
{
    ShipmentUpdateResult result;
    
    try
    {
        auto currentShipment = shipmentRepository->findById(shipmentId);
        if (!currentShipment)
        {
            result.success = false;
            result.message = "Shipment not found with ID: " + std::to_string(shipmentId);
            return result;
        }
        
        bool updateSuccess = shipmentRepository->updateTrackingInfo(shipmentId, trackingNumber, carrier);
        
        if (updateSuccess)
        {
            result.success = true;
            result.message = "Tracking information updated successfully";
            result.updatedShipment = shipmentRepository->findById(shipmentId);
            
            std::string changes;
            if (!trackingNumber.empty())
            {
                changes += "Tracking number: " + trackingNumber;
            }
            if (!carrier.empty())
            {
                if (!changes.empty()) changes += ", ";
                changes += "Carrier: " + carrier;
            }
            result.changes.push_back(changes);
            
            logShipmentAudit("UPDATE_TRACKING", shipmentId, updatedBy,
                            "Tracking info updated: " + changes);
        }
        else
        {
            result.success = false;
            result.message = "Failed to update tracking information";
        }
    }
    catch (const std::exception& e)
    {
        result.success = false;
        result.message = "Error updating tracking information: " + std::string(e.what());
    }
    
    return result;
}

ShipmentUpdateResult ShipmentService::addShipmentNotes(long long shipmentId,
                                                      const std::string& notes,
                                                      long long updatedBy)
{
    ShipmentUpdateResult result;
    
    try
    {
        auto currentShipment = shipmentRepository->findById(shipmentId);
        if (!currentShipment)
        {
            result.success = false;
            result.message = "Shipment not found with ID: " + std::to_string(shipmentId);
            return result;
        }
        
        database::models::Shipment updatedShipment = *currentShipment;
        if (!updatedShipment.notes.isNull())
        {
            updatedShipment.notes = updatedShipment.notes.value() + "\n---\n" + notes;
        }
        else
        {
            updatedShipment.notes = notes;
        }
        
        bool updateSuccess = shipmentRepository->update(shipmentId, updatedShipment);
        
        if (updateSuccess)
        {
            result.success = true;
            result.message = "Shipment notes added successfully";
            result.updatedShipment = shipmentRepository->findById(shipmentId);
            result.changes.push_back("Notes added: " + notes);
            
            logShipmentAudit("ADD_NOTES", shipmentId, updatedBy,
                            "Notes added to shipment");
        }
        else
        {
            result.success = false;
            result.message = "Failed to add shipment notes";
        }
    }
    catch (const std::exception& e)
    {
        result.success = false;
        result.message = "Error adding shipment notes: " + std::string(e.what());
    }
    
    return result;
}

bool ShipmentService::cancelShipment(long long shipmentId,
                                   long long cancelledBy,
                                   const std::string& reason)
{
    try
    {
        auto shipment = shipmentRepository->findById(shipmentId);
        if (!shipment)
        {
            return false;
        }
        
        bool cancelSuccess = shipmentRepository->cancelShipment(shipmentId);
        
        if (cancelSuccess)
        {
            std::string auditDetails = "Shipment cancelled";
            if (!reason.empty())
            {
                auditDetails += ": " + reason;
            }
            
            logShipmentAudit("CANCEL", shipmentId, cancelledBy, auditDetails);
            return true;
        }
    }
    catch (const std::exception&)
    {
    }
    
    return false;
}

bool ShipmentService::markShipmentAsDelivered(long long shipmentId,
                                            const std::string& actualArrival,
                                            long long deliveredBy)
{
    try
    {
        auto shipment = shipmentRepository->findById(shipmentId);
        if (!shipment)
        {
            return false;
        }
        
        bool deliverSuccess = false;
        if (actualArrival.empty())
        {
            deliverSuccess = shipmentRepository->markAsDelivered(shipmentId);
        }
        else
        {
            deliverSuccess = shipmentRepository->updateArrivalDates(shipmentId,
                                                                   shipment->estimatedArrival,
                                                                   actualArrival);
        }
        
        if (deliverSuccess)
        {
            logShipmentAudit("DELIVERED", shipmentId, deliveredBy,
                            "Shipment marked as delivered");
            return true;
        }
    }
    catch (const std::exception&)
    {
    }
    
    return false;
}

Poco::JSON::Object ShipmentService::getShipmentDetails(long long shipmentId)
{
    Poco::JSON::Object result;
    
    try
    {
        auto shipment = shipmentRepository->findById(shipmentId);
        if (shipment)
        {
            result = shipment->toJson();
            
            auto order = orderRepository->findById(shipment->orderId);
            if (order)
            {
                Poco::JSON::Object orderInfo;
                orderInfo.set("orderNumber", order->orderNumber);
                orderInfo.set("customerName", order->customerName);
                orderInfo.set("customerEmail", order->customerEmail);
                orderInfo.set("customerPhone", order->customerPhone);
                orderInfo.set("shippingAddress", order->shippingAddress);
                orderInfo.set("status", database::models::CustomerOrder::statusToString(order->status));
                
                result.set("orderInfo", orderInfo);
            }
        }
        else
        {
            result.set("error", "Shipment not found with ID: " + std::to_string(shipmentId));
        }
    }
    catch (const std::exception& e)
    {
        result.set("error", "Error retrieving shipment details: " + std::string(e.what()));
    }
    
    return result;
}

Poco::JSON::Object ShipmentService::getShipmentWithOrderDetails(long long shipmentId)
{
    Poco::JSON::Object result;
    
    try
    {
        auto shipment = shipmentRepository->getShipmentWithOrderDetails(shipmentId);
        if (shipment)
        {
            result = shipment->toJson();
            
            auto orderItems = itemRepository->findByOrderId(shipment->orderId);
            if (!orderItems.empty())
            {
                Poco::JSON::Array itemsArray;
                for (const auto& item : orderItems)
                {
                    itemsArray.add(item->toJson());
                }
                result.set("orderItems", itemsArray);
            }
        }
        else
        {
            result.set("error", "Shipment not found with ID: " + std::to_string(shipmentId));
        }
    }
    catch (const std::exception& e)
    {
        result.set("error", "Error retrieving shipment with order details: " + std::string(e.what()));
    }
    
    return result;
}

Poco::JSON::Array ShipmentService::findShipmentsByOrder(long long orderId)
{
    Poco::JSON::Array result;
    
    try
    {
        auto shipments = shipmentRepository->findByOrderId(orderId);
        
        for (const auto& shipment : shipments)
        {
            result.add(shipment->toJson());
        }
    }
    catch (const std::exception& e)
    {
        Poco::JSON::Object error;
        error.set("error", "Error retrieving shipments by order: " + std::string(e.what()));
        result.add(error);
    }
    
    return result;
}

Poco::JSON::Array ShipmentService::findShipmentsByStatus(const std::string& status,
                                                        int page,
                                                        int pageSize)
{
    Poco::JSON::Array result;
    
    try
    {
        auto statusEnum = database::models::Shipment::stringToStatus(status);
        auto shipments = shipmentRepository->findByStatus(statusEnum);
        
        int startIndex = (page - 1) * pageSize;
        int endIndex = std::min(static_cast<int>(shipments.size()), startIndex + pageSize);
        
        for (int i = startIndex; i < endIndex && i < static_cast<int>(shipments.size()); ++i)
        {
            result.add(shipments[i]->toJson());
        }
    }
    catch (const std::exception& e)
    {
        Poco::JSON::Object error;
        error.set("error", "Error retrieving shipments by status: " + std::string(e.what()));
        result.add(error);
    }
    
    return result;
}

Poco::JSON::Array ShipmentService::findShipmentsByCarrier(const std::string& carrier,
                                                         int page,
                                                         int pageSize)
{
    Poco::JSON::Array result;
    
    try
    {
        auto shipments = shipmentRepository->findByCarrier(carrier);
        
        int startIndex = (page - 1) * pageSize;
        int endIndex = std::min(static_cast<int>(shipments.size()), startIndex + pageSize);
        
        for (int i = startIndex; i < endIndex && i < static_cast<int>(shipments.size()); ++i)
        {
            result.add(shipments[i]->toJson());
        }
    }
    catch (const std::exception& e)
    {
        Poco::JSON::Object error;
        error.set("error", "Error retrieving shipments by carrier: " + std::string(e.what()));
        result.add(error);
    }
    
    return result;
}

Poco::JSON::Array ShipmentService::findShipmentsByDateRange(const std::string& startDate,
                                                           const std::string& endDate,
                                                           int page,
                                                           int pageSize)
{
    Poco::JSON::Array result;
    
    try
    {
        auto shipments = shipmentRepository->findByDateRange(startDate, endDate);
        
        int startIndex = (page - 1) * pageSize;
        int endIndex = std::min(static_cast<int>(shipments.size()), startIndex + pageSize);
        
        for (int i = startIndex; i < endIndex && i < static_cast<int>(shipments.size()); ++i)
        {
            result.add(shipments[i]->toJson());
        }
    }
    catch (const std::exception& e)
    {
        Poco::JSON::Object error;
        error.set("error", "Error retrieving shipments by date range: " + std::string(e.what()));
        result.add(error);
    }
    
    return result;
}

Poco::JSON::Array ShipmentService::getDelayedShipments(int daysThreshold)
{
    Poco::JSON::Array result;
    
    try
    {
        auto shipments = shipmentRepository->findDelayedShipments();
        
        for (const auto& shipment : shipments)
        {
            auto estimatedDate = utils::DateUtils::parseDate(shipment->estimatedArrival);
            auto currentDate = utils::DateUtils::now();
            int daysDelayed = utils::DateUtils::daysBetween(estimatedDate, currentDate);
            
            if (daysDelayed >= daysThreshold)
            {
                auto shipmentJson = shipment->toJson();
                shipmentJson.set("daysDelayed", daysDelayed);
                result.add(shipmentJson);
            }
        }
    }
    catch (const std::exception& e)
    {
        Poco::JSON::Object error;
        error.set("error", "Error retrieving delayed shipments: " + std::string(e.what()));
        result.add(error);
    }
    
    return result;
}

Poco::JSON::Array ShipmentService::getShipmentsDueToday()
{
    Poco::JSON::Array result;
    
    try
    {
        auto shipments = shipmentRepository->findShipmentsDueToday();
        
        for (const auto& shipment : shipments)
        {
            result.add(shipment->toJson());
        }
    }
    catch (const std::exception& e)
    {
        Poco::JSON::Object error;
        error.set("error", "Error retrieving shipments due today: " + std::string(e.what()));
        result.add(error);
    }
    
    return result;
}

Poco::JSON::Array ShipmentService::getInTransitShipments()
{
    Poco::JSON::Array result;
    
    try
    {
        auto shipments = shipmentRepository->findInTransitShipments();
        
        for (const auto& shipment : shipments)
        {
            result.add(shipment->toJson());
        }
    }
    catch (const std::exception& e)
    {
        Poco::JSON::Object error;
        error.set("error", "Error retrieving in-transit shipments: " + std::string(e.what()));
        result.add(error);
    }
    
    return result;
}

Poco::JSON::Object ShipmentService::getShipmentStatistics()
{
    Poco::JSON::Object stats;
    
    try
    {
        int totalShipments = shipmentRepository->count();
        int preparingShipments = shipmentRepository->countByStatus(
            database::models::ShipmentStatus::PREPARING);
        int inTransitShipments = shipmentRepository->countByStatus(
            database::models::ShipmentStatus::IN_TRANSIT);
        int deliveredShipments = shipmentRepository->countByStatus(
            database::models::ShipmentStatus::DELIVERED);
        int delayedShipments = shipmentRepository->countByStatus(
            database::models::ShipmentStatus::DELAYED);
        int returnedShipments = shipmentRepository->countByStatus(
            database::models::ShipmentStatus::RETURNED);
        
        double totalShippingCost = shipmentRepository->getTotalShippingCost("", "");
        double avgShippingCost = shipmentRepository->getAverageShippingCost();
        int avgTransitDays = shipmentRepository->getAverageTransitDays();
        
        stats.set("totalShipments", totalShipments);
        stats.set("preparingShipments", preparingShipments);
        stats.set("inTransitShipments", inTransitShipments);
        stats.set("deliveredShipments", deliveredShipments);
        stats.set("delayedShipments", delayedShipments);
        stats.set("returnedShipments", returnedShipments);
        stats.set("totalShippingCost", totalShippingCost);
        stats.set("averageShippingCost", avgShippingCost);
        stats.set("averageTransitDays", avgTransitDays);
        stats.set("lastUpdated", utils::DateUtils::formatDateTime(utils::DateUtils::now()));
    }
    catch (const std::exception& e)
    {
        stats.set("error", "Error calculating shipment statistics: " + std::string(e.what()));
    }
    
    return stats;
}

Poco::JSON::Array ShipmentService::getCarrierPerformanceReport()
{
    try
    {
        return shipmentRepository->getCarrierPerformanceReport();
    }
    catch (const std::exception&)
    {
        Poco::JSON::Array result;
        Poco::JSON::Object error;
        error.set("error", "Error generating carrier performance report");
        result.add(error);
        return result;
    }
}

Poco::JSON::Array ShipmentService::getShippingCostAnalysis(const std::string& startDate,
                                                          const std::string& endDate)
{
    try
    {
        return shipmentRepository->getShippingCostAnalysis(startDate, endDate);
    }
    catch (const std::exception&)
    {
        Poco::JSON::Array result;
        Poco::JSON::Object error;
        error.set("error", "Error generating shipping cost analysis");
        result.add(error);
        return result;
    }
}

bool ShipmentService::validateShipmentForOrder(long long orderId, std::string& errorMessage)
{
    try
    {
        auto order = orderRepository->findById(orderId);
        if (!order)
        {
            errorMessage = "Order not found";
            return false;
        }
        
        if (order->status != database::models::OrderStatus::PACKED &&
            order->status != database::models::OrderStatus::PROCESSING)
        {
            errorMessage = "Order must be in PACKED or PROCESSING status to ship";
            return false;
        }
        
        auto orderItems = itemRepository->findByOrderId(orderId);
        for (const auto& item : orderItems)
        {
            if (item->pickingStatus != database::models::PickingStatus::PACKED)
            {
                errorMessage = "Not all items are packed for shipment";
                return false;
            }
        }
        
        return true;
    }
    catch (const std::exception&)
    {
        errorMessage = "Error validating shipment";
        return false;
    }
}

bool ShipmentService::canShipOrder(long long orderId, std::string& errorMessage)
{
    return validateShipmentForOrder(orderId, errorMessage);
}

double ShipmentService::calculateShippingCost(long long orderId,
                                            const std::string& shippingMethod,
                                            double weight)
{
    try
    {
        std::map<std::string, double> baseRates = {
            {"standard", 5.0},
            {"express", 10.0},
            {"priority", 15.0},
            {"freight", 20.0}
        };
        
        if (weight <= 0.0)
        {
            weight = calculateShipmentWeight(orderId);
        }
        
        double baseCost = 0.0;
        auto it = baseRates.find(shippingMethod);
        if (it != baseRates.end())
        {
            baseCost = it->second;
        }
        else
        {
            baseCost = 10.0;
        }
        
        double weightSurcharge = weight * 0.5;
        
        return baseCost + weightSurcharge;
    }
    catch (const std::exception&)
    {
        return 0.0;
    }
}

std::string ShipmentService::generateShipmentNumber()
{
    return "SHIP-" + std::to_string(std::time(nullptr));
}

double ShipmentService::calculateShipmentWeight(long long orderId)
{
    try
    {
        double totalWeight = 0.0;
        
        auto orderItems = itemRepository->findByOrderId(orderId);
        for (const auto& item : orderItems)
        {
            auto product = batchRepository->findById(item->batchId);
            if (product)
            {
                totalWeight += item->quantityOrdered * 0.5;
            }
        }
        
        return totalWeight;
    }
    catch (const std::exception&)
    {
        return 0.0;
    }
}

void ShipmentService::updateOrderStatusOnShipment(long long orderId, long long shipmentId)
{
    try
    {
        orderRepository->markAsShipped(orderId);
        
        logShipmentAudit("ORDER_STATUS_UPDATE", shipmentId, 0,
                        "Order status updated to SHIPPED after shipment creation");
    }
    catch (const std::exception&)
    {
    }
}

void ShipmentService::logShipmentAudit(const std::string& action,
                                      long long shipmentId,
                                      long long changedBy,
                                      const std::string& details)
{
    try
    {
        database::models::AuditLog auditLog;
        auditLog.tableName = "shipments";
        auditLog.recordId = shipmentId;
        auditLog.action = database::models::AuditAction::UPDATE;
        auditLog.changedBy = changedBy;
        auditLog.description = "Shipment " + action + ": " + details;
        
        auditRepository->create(auditLog);
    }
    catch (const std::exception&)
    {
    }
}

std::string ShipmentService::shipmentToJsonString(const database::models::Shipment& shipment) const
{
    try
    {
        auto json = shipment.toJson();
        return utils::JsonUtils::objectToString(json, false);
    }
    catch (const std::exception&)
    {
        return "{}";
    }
}

} // namespace services
