#include "ProductBatchController.hpp"
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

ProductBatchController::ProductBatchController()
    : inventoryService(std::make_unique<services::InventoryService>()),
      authService(std::make_unique<services::AuthService>()),
      productService(std::make_unique<services::ProductService>()),
      batchService(std::make_unique<services::ProductBatchService>())
{
}

void ProductBatchController::handleRequest(Poco::Net::HTTPServerRequest& request, 
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
        
        std::string basePath = "/api/v1/product-batches";
        if (endpoint.find("/api/v1/") != 0)
        {
            basePath = "/api/product-batches";
        }
        
        std::string batchIdStr = getPathParameter(uri, basePath + "/", 0);
        
        if ((endpoint == "/api/v1/product-batches/statistics" || endpoint == "/api/product-batches/statistics") && 
            request.getMethod() == "GET")
        {
            handleGetBatchStatistics(request, response);
        }
        else if ((endpoint.find("/api/v1/product-batches/expiration-report") == 0 || 
                 endpoint.find("/api/product-batches/expiration-report") == 0) && 
                request.getMethod() == "GET")
        {
            handleGetExpirationReport(request, response);
        }
        else if ((endpoint.find("/api/v1/product-batches/quality-report") == 0 || 
                 endpoint.find("/api/product-batches/quality-report") == 0) && 
                request.getMethod() == "GET")
        {
            handleGetQualityStatusReport(request, response);
        }
        else if ((endpoint.find("/api/v1/product-batches/check-availability") == 0 || 
                 endpoint.find("/api/product-batches/check-availability") == 0) && 
                request.getMethod() == "GET")
        {
            handleCheckBatchAvailability(request, response);
        }
        else if ((endpoint.find("/api/v1/product-batches/expiring") == 0 || 
                 endpoint.find("/api/product-batches/expiring") == 0) && 
                request.getMethod() == "GET")
        {
            handleGetExpiringBatches(request, response);
        }
        else if ((endpoint.find("/api/v1/product-batches/needing-inspection") == 0 || 
                 endpoint.find("/api/product-batches/needing-inspection") == 0) && 
                request.getMethod() == "GET")
        {
            handleGetBatchesNeedingInspection(request, response);
        }
        else if ((endpoint.find("/api/v1/product-batches/product") == 0 || 
                 endpoint.find("/api/product-batches/product") == 0) && 
                request.getMethod() == "GET")
        {
            handleGetBatchesByProduct(request, response);
        }
        else if ((endpoint.find("/api/v1/product-batches/supplier") == 0 || 
                 endpoint.find("/api/product-batches/supplier") == 0) && 
                request.getMethod() == "GET")
        {
            handleGetBatchesBySupplier(request, response);
        }
        else if ((endpoint.find("/api/v1/product-batches/search") == 0 || 
                 endpoint.find("/api/product-batches/search") == 0) && 
                request.getMethod() == "GET")
        {
            handleGetBatches(request, response);
        }
        else if ((endpoint.find("/api/v1/product-batches/number") == 0 || 
                 endpoint.find("/api/product-batches/number") == 0) && 
                request.getMethod() == "GET")
        {
            handleGetBatchByNumber(request, response);
        }
        else if ((endpoint.find("/api/v1/product-batches/") == 0 || 
                 endpoint.find("/api/product-batches/") == 0) && 
                !batchIdStr.empty())
        {
            if (endpoint.find("/quality") != std::string::npos && request.getMethod() == "PUT")
            {
                handleUpdateBatchQuality(request, response);
            }
            else if (request.getMethod() == "GET")
            {
                handleGetBatchById(request, response);
            }
            else if (request.getMethod() == "PUT")
            {
                handleUpdateBatch(request, response);
            }
            else if (request.getMethod() == "DELETE")
            {
                handleDeleteBatch(request, response);
            }
        }
        else if ((endpoint == "/api/v1/product-batches" || endpoint == "/api/product-batches") && 
                request.getMethod() == "GET")
        {
            handleGetBatches(request, response);
        }
        else if ((endpoint == "/api/v1/product-batches" || endpoint == "/api/product-batches") && 
                request.getMethod() == "POST")
        {
            handleCreateBatch(request, response);
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

void ProductBatchController::handleGetBatches(Poco::Net::HTTPServerRequest& request, 
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
    
    Poco::JSON::Array batchesArray;
    
    try
    {
        if (filters.find("productId") != filters.end())
        {
            long long supplierId = std::stoll(filters["productId"]);
            batchesArray = batchService->getBatchesBySupplier(supplierId, page, pageSize);
        }
        else if (filters.find("supplierId") != filters.end())
        {
            long long supplierId = std::stoll(filters["supplierId"]);
            batchesArray = batchService->getBatchesBySupplier(supplierId, page, pageSize);
        }
        else if (filters.find("qualityStatus") != filters.end())
        {
            std::string qualityStatus = filters["qualityStatus"];
            batchesArray = batchService->searchBatches("", page, pageSize);
            
            Poco::JSON::Array filteredArray;
            for (int i = 0; i < batchesArray.size(); ++i)
            {
                auto batchObj = batchesArray.getObject(i);
                if (batchObj->getValue<std::string>("quality_status") == qualityStatus)
                {
                    filteredArray.add(*batchObj);
                }
            }
            batchesArray = filteredArray;
        }
        else if (filters.find("expiring") != filters.end())
        {
            int daysThreshold = 30;
            if (filters.find("days") != filters.end())
            {
                try
                {
                    daysThreshold = std::stoi(filters["days"]);
                }
                catch (...)
                {
                }
            }
            batchesArray = batchService->getExpiringBatchesReport(daysThreshold);
            
            Poco::JSON::Array paginatedArray;
            int startIndex = (page - 1) * pageSize;
            int totalBatches = batchesArray.size();
            
            Poco::JSON::Object::Ptr summaryPtr;
            if (totalBatches > 0 && batchesArray.isObject(totalBatches - 1))
            {
                summaryPtr = batchesArray.getObject(totalBatches - 1);
                totalBatches--;
            }
            
            int endIndex = std::min(startIndex + pageSize, totalBatches);
            for (int i = startIndex; i < endIndex; ++i)
            {
                if (batchesArray.isObject(i))
                {
                    paginatedArray.add(*(batchesArray.getObject(i)));
                }
            }
            
            if (summaryPtr)
            {
                summaryPtr->set("page", page);
                summaryPtr->set("pageSize", pageSize);
                summaryPtr->set("totalItems", totalBatches);
                summaryPtr->set("totalPages", (totalBatches + pageSize - 1) / pageSize);
                paginatedArray.add(*summaryPtr);
            }
            
            batchesArray = paginatedArray;
        }
        else if (filters.find("search") != filters.end())
        {
            std::string searchQuery = filters["search"];
            batchesArray = batchService->searchBatches(searchQuery, page, pageSize);
        }
        else
        {
            batchesArray = batchService->searchBatches("", page, pageSize);
        }
        
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object;
        dataPtr->set("batches", batchesArray);
        sendSuccessResponse(response, "Product batches retrieved successfully", dataPtr);
    }
    catch (const std::exception& e)
    {
        sendErrorResponse(response, "Error retrieving product batches: " + std::string(e.what()), 
                        Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
}

void ProductBatchController::handleGetBatchById(Poco::Net::HTTPServerRequest& request, 
                                              Poco::Net::HTTPServerResponse& response)
{
    std::string batchIdStr = getPathParameter(request.getURI(), "/api/v1/product-batches/", 0);
    if (batchIdStr.empty())
    {
        sendErrorResponse(response, "Batch ID is required", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    long long batchId;
    try
    {
        batchId = std::stoll(batchIdStr);
    }
    catch (...)
    {
        sendErrorResponse(response, "Invalid batch ID", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    std::string accessError;
    if (!validateBatchAccess(request, batchId, accessError))
    {
        sendForbiddenResponse(response, accessError);
        return;
    }
    
    Poco::JSON::Object batchInfo = inventoryService->getBatchStockInfo(batchId);
    
    if (batchInfo.has("error"))
    {
        std::string error = batchInfo.getValue<std::string>("error");
        if (error.find("not found") != std::string::npos)
        {
            sendNotFoundResponse(response, "Product batch");
        }
        else
        {
            sendErrorResponse(response, error, 
                            Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
        }
        return;
    }
    
    Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object(batchInfo);
    sendSuccessResponse(response, "Product batch retrieved successfully", dataPtr);
}

void ProductBatchController::handleGetBatchByNumber(Poco::Net::HTTPServerRequest& request, 
                                                  Poco::Net::HTTPServerResponse& response)
{
    std::string batchNumber = getQueryParameter(request.getURI(), "number");
    if (batchNumber.empty())
    {
        sendErrorResponse(response, "Batch number is required", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    try
    {
        auto batch = batchService->getBatchByNumber(batchNumber);
        
        if (!batch)
        {
            sendNotFoundResponse(response, "Batch not found");
            return;
        }
        
        auto batchDetails = batchService->getBatchDetails(batch->id);
        
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object(batchDetails);
        sendSuccessResponse(response, "Batch retrieved successfully", dataPtr);
    }
    catch (const std::exception& e)
    {
        sendErrorResponse(response, "Error retrieving batch: " + std::string(e.what()), 
                        Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
}

void ProductBatchController::handleCreateBatch(Poco::Net::HTTPServerRequest& request, 
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
    if (!validateCreateBatchData(json, errors))
    {
        sendValidationErrorResponse(response, errors);
        return;
    }
    
    auto batchData = extractBatchFromJson(json);
    if (!batchData)
    {
        sendErrorResponse(response, "Invalid batch data", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    std::string authError;
    if (!canCreateBatch(request, *batchData, authError))
    {
        sendForbiddenResponse(response, authError);
        return;
    }
    
    long long createdBy = getCurrentUserId(request);
    
    auto result = inventoryService->receiveProduct(
        batchData->productId,
        batchData->supplierId,
        batchData->quantityReceived,
        batchData->unitCost,
        batchData->batchNumber,
        batchData->expirationDate.isNull() ? "" : batchData->expirationDate.value(),
        batchData->storageCellId,
        createdBy
    );
    
    logBatchEvent(createdBy, "CREATE_BATCH", result.movementId, 
                 getClientIpAddress(request), getUserAgent(request), 
                 result.success, result.success ? "Batch created successfully" : result.message);
    
    if (result.success)
    {
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object(result.details);
        sendSuccessResponse(response, "Product batch created successfully", dataPtr);
    }
    else
    {
        sendErrorResponse(response, result.message, 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
    }
}

void ProductBatchController::handleUpdateBatch(Poco::Net::HTTPServerRequest& request, 
                                             Poco::Net::HTTPServerResponse& response)
{
    std::string batchIdStr = getPathParameter(request.getURI(), "/api/v1/product-batches/", 0);
    if (batchIdStr.empty())
    {
        sendErrorResponse(response, "Batch ID is required", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    long long batchId;
    try
    {
        batchId = std::stoll(batchIdStr);
    }
    catch (...)
    {
        sendErrorResponse(response, "Invalid batch ID", 
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
    if (!validateUpdateBatchData(json, errors))
    {
        sendValidationErrorResponse(response, errors);
        return;
    }
    
    auto batchData = extractBatchFromJson(json);
    if (!batchData)
    {
        sendErrorResponse(response, "Invalid batch data", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    std::string authError;
    if (!canUpdateBatch(request, batchId, *batchData, authError))
    {
        sendForbiddenResponse(response, authError);
        return;
    }
    
    long long updatedBy = getCurrentUserId(request);
    
    try
    {
        auto existingBatch = batchService->getBatchById(batchId);
        if (!existingBatch)
        {
            sendNotFoundResponse(response, "Batch not found");
            return;
        }
        
        database::models::ProductBatch updatedBatch = *existingBatch;
        
        if (json->has("batch_number"))
        {
            updatedBatch.batchNumber = utils::JsonUtils::getString(*json, "batch_number");
        }
        
        if (json->has("product_id"))
        {
            updatedBatch.productId = utils::JsonUtils::getInt(*json, "product_id");
        }
        
        if (json->has("supplier_id"))
        {
            updatedBatch.supplierId = utils::JsonUtils::getInt(*json, "supplier_id");
        }
        
        if (json->has("quantity_received"))
        {
            updatedBatch.quantityReceived = utils::JsonUtils::getInt(*json, "quantity_received");
        }
        
        if (json->has("quantity_available"))
        {
            updatedBatch.quantityAvailable = utils::JsonUtils::getInt(*json, "quantity_available");
        }
        
        if (json->has("unit_cost"))
        {
            updatedBatch.unitCost = utils::JsonUtils::getDouble(*json, "unit_cost");
        }
        
        if (json->has("storage_cell_id"))
        {
            updatedBatch.storageCellId = utils::JsonUtils::getInt(*json, "storage_cell_id");
        }
        
        if (json->has("quality_status"))
        {
            std::string statusStr = utils::JsonUtils::getString(*json, "quality_status");
            updatedBatch.qualityStatus = database::models::ProductBatch::stringToQualityStatus(statusStr);
        }
        
        if (json->has("manufacture_date"))
        {
            std::string dateStr = utils::JsonUtils::getString(*json, "manufacture_date");
            if (dateStr.empty())
            {
                updatedBatch.manufactureDate = Poco::Nullable<std::string>();
            }
            else
            {
                updatedBatch.manufactureDate = dateStr;
            }
        }
        
        if (json->has("expiration_date"))
        {
            std::string dateStr = utils::JsonUtils::getString(*json, "expiration_date");
            if (dateStr.empty())
            {
                updatedBatch.expirationDate = Poco::Nullable<std::string>();
            }
            else
            {
                updatedBatch.expirationDate = dateStr;
            }
        }
        
        if (json->has("invoice_number"))
        {
            std::string invoiceStr = utils::JsonUtils::getString(*json, "invoice_number");
            if (invoiceStr.empty())
            {
                updatedBatch.invoiceNumber = Poco::Nullable<std::string>();
            }
            else
            {
                updatedBatch.invoiceNumber = invoiceStr;
            }
        }
        
        auto result = batchService->updateProductBatch(batchId, updatedBatch, updatedBy);
        
        logBatchEvent(updatedBy, "UPDATE_BATCH", batchId, 
                     getClientIpAddress(request), getUserAgent(request), 
                     result.success, result.success ? "Batch updated successfully" : result.message);
        
        if (result.success)
        {
            Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object(result.toJson());
            sendSuccessResponse(response, "Batch updated successfully", dataPtr);
        }
        else
        {
            sendErrorResponse(response, result.message, 
                            Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        }
    }
    catch (const std::exception& e)
    {
        sendErrorResponse(response, "Error updating batch: " + std::string(e.what()), 
                        Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
}

void ProductBatchController::handleDeleteBatch(Poco::Net::HTTPServerRequest& request, 
                                             Poco::Net::HTTPServerResponse& response)
{
    std::string batchIdStr = getPathParameter(request.getURI(), "/api/v1/product-batches/", 0);
    if (batchIdStr.empty())
    {
        sendErrorResponse(response, "Batch ID is required", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    long long batchId;
    try
    {
        batchId = std::stoll(batchIdStr);
    }
    catch (...)
    {
        sendErrorResponse(response, "Invalid batch ID", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    std::string authError;
    if (!canDeleteBatch(request, batchId, authError))
    {
        sendForbiddenResponse(response, authError);
        return;
    }
    
    long long deletedBy = getCurrentUserId(request);
    
    std::string reason;
    auto json = parseJsonBody(request);
    if (json && json->has("reason"))
    {
        reason = utils::JsonUtils::getString(*json, "reason");
    }
    
    try
    {
        auto batch = batchService->getBatchById(batchId);
        if (!batch)
        {
            sendNotFoundResponse(response, "Batch not found");
            return;
        }
        
        bool success = batchService->deleteProductBatch(batchId, deletedBy, reason);
        
        logBatchEvent(deletedBy, "DELETE_BATCH", batchId, 
                     getClientIpAddress(request), getUserAgent(request), 
                     success, success ? "Batch deleted successfully" : "Failed to delete batch");
        
        if (success)
        {
            sendSuccessResponse(response, "Batch deleted successfully");
        }
        else
        {
            sendErrorResponse(response, "Failed to delete batch. Ensure batch has zero available quantity.", 
                            Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        }
    }
    catch (const std::exception& e)
    {
        sendErrorResponse(response, "Error deleting batch: " + std::string(e.what()), 
                        Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
}

void ProductBatchController::handleUpdateBatchQuality(Poco::Net::HTTPServerRequest& request, 
                                                    Poco::Net::HTTPServerResponse& response)
{
    std::string batchIdStr = getPathParameter(request.getURI(), "/api/v1/product-batches/", 0);
    batchIdStr = batchIdStr.substr(0, batchIdStr.find("/quality"));
    
    if (batchIdStr.empty())
    {
        sendErrorResponse(response, "Batch ID is required", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    long long batchId;
    try
    {
        batchId = std::stoll(batchIdStr);
    }
    catch (...)
    {
        sendErrorResponse(response, "Invalid batch ID", 
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
    if (!validateQualityUpdateData(json, errors))
    {
        sendValidationErrorResponse(response, errors);
        return;
    }
    
    std::string qualityStatus = utils::JsonUtils::getString(*json, "quality_status");
    
    std::string authError;
    if (!canUpdateBatchQuality(request, batchId, authError))
    {
        sendForbiddenResponse(response, authError);
        return;
    }
    
    long long updatedBy = getCurrentUserId(request);
    
    bool success = inventoryService->updateBatchQualityStatus(batchId, qualityStatus, updatedBy);
    
    logBatchEvent(updatedBy, "UPDATE_BATCH_QUALITY", batchId, 
                 getClientIpAddress(request), getUserAgent(request), 
                 success, success ? "Batch quality updated successfully" : "Failed to update batch quality");
    
    if (success)
    {
        sendSuccessResponse(response, "Batch quality updated successfully");
    }
    else
    {
        sendErrorResponse(response, "Failed to update batch quality", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
    }
}

void ProductBatchController::handleGetBatchesByProduct(Poco::Net::HTTPServerRequest& request, 
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
    
    Poco::JSON::Object productStockInfo = inventoryService->getProductStockInfo(productId);
    
    if (productStockInfo.has("error"))
    {
        std::string error = productStockInfo.getValue<std::string>("error");
        if (error.find("not found") != std::string::npos)
        {
            sendNotFoundResponse(response, "Product");
        }
        else
        {
            sendErrorResponse(response, error, 
                            Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
        }
        return;
    }
    
    Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object(productStockInfo);
    sendSuccessResponse(response, "Batches by product retrieved successfully", dataPtr);
}

void ProductBatchController::handleGetBatchesBySupplier(Poco::Net::HTTPServerRequest& request, 
                                                      Poco::Net::HTTPServerResponse& response)
{
    std::string supplierIdStr = getQueryParameter(request.getURI(), "supplierId");
    if (supplierIdStr.empty())
    {
        sendErrorResponse(response, "Supplier ID is required", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    long long supplierId;
    try
    {
        supplierId = std::stoll(supplierIdStr);
    }
    catch (...)
    {
        sendErrorResponse(response, "Invalid supplier ID", 
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
    
    try
    {
        auto batchesArray = batchService->getBatchesBySupplier(supplierId, page, pageSize);
        
        if (batchesArray.size() == 0)
        {
            sendNotFoundResponse(response, "No batches found for this supplier");
            return;
        }
        
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object;
        dataPtr->set("batches", batchesArray);
        sendSuccessResponse(response, "Batches by supplier retrieved successfully", dataPtr);
    }
    catch (const std::exception& e)
    {
        sendErrorResponse(response, "Error retrieving batches by supplier: " + std::string(e.what()), 
                        Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
}

void ProductBatchController::handleGetExpiringBatches(Poco::Net::HTTPServerRequest& request, 
                                                    Poco::Net::HTTPServerResponse& response)
{
    int daysThreshold = 30;
    std::string daysStr = getQueryParameter(request.getURI(), "days");
    if (!daysStr.empty())
    {
        try
        {
            daysThreshold = std::stoi(daysStr);
        }
        catch (...)
        {
            sendErrorResponse(response, "Invalid days threshold", 
                            Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
            return;
        }
    }
    
    Poco::JSON::Array::Ptr batchesArray = new Poco::JSON::Array(inventoryService->findExpiringProducts(daysThreshold));
    Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object;
    dataPtr->set("batches", batchesArray);
    
    sendSuccessResponse(response, "Expiring batches retrieved successfully", dataPtr);
}

void ProductBatchController::handleGetBatchesNeedingInspection(Poco::Net::HTTPServerRequest& request, 
                                                             Poco::Net::HTTPServerResponse& response)
{
    Poco::JSON::Array batchesArray = inventoryService->findProductsNeedingQualityInspection();
    Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object;
    dataPtr->set("batches", batchesArray);
    
    sendSuccessResponse(response, "Batches needing inspection retrieved successfully", dataPtr);
}

void ProductBatchController::handleGetBatchStatistics(Poco::Net::HTTPServerRequest& request, 
                                                    Poco::Net::HTTPServerResponse& response)
{
    auto currentUserRole = getCurrentUserRole(request);
    if (currentUserRole != database::models::UserRole::ADMIN && 
        currentUserRole != database::models::UserRole::MANAGER && 
        currentUserRole != database::models::UserRole::AUDITOR)
    {
        sendForbiddenResponse(response, "You don't have permission to view batch statistics");
        return;
    }
    
    Poco::JSON::Object statistics = inventoryService->getInventoryStatistics();
    Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object(statistics);
    
    sendSuccessResponse(response, "Batch statistics retrieved", dataPtr);
}

void ProductBatchController::handleGetExpirationReport(Poco::Net::HTTPServerRequest& request, 
                                                     Poco::Net::HTTPServerResponse& response)
{
    auto currentUserRole = getCurrentUserRole(request);
    if (currentUserRole != database::models::UserRole::ADMIN && 
        currentUserRole != database::models::UserRole::MANAGER && 
        currentUserRole != database::models::UserRole::AUDITOR)
    {
        sendForbiddenResponse(response, "You don't have permission to view expiration reports");
        return;
    }
    
    Poco::JSON::Array::Ptr report = new Poco::JSON::Array(inventoryService->getExpirationReport());
    Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object;
    dataPtr->set("report", report);
    
    sendSuccessResponse(response, "Expiration report generated", dataPtr);
}

void ProductBatchController::handleGetQualityStatusReport(Poco::Net::HTTPServerRequest& request, 
                                                        Poco::Net::HTTPServerResponse& response)
{
    auto currentUserRole = getCurrentUserRole(request);
    if (currentUserRole != database::models::UserRole::ADMIN && 
        currentUserRole != database::models::UserRole::MANAGER && 
        currentUserRole != database::models::UserRole::AUDITOR)
    {
        sendForbiddenResponse(response, "You don't have permission to view quality status reports");
        return;
    }
    
    Poco::JSON::Array::Ptr report = new Poco::JSON::Array(inventoryService->getQualityStatusReport());
    Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object;
    dataPtr->set("report", report);
    
    sendSuccessResponse(response, "Quality status report generated", dataPtr);
}

void ProductBatchController::handleCheckBatchAvailability(Poco::Net::HTTPServerRequest& request, 
                                                        Poco::Net::HTTPServerResponse& response)
{
    std::string batchIdStr = getQueryParameter(request.getURI(), "batchId");
    std::string quantityStr = getQueryParameter(request.getURI(), "quantity");
    
    if (batchIdStr.empty() || quantityStr.empty())
    {
        sendErrorResponse(response, "Batch ID and quantity are required", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    long long batchId;
    int quantity;
    try
    {
        batchId = std::stoll(batchIdStr);
        quantity = std::stoi(quantityStr);
    }
    catch (...)
    {
        sendErrorResponse(response, "Invalid batch ID or quantity", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    if (quantity <= 0)
    {
        sendErrorResponse(response, "Quantity must be greater than 0", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    auto result = inventoryService->checkBatchAvailability(batchId, quantity);
    
    Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object(result.toJson());
    sendSuccessResponse(response, "Batch availability checked", dataPtr);
}

bool ProductBatchController::validateRequest(Poco::Net::HTTPServerRequest& request, 
                                           Poco::Net::HTTPServerResponse& response,
                                           std::string& errorMessage)
{
    if (!BaseController::validateRequest(request, response, errorMessage))
    {
        return false;
    }
    
    return true;
}

bool ProductBatchController::authorizeRequest(Poco::Net::HTTPServerRequest& request, 
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

bool ProductBatchController::validateBatchAccess(Poco::Net::HTTPServerRequest& request, 
                                               long long batchId,
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
    
    errorMessage = "You don't have permission to access this product batch";
    return false;
}

long long ProductBatchController::getCurrentUserId(Poco::Net::HTTPServerRequest& request)
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

database::models::UserRole ProductBatchController::getCurrentUserRole(Poco::Net::HTTPServerRequest& request)
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

bool ProductBatchController::validateCreateBatchData(const Poco::JSON::Object::Ptr& json, 
                                                   std::vector<std::string>& errors)
{
    errors.clear();
    
    std::vector<std::string> requiredFields = {"batch_number", "product_id", "supplier_id", 
                                               "quantity_received", "unit_cost", "storage_cell_id"};
    std::vector<std::string> missingFields;
    if (!validateRequiredFields(json, requiredFields, missingFields))
    {
        for (const auto& field : missingFields)
        {
            errors.push_back(field + " is required");
        }
    }
    
    if (json->has("batch_number"))
    {
        std::string batchNumber = utils::JsonUtils::getString(*json, "batch_number");
        if (batchNumber.empty() || batchNumber.length() > 100)
        {
            errors.push_back("Batch number must be between 1 and 100 characters");
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
    
    if (json->has("quantity_received"))
    {
        int quantity = utils::JsonUtils::getInt(*json, "quantity_received");
        if (quantity <= 0)
        {
            errors.push_back("Quantity received must be greater than 0");
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
    
    if (json->has("storage_cell_id"))
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
    
    if (json->has("manufacture_date"))
    {
        std::string manufactureDate = utils::JsonUtils::getString(*json, "manufacture_date");
        if (!manufactureDate.empty() && !utils::Validator::isValidDate(manufactureDate))
        {
            errors.push_back("Invalid manufacture date format. Use YYYY-MM-DD");
        }
    }
    
    if (json->has("expiration_date"))
    {
        std::string expirationDate = utils::JsonUtils::getString(*json, "expiration_date");
        if (!expirationDate.empty() && !utils::Validator::isValidDate(expirationDate))
        {
            errors.push_back("Invalid expiration date format. Use YYYY-MM-DD");
        }
    }
    
    if (json->has("quality_status"))
    {
        std::string qualityStatus = utils::JsonUtils::getString(*json, "quality_status");
        if (!qualityStatus.empty())
        {
            try
            {
                auto status = database::models::ProductBatch::stringToQualityStatus(qualityStatus);
            }
            catch (...)
            {
                errors.push_back("Invalid quality status value");
            }
        }
    }
    
    return errors.empty();
}

bool ProductBatchController::validateUpdateBatchData(const Poco::JSON::Object::Ptr& json, 
                                                   std::vector<std::string>& errors)
{
    errors.clear();
    
    bool hasBatchNumber = json->has("batch_number") && !json->get("batch_number").isEmpty();
    bool hasProductId = json->has("product_id") && !json->get("product_id").isEmpty();
    bool hasSupplierId = json->has("supplier_id") && !json->get("supplier_id").isEmpty();
    bool hasQuantity = json->has("quantity_received") && !json->get("quantity_received").isEmpty();
    bool hasUnitCost = json->has("unit_cost") && !json->get("unit_cost").isEmpty();
    bool hasStorageCellId = json->has("storage_cell_id") && !json->get("storage_cell_id").isEmpty();
    bool hasManufactureDate = json->has("manufacture_date") && !json->get("manufacture_date").isEmpty();
    bool hasExpirationDate = json->has("expiration_date") && !json->get("expiration_date").isEmpty();
    bool hasQualityStatus = json->has("quality_status") && !json->get("quality_status").isEmpty();
    bool hasInvoiceNumber = json->has("invoice_number") && !json->get("invoice_number").isEmpty();
    
    if (!hasBatchNumber && !hasProductId && !hasSupplierId && !hasQuantity && 
        !hasUnitCost && !hasStorageCellId && !hasManufactureDate && 
        !hasExpirationDate && !hasQualityStatus && !hasInvoiceNumber)
    {
        errors.push_back("At least one field must be provided for update");
    }
    
    if (hasBatchNumber)
    {
        std::string batchNumber = utils::JsonUtils::getString(*json, "batch_number");
        if (batchNumber.empty() || batchNumber.length() > 100)
        {
            errors.push_back("Batch number must be between 1 and 100 characters");
        }
    }
    
    if (hasProductId)
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
    
    if (hasSupplierId)
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
    
    if (hasQuantity)
    {
        int quantity = utils::JsonUtils::getInt(*json, "quantity_received");
        if (quantity <= 0)
        {
            errors.push_back("Quantity received must be greater than 0");
        }
    }
    
    if (hasUnitCost)
    {
        double unitCost = utils::JsonUtils::getDouble(*json, "unit_cost");
        if (unitCost < 0)
        {
            errors.push_back("Unit cost must be greater than or equal to 0");
        }
    }
    
    if (hasStorageCellId)
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
    
    if (hasManufactureDate)
    {
        std::string manufactureDate = utils::JsonUtils::getString(*json, "manufacture_date");
        if (!manufactureDate.empty() && !utils::Validator::isValidDate(manufactureDate))
        {
            errors.push_back("Invalid manufacture date format. Use YYYY-MM-DD");
        }
    }
    
    if (hasExpirationDate)
    {
        std::string expirationDate = utils::JsonUtils::getString(*json, "expiration_date");
        if (!expirationDate.empty() && !utils::Validator::isValidDate(expirationDate))
        {
            errors.push_back("Invalid expiration date format. Use YYYY-MM-DD");
        }
    }
    
    if (hasQualityStatus)
    {
        std::string qualityStatus = utils::JsonUtils::getString(*json, "quality_status");
        if (!qualityStatus.empty())
        {
            try
            {
                auto status = database::models::ProductBatch::stringToQualityStatus(qualityStatus);
            }
            catch (...)
            {
                errors.push_back("Invalid quality status value");
            }
        }
    }
    
    return errors.empty();
}

bool ProductBatchController::validateQualityUpdateData(const Poco::JSON::Object::Ptr& json, 
                                                     std::vector<std::string>& errors)
{
    errors.clear();
    
    std::vector<std::string> requiredFields = {"quality_status"};
    std::vector<std::string> missingFields;
    if (!validateRequiredFields(json, requiredFields, missingFields))
    {
        for (const auto& field : missingFields)
        {
            errors.push_back(field + " is required");
        }
    }
    
    if (json->has("quality_status"))
    {
        std::string qualityStatus = utils::JsonUtils::getString(*json, "quality_status");
        try
        {
            auto status = database::models::ProductBatch::stringToQualityStatus(qualityStatus);
        }
        catch (...)
        {
            errors.push_back("Invalid quality status value");
        }
    }
    
    return errors.empty();
}

bool ProductBatchController::validateSearchParameters(const std::map<std::string, std::string>& filters,
                                                    std::vector<std::string>& errors)
{
    errors.clear();
    
    for (const auto& [key, value] : filters)
    {
        if (key == "product_id" || key == "supplier_id")
        {
            try
            {
                long long id = std::stoll(value);
                if (id <= 0)
                {
                    errors.push_back(key + " must be greater than 0");
                }
            }
            catch (...)
            {
                errors.push_back("Invalid " + key);
            }
        }
        else if (key == "quality_status")
        {
            try
            {
                auto status = database::models::ProductBatch::stringToQualityStatus(value);
            }
            catch (...)
            {
                errors.push_back("Invalid quality status value");
            }
        }
        else if (key == "days")
        {
            try
            {
                int days = std::stoi(value);
                if (days <= 0)
                {
                    errors.push_back("Days must be greater than 0");
                }
            }
            catch (...)
            {
                errors.push_back("Invalid days threshold");
            }
        }
        else if (key == "expiring")
        {
            if (value != "true" && value != "false")
            {
                errors.push_back("expiring must be 'true' or 'false'");
            }
        }
    }
    
    return errors.empty();
}

void ProductBatchController::logBatchEvent(long long userId, 
                                         const std::string& action,
                                         long long batchId,
                                         const std::string& ipAddress,
                                         const std::string& userAgent,
                                         bool success,
                                         const std::string& details)
{
    std::cout << "[" << getCurrentTimestamp() << "] "
            << "PRODUCT_BATCH " << action << " "
            << "UserID: " << userId << " "
            << "BatchID: " << batchId << " "
            << "IP: " << ipAddress << " "
            << "Success: " << (success ? "Yes" : "No") << " "
            << "Details: " << details << std::endl;
}

std::unique_ptr<database::models::ProductBatch> ProductBatchController::extractBatchFromJson(const Poco::JSON::Object::Ptr& json)
{
    if (!json)
    {
        return nullptr;
    }
    
    try
    {
        auto batch = std::make_unique<database::models::ProductBatch>(*json);
        return batch;
    }
    catch (...)
    {
        return nullptr;
    }
}

bool ProductBatchController::canCreateBatch(Poco::Net::HTTPServerRequest& request, 
                                          const database::models::ProductBatch& batchData,
                                          std::string& errorMessage)
{
    auto currentUserRole = getCurrentUserRole(request);
    
    if (currentUserRole == database::models::UserRole::ADMIN || 
        currentUserRole == database::models::UserRole::MANAGER || 
        currentUserRole == database::models::UserRole::WORKER)
    {
        return true;
    }
    
    errorMessage = "You don't have permission to create product batches";
    return false;
}

bool ProductBatchController::canUpdateBatch(Poco::Net::HTTPServerRequest& request, 
                                          long long batchId,
                                          const database::models::ProductBatch& batchData,
                                          std::string& errorMessage)
{
    auto currentUserRole = getCurrentUserRole(request);
    
    if (currentUserRole == database::models::UserRole::ADMIN || 
        currentUserRole == database::models::UserRole::MANAGER)
    {
        return true;
    }
    
    errorMessage = "You don't have permission to update this product batch";
    return false;
}

bool ProductBatchController::canDeleteBatch(Poco::Net::HTTPServerRequest& request, 
                                          long long batchId,
                                          std::string& errorMessage)
{
    auto currentUserRole = getCurrentUserRole(request);
    
    if (currentUserRole == database::models::UserRole::ADMIN)
    {
        return true;
    }
    
    errorMessage = "You don't have permission to delete this product batch";
    return false;
}

bool ProductBatchController::canUpdateBatchQuality(Poco::Net::HTTPServerRequest& request, 
                                                 long long batchId,
                                                 std::string& errorMessage)
{
    auto currentUserRole = getCurrentUserRole(request);
    
    if (currentUserRole == database::models::UserRole::ADMIN || 
        currentUserRole == database::models::UserRole::MANAGER || 
        currentUserRole == database::models::UserRole::WORKER)
    {
        return true;
    }
    
    errorMessage = "You don't have permission to update batch quality";
    return false;
}

Poco::JSON::Object ProductBatchController::buildPaginationResponse(int page, int pageSize, int totalItems, 
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

std::map<std::string, std::string> ProductBatchController::getFilterParameters(Poco::Net::HTTPServerRequest& request)
{
    std::map<std::string, std::string> filters;
    
    Poco::URI uri(request.getURI());
    auto queryParams = uri.getQueryParameters();
    
    for (const auto& param : queryParams)
    {
        if (param.first == "productId" || param.first == "supplierId" ||
            param.first == "qualityStatus" || param.first == "days" ||
            param.first == "expiring" || param.first == "search")
        {
            filters[param.first] = param.second;
        }
    }
    
    return filters;
}

int ProductBatchController::getQueryParameterInt(const Poco::URI& uri, const std::string& name, int defaultValue)
{
    auto params = uri.getQueryParameters();
    for (const auto& param : params)
    {
        if (param.first == name)
        {
            try
            {
                return std::stoi(param.second);
            }
            catch (...)
            {
                return defaultValue;
            }
        }
    }
    return defaultValue;
}

} // namespace controllers
