#include "ShipmentController.hpp"
#include "../utils/JsonUtils.hpp"
#include "../utils/Validator.hpp"
#include "../utils/DateUtils.hpp"
#include <Poco/Net/HTTPRequestHandler.h>
#include <Poco/Net/HTTPServerRequest.h>
#include <Poco/Net/HTTPServerResponse.h>
#include <Poco/JSON/Parser.h>
#include <Poco/StreamCopier.h>
#include <Poco/URI.h>
#include <iostream>
#include <sstream>
#include <chrono>

namespace warehouse_backend::controllers
{

ShipmentController::ShipmentController()
    : shipmentService(std::make_unique<services::ShipmentService>()),
      authService(std::make_unique<services::AuthService>())
{
}

void ShipmentController::handleRequest(Poco::Net::HTTPServerRequest& request, 
                                     Poco::Net::HTTPServerResponse& response)
{
    auto startTime = std::chrono::steady_clock::now();
    
    try
    {
        setCorsHeaders(response);
        
        if (request.getMethod() == "OPTIONS")
        {
            response.setStatus(Poco::Net::HTTPResponse::HTTP_OK);
            response.send();
            return;
        }
        
        std::string method = request.getMethod();
        std::string uri = request.getURI();
        logRequest(request, method, uri);
        
        std::string validationError;
        if (!validateRequest(request, response, validationError))
        {
            sendErrorResponse(response, validationError, 
                            Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
            return;
        }
        
        std::string authError;
        if (!authorizeRequest(request, response, authError))
        {
            sendErrorResponse(response, authError, 
                            Poco::Net::HTTPResponse::HTTP_UNAUTHORIZED);
            return;
        }
        
        Poco::URI pocoUri(request.getURI());
        std::string endpoint = pocoUri.getPath();
        
        std::string basePath = "/api/v1/shipments";
        if (endpoint.find("/api/v1/") != 0)
        {
            basePath = "/api/shipments";
        }
        
        std::string shipmentIdStr = getPathParameter(uri, basePath + "/", 0);
        
        if ((endpoint.find("/api/v1/shipments/statistics") == 0 || 
             endpoint.find("/api/shipments/statistics") == 0) && 
            request.getMethod() == "GET")
        {
            handleGetShipmentStatistics(request, response);
        }
        else if ((endpoint.find("/api/v1/shipments/carrier-performance") == 0 || 
                  endpoint.find("/api/shipments/carrier-performance") == 0) && 
                 request.getMethod() == "GET")
        {
            handleGetCarrierPerformanceReport(request, response);
        }
        else if ((endpoint.find("/api/v1/shipments/shipping-cost-analysis") == 0 || 
                  endpoint.find("/api/shipments/shipping-cost-analysis") == 0) && 
                 request.getMethod() == "GET")
        {
            handleGetShippingCostAnalysis(request, response);
        }
        else if ((endpoint.find("/api/v1/shipments/delayed") == 0 || 
                  endpoint.find("/api/shipments/delayed") == 0) && 
                 request.getMethod() == "GET")
        {
            handleGetDelayedShipments(request, response);
        }
        else if ((endpoint.find("/api/v1/shipments/due-today") == 0 || 
                  endpoint.find("/api/shipments/due-today") == 0) && 
                 request.getMethod() == "GET")
        {
            handleGetShipmentsDueToday(request, response);
        }
        else if ((endpoint.find("/api/v1/shipments/order") == 0 || 
                  endpoint.find("/api/shipments/order") == 0) && 
                 request.getMethod() == "GET")
        {
            handleGetShipmentsByOrder(request, response);
        }
        else if ((endpoint.find("/api/v1/shipments/status") == 0 || 
                  endpoint.find("/api/shipments/status") == 0) && 
                 request.getMethod() == "GET")
        {
            handleGetShipmentsByStatus(request, response);
        }
        else if ((endpoint.find("/api/v1/shipments/carrier") == 0 || 
                  endpoint.find("/api/shipments/carrier") == 0) && 
                 request.getMethod() == "GET")
        {
            handleGetShipmentsByCarrier(request, response);
        }
        else if ((endpoint == "/api/v1/shipments" || endpoint == "/api/shipments") && 
                 request.getMethod() == "GET")
        {
            handleGetShipments(request, response);
        }
        else if ((endpoint == "/api/v1/shipments" || endpoint == "/api/shipments") && 
                 request.getMethod() == "POST")
        {
            handleCreateShipment(request, response);
        }
        else if ((endpoint.find("/api/v1/shipments/") == 0 || 
                  endpoint.find("/api/shipments/") == 0) && 
                 !shipmentIdStr.empty())
        {
            if (endpoint.find("/cancel") != std::string::npos && request.getMethod() == "POST")
            {
                handleCancelShipment(request, response);
            }
            else if (endpoint.find("/delivered") != std::string::npos && request.getMethod() == "POST")
            {
                handleMarkAsDelivered(request, response);
            }
            else if (endpoint.find("/status") != std::string::npos && request.getMethod() == "PUT")
            {
                handleUpdateShipmentStatus(request, response);
            }
            else if (endpoint.find("/tracking") != std::string::npos && request.getMethod() == "PUT")
            {
                handleUpdateTrackingInfo(request, response);
            }
            else if (request.getMethod() == "GET")
            {
                handleGetShipmentById(request, response);
            }
            else if (request.getMethod() == "PUT")
            {
                handleUpdateShipment(request, response);
            }
        }
        else
        {
            sendErrorResponse(response, "Endpoint not found", 
                            Poco::Net::HTTPResponse::HTTP_NOT_FOUND);
        }
        
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        logResponse(request, response, method, uri, duration.count());
    }
    catch (const Poco::Exception& e)
    {
        sendErrorResponse(response, "Server error: " + e.message(), 
                        Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
    catch (const std::exception& e)
    {
        sendErrorResponse(response, "Server error: " + std::string(e.what()), 
                        Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
}

void ShipmentController::handleGetShipments(Poco::Net::HTTPServerRequest& request, 
                                          Poco::Net::HTTPServerResponse& response)
{
    int page, pageSize;
    if (!getPaginationParameters(request, page, pageSize, 20))
    {
        sendErrorResponse(response, "Invalid pagination parameters", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    auto filters = getFilterParameters(request);
    
    std::vector<std::string> errors;
    if (!validateSearchParameters(filters, errors))
    {
        sendValidationErrorResponse(response, errors);
        return;
    }
    
    std::string authError;
    if (!canViewShipments(request, authError))
    {
        sendForbiddenResponse(response, authError);
        return;
    }
    
    try
    {
        Poco::JSON::Array shipmentsArray;
        
        if (filters.find("orderId") != filters.end())
        {
            long long orderId = std::stoll(filters["orderId"]);
            shipmentsArray = shipmentService->findShipmentsByOrder(orderId);
        }
        else if (filters.find("status") != filters.end())
        {
            std::string status = filters["status"];
            shipmentsArray = shipmentService->findShipmentsByStatus(status, page, pageSize);
        }
        else if (filters.find("carrier") != filters.end())
        {
            std::string carrier = filters["carrier"];
            shipmentsArray = shipmentService->findShipmentsByCarrier(carrier, page, pageSize);
        }
        else if (filters.find("startDate") != filters.end() && filters.find("endDate") != filters.end())
        {
            std::string startDate = filters["startDate"];
            std::string endDate = filters["endDate"];
            shipmentsArray = shipmentService->findShipmentsByDateRange(startDate, endDate, page, pageSize);
        }
        else
        {
            auto now = utils::DateUtils::now();
            std::string defaultStartDate = Poco::DateTimeFormatter::format(utils::DateUtils::addDays(now, -30), "%Y-%m-%d");
            std::string defaultEndDate = Poco::DateTimeFormatter::format(now, "%Y-%m-%d");
            shipmentsArray = shipmentService->findShipmentsByDateRange(defaultStartDate, defaultEndDate, page, pageSize);
        }
        
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object;
        dataPtr->set("shipments", shipmentsArray);
        sendSuccessResponse(response, "Shipments retrieved successfully", dataPtr);
    }
    catch (const std::exception& e)
    {
        sendErrorResponse(response, "Error retrieving shipments: " + std::string(e.what()), 
                        Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
}

void ShipmentController::handleGetShipmentById(Poco::Net::HTTPServerRequest& request, 
                                             Poco::Net::HTTPServerResponse& response)
{
    std::string shipmentIdStr = getPathParameter(request.getURI(), "/api/v1/shipments/", 0);
    if (shipmentIdStr.empty())
    {
        sendErrorResponse(response, "Shipment ID is required", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    long long shipmentId;
    try
    {
        shipmentId = std::stoll(shipmentIdStr);
    }
    catch (...)
    {
        sendErrorResponse(response, "Invalid shipment ID", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    std::string authError;
    if (!canViewShipments(request, authError))
    {
        sendForbiddenResponse(response, authError);
        return;
    }
    
    try
    {
        Poco::JSON::Object shipmentDetails = shipmentService->getShipmentDetails(shipmentId);
        
        if (shipmentDetails.has("error"))
        {
            std::string error = shipmentDetails.getValue<std::string>("error");
            if (error.find("Shipment not found") != std::string::npos)
            {
                sendNotFoundResponse(response, "Shipment");
            }
            else
            {
                sendErrorResponse(response, error, Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
            }
            return;
        }
        
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object(shipmentDetails);
        sendSuccessResponse(response, "Shipment retrieved successfully", dataPtr);
    }
    catch (const std::exception& e)
    {
        sendErrorResponse(response, "Error retrieving shipment: " + std::string(e.what()), 
                        Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
}

void ShipmentController::handleCreateShipment(Poco::Net::HTTPServerRequest& request, 
                                            Poco::Net::HTTPServerResponse& response)
{
    auto json = parseJsonBody(request);
    if (!json)
    {
        sendErrorResponse(response, "Invalid JSON body", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    std::vector<std::string> errors;
    if (!validateCreateShipmentData(json, errors))
    {
        sendValidationErrorResponse(response, errors);
        return;
    }
    
    long long orderId = utils::JsonUtils::getInt(*json, "order_id");
    
    std::string authError;
    if (!canCreateShipment(request, orderId, authError))
    {
        sendForbiddenResponse(response, authError);
        return;
    }
    
    try
    {
        std::string carrier = utils::JsonUtils::getString(*json, "carrier");
        std::string shippingMethod = utils::JsonUtils::getString(*json, "shipping_method");
        double shippingCost = utils::JsonUtils::getDouble(*json, "shipping_cost");
        std::string estimatedArrival = utils::JsonUtils::getString(*json, "estimated_arrival");
        
        long long createdBy = getCurrentUserId(request);
        
        auto result = shipmentService->createShipment(orderId, carrier, shippingMethod, 
                                                     shippingCost, estimatedArrival, createdBy);
        
        logShipmentEvent(createdBy, "CREATE_SHIPMENT", result.shipmentId,
                        getClientIpAddress(request), getUserAgent(request),
                        result.success, result.success ? "Shipment created successfully" : result.message);
        
        if (result.success)
        {
            Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object(result.details);
            sendSuccessResponse(response, "Shipment created successfully", dataPtr);
        }
        else
        {
            sendErrorResponse(response, result.message, 
                            Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        }
    }
    catch (const std::exception& e)
    {
        sendErrorResponse(response, "Error creating shipment: " + std::string(e.what()), 
                        Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
}

void ShipmentController::handleUpdateShipment(Poco::Net::HTTPServerRequest& request, 
                                            Poco::Net::HTTPServerResponse& response)
{
    std::string shipmentIdStr = getPathParameter(request.getURI(), "/api/v1/shipments/", 0);
    if (shipmentIdStr.empty())
    {
        sendErrorResponse(response, "Shipment ID is required", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    long long shipmentId;
    try
    {
        shipmentId = std::stoll(shipmentIdStr);
    }
    catch (...)
    {
        sendErrorResponse(response, "Invalid shipment ID", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    auto json = parseJsonBody(request);
    if (!json)
    {
        sendErrorResponse(response, "Invalid JSON body", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    std::vector<std::string> errors;
    if (!validateUpdateShipmentData(json, errors))
    {
        sendValidationErrorResponse(response, errors);
        return;
    }
    
    std::string authError;
    if (!canUpdateShipment(request, shipmentId, authError))
    {
        sendForbiddenResponse(response, authError);
        return;
    }
    
    logShipmentEvent(getCurrentUserId(request), "UPDATE_SHIPMENT", shipmentId,
                    getClientIpAddress(request), getUserAgent(request),
                    true, "Shipment updated");
    
    Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object;
    dataPtr->set("shipmentId", shipmentId);
    dataPtr->set("message", "Shipment update would be processed here");
    sendSuccessResponse(response, "Shipment update request accepted", dataPtr);
}

void ShipmentController::handleUpdateShipmentStatus(Poco::Net::HTTPServerRequest& request, 
                                                  Poco::Net::HTTPServerResponse& response)
{
    std::string shipmentIdStr = getPathParameter(request.getURI(), "/api/v1/shipments/", 0);
    shipmentIdStr = shipmentIdStr.substr(0, shipmentIdStr.find("/status"));
    
    if (shipmentIdStr.empty())
    {
        sendErrorResponse(response, "Shipment ID is required", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    long long shipmentId;
    try
    {
        shipmentId = std::stoll(shipmentIdStr);
    }
    catch (...)
    {
        sendErrorResponse(response, "Invalid shipment ID", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    auto json = parseJsonBody(request);
    if (!json)
    {
        sendErrorResponse(response, "Invalid JSON body", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    std::vector<std::string> errors;
    if (!validateStatusUpdateData(json, errors))
    {
        sendValidationErrorResponse(response, errors);
        return;
    }
    
    std::string newStatus = utils::JsonUtils::getString(*json, "status");
    std::string notes = utils::JsonUtils::getString(*json, "notes", "");
    
    std::string authError;
    if (!canUpdateShipment(request, shipmentId, authError))
    {
        sendForbiddenResponse(response, authError);
        return;
    }
    
    long long updatedBy = getCurrentUserId(request);
    
    auto result = shipmentService->updateShipmentStatus(shipmentId, newStatus, updatedBy, notes);
    
    logShipmentEvent(updatedBy, "UPDATE_SHIPMENT_STATUS", shipmentId,
                    getClientIpAddress(request), getUserAgent(request),
                    result.success, result.success ? "Shipment status updated" : result.message);
    
    if (result.success)
    {
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object(result.updatedShipment->toJson());
        sendSuccessResponse(response, "Shipment status updated successfully", dataPtr);
    }
    else
    {
        sendErrorResponse(response, result.message, 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
    }
}

void ShipmentController::handleUpdateTrackingInfo(Poco::Net::HTTPServerRequest& request, 
                                                Poco::Net::HTTPServerResponse& response)
{
    std::string shipmentIdStr = getPathParameter(request.getURI(), "/api/v1/shipments/", 0);
    shipmentIdStr = shipmentIdStr.substr(0, shipmentIdStr.find("/tracking"));
    
    if (shipmentIdStr.empty())
    {
        sendErrorResponse(response, "Shipment ID is required", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    long long shipmentId;
    try
    {
        shipmentId = std::stoll(shipmentIdStr);
    }
    catch (...)
    {
        sendErrorResponse(response, "Invalid shipment ID", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    auto json = parseJsonBody(request);
    if (!json)
    {
        sendErrorResponse(response, "Invalid JSON body", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    std::vector<std::string> errors;
    if (!validateTrackingUpdateData(json, errors))
    {
        sendValidationErrorResponse(response, errors);
        return;
    }
    
    std::string trackingNumber = utils::JsonUtils::getString(*json, "tracking_number");
    std::string carrier = utils::JsonUtils::getString(*json, "carrier");
    
    std::string authError;
    if (!canUpdateShipment(request, shipmentId, authError))
    {
        sendForbiddenResponse(response, authError);
        return;
    }
    
    long long updatedBy = getCurrentUserId(request);
    
    auto result = shipmentService->updateTrackingInfo(shipmentId, trackingNumber, carrier, updatedBy);
    
    logShipmentEvent(updatedBy, "UPDATE_TRACKING_INFO", shipmentId,
                    getClientIpAddress(request), getUserAgent(request),
                    result.success, result.success ? "Tracking info updated" : result.message);
    
    if (result.success)
    {
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object(result.updatedShipment->toJson());
        sendSuccessResponse(response, "Tracking information updated successfully", dataPtr);
    }
    else
    {
        sendErrorResponse(response, result.message, 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
    }
}

void ShipmentController::handleCancelShipment(Poco::Net::HTTPServerRequest& request, 
                                            Poco::Net::HTTPServerResponse& response)
{
    std::string shipmentIdStr = getPathParameter(request.getURI(), "/api/v1/shipments/", 0);
    shipmentIdStr = shipmentIdStr.substr(0, shipmentIdStr.find("/cancel"));
    
    if (shipmentIdStr.empty())
    {
        sendErrorResponse(response, "Shipment ID is required", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    long long shipmentId;
    try
    {
        shipmentId = std::stoll(shipmentIdStr);
    }
    catch (...)
    {
        sendErrorResponse(response, "Invalid shipment ID", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    std::string authError;
    if (!canCancelShipment(request, shipmentId, authError))
    {
        sendForbiddenResponse(response, authError);
        return;
    }
    
    std::string reason;
    auto json = parseJsonBody(request);
    if (json && json->has("reason"))
    {
        reason = utils::JsonUtils::getString(*json, "reason");
    }
    
    long long cancelledBy = getCurrentUserId(request);
    
    bool success = shipmentService->cancelShipment(shipmentId, cancelledBy, reason);
    
    logShipmentEvent(cancelledBy, "CANCEL_SHIPMENT", shipmentId,
                    getClientIpAddress(request), getUserAgent(request),
                    success, success ? "Shipment cancelled" : "Failed to cancel shipment");
    
    if (success)
    {
        sendSuccessResponse(response, "Shipment cancelled successfully");
    }
    else
    {
        sendErrorResponse(response, "Failed to cancel shipment", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
    }
}

void ShipmentController::handleMarkAsDelivered(Poco::Net::HTTPServerRequest& request, 
                                             Poco::Net::HTTPServerResponse& response)
{
    std::string shipmentIdStr = getPathParameter(request.getURI(), "/api/v1/shipments/", 0);
    shipmentIdStr = shipmentIdStr.substr(0, shipmentIdStr.find("/delivered"));
    
    if (shipmentIdStr.empty())
    {
        sendErrorResponse(response, "Shipment ID is required", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    long long shipmentId;
    try
    {
        shipmentId = std::stoll(shipmentIdStr);
    }
    catch (...)
    {
        sendErrorResponse(response, "Invalid shipment ID", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    std::string authError;
    if (!canUpdateShipment(request, shipmentId, authError))
    {
        sendForbiddenResponse(response, authError);
        return;
    }
    
    std::string actualArrival;
    auto json = parseJsonBody(request);
    if (json && json->has("actual_arrival"))
    {
        actualArrival = utils::JsonUtils::getString(*json, "actual_arrival");
    }
    
    long long deliveredBy = getCurrentUserId(request);
    
    bool success = shipmentService->markShipmentAsDelivered(shipmentId, actualArrival, deliveredBy);
    
    logShipmentEvent(deliveredBy, "MARK_SHIPMENT_DELIVERED", shipmentId,
                    getClientIpAddress(request), getUserAgent(request),
                    success, success ? "Shipment marked as delivered" : "Failed to mark as delivered");
    
    if (success)
    {
        sendSuccessResponse(response, "Shipment marked as delivered successfully");
    }
    else
    {
        sendErrorResponse(response, "Failed to mark shipment as delivered", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
    }
}

void ShipmentController::handleGetShipmentsByOrder(Poco::Net::HTTPServerRequest& request, 
                                                 Poco::Net::HTTPServerResponse& response)
{
    std::string orderIdStr = getQueryParameter(request.getURI(), "orderId");
    if (orderIdStr.empty())
    {
        sendErrorResponse(response, "Order ID is required", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    long long orderId;
    try
    {
        orderId = std::stoll(orderIdStr);
    }
    catch (...)
    {
        sendErrorResponse(response, "Invalid order ID", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    std::string authError;
    if (!canViewShipments(request, authError))
    {
        sendForbiddenResponse(response, authError);
        return;
    }
    
    try
    {
        Poco::JSON::Array::Ptr shipmentsArray = new Poco::JSON::Array(shipmentService->findShipmentsByOrder(orderId));
        
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object;
        dataPtr->set("shipments", shipmentsArray);
        sendSuccessResponse(response, "Shipments for order retrieved successfully", dataPtr);
    }
    catch (const std::exception& e)
    {
        sendErrorResponse(response, "Error retrieving shipments: " + std::string(e.what()), 
                        Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
}

void ShipmentController::handleGetShipmentsByStatus(Poco::Net::HTTPServerRequest& request, 
                                                  Poco::Net::HTTPServerResponse& response)
{
    std::string status = getQueryParameter(request.getURI(), "status");
    if (status.empty())
    {
        sendErrorResponse(response, "Status is required", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    int page, pageSize;
    if (!getPaginationParameters(request, page, pageSize, 20))
    {
        sendErrorResponse(response, "Invalid pagination parameters", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    std::string authError;
    if (!canViewShipments(request, authError))
    {
        sendForbiddenResponse(response, authError);
        return;
    }
    
    try
    {
        Poco::JSON::Array shipmentsArray = shipmentService->findShipmentsByStatus(status, page, pageSize);
        
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object;
        dataPtr->set("shipments", shipmentsArray);
        sendSuccessResponse(response, "Shipments by status retrieved successfully", dataPtr);
    }
    catch (const std::exception& e)
    {
        sendErrorResponse(response, "Error retrieving shipments: " + std::string(e.what()), 
                        Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
}

void ShipmentController::handleGetShipmentsByCarrier(Poco::Net::HTTPServerRequest& request, 
                                                   Poco::Net::HTTPServerResponse& response)
{
    std::string carrier = getQueryParameter(request.getURI(), "carrier");
    if (carrier.empty())
    {
        sendErrorResponse(response, "Carrier is required", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    int page, pageSize;
    if (!getPaginationParameters(request, page, pageSize, 20))
    {
        sendErrorResponse(response, "Invalid pagination parameters", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    std::string authError;
    if (!canViewShipments(request, authError))
    {
        sendForbiddenResponse(response, authError);
        return;
    }
    
    try
    {
        Poco::JSON::Array::Ptr shipmentsArray = new Poco::JSON::Array(shipmentService->findShipmentsByCarrier(carrier, page, pageSize));
        
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object;
        dataPtr->set("shipments", shipmentsArray);
        sendSuccessResponse(response, "Shipments by carrier retrieved successfully", dataPtr);
    }
    catch (const std::exception& e)
    {
        sendErrorResponse(response, "Error retrieving shipments: " + std::string(e.what()), 
                        Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
}

void ShipmentController::handleGetShipmentStatistics(Poco::Net::HTTPServerRequest& request, 
                                                   Poco::Net::HTTPServerResponse& response)
{
    auto currentUserRole = getCurrentUserRole(request);
    if (currentUserRole != database::models::UserRole::ADMIN && 
        currentUserRole != database::models::UserRole::MANAGER && 
        currentUserRole != database::models::UserRole::AUDITOR)
    {
        sendForbiddenResponse(response, "You don't have permission to view shipment statistics");
        return;
    }
    
    try
    {
        Poco::JSON::Object statistics = shipmentService->getShipmentStatistics();
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object(statistics);
        
        sendSuccessResponse(response, "Shipment statistics retrieved", dataPtr);
    }
    catch (const std::exception& e)
    {
        sendErrorResponse(response, "Error retrieving shipment statistics: " + std::string(e.what()), 
                        Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
}

void ShipmentController::handleGetCarrierPerformanceReport(Poco::Net::HTTPServerRequest& request, 
                                                         Poco::Net::HTTPServerResponse& response)
{
    auto currentUserRole = getCurrentUserRole(request);
    if (currentUserRole != database::models::UserRole::ADMIN && 
        currentUserRole != database::models::UserRole::MANAGER && 
        currentUserRole != database::models::UserRole::AUDITOR)
    {
        sendForbiddenResponse(response, "You don't have permission to view carrier performance reports");
        return;
    }
    
    try
    {
        Poco::JSON::Array report = shipmentService->getCarrierPerformanceReport();
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object;
        dataPtr->set("report", report);
        
        sendSuccessResponse(response, "Carrier performance report generated", dataPtr);
    }
    catch (const std::exception& e)
    {
        sendErrorResponse(response, "Error generating carrier performance report: " + std::string(e.what()), 
                        Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
}

void ShipmentController::handleGetShippingCostAnalysis(Poco::Net::HTTPServerRequest& request, 
                                                     Poco::Net::HTTPServerResponse& response)
{
    auto currentUserRole = getCurrentUserRole(request);
    if (currentUserRole != database::models::UserRole::ADMIN && 
        currentUserRole != database::models::UserRole::MANAGER && 
        currentUserRole != database::models::UserRole::AUDITOR)
    {
        sendForbiddenResponse(response, "You don't have permission to view shipping cost analysis");
        return;
    }
    
    std::string startDate = getQueryParameter(request.getURI(), "startDate");
    std::string endDate = getQueryParameter(request.getURI(), "endDate");
    
    if ((!startDate.empty() && !utils::Validator::isValidDate(startDate)) ||
        (!endDate.empty() && !utils::Validator::isValidDate(endDate)))
    {
        sendErrorResponse(response, "Invalid date format. Use YYYY-MM-DD", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    try
    {
        Poco::JSON::Array::Ptr analysis = new Poco::JSON::Array(shipmentService->getShippingCostAnalysis(startDate, endDate));
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object;
        dataPtr->set("analysis", analysis);
        
        sendSuccessResponse(response, "Shipping cost analysis generated", dataPtr);
    }
    catch (const std::exception& e)
    {
        sendErrorResponse(response, "Error generating shipping cost analysis: " + std::string(e.what()), 
                        Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
}

void ShipmentController::handleGetDelayedShipments(Poco::Net::HTTPServerRequest& request, 
                                                 Poco::Net::HTTPServerResponse& response)
{
    std::string daysThresholdStr = getQueryParameter(request.getURI(), "days");
    int daysThreshold = 3;
    if (!daysThresholdStr.empty())
    {
        try
        {
            daysThreshold = std::stoi(daysThresholdStr);
            if (daysThreshold < 0)
            {
                sendErrorResponse(response, "Days threshold must be non-negative", 
                                Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
                return;
            }
        }
        catch (...)
        {
            sendErrorResponse(response, "Invalid days threshold", 
                            Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
            return;
        }
    }
    
    std::string authError;
    if (!canViewShipments(request, authError))
    {
        sendForbiddenResponse(response, authError);
        return;
    }
    
    try
    {
        Poco::JSON::Array::Ptr delayedShipments = new Poco::JSON::Array(shipmentService->getDelayedShipments(daysThreshold));
        
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object;
        dataPtr->set("delayedShipments", delayedShipments);
        sendSuccessResponse(response, "Delayed shipments retrieved", dataPtr);
    }
    catch (const std::exception& e)
    {
        sendErrorResponse(response, "Error retrieving delayed shipments: " + std::string(e.what()), 
                        Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
}

void ShipmentController::handleGetShipmentsDueToday(Poco::Net::HTTPServerRequest& request, 
                                                  Poco::Net::HTTPServerResponse& response)
{
    std::string authError;
    if (!canViewShipments(request, authError))
    {
        sendForbiddenResponse(response, authError);
        return;
    }
    
    try
    {
        Poco::JSON::Array::Ptr shipmentsDueToday = new Poco::JSON::Array(shipmentService->getShipmentsDueToday());
        
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object;
        dataPtr->set("shipmentsDueToday", shipmentsDueToday);
        sendSuccessResponse(response, "Shipments due today retrieved", dataPtr);
    }
    catch (const std::exception& e)
    {
        sendErrorResponse(response, "Error retrieving shipments due today: " + std::string(e.what()), 
                        Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
}

bool ShipmentController::validateRequest(Poco::Net::HTTPServerRequest& request, 
                                       Poco::Net::HTTPServerResponse& response,
                                       std::string& errorMessage)
{
    if (!BaseController::validateRequest(request, response, errorMessage))
    {
        return false;
    }
    
    return true;
}

bool ShipmentController::authorizeRequest(Poco::Net::HTTPServerRequest& request, 
                                        Poco::Net::HTTPServerResponse& response,
                                        std::string& errorMessage)
{
    std::string authHeader = getAuthorizationHeader(request);
    std::string token = extractBearerToken(authHeader);
    
    if (token.empty())
    {
        errorMessage = "Authorization token is required";
        return false;
    }
    
    auto tokenResult = authService->validateToken(token);
    if (!tokenResult.isValid)
    {
        errorMessage = tokenResult.message;
        return false;
    }
    
    return true;
}

long long ShipmentController::getCurrentUserId(Poco::Net::HTTPServerRequest& request)
{
    std::string authHeader = getAuthorizationHeader(request);
    std::string token = extractBearerToken(authHeader);
    
    if (!token.empty())
    {
        auto tokenResult = authService->validateToken(token);
        if (tokenResult.isValid && tokenResult.user)
        {
            return tokenResult.user->id;
        }
    }
    
    return 0;
}

database::models::UserRole ShipmentController::getCurrentUserRole(Poco::Net::HTTPServerRequest& request)
{
    std::string authHeader = getAuthorizationHeader(request);
    std::string token = extractBearerToken(authHeader);
    
    if (!token.empty())
    {
        auto tokenResult = authService->validateToken(token);
        if (tokenResult.isValid && tokenResult.user)
        {
            return tokenResult.user->role;
        }
    }
    
    return database::models::UserRole::WORKER;
}

bool ShipmentController::canViewShipments(Poco::Net::HTTPServerRequest& request, 
                                        std::string& errorMessage)
{
    auto currentUserRole = getCurrentUserRole(request);
    
    if (currentUserRole == database::models::UserRole::ADMIN || 
        currentUserRole == database::models::UserRole::MANAGER || 
        currentUserRole == database::models::UserRole::WORKER || 
        currentUserRole == database::models::UserRole::AUDITOR)
    {
        return true;
    }
    
    errorMessage = "You don't have permission to view shipments";
    return false;
}

bool ShipmentController::canCreateShipment(Poco::Net::HTTPServerRequest& request, 
                                         long long orderId,
                                         std::string& errorMessage)
{
    auto currentUserRole = getCurrentUserRole(request);
    
    if (currentUserRole == database::models::UserRole::ADMIN || 
        currentUserRole == database::models::UserRole::MANAGER)
    {
        return true;
    }
    
    if (currentUserRole == database::models::UserRole::WORKER)
    {
        return true;
    }
    
    errorMessage = "You don't have permission to create shipments";
    return false;
}

bool ShipmentController::canUpdateShipment(Poco::Net::HTTPServerRequest& request, 
                                         long long shipmentId,
                                         std::string& errorMessage)
{
    auto currentUserRole = getCurrentUserRole(request);
    
    if (currentUserRole == database::models::UserRole::ADMIN || 
        currentUserRole == database::models::UserRole::MANAGER)
    {
        return true;
    }
    
    if (currentUserRole == database::models::UserRole::WORKER)
    {
        return true;
    }
    
    errorMessage = "You don't have permission to update this shipment";
    return false;
}

bool ShipmentController::canCancelShipment(Poco::Net::HTTPServerRequest& request, 
                                         long long shipmentId,
                                         std::string& errorMessage)
{
    auto currentUserRole = getCurrentUserRole(request);
    
    if (currentUserRole == database::models::UserRole::ADMIN || 
        currentUserRole == database::models::UserRole::MANAGER)
    {
        return true;
    }
    
    errorMessage = "You don't have permission to cancel shipments";
    return false;
}

bool ShipmentController::validateCreateShipmentData(const Poco::JSON::Object::Ptr& json, 
                                                  std::vector<std::string>& errors)
{
    errors.clear();
    
    std::vector<std::string> requiredFields = {"order_id", "carrier", "shipping_method", 
                                              "shipping_cost", "estimated_arrival"};
    std::vector<std::string> missingFields;
    if (!validateRequiredFields(json, requiredFields, missingFields))
    {
        for (const auto& field : missingFields)
        {
            errors.push_back(field + " is required");
        }
    }
    
    if (json->has("order_id"))
    {
        try
        {
            long long orderId = utils::JsonUtils::getInt(*json, "order_id");
            if (orderId <= 0)
            {
                errors.push_back("Invalid order ID");
            }
        }
        catch (...)
        {
            errors.push_back("Invalid order ID");
        }
    }
    
    if (json->has("carrier"))
    {
        std::string carrier = utils::JsonUtils::getString(*json, "carrier");
        if (carrier.empty() || carrier.length() > 100)
        {
            errors.push_back("Carrier must be between 1 and 100 characters");
        }
    }
    
    if (json->has("shipping_method"))
    {
        std::string shippingMethod = utils::JsonUtils::getString(*json, "shipping_method");
        if (shippingMethod.empty())
        {
            errors.push_back("Shipping method is required");
        }
    }
    
    if (json->has("shipping_cost"))
    {
        double shippingCost = utils::JsonUtils::getDouble(*json, "shipping_cost");
        if (shippingCost < 0)
        {
            errors.push_back("Shipping cost must be greater than or equal to 0");
        }
    }
    
    if (json->has("estimated_arrival"))
    {
        std::string estimatedArrival = utils::JsonUtils::getString(*json, "estimated_arrival");
        if (!utils::Validator::isValidDate(estimatedArrival))
        {
            errors.push_back("Invalid estimated arrival date format. Use YYYY-MM-DD");
        }
    }
    
    if (json->has("tracking_number") && !json->get("tracking_number").isEmpty())
    {
        std::string trackingNumber = utils::JsonUtils::getString(*json, "tracking_number");
        if (trackingNumber.empty())
        {
            errors.push_back("Tracking number cannot be empty if provided");
        }
    }
    
    return errors.empty();
}

bool ShipmentController::validateUpdateShipmentData(const Poco::JSON::Object::Ptr& json, 
                                                  std::vector<std::string>& errors)
{
    errors.clear();
    
    bool hasCarrier = json->has("carrier") && !json->get("carrier").isEmpty();
    bool hasShippingMethod = json->has("shipping_method") && !json->get("shipping_method").isEmpty();
    bool hasShippingCost = json->has("shipping_cost") && !json->get("shipping_cost").isEmpty();
    bool hasEstimatedArrival = json->has("estimated_arrival") && !json->get("estimated_arrival").isEmpty();
    bool hasTrackingNumber = json->has("tracking_number") && !json->get("tracking_number").isEmpty();
    bool hasNotes = json->has("notes") && !json->get("notes").isEmpty();
    bool hasWeightTotal = json->has("weight_total") && !json->get("weight_total").isEmpty();
    bool hasDimensionsTotal = json->has("dimensions_total") && !json->get("dimensions_total").isEmpty();
    
    if (!hasCarrier && !hasShippingMethod && !hasShippingCost && !hasEstimatedArrival && 
        !hasTrackingNumber && !hasNotes && !hasWeightTotal && !hasDimensionsTotal)
    {
        errors.push_back("At least one field must be provided for update");
    }
    
    if (hasCarrier)
    {
        std::string carrier = utils::JsonUtils::getString(*json, "carrier");
        if (carrier.empty() || carrier.length() > 100)
        {
            errors.push_back("Carrier must be between 1 and 100 characters");
        }
    }
    
    if (hasShippingMethod)
    {
        std::string shippingMethod = utils::JsonUtils::getString(*json, "shipping_method");
        if (shippingMethod.empty())
        {
            errors.push_back("Shipping method cannot be empty");
        }
    }
    
    if (hasShippingCost)
    {
        double shippingCost = utils::JsonUtils::getDouble(*json, "shipping_cost");
        if (shippingCost < 0)
        {
            errors.push_back("Shipping cost must be greater than or equal to 0");
        }
    }
    
    if (hasEstimatedArrival)
    {
        std::string estimatedArrival = utils::JsonUtils::getString(*json, "estimated_arrival");
        if (!utils::Validator::isValidDate(estimatedArrival))
        {
            errors.push_back("Invalid estimated arrival date format. Use YYYY-MM-DD");
        }
    }
    
    if (hasWeightTotal)
    {
        double weightTotal = utils::JsonUtils::getDouble(*json, "weight_total");
        if (weightTotal < 0)
        {
            errors.push_back("Weight total must be greater than or equal to 0");
        }
    }
    
    return errors.empty();
}

bool ShipmentController::validateStatusUpdateData(const Poco::JSON::Object::Ptr& json, 
                                                std::vector<std::string>& errors)
{
    errors.clear();
    
    std::vector<std::string> requiredFields = {"status"};
    std::vector<std::string> missingFields;
    if (!validateRequiredFields(json, requiredFields, missingFields))
    {
        for (const auto& field : missingFields)
        {
            errors.push_back(field + " is required");
        }
    }
    
    if (json->has("status"))
    {
        std::string status = utils::JsonUtils::getString(*json, "status");
        try
        {
            auto shipmentStatus = database::models::Shipment::stringToStatus(status);
            
            std::vector<std::string> validStatuses = {"preparing", "in_transit", "delivered", 
                                                     "delayed", "returned"};
            bool isValid = false;
            for (const auto& validStatus : validStatuses)
            {
                if (status == validStatus)
                {
                    isValid = true;
                    break;
                }
            }
            
            if (!isValid)
            {
                errors.push_back("Invalid status value. Must be: preparing, in_transit, delivered, delayed, or returned");
            }
        }
        catch (...)
        {
            errors.push_back("Invalid status value");
        }
    }
    
    if (json->has("notes") && !json->get("notes").isEmpty())
    {
        std::string notes = utils::JsonUtils::getString(*json, "notes");
    }
    
    return errors.empty();
}

bool ShipmentController::validateTrackingUpdateData(const Poco::JSON::Object::Ptr& json, 
                                                  std::vector<std::string>& errors)
{
    errors.clear();
    
    std::vector<std::string> requiredFields = {"tracking_number", "carrier"};
    std::vector<std::string> missingFields;
    if (!validateRequiredFields(json, requiredFields, missingFields))
    {
        for (const auto& field : missingFields)
        {
            errors.push_back(field + " is required");
        }
    }
    
    if (json->has("tracking_number"))
    {
        std::string trackingNumber = utils::JsonUtils::getString(*json, "tracking_number");
        if (trackingNumber.empty())
        {
            errors.push_back("Tracking number cannot be empty");
        }
    }
    
    if (json->has("carrier"))
    {
        std::string carrier = utils::JsonUtils::getString(*json, "carrier");
        if (carrier.empty() || carrier.length() > 100)
        {
            errors.push_back("Carrier must be between 1 and 100 characters");
        }
    }
    
    return errors.empty();
}

bool ShipmentController::validateSearchParameters(const std::map<std::string, std::string>& filters,
                                                std::vector<std::string>& errors)
{
    errors.clear();
    
    for (const auto& [key, value] : filters)
    {
        if (key == "orderId")
        {
            try
            {
                long long orderId = std::stoll(value);
                if (orderId <= 0)
                {
                    errors.push_back("orderId must be a positive number");
                }
            }
            catch (...)
            {
                errors.push_back("Invalid orderId value");
            }
        }
        else if (key == "carrier")
        {
            if (value.empty())
            {
                errors.push_back("Carrier cannot be empty");
            }
        }
        else if (key == "status")
        {
            try
            {
                auto status = database::models::Shipment::stringToStatus(value);
            }
            catch (...)
            {
                errors.push_back("Invalid status. Must be: preparing, in_transit, delivered, delayed, or returned");
            }
        }
        else if (key == "startDate" || key == "endDate")
        {
            if (!utils::Validator::isValidDate(value))
            {
                errors.push_back(key + " must be in YYYY-MM-DD format");
            }
        }
        else if (key == "minCost" || key == "maxCost")
        {
            try
            {
                double cost = std::stod(value);
                if (cost < 0)
                {
                    errors.push_back(key + " must be greater than or equal to 0");
                }
            }
            catch (...)
            {
                errors.push_back("Invalid " + key + " value");
            }
        }
        else if (key == "shippingMethod")
        {
            if (value.empty())
            {
                errors.push_back("Shipping method cannot be empty");
            }
        }
    }
    
    bool hasStartDate = filters.find("startDate") != filters.end();
    bool hasEndDate = filters.find("endDate") != filters.end();
    
    if (hasStartDate && hasEndDate)
    {
        std::string startDate = filters.at("startDate");
        std::string endDate = filters.at("endDate");
        
        if (startDate > endDate)
        {
            errors.push_back("startDate must be before or equal to endDate");
        }
    }
    else if (hasStartDate != hasEndDate)
    {
        errors.push_back("Both startDate and endDate must be provided together");
    }
    
    bool hasMinCost = filters.find("minCost") != filters.end();
    bool hasMaxCost = filters.find("maxCost") != filters.end();
    
    if (hasMinCost && hasMaxCost)
    {
        try
        {
            double minCost = std::stod(filters.at("minCost"));
            double maxCost = std::stod(filters.at("maxCost"));
            
            if (minCost > maxCost)
            {
                errors.push_back("minCost must be less than or equal to maxCost");
            }
        }
        catch (...)
        {
        }
    }
    
    return errors.empty();
}

std::unique_ptr<database::models::Shipment> ShipmentController::extractShipmentFromJson(
    const Poco::JSON::Object::Ptr& json)
{
    if (!json)
    {
        return nullptr;
    }
    
    try
    {
        auto shipment = std::make_unique<database::models::Shipment>(*json);
        return shipment;
    }
    catch (...)
    {
        return nullptr;
    }
}

void ShipmentController::logShipmentEvent(long long userId, 
                                        const std::string& action,
                                        long long shipmentId,
                                        const std::string& ipAddress,
                                        const std::string& userAgent,
                                        bool success,
                                        const std::string& details)
{
    std::cout << "[" << getCurrentTimestamp() << "] "
              << "SHIPMENT " << action << " "
              << "UserID: " << userId << " "
              << "ShipmentID: " << shipmentId << " "
              << "IP: " << ipAddress << " "
              << "Success: " << (success ? "Yes" : "No") << " "
              << "Details: " << details << std::endl;
}

} // namespace controllers
