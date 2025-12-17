#include "WarehouseCellController.hpp"
#include "../utils/JsonUtils.hpp"
#include "../utils/Validator.hpp"
#include <Poco/Net/HTTPRequestHandler.h>
#include <Poco/Net/HTTPServerRequest.h>
#include <Poco/Net/HTTPServerResponse.h>
#include <Poco/JSON/Parser.h>
#include <Poco/StreamCopier.h>
#include <Poco/URI.h>
#include <iostream>
#include <sstream>

namespace warehouse_backend::controllers
{

WarehouseCellController::WarehouseCellController()
    : cellService(std::make_unique<services::WarehouseCellService>()),
      authService(std::make_unique<services::AuthService>()),
      inventoryService(std::make_unique<services::InventoryService>())
{
}

void WarehouseCellController::handleRequest(Poco::Net::HTTPServerRequest& request, 
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
        
        std::string basePath = "/api/v1/warehouse-cells";
        if (endpoint.find("/api/v1/") != 0)
        {
            basePath = "/api/warehouse-cells";
        }
        
        std::string cellIdStr = getPathParameter(uri, basePath + "/", 0);
        
        if (endpoint.find("/clear") != std::string::npos && request.getMethod() == "POST")
        {
            handleClearCell(request, response);
        }
        else if ((endpoint == "/api/v1/warehouse-cells/statistics" || endpoint == "/api/warehouse-cells/statistics") && 
            request.getMethod() == "GET")
        {
            handleGetCellStatistics(request, response);
        }
        else if ((endpoint.find("/api/v1/warehouse-cells/occupancy-report") == 0 || 
                 endpoint.find("/api/warehouse-cells/occupancy-report") == 0) && 
                request.getMethod() == "GET")
        {
            handleGetOccupancyReport(request, response);
        }
        else if ((endpoint.find("/api/v1/warehouse-cells/find-best") == 0 || 
                 endpoint.find("/api/warehouse-cells/find-best") == 0) && 
                request.getMethod() == "GET")
        {
            handleFindBestCellForStorage(request, response);
        }
        else if ((endpoint.find("/api/v1/warehouse-cells/available") == 0 || 
                 endpoint.find("/api/warehouse-cells/available") == 0) && 
                request.getMethod() == "GET")
        {
            handleGetAvailableCells(request, response);
        }
        else if ((endpoint.find("/api/v1/warehouse-cells/zone") == 0 || 
                 endpoint.find("/api/warehouse-cells/zone") == 0) && 
                request.getMethod() == "GET")
        {
            handleGetCellsByZone(request, response);
        }
        else if ((endpoint.find("/api/v1/warehouse-cells/status") == 0 || 
                 endpoint.find("/api/warehouse-cells/status") == 0) && 
                request.getMethod() == "GET")
        {
            handleGetCellsByStatus(request, response);
        }
        else if ((endpoint.find("/api/v1/warehouse-cells/search") == 0 || 
                 endpoint.find("/api/warehouse-cells/search") == 0) && 
                request.getMethod() == "GET")
        {
            handleGetCells(request, response);
        }
        else if ((endpoint.find("/api/v1/warehouse-cells/code") == 0 || 
                 endpoint.find("/api/warehouse-cells/code") == 0) && 
                request.getMethod() == "GET")
        {
            handleGetCellByCode(request, response);
        }
        else if ((endpoint.find("/api/v1/warehouse-cells/") == 0 || 
                 endpoint.find("/api/warehouse-cells/") == 0) && 
                !cellIdStr.empty())
        {
            if (endpoint.find("/batches") != std::string::npos && request.getMethod() == "GET")
            {
                handleGetCellBatches(request, response);
            }
            else if (endpoint.find("/block") != std::string::npos && request.getMethod() == "POST")
            {
                handleBlockCell(request, response);
            }
            else if (endpoint.find("/unblock") != std::string::npos && request.getMethod() == "POST")
            {
                handleUnblockCell(request, response);
            }
            else if (request.getMethod() == "GET")
            {
                handleGetCellById(request, response);
            }
            else if (request.getMethod() == "PUT")
            {
                handleUpdateCell(request, response);
            }
            else if (request.getMethod() == "DELETE")
            {
                handleDeleteCell(request, response);
            }
        }
        else if ((endpoint == "/api/v1/warehouse-cells" || endpoint == "/api/warehouse-cells") && 
                request.getMethod() == "GET")
        {
            handleGetCells(request, response);
        }
        else if ((endpoint == "/api/v1/warehouse-cells" || endpoint == "/api/warehouse-cells") && 
                request.getMethod() == "POST")
        {
            handleCreateCell(request, response);
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

void WarehouseCellController::handleGetCells(Poco::Net::HTTPServerRequest& request, 
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
    
    Poco::JSON::Array cellsArray;
    
    try
    {
        if (filters.find("zone") != filters.end())
        {
            std::string zone = filters["zone"];
            cellsArray = cellService->getCellsByZone(zone);
        }
        else if (filters.find("status") != filters.end())
        {
            std::string status = filters["status"];
            cellsArray = cellService->getCellsByStatus(status);
        }
        else if (filters.find("temperatureZone") != filters.end())
        {
            sendErrorResponse(response, "Filter by temperature zone not yet implemented", 
                            Poco::Net::HTTPResponse::HTTP_NOT_IMPLEMENTED);
            return;
        }
        else
        {
            auto allCells = cellService->getCellsByZone("");
            cellsArray = allCells;
        }
        
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object;
        dataPtr->set("cells", cellsArray);
        sendSuccessResponse(response, "Warehouse cells retrieved successfully", dataPtr);
    }
    catch (const std::exception& e)
    {
        sendErrorResponse(response, "Error retrieving warehouse cells: " + std::string(e.what()), 
                        Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
}

void WarehouseCellController::handleGetCellById(Poco::Net::HTTPServerRequest& request, 
                                              Poco::Net::HTTPServerResponse& response)
{
    std::string cellIdStr = getPathParameter(request.getURI(), "/api/v1/warehouse-cells/", 0);
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
    
    std::string accessError;
    if (!validateCellAccess(request, cellId, accessError))
    {
        sendForbiddenResponse(response, accessError);
        return;
    }
    
    auto result = cellService->getCellById(cellId);
    
    if (!result.success)
    {
        if (result.message.find("not found") != std::string::npos)
        {
            sendNotFoundResponse(response, "Warehouse cell");
        }
        else
        {
            sendErrorResponse(response, result.message, 
                            Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
        }
        return;
    }
    
    Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object(result.data);
    sendSuccessResponse(response, "Warehouse cell retrieved successfully", dataPtr);
}

void WarehouseCellController::handleGetCellByCode(Poco::Net::HTTPServerRequest& request, 
                                                Poco::Net::HTTPServerResponse& response)
{
    std::string cellCode = getQueryParameter(request.getURI(), "code");
    if (cellCode.empty())
    {
        sendErrorResponse(response, "Cell code is required", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    auto result = cellService->getCellByCode(cellCode);
    
    if (!result.success)
    {
        if (result.message.find("not found") != std::string::npos)
        {
            sendNotFoundResponse(response, "Warehouse cell");
        }
        else
        {
            sendErrorResponse(response, result.message, 
                            Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
        }
        return;
    }
    
    Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object(result.data);
    sendSuccessResponse(response, "Warehouse cell retrieved successfully", dataPtr);
}

void WarehouseCellController::handleCreateCell(Poco::Net::HTTPServerRequest& request, 
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
    if (!validateCreateCellData(json, errors))
    {
        sendValidationErrorResponse(response, errors);
        return;
    }
    
    auto cellData = extractCellFromJson(json);
    if (!cellData)
    {
        sendErrorResponse(response, "Invalid cell data", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    std::string authError;
    if (!canCreateCell(request, *cellData, authError))
    {
        sendForbiddenResponse(response, authError);
        return;
    }
    
    long long createdBy = getCurrentUserId(request);
    
    auto result = cellService->createCell(*cellData, createdBy);
    
    logCellEvent(createdBy, "CREATE_CELL", result.cellId, 
                getClientIpAddress(request), getUserAgent(request), 
                result.success, result.success ? "Cell created successfully" : result.message);
    
    if (result.success)
    {
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object(result.data);
        sendSuccessResponse(response, "Warehouse cell created successfully", dataPtr);
    }
    else
    {
        sendErrorResponse(response, result.message, 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
    }
}

void WarehouseCellController::handleUpdateCell(Poco::Net::HTTPServerRequest& request, 
                                             Poco::Net::HTTPServerResponse& response)
{
    std::string cellIdStr = getPathParameter(request.getURI(), "/api/v1/warehouse-cells/", 0);
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
    
    auto json = parseJsonBody(request);
    if (!json)
    {
        sendErrorResponse(response, "Invalid JSON body", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    std::vector<std::string> errors;
    if (!validateUpdateCellData(json, errors))
    {
        sendValidationErrorResponse(response, errors);
        return;
    }
    
    auto cellData = extractCellFromJson(json);
    if (!cellData)
    {
        sendErrorResponse(response, "Invalid cell data", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    std::string authError;
    if (!canUpdateCell(request, cellId, *cellData, authError))
    {
        sendForbiddenResponse(response, authError);
        return;
    }
    
    long long updatedBy = getCurrentUserId(request);
    
    auto result = cellService->updateCell(cellId, *cellData, updatedBy);
    
    logCellEvent(updatedBy, "UPDATE_CELL", cellId, 
                getClientIpAddress(request), getUserAgent(request), 
                result.success, result.success ? "Cell updated successfully" : result.message);
    
    if (result.success)
    {
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object(result.data);
        sendSuccessResponse(response, "Warehouse cell updated successfully", dataPtr);
    }
    else
    {
        sendErrorResponse(response, result.message, 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
    }
}

void WarehouseCellController::handleDeleteCell(Poco::Net::HTTPServerRequest& request, 
                                             Poco::Net::HTTPServerResponse& response)
{
    std::string cellIdStr = getPathParameter(request.getURI(), "/api/v1/warehouse-cells/", 0);
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
    
    std::string authError;
    if (!canDeleteCell(request, cellId, authError))
    {
        sendForbiddenResponse(response, authError);
        return;
    }
    
    long long deletedBy = getCurrentUserId(request);
    
    bool forceDelete = false;
    bool clearOnly = false;
    
    auto queryParams = getFilterParameters(request);
    if (queryParams.find("force") != queryParams.end() && 
        (queryParams["force"] == "true" || queryParams["force"] == "1"))
    {
        forceDelete = true;
    }
    
    if (queryParams.find("clear") != queryParams.end() && 
        (queryParams["clear"] == "true" || queryParams["clear"] == "1"))
    {
        clearOnly = true;
    }
    
    services::WarehouseCellServiceResult result;
    
    if (clearOnly)
    {
        result = cellService->clearCell(cellId, deletedBy);
    }
    else
    {
        result = cellService->deleteCell(cellId, deletedBy);
    }
    
    logCellEvent(deletedBy, clearOnly ? "CLEAR_CELL" : "DELETE_CELL", cellId, 
                getClientIpAddress(request), getUserAgent(request), 
                result.success, result.success ? (clearOnly ? "Cell cleared successfully" : "Cell deleted successfully") : result.message);
    
    if (result.success)
    {
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object(result.data);
        sendSuccessResponse(response, 
                          clearOnly ? "Warehouse cell cleared successfully" : "Warehouse cell deleted successfully", 
                          dataPtr);
    }
    else
    {
        sendErrorResponse(response, result.message, 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
    }
}

void WarehouseCellController::handleClearCell(Poco::Net::HTTPServerRequest& request, 
                                            Poco::Net::HTTPServerResponse& response)
{
    std::string cellIdStr = getPathParameter(request.getURI(), "/api/v1/warehouse-cells/", 0);
    cellIdStr = cellIdStr.substr(0, cellIdStr.find("/clear"));
    
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
    
    std::string authError;
    if (!canUpdateCell(request, cellId, database::models::WarehouseCell(), authError))
    {
        sendForbiddenResponse(response, authError);
        return;
    }
    
    long long clearedBy = getCurrentUserId(request);
    
    auto result = cellService->clearCell(cellId, clearedBy);
    
    logCellEvent(clearedBy, "CLEAR_CELL", cellId, 
                getClientIpAddress(request), getUserAgent(request), 
                result.success, result.success ? "Cell cleared successfully" : result.message);
    
    if (result.success)
    {
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object(result.data);
        sendSuccessResponse(response, "Warehouse cell cleared successfully", dataPtr);
    }
    else
    {
        sendErrorResponse(response, result.message, 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
    }
}

void WarehouseCellController::handleBlockCell(Poco::Net::HTTPServerRequest& request, 
                                            Poco::Net::HTTPServerResponse& response)
{
    std::string cellIdStr = getPathParameter(request.getURI(), "/api/v1/warehouse-cells/", 0);
    cellIdStr = cellIdStr.substr(0, cellIdStr.find("/block"));
    
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
    
    std::string authError;
    if (!canBlockCell(request, cellId, authError))
    {
        sendForbiddenResponse(response, authError);
        return;
    }
    
    long long blockedBy = getCurrentUserId(request);
    
    auto result = cellService->blockCell(cellId, blockedBy);
    
    logCellEvent(blockedBy, "BLOCK_CELL", cellId, 
                getClientIpAddress(request), getUserAgent(request), 
                result.success, result.success ? "Cell blocked successfully" : result.message);
    
    if (result.success)
    {
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object(result.data);
        sendSuccessResponse(response, "Warehouse cell blocked successfully", dataPtr);
    }
    else
    {
        sendErrorResponse(response, result.message, 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
    }
}

void WarehouseCellController::handleUnblockCell(Poco::Net::HTTPServerRequest& request, 
                                              Poco::Net::HTTPServerResponse& response)
{
    std::string cellIdStr = getPathParameter(request.getURI(), "/api/v1/warehouse-cells/", 0);
    cellIdStr = cellIdStr.substr(0, cellIdStr.find("/unblock"));
    
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
    
    std::string authError;
    if (!canBlockCell(request, cellId, authError))
    {
        sendForbiddenResponse(response, authError);
        return;
    }
    
    long long unblockedBy = getCurrentUserId(request);
    
    auto result = cellService->unblockCell(cellId, unblockedBy);
    
    logCellEvent(unblockedBy, "UNBLOCK_CELL", cellId, 
                getClientIpAddress(request), getUserAgent(request), 
                result.success, result.success ? "Cell unblocked successfully" : result.message);
    
    if (result.success)
    {
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object(result.data);
        sendSuccessResponse(response, "Warehouse cell unblocked successfully", dataPtr);
    }
    else
    {
        sendErrorResponse(response, result.message, 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
    }
}

void WarehouseCellController::handleGetAvailableCells(Poco::Net::HTTPServerRequest& request, 
                                                    Poco::Net::HTTPServerResponse& response)
{
    double requiredVolume = 0.0;
    double requiredWeight = 0.0;
    
    auto queryParams = getFilterParameters(request);
    
    if (queryParams.find("volume") != queryParams.end())
    {
        try
        {
            requiredVolume = std::stod(queryParams["volume"]);
        }
        catch (...)
        {
            sendErrorResponse(response, "Invalid volume parameter", 
                            Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
            return;
        }
    }
    
    if (queryParams.find("weight") != queryParams.end())
    {
        try
        {
            requiredWeight = std::stod(queryParams["weight"]);
        }
        catch (...)
        {
            sendErrorResponse(response, "Invalid weight parameter", 
                            Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
            return;
        }
    }
    
    if (requiredVolume <= 0 && requiredWeight <= 0)
    {
        sendErrorResponse(response, "Either volume or weight must be specified and greater than 0", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    Poco::JSON::Array cellsArray = cellService->getAvailableCells(requiredVolume, requiredWeight);
    Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object;
    dataPtr->set("cells", cellsArray);
    
    sendSuccessResponse(response, "Available cells retrieved successfully", dataPtr);
}

void WarehouseCellController::handleGetCellsByZone(Poco::Net::HTTPServerRequest& request, 
                                                 Poco::Net::HTTPServerResponse& response)
{
    std::string zone = getQueryParameter(request.getURI(), "zone");
    if (zone.empty())
    {
        sendErrorResponse(response, "Zone is required", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    Poco::JSON::Array cellsArray = cellService->getCellsByZone(zone);
    Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object;
    dataPtr->set("cells", cellsArray);
    
    sendSuccessResponse(response, "Cells by zone retrieved successfully", dataPtr);
}

void WarehouseCellController::handleGetCellsByStatus(Poco::Net::HTTPServerRequest& request, 
                                                   Poco::Net::HTTPServerResponse& response)
{
    std::string status = getQueryParameter(request.getURI(), "status");
    if (status.empty())
    {
        sendErrorResponse(response, "Status is required", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    Poco::JSON::Array cellsArray = cellService->getCellsByStatus(status);
    Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object;
    dataPtr->set("cells", cellsArray);
    
    sendSuccessResponse(response, "Cells by status retrieved successfully", dataPtr);
}

void WarehouseCellController::handleGetCellBatches(Poco::Net::HTTPServerRequest& request, 
                                                 Poco::Net::HTTPServerResponse& response)
{
    std::string cellIdStr = getPathParameter(request.getURI(), "/api/v1/warehouse-cells/", 0);
    cellIdStr = cellIdStr.substr(0, cellIdStr.find("/batches"));
    
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
    
    Poco::JSON::Array::Ptr batchesArray = new Poco::JSON::Array(cellService->getCellBatches(cellId));
    Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object;
    dataPtr->set("batches", batchesArray);
    
    sendSuccessResponse(response, "Cell batches retrieved successfully", dataPtr);
}

void WarehouseCellController::handleFindBestCellForStorage(Poco::Net::HTTPServerRequest& request, 
                                                         Poco::Net::HTTPServerResponse& response)
{
    auto json = parseJsonBody(request);
    double requiredVolume = 0.0;
    double requiredWeight = 0.0;
    std::string temperatureZone = "";
    
    if (json)
    {
        requiredVolume = std::stod(getQueryParameter(request.getURI(), "volume"));
        requiredWeight = std::stod(getQueryParameter(request.getURI(), "weight"));
        temperatureZone = getQueryParameter(request.getURI(), "temperatureZone");
    }
    else
    {
        auto queryParams = getFilterParameters(request);
        
        if (queryParams.find("volume") != queryParams.end())
        {
            try
            {
                requiredVolume = std::stod(queryParams["volume"]);
            }
            catch (...)
            {
                sendErrorResponse(response, "Invalid volume parameter", 
                                Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
                return;
            }
        }
        
        if (queryParams.find("weight") != queryParams.end())
        {
            try
            {
                requiredWeight = std::stod(queryParams["weight"]);
            }
            catch (...)
            {
                sendErrorResponse(response, "Invalid weight parameter", 
                                Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
                return;
            }
        }
        
        if (queryParams.find("temperatureZone") != queryParams.end())
        {
            temperatureZone = queryParams["temperatureZone"];
        }
    }
    
    if (requiredVolume <= 0 && requiredWeight <= 0)
    {
        sendErrorResponse(response, "Either volume or weight must be specified and greater than 0", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    auto result = cellService->findBestCellForStorage(requiredVolume, requiredWeight, temperatureZone);
    
    if (!result.success)
    {
        sendErrorResponse(response, result.message, 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object(result.data);
    sendSuccessResponse(response, "Best cell for storage found", dataPtr);
}

void WarehouseCellController::handleGetCellStatistics(Poco::Net::HTTPServerRequest& request, 
                                                    Poco::Net::HTTPServerResponse& response)
{
    auto currentUserRole = getCurrentUserRole(request);
    if (currentUserRole != database::models::UserRole::ADMIN && 
        currentUserRole != database::models::UserRole::MANAGER && 
        currentUserRole != database::models::UserRole::AUDITOR)
    {
        sendForbiddenResponse(response, "You don't have permission to view cell statistics");
        return;
    }
    
    Poco::JSON::Object statistics = inventoryService->getInventoryStatistics();
    Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object(statistics);
    
    sendSuccessResponse(response, "Cell statistics retrieved", dataPtr);
}

void WarehouseCellController::handleGetOccupancyReport(Poco::Net::HTTPServerRequest& request, 
                                                     Poco::Net::HTTPServerResponse& response)
{
    auto currentUserRole = getCurrentUserRole(request);
    if (currentUserRole != database::models::UserRole::ADMIN && 
        currentUserRole != database::models::UserRole::MANAGER && 
        currentUserRole != database::models::UserRole::AUDITOR)
    {
        sendForbiddenResponse(response, "You don't have permission to view occupancy reports");
        return;
    }
    
    Poco::JSON::Array report = inventoryService->getStockValueReport();
    Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object;
    dataPtr->set("report", report);
    
    sendSuccessResponse(response, "Occupancy report generated", dataPtr);
}

bool WarehouseCellController::validateRequest(Poco::Net::HTTPServerRequest& request, 
                                            Poco::Net::HTTPServerResponse& response,
                                            std::string& errorMessage)
{
    if (!BaseController::validateRequest(request, response, errorMessage))
    {
        return false;
    }
    
    return true;
}

bool WarehouseCellController::authorizeRequest(Poco::Net::HTTPServerRequest& request, 
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

bool WarehouseCellController::validateCellAccess(Poco::Net::HTTPServerRequest& request, 
                                               long long cellId,
                                               std::string& errorMessage)
{
    auto currentUserRole = getCurrentUserRole(request);
    
    if (currentUserRole == database::models::UserRole::ADMIN || 
        currentUserRole == database::models::UserRole::MANAGER || 
        currentUserRole == database::models::UserRole::AUDITOR)
    {
        return true;
    }
    
    if (currentUserRole == database::models::UserRole::WORKER)
    {
        return true;
    }
    
    errorMessage = "You don't have permission to access this warehouse cell";
    return false;
}

long long WarehouseCellController::getCurrentUserId(Poco::Net::HTTPServerRequest& request)
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

database::models::UserRole WarehouseCellController::getCurrentUserRole(Poco::Net::HTTPServerRequest& request)
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

bool WarehouseCellController::validateCreateCellData(const Poco::JSON::Object::Ptr& json, 
                                                   std::vector<std::string>& errors)
{
    errors.clear();
    
    std::vector<std::string> requiredFields = {"cell_code", "zone", "rack", "shelf", "position", 
                                               "max_volume", "max_weight"};
    std::vector<std::string> missingFields;
    if (!validateRequiredFields(json, requiredFields, missingFields))
    {
        for (const auto& field : missingFields)
        {
            errors.push_back(field + " is required");
        }
    }
    
    if (json->has("cell_code"))
    {
        std::string cellCode = utils::JsonUtils::getString(*json, "cell_code");
        if (cellCode.empty() || cellCode.length() > 50)
        {
            errors.push_back("Cell code must be between 1 and 50 characters");
        }
    }
    
    if (json->has("zone"))
    {
        std::string zone = utils::JsonUtils::getString(*json, "zone");
        if (zone.empty() || zone.length() > 20)
        {
            errors.push_back("Zone must be between 1 and 20 characters");
        }
    }
    
    if (json->has("rack"))
    {
        std::string rack = utils::JsonUtils::getString(*json, "rack");
        if (rack.empty() || rack.length() > 10)
        {
            errors.push_back("Rack must be between 1 and 10 characters");
        }
    }
    
    if (json->has("shelf"))
    {
        std::string shelf = utils::JsonUtils::getString(*json, "shelf");
        if (shelf.empty() || shelf.length() > 10)
        {
            errors.push_back("Shelf must be between 1 and 10 characters");
        }
    }
    
    if (json->has("position"))
    {
        std::string position = utils::JsonUtils::getString(*json, "position");
        if (position.empty() || position.length() > 10)
        {
            errors.push_back("Position must be between 1 and 10 characters");
        }
    }
    
    if (json->has("max_volume"))
    {
        double maxVolume = utils::JsonUtils::getDouble(*json, "max_volume");
        if (maxVolume <= 0)
        {
            errors.push_back("Max volume must be greater than 0");
        }
    }
    
    if (json->has("max_weight"))
    {
        double maxWeight = utils::JsonUtils::getDouble(*json, "max_weight");
        if (maxWeight <= 0)
        {
            errors.push_back("Max weight must be greater than 0");
        }
    }
    
    if (json->has("temperature_zone"))
    {
        std::string tempZone = utils::JsonUtils::getString(*json, "temperature_zone");
        if (!tempZone.empty())
        {
            try
            {
                auto temperatureZone = database::models::WarehouseCell::stringToTemperatureZone(tempZone);
            }
            catch (...)
            {
                errors.push_back("Invalid temperature zone value");
            }
        }
    }
    
    return errors.empty();
}

bool WarehouseCellController::validateUpdateCellData(const Poco::JSON::Object::Ptr& json, 
                                                   std::vector<std::string>& errors)
{
    errors.clear();
    
    bool hasCellCode = json->has("cell_code") && !json->get("cell_code").isEmpty();
    bool hasZone = json->has("zone") && !json->get("zone").isEmpty();
    bool hasRack = json->has("rack") && !json->get("rack").isEmpty();
    bool hasShelf = json->has("shelf") && !json->get("shelf").isEmpty();
    bool hasPosition = json->has("position") && !json->get("position").isEmpty();
    bool hasMaxVolume = json->has("max_volume") && !json->get("max_volume").isEmpty();
    bool hasMaxWeight = json->has("max_weight") && !json->get("max_weight").isEmpty();
    bool hasTempZone = json->has("temperature_zone") && !json->get("temperature_zone").isEmpty();
    
    if (!hasCellCode && !hasZone && !hasRack && !hasShelf && !hasPosition && 
        !hasMaxVolume && !hasMaxWeight && !hasTempZone)
    {
        errors.push_back("At least one field must be provided for update");
    }
    
    if (hasCellCode)
    {
        std::string cellCode = utils::JsonUtils::getString(*json, "cell_code");
        if (cellCode.empty() || cellCode.length() > 50)
        {
            errors.push_back("Cell code must be between 1 and 50 characters");
        }
    }
    
    if (hasZone)
    {
        std::string zone = utils::JsonUtils::getString(*json, "zone");
        if (zone.empty() || zone.length() > 20)
        {
            errors.push_back("Zone must be between 1 and 20 characters");
        }
    }
    
    if (hasRack)
    {
        std::string rack = utils::JsonUtils::getString(*json, "rack");
        if (rack.empty() || rack.length() > 10)
        {
            errors.push_back("Rack must be between 1 and 10 characters");
        }
    }
    
    if (hasShelf)
    {
        std::string shelf = utils::JsonUtils::getString(*json, "shelf");
        if (shelf.empty() || shelf.length() > 10)
        {
            errors.push_back("Shelf must be between 1 and 10 characters");
        }
    }
    
    if (hasPosition)
    {
        std::string position = utils::JsonUtils::getString(*json, "position");
        if (position.empty() || position.length() > 10)
        {
            errors.push_back("Position must be between 1 and 10 characters");
        }
    }
    
    if (hasMaxVolume)
    {
        double maxVolume = utils::JsonUtils::getDouble(*json, "max_volume");
        if (maxVolume <= 0)
        {
            errors.push_back("Max volume must be greater than 0");
        }
    }
    
    if (hasMaxWeight)
    {
        double maxWeight = utils::JsonUtils::getDouble(*json, "max_weight");
        if (maxWeight <= 0)
        {
            errors.push_back("Max weight must be greater than 0");
        }
    }
    
    if (hasTempZone)
    {
        std::string tempZone = utils::JsonUtils::getString(*json, "temperature_zone");
        if (!tempZone.empty())
        {
            try
            {
                auto temperatureZone = database::models::WarehouseCell::stringToTemperatureZone(tempZone);
            }
            catch (...)
            {
                errors.push_back("Invalid temperature zone value");
            }
        }
    }
    
    return errors.empty();
}

bool WarehouseCellController::validateStorageParameters(const Poco::JSON::Object::Ptr& json, 
                                                      std::vector<std::string>& errors)
{
    errors.clear();
    
    std::vector<std::string> requiredFields = {"volume", "weight"};
    std::vector<std::string> missingFields;
    if (!validateRequiredFields(json, requiredFields, missingFields))
    {
        for (const auto& field : missingFields)
        {
            errors.push_back(field + " is required");
        }
    }
    
    if (json->has("volume"))
    {
        double volume = utils::JsonUtils::getDouble(*json, "volume");
        if (volume <= 0)
        {
            errors.push_back("Volume must be greater than 0");
        }
    }
    
    if (json->has("weight"))
    {
        double weight = utils::JsonUtils::getDouble(*json, "weight");
        if (weight <= 0)
        {
            errors.push_back("Weight must be greater than 0");
        }
    }
    
    if (json->has("temperature_zone"))
    {
        std::string tempZone = utils::JsonUtils::getString(*json, "temperature_zone");
        if (!tempZone.empty())
        {
            try
            {
                auto temperatureZone = database::models::WarehouseCell::stringToTemperatureZone(tempZone);
            }
            catch (...)
            {
                errors.push_back("Invalid temperature zone value");
            }
        }
    }
    
    return errors.empty();
}

bool WarehouseCellController::validateSearchParameters(const std::map<std::string, std::string>& filters,
                                                     std::vector<std::string>& errors)
{
    errors.clear();
    
    for (const auto& [key, value] : filters)
    {
        if (key == "zone")
        {
            if (value.empty() || value.length() > 20)
            {
                errors.push_back("Zone must be between 1 and 20 characters");
            }
        }
        else if (key == "status")
        {
            try
            {
                auto status = database::models::WarehouseCell::stringToStatus(value);
            }
            catch (...)
            {
                errors.push_back("Invalid status value");
            }
        }
        else if (key == "temperature_zone")
        {
            try
            {
                auto tempZone = database::models::WarehouseCell::stringToTemperatureZone(value);
            }
            catch (...)
            {
                errors.push_back("Invalid temperature zone value");
            }
        }
        else if (key == "volume")
        {
            try
            {
                double volume = std::stod(value);
                if (volume < 0)
                {
                    errors.push_back("Volume must be greater than or equal to 0");
                }
            }
            catch (...)
            {
                errors.push_back("Invalid volume");
            }
        }
        else if (key == "weight")
        {
            try
            {
                double weight = std::stod(value);
                if (weight < 0)
                {
                    errors.push_back("Weight must be greater than or equal to 0");
                }
            }
            catch (...)
            {
                errors.push_back("Invalid weight");
            }
        }
    }
    
    return errors.empty();
}

void WarehouseCellController::logCellEvent(long long userId, 
                                         const std::string& action,
                                         long long cellId,
                                         const std::string& ipAddress,
                                         const std::string& userAgent,
                                         bool success,
                                         const std::string& details)
{
    std::cout << "[" << getCurrentTimestamp() << "] "
            << "WAREHOUSE_CELL " << action << " "
            << "UserID: " << userId << " "
            << "CellID: " << cellId << " "
            << "IP: " << ipAddress << " "
            << "Success: " << (success ? "Yes" : "No") << " "
            << "Details: " << details << std::endl;
}

std::unique_ptr<database::models::WarehouseCell> WarehouseCellController::extractCellFromJson(const Poco::JSON::Object::Ptr& json)
{
    if (!json)
    {
        return nullptr;
    }
    
    try
    {
        auto cell = std::make_unique<database::models::WarehouseCell>(*json);
        return cell;
    }
    catch (...)
    {
        return nullptr;
    }
}

bool WarehouseCellController::canCreateCell(Poco::Net::HTTPServerRequest& request, 
                                          const database::models::WarehouseCell& cellData,
                                          std::string& errorMessage)
{
    auto currentUserRole = getCurrentUserRole(request);
    
    if (currentUserRole == database::models::UserRole::ADMIN || 
        currentUserRole == database::models::UserRole::MANAGER)
    {
        return true;
    }
    
    errorMessage = "You don't have permission to create warehouse cells";
    return false;
}

bool WarehouseCellController::canUpdateCell(Poco::Net::HTTPServerRequest& request, 
                                          long long cellId,
                                          const database::models::WarehouseCell& cellData,
                                          std::string& errorMessage)
{
    auto currentUserRole = getCurrentUserRole(request);
    
    if (currentUserRole == database::models::UserRole::ADMIN || 
        currentUserRole == database::models::UserRole::MANAGER)
    {
        return true;
    }
    
    errorMessage = "You don't have permission to update this warehouse cell";
    return false;
}

bool WarehouseCellController::canDeleteCell(Poco::Net::HTTPServerRequest& request, 
                                          long long cellId,
                                          std::string& errorMessage)
{
    auto currentUserRole = getCurrentUserRole(request);
    
    if (currentUserRole == database::models::UserRole::ADMIN)
    {
        return true;
    }
    
    errorMessage = "You don't have permission to delete this warehouse cell";
    return false;
}

bool WarehouseCellController::canBlockCell(Poco::Net::HTTPServerRequest& request, 
                                         long long cellId,
                                         std::string& errorMessage)
{
    auto currentUserRole = getCurrentUserRole(request);
    
    if (currentUserRole == database::models::UserRole::ADMIN || 
        currentUserRole == database::models::UserRole::MANAGER)
    {
        return true;
    }
    
    errorMessage = "You don't have permission to block/unblock this warehouse cell";
    return false;
}

Poco::JSON::Object WarehouseCellController::buildPaginationResponse(int page, int pageSize, int totalItems, 
                                                                  const Poco::JSON::Array& data)
{
    Poco::JSON::Object response;
    response.set("page", page);
    response.set("pageSize", pageSize);
    response.set("totalItems", totalItems);
    response.set("totalPages", (totalItems + pageSize - 1) / pageSize);
    response.set("data", data);
    
    return response;
}

} // namespace controllers
