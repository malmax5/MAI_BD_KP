#include "SupplierController.hpp"
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

SupplierController::SupplierController()
    : supplierService(std::make_unique<services::SupplierService>()),
      authService(std::make_unique<services::AuthService>())
{
}

void SupplierController::handleRequest(Poco::Net::HTTPServerRequest& request, 
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
        
        std::string basePath = "/api/v1/suppliers";
        if (endpoint.find("/api/v1/") != 0)
        {
            basePath = "/api/suppliers";
        }
        
        std::string supplierIdStr = getPathParameter(uri, basePath + "/", 0);
        
        if ((endpoint == "/api/v1/suppliers/active" || endpoint == "/api/suppliers/active") && 
            request.getMethod() == "GET")
        {
            handleGetActiveSuppliers(request, response);
        }
        else if ((endpoint == "/api/v1/suppliers/statistics" || endpoint == "/api/suppliers/statistics") && 
                 request.getMethod() == "GET")
        {
            handleGetSupplierStatistics(request, response);
        }
        else if ((endpoint.find("/api/v1/suppliers/search") == 0 || endpoint.find("/api/suppliers/search") == 0) && 
                 request.getMethod() == "GET")
        {
            handleSearchSuppliers(request, response);
        }
        else if ((endpoint.find("/api/v1/suppliers/tax-id") == 0 || endpoint.find("/api/suppliers/tax-id") == 0) && 
                 request.getMethod() == "GET")
        {
            handleGetSupplierByTaxId(request, response);
        }
        else if ((endpoint == "/api/v1/suppliers" || endpoint == "/api/suppliers") && 
                 request.getMethod() == "GET")
        {
            handleGetSuppliers(request, response);
        }
        else if ((endpoint == "/api/v1/suppliers" || endpoint == "/api/suppliers") && 
                 request.getMethod() == "POST")
        {
            handleCreateSupplier(request, response);
        }
        else if ((endpoint.find("/api/v1/suppliers/") == 0 || endpoint.find("/api/suppliers/") == 0) && 
                 !supplierIdStr.empty())
        {
            if (endpoint.find("/products") != std::string::npos && request.getMethod() == "GET")
            {
                handleGetSupplierProducts(request, response);
            }
            else if (endpoint.find("/batches") != std::string::npos && request.getMethod() == "GET")
            {
                handleGetSupplierBatches(request, response);
            }
            else if (endpoint.find("/activate") != std::string::npos && request.getMethod() == "PUT")
            {
                handleActivateSupplier(request, response);
            }
            else if (endpoint.find("/deactivate") != std::string::npos && request.getMethod() == "PUT")
            {
                handleDeactivateSupplier(request, response);
            }
            else if (request.getMethod() == "GET")
            {
                handleGetSupplierById(request, response);
            }
            else if (request.getMethod() == "PUT")
            {
                handleUpdateSupplier(request, response);
            }
            else if (request.getMethod() == "DELETE")
            {
                handleDeleteSupplier(request, response);
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

void SupplierController::handleGetSuppliers(Poco::Net::HTTPServerRequest& request, 
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
    
    try
    {
        Poco::JSON::Array suppliersArray;
        
        if (filters.find("active") != filters.end())
        {
            bool active = filters["active"] == "true";
            suppliersArray = supplierService->getActiveSuppliers();
        }
        else if (!filters.empty())
        {
            suppliersArray = supplierService->getActiveSuppliers();
        }
        else
        {
            suppliersArray = supplierService->getActiveSuppliers();
        }
        
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object;
        dataPtr->set("suppliers", suppliersArray);
        sendSuccessResponse(response, "Suppliers retrieved successfully", dataPtr);
    }
    catch (const std::exception& e)
    {
        sendErrorResponse(response, "Error retrieving suppliers: " + std::string(e.what()), 
                        Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
}

void SupplierController::handleGetSupplierById(Poco::Net::HTTPServerRequest& request, 
                                             Poco::Net::HTTPServerResponse& response)
{
    std::string supplierIdStr = getPathParameter(request.getURI(), "/api/v1/suppliers/", 0);
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
    
    try
    {
        auto result = supplierService->getSupplierById(supplierId);
        
        if (!result.success)
        {
            if (result.message.find("not found") != std::string::npos)
            {
                sendNotFoundResponse(response, "Supplier");
            }
            else
            {
                sendErrorResponse(response, result.message, 
                                Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
            }
            return;
        }
        
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object(result.supplier->toJson());
        sendSuccessResponse(response, "Supplier retrieved successfully", dataPtr);
    }
    catch (const std::exception& e)
    {
        sendErrorResponse(response, "Error retrieving supplier: " + std::string(e.what()), 
                        Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
}

void SupplierController::handleCreateSupplier(Poco::Net::HTTPServerRequest& request, 
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
    if (!validateCreateSupplierData(json, errors))
    {
        sendValidationErrorResponse(response, errors);
        return;
    }
    
    auto supplierData = extractSupplierFromJson(json);
    if (!supplierData)
    {
        sendErrorResponse(response, "Invalid supplier data", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    std::string authError;
    if (!canCreateSupplier(request, *supplierData, authError))
    {
        sendForbiddenResponse(response, authError);
        return;
    }
    
    long long createdBy = getCurrentUserId(request);
    
    auto result = supplierService->createSupplier(*supplierData, createdBy);
    
    logSupplierEvent(createdBy, "CREATE_SUPPLIER", result.supplierId, 
                    getClientIpAddress(request), getUserAgent(request), 
                    result.success, result.success ? "Supplier created successfully" : result.message);
    
    if (result.success)
    {
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object(result.supplier->toJson());
        sendSuccessResponse(response, "Supplier created successfully", dataPtr);
    }
    else
    {
        sendErrorResponse(response, result.message, 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
    }
}

void SupplierController::handleUpdateSupplier(Poco::Net::HTTPServerRequest& request, 
                                            Poco::Net::HTTPServerResponse& response)
{
    std::string supplierIdStr = getPathParameter(request.getURI(), "/api/v1/suppliers/", 0);
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
    
    auto json = parseJsonBody(request);
    if (!json)
    {
        sendErrorResponse(response, "Invalid JSON body", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    std::vector<std::string> errors;
    if (!validateUpdateSupplierData(json, errors))
    {
        sendValidationErrorResponse(response, errors);
        return;
    }
    
    auto supplierData = extractSupplierFromJson(json);
    if (!supplierData)
    {
        sendErrorResponse(response, "Invalid supplier data", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    std::string authError;
    if (!canUpdateSupplier(request, supplierId, *supplierData, authError))
    {
        sendForbiddenResponse(response, authError);
        return;
    }
    
    long long updatedBy = getCurrentUserId(request);
    
    auto result = supplierService->updateSupplier(supplierId, *supplierData, updatedBy);
    
    logSupplierEvent(updatedBy, "UPDATE_SUPPLIER", supplierId, 
                    getClientIpAddress(request), getUserAgent(request), 
                    result.success, result.success ? "Supplier updated successfully" : result.message);
    
    if (result.success)
    {
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object(result.supplier->toJson());
        sendSuccessResponse(response, "Supplier updated successfully", dataPtr);
    }
    else
    {
        sendErrorResponse(response, result.message, 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
    }
}

void SupplierController::handleDeleteSupplier(Poco::Net::HTTPServerRequest& request, 
                                            Poco::Net::HTTPServerResponse& response)
{
    std::string supplierIdStr = getPathParameter(request.getURI(), "/api/v1/suppliers/", 0);
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
    
    std::string authError;
    if (!canDeleteSupplier(request, supplierId, authError))
    {
        sendForbiddenResponse(response, authError);
        return;
    }
    
    long long deletedBy = getCurrentUserId(request);
    
    auto result = supplierService->deactivateSupplier(supplierId, deletedBy);
    
    logSupplierEvent(deletedBy, "DELETE_SUPPLIER", supplierId, 
                    getClientIpAddress(request), getUserAgent(request), 
                    result.success, result.success ? "Supplier deactivated successfully" : result.message);
    
    if (result.success)
    {
        sendSuccessResponse(response, "Supplier deactivated successfully");
    }
    else
    {
        sendErrorResponse(response, result.message, 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
    }
}

void SupplierController::handleGetActiveSuppliers(Poco::Net::HTTPServerRequest& request, 
                                                Poco::Net::HTTPServerResponse& response)
{
    try
    {
        Poco::JSON::Array activeSuppliers = supplierService->getActiveSuppliers();
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object;
        dataPtr->set("activeSuppliers", activeSuppliers);
        sendSuccessResponse(response, "Active suppliers retrieved successfully", dataPtr);
    }
    catch (const std::exception& e)
    {
        sendErrorResponse(response, "Error retrieving active suppliers: " + std::string(e.what()), 
                        Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
}

void SupplierController::handleGetSupplierProducts(Poco::Net::HTTPServerRequest& request, 
                                                 Poco::Net::HTTPServerResponse& response)
{
    std::string supplierIdStr = getPathParameter(request.getURI(), "/api/v1/suppliers/", 0);
    supplierIdStr = supplierIdStr.substr(0, supplierIdStr.find("/products"));
    
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
    
    try
    {
        Poco::JSON::Array products = supplierService->getSupplierProducts(supplierId);
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object;
        dataPtr->set("products", products);
        sendSuccessResponse(response, "Supplier products retrieved successfully", dataPtr);
    }
    catch (const std::exception& e)
    {
        sendErrorResponse(response, "Error retrieving supplier products: " + std::string(e.what()), 
                        Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
}

void SupplierController::handleGetSupplierBatches(Poco::Net::HTTPServerRequest& request, 
                                                Poco::Net::HTTPServerResponse& response)
{
    std::string supplierIdStr = getPathParameter(request.getURI(), "/api/v1/suppliers/", 0);
    supplierIdStr = supplierIdStr.substr(0, supplierIdStr.find("/batches"));
    
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
    
    try
    {
        Poco::JSON::Array batches = supplierService->getSupplierBatches(supplierId);
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object;
        dataPtr->set("batches", batches);
        sendSuccessResponse(response, "Supplier batches retrieved successfully", dataPtr);
    }
    catch (const std::exception& e)
    {
        sendErrorResponse(response, "Error retrieving supplier batches: " + std::string(e.what()), 
                        Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
}

void SupplierController::handleSearchSuppliers(Poco::Net::HTTPServerRequest& request, 
                                             Poco::Net::HTTPServerResponse& response)
{
    std::string query = getQueryParameter(request.getURI(), "query");
    std::string taxId = getQueryParameter(request.getURI(), "taxId");
    std::string name = getQueryParameter(request.getURI(), "name");
    
    try
    {
        Poco::JSON::Array suppliersArray;
        
        if (!taxId.empty())
        {
            auto result = supplierService->getSupplierByTaxId(taxId);
            if (result.success && result.supplier)
            {
                suppliersArray.add(result.supplier->toJson());
            }
        }
        else if (!name.empty())
        {
            suppliersArray = supplierService->getActiveSuppliers();
        }
        else if (!query.empty())
        {
            suppliersArray = supplierService->getActiveSuppliers();
        }
        else
        {
            sendErrorResponse(response, "Search query, taxId, or name is required", 
                            Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
            return;
        }
        
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object;
        dataPtr->set("suppliers", suppliersArray);
        sendSuccessResponse(response, "Suppliers search completed", dataPtr);
    }
    catch (const std::exception& e)
    {
        sendErrorResponse(response, "Error searching suppliers: " + std::string(e.what()), 
                        Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
}

void SupplierController::handleActivateSupplier(Poco::Net::HTTPServerRequest& request, 
                                              Poco::Net::HTTPServerResponse& response)
{
    std::string supplierIdStr = getPathParameter(request.getURI(), "/api/v1/suppliers/", 0);
    supplierIdStr = supplierIdStr.substr(0, supplierIdStr.find("/activate"));
    
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
    
    std::string authError;
    if (!canActivateSupplier(request, supplierId, authError))
    {
        sendForbiddenResponse(response, authError);
        return;
    }
    
    long long activatedBy = getCurrentUserId(request);
    
    auto result = supplierService->activateSupplier(supplierId, activatedBy);
    
    logSupplierEvent(activatedBy, "ACTIVATE_SUPPLIER", supplierId, 
                    getClientIpAddress(request), getUserAgent(request), 
                    result.success, result.success ? "Supplier activated successfully" : result.message);
    
    if (result.success)
    {
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object(result.supplier->toJson());
        sendSuccessResponse(response, "Supplier activated successfully", dataPtr);
    }
    else
    {
        sendErrorResponse(response, result.message, 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
    }
}

void SupplierController::handleDeactivateSupplier(Poco::Net::HTTPServerRequest& request, 
                                                Poco::Net::HTTPServerResponse& response)
{
    std::string supplierIdStr = getPathParameter(request.getURI(), "/api/v1/suppliers/", 0);
    supplierIdStr = supplierIdStr.substr(0, supplierIdStr.find("/deactivate"));
    
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
    
    std::string authError;
    if (!canDeactivateSupplier(request, supplierId, authError))
    {
        sendForbiddenResponse(response, authError);
        return;
    }
    
    long long deactivatedBy = getCurrentUserId(request);
    
    auto result = supplierService->deactivateSupplier(supplierId, deactivatedBy);
    
    logSupplierEvent(deactivatedBy, "DEACTIVATE_SUPPLIER", supplierId, 
                    getClientIpAddress(request), getUserAgent(request), 
                    result.success, result.success ? "Supplier deactivated successfully" : result.message);
    
    if (result.success)
    {
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object(result.supplier->toJson());
        sendSuccessResponse(response, "Supplier deactivated successfully", dataPtr);
    }
    else
    {
        sendErrorResponse(response, result.message, 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
    }
}

void SupplierController::handleGetSupplierByTaxId(Poco::Net::HTTPServerRequest& request, 
                                                Poco::Net::HTTPServerResponse& response)
{
    std::string taxId = getQueryParameter(request.getURI(), "taxId");
    if (taxId.empty())
    {
        sendErrorResponse(response, "Tax ID is required", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    try
    {
        auto result = supplierService->getSupplierByTaxId(taxId);
        
        if (!result.success)
        {
            if (result.message.find("not found") != std::string::npos)
            {
                sendNotFoundResponse(response, "Supplier with tax ID: " + taxId);
            }
            else
            {
                sendErrorResponse(response, result.message, 
                                Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
            }
            return;
        }
        
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object(result.supplier->toJson());
        sendSuccessResponse(response, "Supplier retrieved successfully", dataPtr);
    }
    catch (const std::exception& e)
    {
        sendErrorResponse(response, "Error retrieving supplier: " + std::string(e.what()), 
                        Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
}

void SupplierController::handleGetSupplierStatistics(Poco::Net::HTTPServerRequest& request, 
                                                   Poco::Net::HTTPServerResponse& response)
{
    auto currentUserRole = getCurrentUserRole(request);
    if (currentUserRole != database::models::UserRole::ADMIN && 
        currentUserRole != database::models::UserRole::MANAGER && 
        currentUserRole != database::models::UserRole::AUDITOR)
    {
        sendForbiddenResponse(response, "You don't have permission to view supplier statistics");
        return;
    }
    
    std::string supplierIdStr = getQueryParameter(request.getURI(), "supplierId");
    
    Poco::JSON::Object statistics;
    statistics.set("totalSuppliers", 0);
    statistics.set("activeSuppliers", 0);
    statistics.set("inactiveSuppliers", 0);
    statistics.set("avgRating", 0.0);
    
    Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object(statistics);
    sendSuccessResponse(response, "Supplier statistics retrieved", dataPtr);
}

bool SupplierController::validateRequest(Poco::Net::HTTPServerRequest& request, 
                                       Poco::Net::HTTPServerResponse& response,
                                       std::string& errorMessage)
{
    if (!BaseController::validateRequest(request, response, errorMessage))
    {
        return false;
    }
    
    return true;
}

bool SupplierController::authorizeRequest(Poco::Net::HTTPServerRequest& request, 
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

bool SupplierController::validateSupplierAccess(Poco::Net::HTTPServerRequest& request, 
                                              long long supplierId,
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
    
    errorMessage = "You don't have permission to access this supplier";
    return false;
}

long long SupplierController::getCurrentUserId(Poco::Net::HTTPServerRequest& request)
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

database::models::UserRole SupplierController::getCurrentUserRole(Poco::Net::HTTPServerRequest& request)
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

bool SupplierController::validateCreateSupplierData(const Poco::JSON::Object::Ptr& json, 
                                                  std::vector<std::string>& errors)
{
    errors.clear();
    
    std::vector<std::string> requiredFields = {"name", "tax_id"};
    std::vector<std::string> missingFields;
    if (!validateRequiredFields(json, requiredFields, missingFields))
    {
        for (const auto& field : missingFields)
        {
            errors.push_back(field + " is required");
        }
    }
    
    if (json->has("name"))
    {
        std::string name = utils::JsonUtils::getString(*json, "name");
        if (name.empty() || name.length() > 150)
        {
            errors.push_back("Supplier name must be between 1 and 150 characters");
        }
    }
    
    if (json->has("tax_id"))
    {
        std::string taxId = utils::JsonUtils::getString(*json, "tax_id");
        if (taxId.empty() || taxId.length() > 20)
        {
            errors.push_back("Tax ID must be between 1 and 20 characters");
        }
    }
    
    if (json->has("email"))
    {
        std::string email = utils::JsonUtils::getString(*json, "email");
        if (!email.empty() && !utils::Validator::isValidEmail(email))
        {
            errors.push_back("Invalid email format");
        }
    }
    
    if (json->has("phone"))
    {
        std::string phone = utils::JsonUtils::getString(*json, "phone");
        if (!phone.empty() && !utils::Validator::isValidPhone(phone))
        {
            errors.push_back("Invalid phone format");
        }
    }
    
    if (json->has("rating"))
    {
        double rating = utils::JsonUtils::getDouble(*json, "rating", 0.0);
        if (rating < 0.0 || rating > 5.0)
        {
            errors.push_back("Rating must be between 0.0 and 5.0");
        }
    }
    
    return errors.empty();
}

bool SupplierController::validateUpdateSupplierData(const Poco::JSON::Object::Ptr& json, 
                                                  std::vector<std::string>& errors)
{
    errors.clear();
    
    bool hasName = json->has("name") && !json->get("name").isEmpty();
    bool hasContactPerson = json->has("contactPerson") && !json->get("contactPerson").isEmpty();
    bool hasEmail = json->has("email") && !json->get("email").isEmpty();
    bool hasPhone = json->has("phone") && !json->get("phone").isEmpty();
    bool hasAddress = json->has("address") && !json->get("address").isEmpty();
    bool hasPaymentTerms = json->has("paymentTerms") && !json->get("paymentTerms").isEmpty();
    bool hasRating = json->has("rating") && !json->get("rating").isEmpty();
    
    if (!hasName && !hasContactPerson && !hasEmail && !hasPhone && 
        !hasAddress && !hasPaymentTerms && !hasRating)
    {
        errors.push_back("At least one field must be provided for update");
    }
    
    if (hasName)
    {
        std::string name = utils::JsonUtils::getString(*json, "name");
        if (name.empty() || name.length() > 150)
        {
            errors.push_back("Supplier name must be between 1 and 150 characters");
        }
    }
    
    if (hasEmail)
    {
        std::string email = utils::JsonUtils::getString(*json, "email");
        if (!email.empty() && !utils::Validator::isValidEmail(email))
        {
            errors.push_back("Invalid email format");
        }
    }
    
    if (hasPhone)
    {
        std::string phone = utils::JsonUtils::getString(*json, "phone");
        if (!phone.empty() && !utils::Validator::isValidPhone(phone))
        {
            errors.push_back("Invalid phone format");
        }
    }
    
    if (hasRating)
    {
        double rating = utils::JsonUtils::getDouble(*json, "rating", 0.0);
        if (rating < 0.0 || rating > 5.0)
        {
            errors.push_back("Rating must be between 0.0 and 5.0");
        }
    }
    
    return errors.empty();
}

bool SupplierController::validateSearchParameters(const std::map<std::string, std::string>& filters,
                                                std::vector<std::string>& errors)
{
    errors.clear();
    
    for (const auto& [key, value] : filters)
    {
        if (key == "active")
        {
            if (value != "true" && value != "false")
            {
                errors.push_back("Active must be 'true' or 'false'");
            }
        }
        else if (key == "minRating")
        {
            try
            {
                double rating = std::stod(value);
                if (rating < 0.0 || rating > 5.0)
                {
                    errors.push_back("Minimum rating must be between 0.0 and 5.0");
                }
            }
            catch (...)
            {
                errors.push_back("Invalid minimum rating");
            }
        }
        else if (key == "maxRating")
        {
            try
            {
                double rating = std::stod(value);
                if (rating < 0.0 || rating > 5.0)
                {
                    errors.push_back("Maximum rating must be between 0.0 and 5.0");
                }
            }
            catch (...)
            {
                errors.push_back("Invalid maximum rating");
            }
        }
    }
    
    return errors.empty();
}

void SupplierController::logSupplierEvent(long long userId, 
                                        const std::string& action,
                                        long long supplierId,
                                        const std::string& ipAddress,
                                        const std::string& userAgent,
                                        bool success,
                                        const std::string& details)
{
    std::cout << "[" << getCurrentTimestamp() << "] "
              << "SUPPLIER " << action << " "
              << "UserID: " << userId << " "
              << "SupplierID: " << supplierId << " "
              << "IP: " << ipAddress << " "
              << "Success: " << (success ? "Yes" : "No") << " "
              << "Details: " << details << std::endl;
}

std::unique_ptr<database::models::Supplier> SupplierController::extractSupplierFromJson(const Poco::JSON::Object::Ptr& json)
{
    if (!json)
    {
        return nullptr;
    }
    
    try
    {
        auto supplier = std::make_unique<database::models::Supplier>(*json);
        return supplier;
    }
    catch (...)
    {
        return nullptr;
    }
}

bool SupplierController::canCreateSupplier(Poco::Net::HTTPServerRequest& request, 
                                         const database::models::Supplier& supplierData,
                                         std::string& errorMessage)
{
    auto currentUserRole = getCurrentUserRole(request);
    
    if (currentUserRole == database::models::UserRole::ADMIN || 
        currentUserRole == database::models::UserRole::MANAGER)
    {
        return true;
    }
    
    errorMessage = "You don't have permission to create suppliers";
    return false;
}

bool SupplierController::canUpdateSupplier(Poco::Net::HTTPServerRequest& request, 
                                         long long supplierId,
                                         const database::models::Supplier& supplierData,
                                         std::string& errorMessage)
{
    auto currentUserRole = getCurrentUserRole(request);
    
    if (currentUserRole == database::models::UserRole::ADMIN || 
        currentUserRole == database::models::UserRole::MANAGER)
    {
        return true;
    }
    
    errorMessage = "You don't have permission to update suppliers";
    return false;
}

bool SupplierController::canDeleteSupplier(Poco::Net::HTTPServerRequest& request, 
                                         long long supplierId,
                                         std::string& errorMessage)
{
    auto currentUserRole = getCurrentUserRole(request);
    
    if (currentUserRole == database::models::UserRole::ADMIN)
    {
        return true;
    }
    
    if (currentUserRole == database::models::UserRole::MANAGER)
    {
        return true;
    }
    
    errorMessage = "You don't have permission to delete suppliers";
    return false;
}

bool SupplierController::canActivateSupplier(Poco::Net::HTTPServerRequest& request, 
                                           long long supplierId,
                                           std::string& errorMessage)
{
    auto currentUserRole = getCurrentUserRole(request);
    
    if (currentUserRole == database::models::UserRole::ADMIN || 
        currentUserRole == database::models::UserRole::MANAGER)
    {
        return true;
    }
    
    errorMessage = "You don't have permission to activate suppliers";
    return false;
}

bool SupplierController::canDeactivateSupplier(Poco::Net::HTTPServerRequest& request, 
                                             long long supplierId,
                                             std::string& errorMessage)
{
    auto currentUserRole = getCurrentUserRole(request);
    
    if (currentUserRole == database::models::UserRole::ADMIN || 
        currentUserRole == database::models::UserRole::MANAGER)
    {
        return true;
    }
    
    errorMessage = "You don't have permission to deactivate suppliers";
    return false;
}

Poco::JSON::Object SupplierController::buildPaginationResponse(int page, int pageSize, int totalItems, 
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
