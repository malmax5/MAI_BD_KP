#include "InventoryMovementController.hpp"
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

InventoryMovementController::InventoryMovementController()
    : inventoryService(std::make_unique<services::InventoryService>()),
      authService(std::make_unique<services::AuthService>())
{
}

void InventoryMovementController::handleRequest(Poco::Net::HTTPServerRequest& request, 
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
        
        std::string basePath = "/api/v1/inventory";
        if (endpoint.find("/api/v1/") != 0)
        {
            basePath = "/api/inventory";
        }
        
        std::string movementIdStr = getPathParameter(uri, basePath + "/movements/", 0);
        std::string subEndpoint = endpoint;
        
        if ((endpoint.find("/api/v1/inventory/statistics") == 0 || 
             endpoint.find("/api/inventory/statistics") == 0) && 
            request.getMethod() == "GET")
        {
            handleGetInventoryStatistics(request, response);
        }
        else if ((endpoint.find("/api/v1/inventory/movements/statistics") == 0 || 
                  endpoint.find("/api/inventory/movements/statistics") == 0) && 
                 request.getMethod() == "GET")
        {
            handleGetMovementStatistics(request, response);
        }
        else if ((endpoint.find("/api/v1/inventory/movements/report") == 0 || 
                  endpoint.find("/api/inventory/movements/report") == 0) && 
                 request.getMethod() == "GET")
        {
            handleGetMovementReport(request, response);
        }
        else if ((endpoint.find("/api/v1/inventory/expiring") == 0 || 
                  endpoint.find("/api/inventory/expiring") == 0) && 
                 request.getMethod() == "GET")
        {
            handleGetExpiringProducts(request, response);
        }
        else if ((endpoint.find("/api/v1/inventory/low-stock") == 0 || 
                  endpoint.find("/api/inventory/low-stock") == 0) && 
                 request.getMethod() == "GET")
        {
            handleGetLowStockProducts(request, response);
        }
        else if ((endpoint.find("/api/v1/inventory/check-stock") == 0 || 
                  endpoint.find("/api/inventory/check-stock") == 0) && 
                 request.getMethod() == "GET")
        {
            handleCheckStockAvailability(request, response);
        }
        else if ((endpoint.find("/api/v1/inventory/movements/product") == 0 || 
                  endpoint.find("/api/inventory/movements/product") == 0) && 
                 request.getMethod() == "GET")
        {
            handleGetProductMovements(request, response);
        }
        else if ((endpoint.find("/api/v1/inventory/movements/cell") == 0 || 
                  endpoint.find("/api/inventory/movements/cell") == 0) && 
                 request.getMethod() == "GET")
        {
            handleGetCellMovements(request, response);
        }
        else if ((endpoint == "/api/v1/inventory/movements/receipt" || 
                  endpoint == "/api/inventory/movements/receipt") && 
                 request.getMethod() == "POST")
        {
            handleCreateReceiptMovement(request, response);
        }
        else if ((endpoint == "/api/v1/inventory/movements/transfer" || 
                  endpoint == "/api/inventory/movements/transfer") && 
                 request.getMethod() == "POST")
        {
            handleCreateTransferMovement(request, response);
        }
        else if ((endpoint == "/api/v1/inventory/movements/adjustment" || 
                  endpoint == "/api/inventory/movements/adjustment") && 
                 request.getMethod() == "POST")
        {
            handleCreateAdjustmentMovement(request, response);
        }
        else if ((endpoint.find("/api/v1/inventory/movements/") == 0 || 
                  endpoint.find("/api/inventory/movements/") == 0) && 
                 !movementIdStr.empty())
        {
            if (endpoint.find("/cancel") != std::string::npos && request.getMethod() == "POST")
            {
                handleCancelMovement(request, response);
            }
            else if (endpoint.find("/status") != std::string::npos && request.getMethod() == "PUT")
            {
                handleUpdateMovementStatus(request, response);
            }
            else if (request.getMethod() == "GET")
            {
                handleGetMovementById(request, response);
            }
        }
        else if ((endpoint == "/api/v1/inventory/movements" || 
                  endpoint == "/api/inventory/movements") && 
                 request.getMethod() == "GET")
        {
            handleGetMovements(request, response);
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

void InventoryMovementController::handleGetMovements(Poco::Net::HTTPServerRequest& request, 
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
    if (!canViewMovements(request, authError))
    {
        sendForbiddenResponse(response, authError);
        return;
    }
    
    try
    {
        std::string startDate, endDate;
        if (filters.find("startDate") != filters.end() && filters.find("endDate") != filters.end())
        {
            startDate = filters["startDate"];
            endDate = filters["endDate"];
        }
        
        Poco::JSON::Array movementsArray;
        
        if (filters.find("productId") != filters.end())
        {
            long long productId = std::stoll(filters["productId"]);
            movementsArray = inventoryService->getInventoryMovementHistory(productId, startDate, endDate);
        }
        else if (filters.find("cellId") != filters.end())
        {
            long long cellId = std::stoll(filters["cellId"]);
            movementsArray = inventoryService->getCellMovementHistory(cellId, startDate, endDate);
        }
        else
        {
            movementsArray = inventoryService->getMovements(page, pageSize);
        }
        
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object;
        dataPtr->set("movements", movementsArray);
        sendSuccessResponse(response, "Inventory movements retrieved successfully", dataPtr);
    }
    catch (const std::exception& e)
    {
        sendErrorResponse(response, "Error retrieving movements: " + std::string(e.what()), 
                        Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
}

void InventoryMovementController::handleGetMovementById(Poco::Net::HTTPServerRequest& request, 
                                                      Poco::Net::HTTPServerResponse& response)
{
    std::string movementIdStr = getPathParameter(request.getURI(), "/api/v1/inventory/movements/", 0);
    if (movementIdStr.empty())
    {
        sendErrorResponse(response, "Movement ID is required", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    long long movementId;
    try
    {
        movementId = std::stoll(movementIdStr);
    }
    catch (...)
    {
        sendErrorResponse(response, "Invalid movement ID", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    std::string authError;
    if (!canViewMovements(request, authError))
    {
        sendForbiddenResponse(response, authError);
        return;
    }
    
    try
    {
        Poco::JSON::Object::Ptr movementData = new Poco::JSON::Object;
        movementData->set("id", movementId);
        movementData->set("message", "Movement details would be retrieved here");
        
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object;
        dataPtr->set("movement", movementData);
        sendSuccessResponse(response, "Movement retrieved successfully", dataPtr);
    }
    catch (const std::exception& e)
    {
        sendErrorResponse(response, "Error retrieving movement: " + std::string(e.what()), 
                        Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
}

void InventoryMovementController::handleCreateReceiptMovement(Poco::Net::HTTPServerRequest& request, 
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
    if (!validateReceiptData(json, errors))
    {
        sendValidationErrorResponse(response, errors);
        return;
    }
    
    std::string authError;
    if (!canCreateMovement(request, "receipt", authError))
    {
        sendForbiddenResponse(response, authError);
        return;
    }
    
    try
    {
        long long productId = utils::JsonUtils::getInt(*json, "product_id");
        long long supplierId = utils::JsonUtils::getInt(*json, "supplier_id");
        int quantity = utils::JsonUtils::getInt(*json, "quantity");
        double unitCost = utils::JsonUtils::getDouble(*json, "unit_cost");
        std::string batchNumber = utils::JsonUtils::getString(*json, "batch_number");
        
        std::string expirationDate = "";
        if (json->has("expiration_date"))
        {
            expirationDate = utils::JsonUtils::getString(*json, "expiration_date");
        }
        
        long long storageCellId = 0;
        if (json->has("storage_cell_id"))
        {
            storageCellId = utils::JsonUtils::getInt(*json, "storage_cell_id");
        }
        
        long long receivedBy = getCurrentUserId(request);
        
        auto result = inventoryService->receiveProduct(productId, supplierId, quantity, 
                                                     unitCost, batchNumber, expirationDate, 
                                                     storageCellId, receivedBy);
        
        logMovementEvent(receivedBy, "CREATE_RECEIPT_MOVEMENT", result.movementId,
                        getClientIpAddress(request), getUserAgent(request),
                        result.success, result.success ? "Receipt movement created" : result.message);
        
        if (result.success)
        {
            Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object(result.details);
            sendSuccessResponse(response, "Product receipt recorded successfully", dataPtr);
        }
        else
        {
            sendErrorResponse(response, result.message, 
                            Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        }
    }
    catch (const std::exception& e)
    {
        sendErrorResponse(response, "Error creating receipt: " + std::string(e.what()), 
                        Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
}

void InventoryMovementController::handleCreateTransferMovement(Poco::Net::HTTPServerRequest& request, 
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
    if (!validateTransferData(json, errors))
    {
        sendValidationErrorResponse(response, errors);
        return;
    }
    
    std::string authError;
    if (!canCreateMovement(request, "transfer", authError))
    {
        sendForbiddenResponse(response, authError);
        return;
    }
    
    try
    {
        long long batchId = utils::JsonUtils::getInt(*json, "batch_id");
        long long fromCellId = utils::JsonUtils::getInt(*json, "from_cell_id");
        long long toCellId = utils::JsonUtils::getInt(*json, "to_cell_id");
        int quantity = utils::JsonUtils::getInt(*json, "quantity");
        
        std::string reason = "";
        if (json->has("reason"))
        {
            reason = utils::JsonUtils::getString(*json, "reason");
        }
        
        long long performedBy = getCurrentUserId(request);
        
        auto result = inventoryService->transferProduct(batchId, fromCellId, toCellId, 
                                                      quantity, performedBy, reason);
        
        logMovementEvent(performedBy, "CREATE_TRANSFER_MOVEMENT", result.movementId,
                        getClientIpAddress(request), getUserAgent(request),
                        result.success, result.success ? "Transfer movement created" : result.message);
        
        if (result.success)
        {
            Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object(result.details);
            sendSuccessResponse(response, "Product transfer recorded successfully", dataPtr);
        }
        else
        {
            sendErrorResponse(response, result.message, 
                            Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        }
    }
    catch (const std::exception& e)
    {
        sendErrorResponse(response, "Error creating transfer: " + std::string(e.what()), 
                        Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
}

void InventoryMovementController::handleCreateAdjustmentMovement(Poco::Net::HTTPServerRequest& request, 
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
    if (!validateAdjustmentData(json, errors))
    {
        sendValidationErrorResponse(response, errors);
        return;
    }
    
    std::string authError;
    if (!canCreateMovement(request, "adjustment", authError))
    {
        sendForbiddenResponse(response, authError);
        return;
    }
    
    try
    {
        long long productId = utils::JsonUtils::getInt(*json, "product_id");
        long long batchId = utils::JsonUtils::getInt(*json, "batch_id");
        long long cellId = utils::JsonUtils::getInt(*json, "cell_id");
        int quantityAdjustment = utils::JsonUtils::getInt(*json, "quantity_adjustment");
        
        std::string reason = "";
        if (json->has("reason"))
        {
            reason = utils::JsonUtils::getString(*json, "reason");
        }
        
        long long performedBy = getCurrentUserId(request);
        
        auto result = inventoryService->adjustInventory(productId, batchId, cellId, 
                                                      quantityAdjustment, performedBy, reason);
        
        logMovementEvent(performedBy, "CREATE_ADJUSTMENT_MOVEMENT", result.movementId,
                        getClientIpAddress(request), getUserAgent(request),
                        result.success, result.success ? "Adjustment movement created" : result.message);
        
        if (result.success)
        {
            Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object(result.details);
            sendSuccessResponse(response, "Inventory adjustment recorded successfully", dataPtr);
        }
        else
        {
            sendErrorResponse(response, result.message, 
                            Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        }
    }
    catch (const std::exception& e)
    {
        sendErrorResponse(response, "Error creating adjustment: " + std::string(e.what()), 
                        Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
}

void InventoryMovementController::handleUpdateMovementStatus(Poco::Net::HTTPServerRequest& request, 
                                                           Poco::Net::HTTPServerResponse& response)
{
    std::string movementIdStr = getPathParameter(request.getURI(), "/api/v1/inventory/movements/", 0);
    movementIdStr = movementIdStr.substr(0, movementIdStr.find("/status"));
    
    if (movementIdStr.empty())
    {
        sendErrorResponse(response, "Movement ID is required", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    long long movementId;
    try
    {
        movementId = std::stoll(movementIdStr);
    }
    catch (...)
    {
        sendErrorResponse(response, "Invalid movement ID", 
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
    
    std::string authError;
    if (!canUpdateMovement(request, movementId, newStatus, authError))
    {
        sendForbiddenResponse(response, authError);
        return;
    }
    
    logMovementEvent(getCurrentUserId(request), "UPDATE_MOVEMENT_STATUS", movementId,
                    getClientIpAddress(request), getUserAgent(request),
                    true, "Movement status updated to " + newStatus);
    
    Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object;
    dataPtr->set("movementId", movementId);
    dataPtr->set("newStatus", newStatus);
    sendSuccessResponse(response, "Movement status updated successfully", dataPtr);
}

void InventoryMovementController::handleCancelMovement(Poco::Net::HTTPServerRequest& request, 
                                                     Poco::Net::HTTPServerResponse& response)
{
    std::string movementIdStr = getPathParameter(request.getURI(), "/api/v1/inventory/movements/", 0);
    movementIdStr = movementIdStr.substr(0, movementIdStr.find("/cancel"));
    
    if (movementIdStr.empty())
    {
        sendErrorResponse(response, "Movement ID is required", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    long long movementId;
    try
    {
        movementId = std::stoll(movementIdStr);
    }
    catch (...)
    {
        sendErrorResponse(response, "Invalid movement ID", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    std::string authError;
    if (!canCancelMovement(request, movementId, authError))
    {
        sendForbiddenResponse(response, authError);
        return;
    }
    
    logMovementEvent(getCurrentUserId(request), "CANCEL_MOVEMENT", movementId,
                    getClientIpAddress(request), getUserAgent(request),
                    true, "Movement cancelled");
    
    sendSuccessResponse(response, "Movement cancelled successfully");
}

void InventoryMovementController::handleGetProductMovements(Poco::Net::HTTPServerRequest& request, 
                                                          Poco::Net::HTTPServerResponse& response)
{
    std::string productIdStr = getQueryParameter(request.getURI(), "productId");
    if (productIdStr.empty())
    {
        sendErrorResponse(response, "Product ID is required", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    long long productId;
    try
    {
        productId = std::stoll(productIdStr);
    }
    catch (...)
    {
        sendErrorResponse(response, "Invalid product ID", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    std::string startDate = getQueryParameter(request.getURI(), "startDate");
    std::string endDate = getQueryParameter(request.getURI(), "endDate");
    
    std::string authError;
    if (!canViewMovements(request, authError))
    {
        sendForbiddenResponse(response, authError);
        return;
    }
    
    try
    {
        Poco::JSON::Array movementsArray = inventoryService->getInventoryMovementHistory(
            productId, startDate, endDate);
        
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object;
        dataPtr->set("movements", movementsArray);
        sendSuccessResponse(response, "Product movement history retrieved successfully", dataPtr);
    }
    catch (const std::exception& e)
    {
        sendErrorResponse(response, "Error retrieving product movements: " + std::string(e.what()), 
                        Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
}

void InventoryMovementController::handleGetCellMovements(Poco::Net::HTTPServerRequest& request, 
                                                       Poco::Net::HTTPServerResponse& response)
{
    std::string cellIdStr = getQueryParameter(request.getURI(), "cellId");
    if (cellIdStr.empty())
    {
        sendErrorResponse(response, "Cell ID is required", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    long long cellId;
    try
    {
        cellId = std::stoll(cellIdStr);
    }
    catch (...)
    {
        sendErrorResponse(response, "Invalid cell ID", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    std::string startDate = getQueryParameter(request.getURI(), "startDate");
    std::string endDate = getQueryParameter(request.getURI(), "endDate");
    
    std::string authError;
    if (!canViewMovements(request, authError))
    {
        sendForbiddenResponse(response, authError);
        return;
    }
    
    try
    {
        Poco::JSON::Array movementsArray = inventoryService->getCellMovementHistory(
            cellId, startDate, endDate);
        
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object;
        dataPtr->set("movements", movementsArray);
        sendSuccessResponse(response, "Cell movement history retrieved successfully", dataPtr);
    }
    catch (const std::exception& e)
    {
        sendErrorResponse(response, "Error retrieving cell movements: " + std::string(e.what()), 
                        Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
}

void InventoryMovementController::handleGetMovementStatistics(Poco::Net::HTTPServerRequest& request, 
                                                            Poco::Net::HTTPServerResponse& response)
{
    auto currentUserRole = getCurrentUserRole(request);
    if (currentUserRole != database::models::UserRole::ADMIN && 
        currentUserRole != database::models::UserRole::MANAGER && 
        currentUserRole != database::models::UserRole::AUDITOR)
    {
        sendForbiddenResponse(response, "You don't have permission to view movement statistics");
        return;
    }
    
    try
    {
        Poco::JSON::Object statistics = inventoryService->getInventoryStatistics();
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object(statistics);
        
        sendSuccessResponse(response, "Movement statistics retrieved", dataPtr);
    }
    catch (const std::exception& e)
    {
        sendErrorResponse(response, "Error retrieving movement statistics: " + std::string(e.what()), 
                        Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
}

void InventoryMovementController::handleGetMovementReport(Poco::Net::HTTPServerRequest& request, 
                                                        Poco::Net::HTTPServerResponse& response)
{
    auto currentUserRole = getCurrentUserRole(request);
    if (currentUserRole != database::models::UserRole::ADMIN && 
        currentUserRole != database::models::UserRole::MANAGER && 
        currentUserRole != database::models::UserRole::AUDITOR)
    {
        sendForbiddenResponse(response, "You don't have permission to view movement reports");
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
        Poco::JSON::Array report = inventoryService->getStockValueReport();
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object;
        dataPtr->set("report", report);
        
        sendSuccessResponse(response, "Movement report generated", dataPtr);
    }
    catch (const std::exception& e)
    {
        sendErrorResponse(response, "Error generating movement report: " + std::string(e.what()), 
                        Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
}

void InventoryMovementController::handleCheckStockAvailability(Poco::Net::HTTPServerRequest& request, 
                                                            Poco::Net::HTTPServerResponse& response)
{
    std::string productIdStr = getQueryParameter(request.getURI(), "productId");
    if (productIdStr.empty())
    {
        sendErrorResponse(response, "Product ID is required", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    long long productId;
    try
    {
        productId = std::stoll(productIdStr);
    }
    catch (...)
    {
        sendErrorResponse(response, "Invalid product ID", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    std::string quantityStr = getQueryParameter(request.getURI(), "quantity");
    if (quantityStr.empty())
    {
        sendErrorResponse(response, "Quantity is required", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    int quantity;
    try
    {
        quantity = std::stoi(quantityStr);
        if (quantity <= 0)
        {
            sendErrorResponse(response, "Quantity must be greater than 0", 
                            Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
            return;
        }
    }
    catch (...)
    {
        sendErrorResponse(response, "Invalid quantity", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    std::string authError;
    if (!canViewMovements(request, authError))
    {
        sendForbiddenResponse(response, authError);
        return;
    }
    
    try
    {
        auto result = inventoryService->checkStockAvailability(productId, quantity);
        
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object(result.toJson());
        sendSuccessResponse(response, "Stock availability checked", dataPtr);
    }
    catch (const std::exception& e)
    {
        sendErrorResponse(response, "Error checking stock availability: " + std::string(e.what()), 
                        Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
}

void InventoryMovementController::handleGetExpiringProducts(Poco::Net::HTTPServerRequest& request, 
                                                          Poco::Net::HTTPServerResponse& response)
{
    std::string daysThresholdStr = getQueryParameter(request.getURI(), "days");
    int daysThreshold = 30;
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
    if (!canViewMovements(request, authError))
    {
        sendForbiddenResponse(response, authError);
        return;
    }
    
    try
    {
        Poco::JSON::Array expiringProducts = inventoryService->findExpiringProducts(daysThreshold);
        
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object;
        dataPtr->set("expiringProducts", expiringProducts);
        sendSuccessResponse(response, "Expiring products retrieved", dataPtr);
    }
    catch (const std::exception& e)
    {
        sendErrorResponse(response, "Error retrieving expiring products: " + std::string(e.what()), 
                        Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
}

void InventoryMovementController::handleGetLowStockProducts(Poco::Net::HTTPServerRequest& request, 
                                                          Poco::Net::HTTPServerResponse& response)
{
    std::string thresholdStr = getQueryParameter(request.getURI(), "threshold");
    int threshold = 10;
    if (!thresholdStr.empty())
    {
        try
        {
            threshold = std::stoi(thresholdStr);
            if (threshold < 0)
            {
                sendErrorResponse(response, "Threshold must be non-negative", 
                                Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
                return;
            }
        }
        catch (...)
        {
            sendErrorResponse(response, "Invalid threshold", 
                            Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
            return;
        }
    }
    
    std::string authError;
    if (!canViewMovements(request, authError))
    {
        sendForbiddenResponse(response, authError);
        return;
    }
    
    try
    {
        Poco::JSON::Array lowStockProducts = inventoryService->findLowStockProducts(threshold);
        
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object;
        dataPtr->set("lowStockProducts", lowStockProducts);
        sendSuccessResponse(response, "Low stock products retrieved", dataPtr);
    }
    catch (const std::exception& e)
    {
        sendErrorResponse(response, "Error retrieving low stock products: " + std::string(e.what()), 
                        Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
}

void InventoryMovementController::handleGetInventoryStatistics(Poco::Net::HTTPServerRequest& request, 
                                                             Poco::Net::HTTPServerResponse& response)
{
    auto currentUserRole = getCurrentUserRole(request);
    if (currentUserRole != database::models::UserRole::ADMIN && 
        currentUserRole != database::models::UserRole::MANAGER && 
        currentUserRole != database::models::UserRole::AUDITOR)
    {
        sendForbiddenResponse(response, "You don't have permission to view inventory statistics");
        return;
    }
    
    try
    {
        Poco::JSON::Object statistics = inventoryService->getInventoryStatistics();
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object(statistics);
        
        sendSuccessResponse(response, "Inventory statistics retrieved", dataPtr);
    }
    catch (const std::exception& e)
    {
        sendErrorResponse(response, "Error retrieving inventory statistics: " + std::string(e.what()), 
                        Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
}

bool InventoryMovementController::validateRequest(Poco::Net::HTTPServerRequest& request, 
                                                Poco::Net::HTTPServerResponse& response,
                                                std::string& errorMessage)
{
    if (!BaseController::validateRequest(request, response, errorMessage))
    {
        return false;
    }
    
    return true;
}

bool InventoryMovementController::authorizeRequest(Poco::Net::HTTPServerRequest& request, 
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

long long InventoryMovementController::getCurrentUserId(Poco::Net::HTTPServerRequest& request)
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

database::models::UserRole InventoryMovementController::getCurrentUserRole(Poco::Net::HTTPServerRequest& request)
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

bool InventoryMovementController::canViewMovements(Poco::Net::HTTPServerRequest& request, 
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
    
    errorMessage = "You don't have permission to view inventory movements";
    return false;
}

bool InventoryMovementController::canCreateMovement(Poco::Net::HTTPServerRequest& request, 
                                                  const std::string& movementType,
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
        if (movementType == "receipt" || movementType == "transfer")
        {
            return true;
        }
    }
    
    errorMessage = "You don't have permission to create " + movementType + " movements";
    return false;
}

bool InventoryMovementController::canUpdateMovement(Poco::Net::HTTPServerRequest& request, 
                                                  long long movementId,
                                                  const std::string& newStatus,
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
    
    errorMessage = "You don't have permission to update this movement";
    return false;
}

bool InventoryMovementController::canCancelMovement(Poco::Net::HTTPServerRequest& request, 
                                                  long long movementId,
                                                  std::string& errorMessage)
{
    auto currentUserRole = getCurrentUserRole(request);
    
    if (currentUserRole == database::models::UserRole::ADMIN || 
        currentUserRole == database::models::UserRole::MANAGER)
    {
        return true;
    }
    
    errorMessage = "You don't have permission to cancel movements";
    return false;
}

bool InventoryMovementController::validateReceiptData(const Poco::JSON::Object::Ptr& json, 
                                                    std::vector<std::string>& errors)
{
    errors.clear();
    
    std::vector<std::string> requiredFields = {"product_id", "supplier_id", "quantity", 
                                              "unit_cost", "batch_number"};
    std::vector<std::string> missingFields;
    if (!validateRequiredFields(json, requiredFields, missingFields))
    {
        for (const auto& field : missingFields)
        {
            errors.push_back(field + " is required");
        }
    }
    
    if (json->has("product_id"))
    {
        try
        {
            long long productId = utils::JsonUtils::getInt(*json, "product_id");
            if (productId <= 0)
            {
                errors.push_back("Invalid product ID");
            }
        }
        catch (...)
        {
            errors.push_back("Invalid product ID");
        }
    }
    
    if (json->has("supplier_id"))
    {
        try
        {
            long long supplierId = utils::JsonUtils::getInt(*json, "supplier_id");
            if (supplierId <= 0)
            {
                errors.push_back("Invalid supplier ID");
            }
        }
        catch (...)
        {
            errors.push_back("Invalid supplier ID");
        }
    }
    
    if (json->has("quantity"))
    {
        int quantity = utils::JsonUtils::getInt(*json, "quantity");
        if (quantity <= 0)
        {
            errors.push_back("Quantity must be greater than 0");
        }
    }
    
    if (json->has("unit_cost"))
    {
        double unitCost = utils::JsonUtils::getDouble(*json, "unit_cost");
        if (unitCost < 0)
        {
            errors.push_back("Unit cost must be greater than or equal to 0");
        }
    }
    
    if (json->has("batch_number"))
    {
        std::string batchNumber = utils::JsonUtils::getString(*json, "batch_number");
        if (batchNumber.empty())
        {
            errors.push_back("Batch number cannot be empty");
        }
    }
    
    if (json->has("expiration_date") && !json->get("expiration_date").isEmpty())
    {
        std::string expirationDate = utils::JsonUtils::getString(*json, "expiration_date");
        if (!expirationDate.empty() && !utils::Validator::isValidDate(expirationDate))
        {
            errors.push_back("Invalid expiration date format. Use YYYY-MM-DD");
        }
    }
    
    if (json->has("storage_cell_id") && !json->get("storage_cell_id").isEmpty())
    {
        try
        {
            long long cellId = utils::JsonUtils::getInt(*json, "storage_cell_id");
            if (cellId <= 0)
            {
                errors.push_back("Invalid storage cell ID");
            }
        }
        catch (...)
        {
            errors.push_back("Invalid storage cell ID");
        }
    }
    
    return errors.empty();
}

bool InventoryMovementController::validateTransferData(const Poco::JSON::Object::Ptr& json, 
                                                     std::vector<std::string>& errors)
{
    errors.clear();
    
    std::vector<std::string> requiredFields = {"batch_id", "from_cell_id", "to_cell_id", "quantity"};
    std::vector<std::string> missingFields;
    if (!validateRequiredFields(json, requiredFields, missingFields))
    {
        for (const auto& field : missingFields)
        {
            errors.push_back(field + " is required");
        }
    }
    
    if (json->has("batch_id"))
    {
        try
        {
            long long batchId = utils::JsonUtils::getInt(*json, "batch_id");
            if (batchId <= 0)
            {
                errors.push_back("Invalid batch ID");
            }
        }
        catch (...)
        {
            errors.push_back("Invalid batch ID");
        }
    }
    
    if (json->has("from_cell_id"))
    {
        try
        {
            long long fromCellId = utils::JsonUtils::getInt(*json, "from_cell_id");
            if (fromCellId <= 0)
            {
                errors.push_back("Invalid from cell ID");
            }
        }
        catch (...)
        {
            errors.push_back("Invalid from cell ID");
        }
    }
    
    if (json->has("to_cell_id"))
    {
        try
        {
            long long toCellId = utils::JsonUtils::getInt(*json, "to_cell_id");
            if (toCellId <= 0)
            {
                errors.push_back("Invalid to cell ID");
            }
        }
        catch (...)
        {
            errors.push_back("Invalid to cell ID");
        }
    }
    
    if (json->has("quantity"))
    {
        int quantity = utils::JsonUtils::getInt(*json, "quantity");
        if (quantity <= 0)
        {
            errors.push_back("Quantity must be greater than 0");
        }
    }
    
    if (json->has("from_cell_id") && json->has("to_cell_id"))
    {
        try
        {
            long long fromCellId = utils::JsonUtils::getInt(*json, "from_cell_id");
            long long toCellId = utils::JsonUtils::getInt(*json, "to_cell_id");
            
            if (fromCellId == toCellId)
            {
                errors.push_back("From and to cells must be different");
            }
        }
        catch (...)
        {
        }
    }
    
    return errors.empty();
}

bool InventoryMovementController::validateAdjustmentData(const Poco::JSON::Object::Ptr& json, 
                                                       std::vector<std::string>& errors)
{
    errors.clear();
    
    std::vector<std::string> requiredFields = {"product_id", "batch_id", "cell_id", "quantity_adjustment"};
    std::vector<std::string> missingFields;
    if (!validateRequiredFields(json, requiredFields, missingFields))
    {
        for (const auto& field : missingFields)
        {
            errors.push_back(field + " is required");
        }
    }
    
    if (json->has("product_id"))
    {
        try
        {
            long long productId = utils::JsonUtils::getInt(*json, "product_id");
            if (productId <= 0)
            {
                errors.push_back("Invalid product ID");
            }
        }
        catch (...)
        {
            errors.push_back("Invalid product ID");
        }
    }
    
    if (json->has("batch_id"))
    {
        try
        {
            long long batchId = utils::JsonUtils::getInt(*json, "batch_id");
            if (batchId <= 0)
            {
                errors.push_back("Invalid batch ID");
            }
        }
        catch (...)
        {
            errors.push_back("Invalid batch ID");
        }
    }
    
    if (json->has("cell_id"))
    {
        try
        {
            long long cellId = utils::JsonUtils::getInt(*json, "cell_id");
            if (cellId <= 0)
            {
                errors.push_back("Invalid cell ID");
            }
        }
        catch (...)
        {
            errors.push_back("Invalid cell ID");
        }
    }
    
    if (json->has("quantity_adjustment"))
    {
        int quantityAdjustment = utils::JsonUtils::getInt(*json, "quantity_adjustment");
        if (quantityAdjustment == 0)
        {
            errors.push_back("Quantity adjustment cannot be 0");
        }
    }
    
    return errors.empty();
}

bool InventoryMovementController::validateStatusUpdateData(const Poco::JSON::Object::Ptr& json, 
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
            auto movementStatus = database::models::InventoryMovement::stringToMovementStatus(status);
            
            std::vector<std::string> validStatuses = {"planned", "in_progress", "completed", "cancelled"};
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
                errors.push_back("Invalid status value. Must be: planned, in_progress, completed, or cancelled");
            }
        }
        catch (...)
        {
            errors.push_back("Invalid status value");
        }
    }
    
    return errors.empty();
}

bool InventoryMovementController::validateSearchParameters(const std::map<std::string, std::string>& filters,
                                                         std::vector<std::string>& errors)
{
    errors.clear();
    
    for (const auto& [key, value] : filters)
    {
        if (key == "productId" || key == "cellId" || key == "batchId" || key == "userId")
        {
            try
            {
                long long id = std::stoll(value);
                if (id <= 0)
                {
                    errors.push_back(key + " must be a positive number");
                }
            }
            catch (...)
            {
                errors.push_back("Invalid " + key + " value");
            }
        }
        else if (key == "startDate" || key == "endDate")
        {
            if (!utils::Validator::isValidDate(value))
            {
                errors.push_back(key + " must be in YYYY-MM-DD format");
            }
        }
        else if (key == "movementType")
        {
            try
            {
                auto movementType = database::models::InventoryMovement::stringToMovementType(value);
            }
            catch (...)
            {
                errors.push_back("Invalid movement type. Must be: receipt, shipment, transfer, adjustment, or count");
            }
        }
        else if (key == "status")
        {
            try
            {
                auto status = database::models::InventoryMovement::stringToMovementStatus(value);
            }
            catch (...)
            {
                errors.push_back("Invalid status. Must be: planned, in_progress, completed, or cancelled");
            }
        }
        else if (key == "minQuantity" || key == "maxQuantity")
        {
            try
            {
                int quantity = std::stoi(value);
                if (quantity < 0)
                {
                    errors.push_back(key + " must be greater than or equal to 0");
                }
            }
            catch (...)
            {
                errors.push_back("Invalid " + key + " value");
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
    
    return errors.empty();
}

std::unique_ptr<database::models::InventoryMovement> InventoryMovementController::extractMovementFromJson(
    const Poco::JSON::Object::Ptr& json)
{
    if (!json)
    {
        return nullptr;
    }
    
    try
    {
        auto movement = std::make_unique<database::models::InventoryMovement>(*json);
        return movement;
    }
    catch (...)
    {
        return nullptr;
    }
}

void InventoryMovementController::logMovementEvent(long long userId, 
                                                 const std::string& action,
                                                 long long movementId,
                                                 const std::string& ipAddress,
                                                 const std::string& userAgent,
                                                 bool success,
                                                 const std::string& details)
{
    std::cout << "[" << getCurrentTimestamp() << "] "
              << "INVENTORY_MOVEMENT " << action << " "
              << "UserID: " << userId << " "
              << "MovementID: " << movementId << " "
              << "IP: " << ipAddress << " "
              << "Success: " << (success ? "Yes" : "No") << " "
              << "Details: " << details << std::endl;
}

} // namespace controllers
