#include "CategoryController.hpp"
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

CategoryController::CategoryController()
    : categoryService(std::make_unique<services::CategoryService>()),
      authService(std::make_unique<services::AuthService>())
{
}

void CategoryController::handleRequest(Poco::Net::HTTPServerRequest& request, 
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
        
        std::string basePath = "/api/v1/categories";
        if (endpoint.find("/api/v1/") != 0)
        {
            basePath = "/api/categories";
        }
        
        std::string categoryIdStr = getPathParameter(uri, basePath + "/", 0);
        
        if ((endpoint == "/api/v1/categories/tree" || endpoint == "/api/categories/tree") && 
            request.getMethod() == "GET")
        {
            handleGetCategoryTree(request, response);
        }
        else if ((endpoint == "/api/v1/categories/roots" || endpoint == "/api/categories/roots") && 
                 request.getMethod() == "GET")
        {
            handleGetRootCategories(request, response);
        }
        else if ((endpoint.find("/api/v1/categories/statistics") == 0 || endpoint.find("/api/categories/statistics") == 0) && 
                 request.getMethod() == "GET")
        {
            handleGetCategoryStatistics(request, response);
        }
        else if ((endpoint.find("/api/v1/categories/import") == 0 || endpoint.find("/api/categories/import") == 0) && 
                 request.getMethod() == "POST")
        {
            handleImportCategories(request, response);
        }
        else if ((endpoint.find("/api/v1/categories/export") == 0 || endpoint.find("/api/categories/export") == 0) && 
                 request.getMethod() == "GET")
        {
            handleExportCategories(request, response);
        }
        else if ((endpoint.find("/api/v1/categories/search") == 0 || endpoint.find("/api/categories/search") == 0) && 
                 request.getMethod() == "GET")
        {
            handleSearchCategories(request, response);
        }
        else if ((endpoint.find("/api/v1/categories/move") == 0 || endpoint.find("/api/categories/move") == 0) && 
                 request.getMethod() == "PUT")
        {
            handleMoveCategory(request, response);
        }
        else if ((endpoint.find("/api/v1/categories/children") == 0 || endpoint.find("/api/categories/children") == 0) && 
                 request.getMethod() == "GET")
        {
            handleGetChildCategories(request, response);
        }
        else if ((endpoint.find("/api/v1/categories/products") == 0 || endpoint.find("/api/categories/products") == 0) && 
                 request.getMethod() == "GET")
        {
            handleGetCategoryProducts(request, response);
        }
        else if ((endpoint == "/api/v1/categories" || endpoint == "/api/categories") && 
                 request.getMethod() == "GET")
        {
            handleGetCategories(request, response);
        }
        else if ((endpoint == "/api/v1/categories" || endpoint == "/api/categories") && 
                 request.getMethod() == "POST")
        {
            handleCreateCategory(request, response);
        }
        else if ((endpoint.find("/api/v1/categories/") == 0 || endpoint.find("/api/categories/") == 0) && 
                 !categoryIdStr.empty())
        {
            if (endpoint.find("/tree") != std::string::npos && request.getMethod() == "GET")
            {
                handleGetCategorySubtree(request, response);
            }
            else if (endpoint.find("/children") != std::string::npos && request.getMethod() == "GET")
            {
                handleGetChildCategories(request, response);
            }
            else if (endpoint.find("/products") != std::string::npos && request.getMethod() == "GET")
            {
                handleGetCategoryProducts(request, response);
            }
            else if (endpoint.find("/move") != std::string::npos && request.getMethod() == "PUT")
            {
                handleMoveCategory(request, response);
            }
            else if (request.getMethod() == "GET")
            {
                handleGetCategoryById(request, response);
            }
            else if (request.getMethod() == "PUT")
            {
                handleUpdateCategory(request, response);
            }
            else if (request.getMethod() == "DELETE")
            {
                handleDeleteCategory(request, response);
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

void CategoryController::handleGetCategories(Poco::Net::HTTPServerRequest& request, 
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
        Poco::JSON::Array categoriesArray;
        
        if (filters.find("parentId") != filters.end())
        {
            long long parentId = std::stoll(filters["parentId"]);
            categoriesArray = categoryService->getChildCategories(parentId);
        }
        else if (!filters.empty())
        {
            std::string query;
            if (filters.find("query") != filters.end())
            {
                query = filters["query"];
            }
            categoriesArray = categoryService->searchCategories(query, page, pageSize);
        }
        else
        {
            categoriesArray = categoryService->getAllCategories(page, pageSize);
        }
        
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object;
        dataPtr->set("categories", categoriesArray);
        sendSuccessResponse(response, "Categories retrieved successfully", dataPtr);
    }
    catch (const std::exception& e)
    {
        sendErrorResponse(response, "Error retrieving categories: " + std::string(e.what()), 
                        Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
}

void CategoryController::handleGetCategoryById(Poco::Net::HTTPServerRequest& request, 
                                             Poco::Net::HTTPServerResponse& response)
{
    std::string categoryIdStr = getPathParameter(request.getURI(), "/api/v1/categories/", 0);
    if (categoryIdStr.empty())
    {
        sendErrorResponse(response, "Category ID is required", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    long long categoryId;
    try
    {
        categoryId = std::stoll(categoryIdStr);
    }
    catch (...)
    {
        sendErrorResponse(response, "Invalid category ID", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    try
    {
        auto result = categoryService->getCategoryById(categoryId);
        
        if (!result.success)
        {
            if (result.message.find("not found") != std::string::npos)
            {
                sendNotFoundResponse(response, "Category");
            }
            else
            {
                sendErrorResponse(response, result.message, 
                                Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
            }
            return;
        }
        
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object(result.category->toJson());
        sendSuccessResponse(response, "Category retrieved successfully", dataPtr);
    }
    catch (const std::exception& e)
    {
        sendErrorResponse(response, "Error retrieving category: " + std::string(e.what()), 
                        Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
}

void CategoryController::handleCreateCategory(Poco::Net::HTTPServerRequest& request, 
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
    if (!validateCreateCategoryData(json, errors))
    {
        sendValidationErrorResponse(response, errors);
        return;
    }
    
    auto categoryData = extractCategoryFromJson(json);
    if (!categoryData)
    {
        sendErrorResponse(response, "Invalid category data", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    std::string authError;
    if (!canCreateCategory(request, *categoryData, authError))
    {
        sendForbiddenResponse(response, authError);
        return;
    }
    
    long long createdBy = getCurrentUserId(request);
    
    auto result = categoryService->createCategory(*categoryData, createdBy);
    
    logCategoryEvent(createdBy, "CREATE_CATEGORY", result.categoryId, 
                    getClientIpAddress(request), getUserAgent(request), 
                    result.success, result.success ? "Category created successfully" : result.message);
    
    if (result.success)
    {
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object(result.category->toJson());
        sendSuccessResponse(response, "Category created successfully", dataPtr);
    }
    else
    {
        sendErrorResponse(response, result.message, 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
    }
}

void CategoryController::handleUpdateCategory(Poco::Net::HTTPServerRequest& request, 
                                            Poco::Net::HTTPServerResponse& response)
{
    std::string categoryIdStr = getPathParameter(request.getURI(), "/api/v1/categories/", 0);
    if (categoryIdStr.empty())
    {
        sendErrorResponse(response, "Category ID is required", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    long long categoryId;
    try
    {
        categoryId = std::stoll(categoryIdStr);
    }
    catch (...)
    {
        sendErrorResponse(response, "Invalid category ID", 
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
    if (!validateUpdateCategoryData(json, errors))
    {
        sendValidationErrorResponse(response, errors);
        return;
    }
    
    auto categoryData = extractCategoryFromJson(json);
    if (!categoryData)
    {
        sendErrorResponse(response, "Invalid category data", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    std::string authError;
    if (!canUpdateCategory(request, categoryId, *categoryData, authError))
    {
        sendForbiddenResponse(response, authError);
        return;
    }
    
    long long updatedBy = getCurrentUserId(request);
    
    auto result = categoryService->updateCategory(categoryId, *categoryData, updatedBy);
    
    logCategoryEvent(updatedBy, "UPDATE_CATEGORY", categoryId, 
                    getClientIpAddress(request), getUserAgent(request), 
                    result.success, result.success ? "Category updated successfully" : result.message);
    
    if (result.success)
    {
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object(result.category->toJson());
        sendSuccessResponse(response, "Category updated successfully", dataPtr);
    }
    else
    {
        sendErrorResponse(response, result.message, 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
    }
}

void CategoryController::handleDeleteCategory(Poco::Net::HTTPServerRequest& request, 
                                            Poco::Net::HTTPServerResponse& response)
{
    std::string categoryIdStr = getPathParameter(request.getURI(), "/api/v1/categories/", 0);
    if (categoryIdStr.empty())
    {
        sendErrorResponse(response, "Category ID is required", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    long long categoryId;
    try
    {
        categoryId = std::stoll(categoryIdStr);
    }
    catch (...)
    {
        sendErrorResponse(response, "Invalid category ID", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    std::string authError;
    if (!canDeleteCategory(request, categoryId, authError))
    {
        sendForbiddenResponse(response, authError);
        return;
    }
    
    long long deletedBy = getCurrentUserId(request);
    
    bool forceDelete = false;
    auto json = parseJsonBody(request);
    if (json && json->has("forceDelete"))
    {
        forceDelete = utils::JsonUtils::getBool(*json, "forceDelete");
    }
    
    auto result = categoryService->deleteCategory(categoryId, deletedBy, forceDelete);
    
    logCategoryEvent(deletedBy, "DELETE_CATEGORY", categoryId, 
                    getClientIpAddress(request), getUserAgent(request), 
                    result.success, result.success ? "Category deleted successfully" : result.message);
    
    if (result.success)
    {
        sendSuccessResponse(response, "Category deleted successfully");
    }
    else
    {
        sendErrorResponse(response, result.message, 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
    }
}

void CategoryController::handleGetCategoryTree(Poco::Net::HTTPServerRequest& request, 
                                             Poco::Net::HTTPServerResponse& response)
{
    try
    {
        Poco::JSON::Array categoryTree = categoryService->getCategoriesTree();
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object;
        dataPtr->set("categoryTree", categoryTree);
        sendSuccessResponse(response, "Full category tree retrieved successfully", dataPtr);
    }
    catch (const std::exception& e)
    {
        sendErrorResponse(response, "Error retrieving category tree: " + std::string(e.what()), 
                        Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
}

void CategoryController::handleGetCategorySubtree(Poco::Net::HTTPServerRequest& request, 
                                                Poco::Net::HTTPServerResponse& response)
{
    std::string categoryIdStr = getPathParameter(request.getURI(), "/api/v1/categories/", 0);
    categoryIdStr = categoryIdStr.substr(0, categoryIdStr.find("/tree"));
    
    if (categoryIdStr.empty())
    {
        sendErrorResponse(response, "Category ID is required for subtree", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    try
    {
        long long categoryId = std::stoll(categoryIdStr);
        auto result = categoryService->getCategoryWithChildren(categoryId);
        
        if (!result.success)
        {
            if (result.message.find("not found") != std::string::npos)
            {
                sendNotFoundResponse(response, "Category");
            }
            else
            {
                sendErrorResponse(response, result.message, 
                                Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
            }
            return;
        }
        
        Poco::JSON::Array categoryTree;
        categoryTree.add(result.category->toJsonWithChildren());
        
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object;
        dataPtr->set("categoryTree", categoryTree);
        sendSuccessResponse(response, "Category subtree retrieved successfully", dataPtr);
    }
    catch (const std::exception& e)
    {
        sendErrorResponse(response, "Invalid category ID: " + std::string(e.what()), 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
    }
}

void CategoryController::handleGetRootCategories(Poco::Net::HTTPServerRequest& request, 
                                               Poco::Net::HTTPServerResponse& response)
{
    try
    {
        Poco::JSON::Array rootCategories = categoryService->getRootCategories();
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object;
        dataPtr->set("rootCategories", rootCategories);
        sendSuccessResponse(response, "Root categories retrieved successfully", dataPtr);
    }
    catch (const std::exception& e)
    {
        sendErrorResponse(response, "Error retrieving root categories: " + std::string(e.what()), 
                        Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
}

void CategoryController::handleGetChildCategories(Poco::Net::HTTPServerRequest& request, 
                                                Poco::Net::HTTPServerResponse& response)
{
    std::string parentIdStr = getQueryParameter(request.getURI(), "parentId");
    if (parentIdStr.empty())
    {
        sendErrorResponse(response, "Parent category ID is required", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    long long parentId;
    try
    {
        parentId = std::stoll(parentIdStr);
    }
    catch (...)
    {
        sendErrorResponse(response, "Invalid parent category ID", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    try
    {
        Poco::JSON::Array childCategories = categoryService->getChildCategories(parentId);
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object;
        dataPtr->set("childCategories", childCategories);
        sendSuccessResponse(response, "Child categories retrieved successfully", dataPtr);
    }
    catch (const std::exception& e)
    {
        sendErrorResponse(response, "Error retrieving child categories: " + std::string(e.what()), 
                        Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
}

void CategoryController::handleGetCategoryProducts(Poco::Net::HTTPServerRequest& request, 
                                                 Poco::Net::HTTPServerResponse& response)
{
    std::string categoryIdStr = getPathParameter(request.getURI(), "/api/v1/categories/", 0);
    categoryIdStr = categoryIdStr.substr(0, categoryIdStr.find("/products"));
    
    if (categoryIdStr.empty())
    {
        sendErrorResponse(response, "Category ID is required", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    long long categoryId;
    try
    {
        categoryId = std::stoll(categoryIdStr);
    }
    catch (...)
    {
        sendErrorResponse(response, "Invalid category ID", 
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
        Poco::JSON::Array products = categoryService->getCategoryProducts(categoryId, page, pageSize);
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object;
        dataPtr->set("products", products);
        sendSuccessResponse(response, "Category products retrieved successfully", dataPtr);
    }
    catch (const std::exception& e)
    {
        sendErrorResponse(response, "Error retrieving category products: " + std::string(e.what()), 
                        Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
}

void CategoryController::handleSearchCategories(Poco::Net::HTTPServerRequest& request, 
                                              Poco::Net::HTTPServerResponse& response)
{
    std::string query = getQueryParameter(request.getURI(), "query");
    
    int page, pageSize;
    if (!getPaginationParameters(request, page, pageSize, 20))
    {
        sendErrorResponse(response, "Invalid pagination parameters", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    try
    {
        Poco::JSON::Array categories = categoryService->searchCategories(query, page, pageSize);
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object;
        dataPtr->set("categories", categories);
        sendSuccessResponse(response, "Categories search completed", dataPtr);
    }
    catch (const std::exception& e)
    {
        sendErrorResponse(response, "Error searching categories: " + std::string(e.what()), 
                        Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
}

void CategoryController::handleMoveCategory(Poco::Net::HTTPServerRequest& request, 
                                          Poco::Net::HTTPServerResponse& response)
{
    std::string categoryIdStr = getPathParameter(request.getURI(), "/api/v1/categories/", 0);
    categoryIdStr = categoryIdStr.substr(0, categoryIdStr.find("/move"));
    
    if (categoryIdStr.empty())
    {
        sendErrorResponse(response, "Category ID is required", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    long long categoryId;
    try
    {
        categoryId = std::stoll(categoryIdStr);
    }
    catch (...)
    {
        sendErrorResponse(response, "Invalid category ID", 
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
    if (!validateMoveCategoryData(json, errors))
    {
        sendValidationErrorResponse(response, errors);
        return;
    }
    
    long long newParentId = utils::JsonUtils::getInt(*json, "newParentId");
    
    std::string authError;
    if (!canMoveCategory(request, categoryId, newParentId, authError))
    {
        sendForbiddenResponse(response, authError);
        return;
    }
    
    long long movedBy = getCurrentUserId(request);
    
    auto result = categoryService->moveCategory(categoryId, newParentId, movedBy);
    
    logCategoryEvent(movedBy, "MOVE_CATEGORY", categoryId, 
                    getClientIpAddress(request), getUserAgent(request), 
                    result.success, result.success ? "Category moved successfully" : result.message);
    
    if (result.success)
    {
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object(result.category->toJson());
        sendSuccessResponse(response, "Category moved successfully", dataPtr);
    }
    else
    {
        sendErrorResponse(response, result.message, 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
    }
}

void CategoryController::handleGetCategoryStatistics(Poco::Net::HTTPServerRequest& request, 
                                                   Poco::Net::HTTPServerResponse& response)
{
    std::string categoryIdStr = getQueryParameter(request.getURI(), "categoryId");
    
    try
    {
        Poco::JSON::Object statistics;
        
        if (!categoryIdStr.empty())
        {
            long long categoryId = std::stoll(categoryIdStr);
            statistics = categoryService->getCategoryStatistics(categoryId);
        }
        else
        {
            statistics = Poco::JSON::Object();
        }
        
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object(statistics);
        sendSuccessResponse(response, "Category statistics retrieved", dataPtr);
    }
    catch (...)
    {
        sendErrorResponse(response, "Invalid category ID", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
    }
}

void CategoryController::handleImportCategories(Poco::Net::HTTPServerRequest& request, 
                                              Poco::Net::HTTPServerResponse& response)
{
    auto currentUserRole = getCurrentUserRole(request);
    if (currentUserRole != database::models::UserRole::ADMIN && 
        currentUserRole != database::models::UserRole::MANAGER)
    {
        sendForbiddenResponse(response, "You don't have permission to import categories");
        return;
    }
    
    auto json = parseJsonBody(request);
    if (!json)
    {
        sendErrorResponse(response, "Invalid JSON body", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    if (!json->has("categories"))
    {
        sendErrorResponse(response, "Categories array is required", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    auto categoriesArray = json->getArray("categories");
    if (!categoriesArray || categoriesArray->size() == 0)
    {
        sendErrorResponse(response, "Categories array cannot be empty", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    long long importedBy = getCurrentUserId(request);
    
    try
    {
        Poco::JSON::Array result = categoryService->importCategories(*categoriesArray, importedBy);
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object;
        dataPtr->set("importResult", result);
        sendSuccessResponse(response, "Categories imported successfully", dataPtr);
    }
    catch (const std::exception& e)
    {
        sendErrorResponse(response, "Error importing categories: " + std::string(e.what()), 
                        Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
}

void CategoryController::handleExportCategories(Poco::Net::HTTPServerRequest& request, 
                                              Poco::Net::HTTPServerResponse& response)
{
    auto currentUserRole = getCurrentUserRole(request);
    if (currentUserRole != database::models::UserRole::ADMIN && 
        currentUserRole != database::models::UserRole::MANAGER)
    {
        sendForbiddenResponse(response, "You don't have permission to export categories");
        return;
    }
    
    std::string categoryIdsStr = getQueryParameter(request.getURI(), "categoryIds");
    std::vector<long long> categoryIds;
    
    if (!categoryIdsStr.empty())
    {
        std::stringstream ss(categoryIdsStr);
        std::string id;
        while (std::getline(ss, id, ','))
        {
            try
            {
                categoryIds.push_back(std::stoll(id));
            }
            catch (...)
            {
            }
        }
    }
    
    try
    {
        Poco::JSON::Array result = categoryService->exportCategories(categoryIds);
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object;
        dataPtr->set("categories", result);
        
        response.setContentType("application/json");
        response.set("Content-Disposition", "attachment; filename=categories_export.json");
        
        std::ostream& ostr = response.send();
        result.stringify(ostr);
    }
    catch (const std::exception& e)
    {
        sendErrorResponse(response, "Error exporting categories: " + std::string(e.what()), 
                        Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
}

bool CategoryController::validateRequest(Poco::Net::HTTPServerRequest& request, 
                                       Poco::Net::HTTPServerResponse& response,
                                       std::string& errorMessage)
{
    if (!BaseController::validateRequest(request, response, errorMessage))
    {
        return false;
    }
    
    return true;
}

bool CategoryController::authorizeRequest(Poco::Net::HTTPServerRequest& request, 
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

bool CategoryController::validateCategoryAccess(Poco::Net::HTTPServerRequest& request, 
                                              long long categoryId,
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
    
    errorMessage = "You don't have permission to access this category";
    return false;
}

long long CategoryController::getCurrentUserId(Poco::Net::HTTPServerRequest& request)
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

database::models::UserRole CategoryController::getCurrentUserRole(Poco::Net::HTTPServerRequest& request)
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

bool CategoryController::validateCreateCategoryData(const Poco::JSON::Object::Ptr& json, 
                                                   std::vector<std::string>& errors)
{
    errors.clear();
    
    std::vector<std::string> requiredFields = {"name"};
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
        if (name.empty() || name.length() > 100)
        {
            errors.push_back("Category name must be between 1 and 100 characters");
        }
    }
    
    if (json->has("description"))
    {
        std::string description = utils::JsonUtils::getString(*json, "description");
        if (description.length() > 1000)
        {
            errors.push_back("Description cannot exceed 1000 characters");
        }
    }
    
    if (json->has("parentId"))
    {
        try
        {
            long long parentId = utils::JsonUtils::getInt(*json, "parentId");
            if (parentId < 0)
            {
                errors.push_back("Parent ID must be a positive integer");
            }
        }
        catch (...)
        {
            errors.push_back("Invalid parent ID");
        }
    }
    
    if (json->has("sortOrder"))
    {
        try
        {
            int sortOrder = utils::JsonUtils::getInt(*json, "sortOrder");
            if (sortOrder < 0)
            {
                errors.push_back("Sort order must be a non-negative integer");
            }
        }
        catch (...)
        {
            errors.push_back("Invalid sort order");
        }
    }
    
    return errors.empty();
}

bool CategoryController::validateUpdateCategoryData(const Poco::JSON::Object::Ptr& json, 
                                                   std::vector<std::string>& errors)
{
    errors.clear();
    
    bool hasName = json->has("name") && !json->get("name").isEmpty();
    bool hasDescription = json->has("description") && !json->get("description").isEmpty();
    bool hasSortOrder = json->has("sortOrder") && !json->get("sortOrder").isEmpty();
    
    if (!hasName && !hasDescription && !hasSortOrder)
    {
        errors.push_back("At least one field must be provided for update");
    }
    
    if (hasName)
    {
        std::string name = utils::JsonUtils::getString(*json, "name");
        if (name.empty() || name.length() > 100)
        {
            errors.push_back("Category name must be between 1 and 100 characters");
        }
    }
    
    if (hasDescription)
    {
        std::string description = utils::JsonUtils::getString(*json, "description");
        if (description.length() > 1000)
        {
            errors.push_back("Description cannot exceed 1000 characters");
        }
    }
    
    if (hasSortOrder)
    {
        try
        {
            int sortOrder = utils::JsonUtils::getInt(*json, "sortOrder");
            if (sortOrder < 0)
            {
                errors.push_back("Sort order must be a non-negative integer");
            }
        }
        catch (...)
        {
            errors.push_back("Invalid sort order");
        }
    }
    
    return errors.empty();
}

bool CategoryController::validateMoveCategoryData(const Poco::JSON::Object::Ptr& json, 
                                                 std::vector<std::string>& errors)
{
    errors.clear();
    
    std::vector<std::string> requiredFields = {"newParentId"};
    std::vector<std::string> missingFields;
    if (!validateRequiredFields(json, requiredFields, missingFields))
    {
        for (const auto& field : missingFields)
        {
            errors.push_back(field + " is required");
        }
    }
    
    if (json->has("newParentId"))
    {
        try
        {
            long long newParentId = utils::JsonUtils::getInt(*json, "newParentId");
            if (newParentId < 0)
            {
                errors.push_back("New parent ID must be a non-negative integer");
            }
        }
        catch (...)
        {
            errors.push_back("Invalid new parent ID");
        }
    }
    
    return errors.empty();
}

bool CategoryController::validateSearchParameters(const std::map<std::string, std::string>& filters,
                                                 std::vector<std::string>& errors)
{
    errors.clear();
    
    for (const auto& [key, value] : filters)
    {
        if (key == "parentId")
        {
            try
            {
                long long parentId = std::stoll(value);
                if (parentId < 0)
                {
                    errors.push_back("Parent ID must be a non-negative integer");
                }
            }
            catch (...)
            {
                errors.push_back("Invalid parent ID");
            }
        }
        else if (key == "sortOrder")
        {
            try
            {
                int sortOrder = std::stoi(value);
                if (sortOrder < 0)
                {
                    errors.push_back("Sort order must be a non-negative integer");
                }
            }
            catch (...)
            {
                errors.push_back("Invalid sort order");
            }
        }
    }
    
    return errors.empty();
}

void CategoryController::logCategoryEvent(long long userId, 
                                         const std::string& action,
                                         long long categoryId,
                                         const std::string& ipAddress,
                                         const std::string& userAgent,
                                         bool success,
                                         const std::string& details)
{
    std::cout << "[" << getCurrentTimestamp() << "] "
              << "CATEGORY " << action << " "
              << "UserID: " << userId << " "
              << "CategoryID: " << categoryId << " "
              << "IP: " << ipAddress << " "
              << "Success: " << (success ? "Yes" : "No") << " "
              << "Details: " << details << std::endl;
}

std::unique_ptr<database::models::Category> CategoryController::extractCategoryFromJson(const Poco::JSON::Object::Ptr& json)
{
    if (!json)
    {
        return nullptr;
    }
    
    try
    {
        auto category = std::make_unique<database::models::Category>(*json);
        return category;
    }
    catch (...)
    {
        return nullptr;
    }
}

bool CategoryController::canCreateCategory(Poco::Net::HTTPServerRequest& request, 
                                          const database::models::Category& categoryData,
                                          std::string& errorMessage)
{
    auto currentUserRole = getCurrentUserRole(request);
    
    if (currentUserRole == database::models::UserRole::ADMIN || 
        currentUserRole == database::models::UserRole::MANAGER)
    {
        return true;
    }
    
    errorMessage = "You don't have permission to create categories";
    return false;
}

bool CategoryController::canUpdateCategory(Poco::Net::HTTPServerRequest& request, 
                                          long long categoryId,
                                          const database::models::Category& categoryData,
                                          std::string& errorMessage)
{
    auto currentUserRole = getCurrentUserRole(request);
    
    if (currentUserRole == database::models::UserRole::ADMIN || 
        currentUserRole == database::models::UserRole::MANAGER)
    {
        return true;
    }
    
    errorMessage = "You don't have permission to update categories";
    return false;
}

bool CategoryController::canDeleteCategory(Poco::Net::HTTPServerRequest& request, 
                                          long long categoryId,
                                          std::string& errorMessage)
{
    auto currentUserRole = getCurrentUserRole(request);
    
    if (currentUserRole == database::models::UserRole::ADMIN)
    {
        return true;
    }
    
    if (currentUserRole == database::models::UserRole::MANAGER)
    {
        bool hasProducts = categoryService->hasProducts(categoryId);
        bool hasChildren = categoryService->hasChildren(categoryId);
        
        if (!hasProducts && !hasChildren)
        {
            return true;
        }
        
        errorMessage = "Category contains products or subcategories and cannot be deleted";
        return false;
    }
    
    errorMessage = "You don't have permission to delete categories";
    return false;
}

bool CategoryController::canMoveCategory(Poco::Net::HTTPServerRequest& request, 
                                        long long categoryId,
                                        long long newParentId,
                                        std::string& errorMessage)
{
    auto currentUserRole = getCurrentUserRole(request);
    
    if (currentUserRole == database::models::UserRole::ADMIN || 
        currentUserRole == database::models::UserRole::MANAGER)
    {
        return true;
    }
    
    errorMessage = "You don't have permission to move categories";
    return false;
}

Poco::JSON::Object CategoryController::buildPaginationResponse(int page, int pageSize, int totalItems, 
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
