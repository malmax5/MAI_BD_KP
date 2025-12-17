#include "ProductController.hpp"
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

ProductController::ProductController()
    : productService(std::make_unique<services::ProductService>()),
      authService(std::make_unique<services::AuthService>()),
      categoryService(std::make_unique<services::CategoryService>()),
      supplierService(std::make_unique<services::SupplierService>())
{
}

void ProductController::handleRequest(Poco::Net::HTTPServerRequest& request, 
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
        
        if ((endpoint == "/api/v1/products" || endpoint == "/api/products") && request.getMethod() == "GET")
        {
            handleGetProducts(request, response);
        }
        else if ((endpoint == "/api/v1/products" || endpoint == "/api/products") && request.getMethod() == "POST")
        {
            handleCreateProduct(request, response);
        }
        else if ((endpoint.find("/api/v1/products/search") == 0 || endpoint.find("/api/products/search") == 0) && 
                 request.getMethod() == "GET")
        {
            handleSearchProducts(request, response);
        }
        else if ((endpoint.find("/api/v1/products/statistics") == 0 || endpoint.find("/api/products/statistics") == 0) && 
                 request.getMethod() == "GET")
        {
            handleGetProductStatistics(request, response);
        }
        else if ((endpoint.find("/api/v1/products/stock-report") == 0 || endpoint.find("/api/products/stock-report") == 0) && 
                 request.getMethod() == "GET")
        {
            handleGetStockReport(request, response);
        }
        else if ((endpoint.find("/api/v1/products/low-stock") == 0 || endpoint.find("/api/products/low-stock") == 0) && 
                 request.getMethod() == "GET")
        {
            handleGetLowStockProducts(request, response);
        }
        else if ((endpoint.find("/api/v1/products/needing-reorder") == 0 || endpoint.find("/api/products/needing-reorder") == 0) && 
                 request.getMethod() == "GET")
        {
            handleGetProductsNeedingReorder(request, response);
        }
        else if ((endpoint.find("/api/v1/products/import") == 0 || endpoint.find("/api/products/import") == 0) && 
                 request.getMethod() == "POST")
        {
            handleImportProducts(request, response);
        }
        else if ((endpoint.find("/api/v1/products/export") == 0 || endpoint.find("/api/products/export") == 0) && 
                 request.getMethod() == "GET")
        {
            handleExportProducts(request, response);
        }
        else if ((endpoint.find("/api/v1/products/by-category") == 0 || endpoint.find("/api/products/by-category") == 0) && 
                 request.getMethod() == "GET")
        {
            handleGetProductsByCategory(request, response);
        }
        else if ((endpoint.find("/api/v1/products/by-supplier") == 0 || endpoint.find("/api/products/by-supplier") == 0) && 
                 request.getMethod() == "GET")
        {
            handleGetProductsBySupplier(request, response);
        }
        else if ((endpoint.find("/api/v1/products/sku/") == 0 || endpoint.find("/api/products/sku/") == 0) && 
                 request.getMethod() == "GET")
        {
            handleGetProductBySku(request, response);
        }
        else if ((endpoint.find("/api/v1/products/") == 0 || endpoint.find("/api/products/") == 0))
        {
            long long productId = extractProductIdFromPath(endpoint);
            if (productId > 0)
            {
                if (request.getMethod() == "GET")
                {
                    handleGetProductById(request, response);
                }
                else if (request.getMethod() == "PUT")
                {
                    handleUpdateProduct(request, response);
                }
                else if (request.getMethod() == "DELETE")
                {
                    handleDeleteProduct(request, response);
                }
                else if (endpoint.find("/soft-delete") != std::string::npos && request.getMethod() == "POST")
                {
                    handleSoftDeleteProduct(request, response);
                }
                else if (endpoint.find("/activate") != std::string::npos && request.getMethod() == "POST")
                {
                    handleActivateProduct(request, response);
                }
                else if (endpoint.find("/deactivate") != std::string::npos && request.getMethod() == "POST")
                {
                    handleDeactivateProduct(request, response);
                }
                else if (endpoint.find("/stock-levels") != std::string::npos && request.getMethod() == "PUT")
                {
                    handleUpdateStockLevels(request, response);
                }
                else if (endpoint.find("/price") != std::string::npos && request.getMethod() == "PUT")
                {
                    handleUpdatePrice(request, response);
                }
                else if (endpoint.find("/stock-summary") != std::string::npos && request.getMethod() == "GET")
                {
                    handleGetProductStockSummary(request, response);
                }
                else
                {
                    sendErrorResponse(response, "Endpoint not found", 
                                    Poco::Net::HTTPResponse::HTTP_NOT_FOUND);
                }
            }
            else
            {
                sendErrorResponse(response, "Invalid product ID", 
                                Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
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

void ProductController::handleGetProducts(Poco::Net::HTTPServerRequest& request, 
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
    
    if (filters.find("is_active") == filters.end())
    {
        filters["is_active"] = "true";
    }
    
    auto currentUserRole = getCurrentUserRole(request);
    
    if (currentUserRole != database::models::UserRole::ADMIN && 
        currentUserRole != database::models::UserRole::MANAGER)
    {
        filters["is_active"] = "true";
    }
    
    Poco::JSON::Array productsArray = productService->searchProducts("", page, pageSize);
    
    Poco::JSON::Object::Ptr responseObj = new Poco::JSON::Object;
    responseObj->set("products", productsArray);
    responseObj->set("page", page);
    responseObj->set("pageSize", pageSize);
    
    sendSuccessResponse(response, "Products retrieved successfully", responseObj);
}

void ProductController::handleGetProductById(Poco::Net::HTTPServerRequest& request, 
                                           Poco::Net::HTTPServerResponse& response)
{
    long long productId = extractProductIdFromPath(request.getURI());
    if (productId <= 0)
    {
        sendErrorResponse(response, "Product ID is required", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    std::string accessError;
    if (!canViewProduct(request, productId, accessError))
    {
        sendForbiddenResponse(response, accessError);
        return;
    }
    
    auto result = productService->getProductById(productId);
    
    if (result.success)
    {
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object(result.product->toJson());
        sendSuccessResponse(response, "Product retrieved successfully", dataPtr);
    }
    else
    {
        sendNotFoundResponse(response, "Product");
    }
}

void ProductController::handleGetProductBySku(Poco::Net::HTTPServerRequest& request, 
                                            Poco::Net::HTTPServerResponse& response)
{
    std::string sku = getPathParameter(request.getURI(), "/api/v1/products/sku/", 0);
    if (sku.empty())
    {
        sku = getPathParameter(request.getURI(), "/api/products/sku/", 0);
    }
    
    if (sku.empty())
    {
        sendErrorResponse(response, "Product SKU is required", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    auto result = productService->getProductBySku(sku);
    
    if (result.success)
    {
        std::string accessError;
        if (!canViewProduct(request, result.productId, accessError))
        {
            sendForbiddenResponse(response, accessError);
            return;
        }
        
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object(result.product->toJson());
        sendSuccessResponse(response, "Product retrieved successfully", dataPtr);
    }
    else
    {
        sendNotFoundResponse(response, "Product");
    }
}

void ProductController::handleCreateProduct(Poco::Net::HTTPServerRequest& request, 
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
    if (!validateCreateProductData(json, errors))
    {
        sendValidationErrorResponse(response, errors);
        return;
    }
    
    auto productData = extractProductFromJson(json);
    if (!productData)
    {
        sendErrorResponse(response, "Invalid product data", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    if (!productService->isSkuAvailable(productData->sku))
    {
        sendErrorResponse(response, "SKU already exists", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    std::string authError;
    if (!canCreateProduct(request, *productData, authError))
    {
        sendForbiddenResponse(response, authError);
        return;
    }
    
    long long createdBy = getCurrentUserId(request);
    
    auto result = productService->createProduct(*productData, createdBy);
    
    std::string ipAddress = getClientIpAddress(request);
    std::string userAgent = getUserAgent(request);
    
    logProductEvent(createdBy, "CREATE_PRODUCT", result.productId, ipAddress, userAgent, 
                   result.success, result.success ? "Product created successfully" : result.message);
    
    if (result.success)
    {
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object(result.product->toJson());
        sendSuccessResponse(response, "Product created successfully", dataPtr);
    }
    else
    {
        sendErrorResponse(response, result.message, 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
    }
}

void ProductController::handleUpdateProduct(Poco::Net::HTTPServerRequest& request, 
                                          Poco::Net::HTTPServerResponse& response)
{
    long long productId = extractProductIdFromPath(request.getURI());
    if (productId <= 0)
    {
        sendErrorResponse(response, "Product ID is required", 
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
    if (!validateUpdateProductData(json, errors))
    {
        sendValidationErrorResponse(response, errors);
        return;
    }
    
    auto productData = extractProductFromJson(json);
    if (!productData)
    {
        sendErrorResponse(response, "Invalid product data", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    if (json->has("sku"))
    {
        std::string newSku = utils::JsonUtils::getString(*json, "sku");
        if (!productService->isSkuAvailable(newSku, productId))
        {
            sendErrorResponse(response, "SKU already exists", 
                            Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
            return;
        }
    }
    
    std::string authError;
    if (!canUpdateProduct(request, productId, *productData, authError))
    {
        sendForbiddenResponse(response, authError);
        return;
    }
    
    long long updatedBy = getCurrentUserId(request);
    
    auto result = productService->updateProduct(productId, *productData, updatedBy);
    
    std::string ipAddress = getClientIpAddress(request);
    std::string userAgent = getUserAgent(request);
    
    logProductEvent(updatedBy, "UPDATE_PRODUCT", productId, ipAddress, userAgent, 
                   result.success, result.success ? "Product updated successfully" : result.message);
    
    if (result.success)
    {
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object(result.product->toJson());
        sendSuccessResponse(response, "Product updated successfully", dataPtr);
    }
    else
    {
        sendErrorResponse(response, result.message, 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
    }
}

void ProductController::handleDeleteProduct(Poco::Net::HTTPServerRequest& request, 
                                          Poco::Net::HTTPServerResponse& response)
{
    long long productId = extractProductIdFromPath(request.getURI());
    if (productId <= 0)
    {
        sendErrorResponse(response, "Product ID is required", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    std::string authError;
    if (!canDeleteProduct(request, productId, authError))
    {
        sendForbiddenResponse(response, authError);
        return;
    }
    
    
    long long deletedBy = getCurrentUserId(request);
    
    auto result = productService->deactivateProduct(productId, deletedBy);
    
    std::string ipAddress = getClientIpAddress(request);
    std::string userAgent = getUserAgent(request);
    
    logProductEvent(deletedBy, "DELETE_PRODUCT", productId, ipAddress, userAgent, 
                   result.success, result.success ? "Product deleted successfully" : result.message);
    
    if (result.success)
    {
        sendSuccessResponse(response, "Product deleted successfully");
    }
    else
    {
        sendErrorResponse(response, result.message, 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
    }
}

void ProductController::handleSoftDeleteProduct(Poco::Net::HTTPServerRequest& request, 
                                              Poco::Net::HTTPServerResponse& response)
{
    long long productId = extractProductIdFromPath(request.getURI());
    if (productId <= 0)
    {
        sendErrorResponse(response, "Product ID is required", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    std::string authError;
    if (!canDeleteProduct(request, productId, authError))
    {
        sendForbiddenResponse(response, authError);
        return;
    }
    
    long long deletedBy = getCurrentUserId(request);
    
    auto result = productService->deactivateProduct(productId, deletedBy);
    
    std::string ipAddress = getClientIpAddress(request);
    std::string userAgent = getUserAgent(request);
    
    logProductEvent(deletedBy, "SOFT_DELETE_PRODUCT", productId, ipAddress, userAgent, 
                   result.success, result.success ? "Product soft-deleted successfully" : result.message);
    
    if (result.success)
    {
        sendSuccessResponse(response, "Product soft-deleted successfully");
    }
    else
    {
        sendErrorResponse(response, result.message, 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
    }
}

void ProductController::handleActivateProduct(Poco::Net::HTTPServerRequest& request, 
                                            Poco::Net::HTTPServerResponse& response)
{
    long long productId = extractProductIdFromPath(request.getURI());
    if (productId <= 0)
    {
        sendErrorResponse(response, "Product ID is required", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    auto currentUserRole = getCurrentUserRole(request);
    if (currentUserRole != database::models::UserRole::ADMIN && 
        currentUserRole != database::models::UserRole::MANAGER)
    {
        sendForbiddenResponse(response, "Only admin and manager can activate products");
        return;
    }
    
    long long activatedBy = getCurrentUserId(request);
    
    auto result = productService->activateProduct(productId, activatedBy);
    
    std::string ipAddress = getClientIpAddress(request);
    std::string userAgent = getUserAgent(request);
    
    logProductEvent(activatedBy, "ACTIVATE_PRODUCT", productId, ipAddress, userAgent, 
                   result.success, result.success ? "Product activated successfully" : result.message);
    
    if (result.success)
    {
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object(result.data);
        sendSuccessResponse(response, "Product activated successfully", dataPtr);
    }
    else
    {
        sendErrorResponse(response, result.message, 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
    }
}

void ProductController::handleDeactivateProduct(Poco::Net::HTTPServerRequest& request, 
                                              Poco::Net::HTTPServerResponse& response)
{
    long long productId = extractProductIdFromPath(request.getURI());
    if (productId <= 0)
    {
        sendErrorResponse(response, "Product ID is required", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    std::string authError;
    if (!canDeleteProduct(request, productId, authError))
    {
        sendForbiddenResponse(response, authError);
        return;
    }
    
    long long deactivatedBy = getCurrentUserId(request);
    
    auto result = productService->deactivateProduct(productId, deactivatedBy);
    
    std::string ipAddress = getClientIpAddress(request);
    std::string userAgent = getUserAgent(request);
    
    logProductEvent(deactivatedBy, "DEACTIVATE_PRODUCT", productId, ipAddress, userAgent, 
                   result.success, result.success ? "Product deactivated successfully" : result.message);
    
    if (result.success)
    {
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object(result.data);
        sendSuccessResponse(response, "Product deactivated successfully", dataPtr);
    }
    else
    {
        sendErrorResponse(response, result.message, 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
    }
}

void ProductController::handleUpdateStockLevels(Poco::Net::HTTPServerRequest& request, 
                                              Poco::Net::HTTPServerResponse& response)
{
    long long productId = extractProductIdFromPath(request.getURI());
    if (productId <= 0)
    {
        sendErrorResponse(response, "Product ID is required", 
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
    if (!validateStockLevelsData(json, errors))
    {
        sendValidationErrorResponse(response, errors);
        return;
    }
    
    auto currentUserRole = getCurrentUserRole(request);
    if (currentUserRole != database::models::UserRole::ADMIN && 
        currentUserRole != database::models::UserRole::MANAGER)
    {
        sendForbiddenResponse(response, "Only admin and manager can update stock levels");
        return;
    }
    
    int minStockLevel = utils::JsonUtils::getInt(*json, "min_stock_level");
    int maxStockLevel = utils::JsonUtils::getInt(*json, "max_stock_level");
    
    long long updatedBy = getCurrentUserId(request);
    
    auto result = productService->updateStockLevels(productId, minStockLevel, maxStockLevel, updatedBy);
    
    std::string ipAddress = getClientIpAddress(request);
    std::string userAgent = getUserAgent(request);
    
    logProductEvent(updatedBy, "UPDATE_STOCK_LEVELS", productId, ipAddress, userAgent, 
                   result.success, result.success ? "Stock levels updated successfully" : result.message);
    
    if (result.success)
    {
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object(result.data);
        sendSuccessResponse(response, "Stock levels updated successfully", dataPtr);
    }
    else
    {
        sendErrorResponse(response, result.message, 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
    }
}

void ProductController::handleUpdatePrice(Poco::Net::HTTPServerRequest& request, 
                                        Poco::Net::HTTPServerResponse& response)
{
    long long productId = extractProductIdFromPath(request.getURI());
    if (productId <= 0)
    {
        sendErrorResponse(response, "Product ID is required", 
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
    if (!validatePriceUpdateData(json, errors))
    {
        sendValidationErrorResponse(response, errors);
        return;
    }
    
    auto currentUserRole = getCurrentUserRole(request);
    if (currentUserRole != database::models::UserRole::ADMIN && 
        currentUserRole != database::models::UserRole::MANAGER)
    {
        sendForbiddenResponse(response, "Only admin and manager can update prices");
        return;
    }
    
    double newPrice = utils::JsonUtils::getDouble(*json, "new_price");
    
    long long updatedBy = getCurrentUserId(request);
    
    auto result = productService->updatePrice(productId, newPrice, updatedBy);
    
    std::string ipAddress = getClientIpAddress(request);
    std::string userAgent = getUserAgent(request);
    
    logProductEvent(updatedBy, "UPDATE_PRICE", productId, ipAddress, userAgent, 
                   result.success, result.success ? "Price updated successfully" : result.message);
    
    if (result.success)
    {
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object(result.data);
        sendSuccessResponse(response, "Price updated successfully", dataPtr);
    }
    else
    {
        sendErrorResponse(response, result.message, 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
    }
}

void ProductController::handleSearchProducts(Poco::Net::HTTPServerRequest& request, 
                                           Poco::Net::HTTPServerResponse& response)
{
    std::string query = getQueryParameter(request.getURI(), "q");
    
    int page, pageSize;
    if (!getPaginationParameters(request, page, pageSize, 20))
    {
        sendErrorResponse(response, "Invalid pagination parameters", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    auto filters = getFilterParameters(request);
    
    if (filters.find("is_active") == filters.end())
    {
        filters["is_active"] = "true";
    }
    
    auto currentUserRole = getCurrentUserRole(request);
    
    if (currentUserRole != database::models::UserRole::ADMIN && 
        currentUserRole != database::models::UserRole::MANAGER)
    {
        filters["is_active"] = "true";
    }
    
    std::vector<std::string> errors;
    if (!validateSearchParameters(filters, errors))
    {
        sendValidationErrorResponse(response, errors);
        return;
    }
    
    Poco::JSON::Array productsArray = productService->searchProducts(query, page, pageSize);
    
    Poco::JSON::Object::Ptr responseObj = new Poco::JSON::Object;
    responseObj->set("products", productsArray);
    responseObj->set("page", page);
    responseObj->set("pageSize", pageSize);
    
    sendSuccessResponse(response, "Products search completed", responseObj);
}

void ProductController::handleGetProductsByCategory(Poco::Net::HTTPServerRequest& request, 
                                                  Poco::Net::HTTPServerResponse& response)
{
    long long categoryId = extractCategoryIdFromQuery(request.getURI());
    if (categoryId <= 0)
    {
        sendErrorResponse(response, "Category ID is required", 
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
    
    auto currentUserRole = getCurrentUserRole(request);
    
    Poco::JSON::Array productsArray = productService->getProductsByCategory(categoryId, page, pageSize);
    
    Poco::JSON::Object::Ptr responseObj = new Poco::JSON::Object;
    responseObj->set("products", productsArray);
    responseObj->set("category_id", categoryId);
    responseObj->set("page", page);
    responseObj->set("pageSize", pageSize);
    
    sendSuccessResponse(response, "Products by category retrieved", responseObj);
}

void ProductController::handleGetProductsBySupplier(Poco::Net::HTTPServerRequest& request, 
                                                  Poco::Net::HTTPServerResponse& response)
{
    long long supplierId = extractSupplierIdFromQuery(request.getURI());
    if (supplierId <= 0)
    {
        sendErrorResponse(response, "Supplier ID is required", 
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
    
    auto currentUserRole = getCurrentUserRole(request);
    
    std::string query = "";
    Poco::JSON::Array productsArray = productService->searchProducts(query, page, pageSize);
    
    Poco::JSON::Object::Ptr responseObj = new Poco::JSON::Object;
    responseObj->set("products", productsArray);
    responseObj->set("supplier_id", supplierId);
    responseObj->set("page", page);
    responseObj->set("pageSize", pageSize);
    
    sendSuccessResponse(response, "Products by supplier retrieved", responseObj);
}

void ProductController::handleGetLowStockProducts(Poco::Net::HTTPServerRequest& request, 
                                                Poco::Net::HTTPServerResponse& response)
{
    std::string thresholdStr = getQueryParameter(request.getURI(), "threshold");
    int threshold = 10;
    if (!thresholdStr.empty())
    {
        try
        {
            threshold = std::stoi(thresholdStr);
        }
        catch (...)
        {
            threshold = 10;
        }
    }
    
    auto currentUserRole = getCurrentUserRole(request);
    if (currentUserRole != database::models::UserRole::ADMIN && 
        currentUserRole != database::models::UserRole::MANAGER &&
        currentUserRole != database::models::UserRole::WORKER)
    {
        sendForbiddenResponse(response, "Only admin, manager, and workers can view low stock products");
        return;
    }
    
    Poco::JSON::Array productsArray = productService->getLowStockProducts(threshold);
    
    Poco::JSON::Object::Ptr responseObj = new Poco::JSON::Object;
    responseObj->set("lowStockProducts", productsArray);
    responseObj->set("threshold", threshold);
    
    sendSuccessResponse(response, "Low stock products retrieved", responseObj);
}

void ProductController::handleGetProductsNeedingReorder(Poco::Net::HTTPServerRequest& request, 
                                                      Poco::Net::HTTPServerResponse& response)
{
    auto currentUserRole = getCurrentUserRole(request);
    if (currentUserRole != database::models::UserRole::ADMIN && 
        currentUserRole != database::models::UserRole::MANAGER)
    {
        sendForbiddenResponse(response, "Only admin and manager can view products needing reorder");
        return;
    }
    
    Poco::JSON::Array productsArray = productService->getProductsNeedingReorder();
    
    Poco::JSON::Object::Ptr responseObj = new Poco::JSON::Object;
    responseObj->set("productsNeedingReorder", productsArray);
    
    sendSuccessResponse(response, "Products needing reorder retrieved", responseObj);
}

void ProductController::handleGetProductStockSummary(Poco::Net::HTTPServerRequest& request, 
                                                   Poco::Net::HTTPServerResponse& response)
{
    long long productId = extractProductIdFromPath(request.getURI());
    if (productId <= 0)
    {
        sendErrorResponse(response, "Product ID is required", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    std::string accessError;
    if (!canViewProduct(request, productId, accessError))
    {
        sendForbiddenResponse(response, accessError);
        return;
    }
    
    Poco::JSON::Object stockSummary = productService->getProductStockSummary(productId);
    
    Poco::JSON::Object::Ptr responseObj = new Poco::JSON::Object(stockSummary);
    
    sendSuccessResponse(response, "Product stock summary retrieved", responseObj);
}

void ProductController::handleGetProductStatistics(Poco::Net::HTTPServerRequest& request, 
                                                 Poco::Net::HTTPServerResponse& response)
{
    auto currentUserRole = getCurrentUserRole(request);
    if (currentUserRole != database::models::UserRole::ADMIN && 
        currentUserRole != database::models::UserRole::MANAGER)
    {
        sendForbiddenResponse(response, "Only admin and manager can view product statistics");
        return;
    }
    
    Poco::JSON::Array statistics = productService->getProductStatistics();
    
    Poco::JSON::Object::Ptr responseObj = new Poco::JSON::Object;
    responseObj->set("statistics", statistics);
    
    sendSuccessResponse(response, "Product statistics retrieved", responseObj);
}

void ProductController::handleGetStockReport(Poco::Net::HTTPServerRequest& request, 
                                           Poco::Net::HTTPServerResponse& response)
{
    auto currentUserRole = getCurrentUserRole(request);
    if (currentUserRole != database::models::UserRole::ADMIN && 
        currentUserRole != database::models::UserRole::MANAGER)
    {
        sendForbiddenResponse(response, "Only admin and manager can view stock report");
        return;
    }
    
    
    Poco::JSON::Array statistics = productService->getProductStatistics();
    
    Poco::JSON::Object::Ptr responseObj = new Poco::JSON::Object;
    responseObj->set("stockReport", statistics);
    
    sendSuccessResponse(response, "Stock report generated", responseObj);
}

void ProductController::handleImportProducts(Poco::Net::HTTPServerRequest& request, 
                                           Poco::Net::HTTPServerResponse& response)
{
    auto currentUserRole = getCurrentUserRole(request);
    if (currentUserRole != database::models::UserRole::ADMIN)
    {
        sendForbiddenResponse(response, "Only admin can import products");
        return;
    }
    
    auto json = parseJsonBody(request);
    if (!json)
    {
        sendErrorResponse(response, "Invalid JSON body", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    if (!json->has("products"))
    {
        sendErrorResponse(response, "Missing 'products' array in request body", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    auto productsArray = json->getArray("products");
    if (!productsArray || productsArray->size() == 0)
    {
        sendErrorResponse(response, "Empty products array", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    long long importedBy = getCurrentUserId(request);
    
    
    int successCount = 0;
    int failureCount = 0;
    Poco::JSON::Array failures;
    
    for (size_t i = 0; i < productsArray->size(); ++i)
    {
        try
        {
            auto productJson = productsArray->getObject(i);
            if (productJson)
            {
                database::models::Product product(*productJson);
                std::string errorMessage;
                if (productService->validateProductData(product, errorMessage))
                {
                    if (productService->isSkuAvailable(product.sku))
                    {
                        auto result = productService->createProduct(product, importedBy);
                        if (result.success)
                        {
                            successCount++;
                        }
                        else
                        {
                            failureCount++;
                            Poco::JSON::Object::Ptr failure = new Poco::JSON::Object;
                            failure->set("index", static_cast<int>(i));
                            failure->set("sku", product.sku);
                            failure->set("error", result.message);
                            failures.add(failure);
                        }
                    }
                    else
                    {
                        failureCount++;
                        Poco::JSON::Object::Ptr failure = new Poco::JSON::Object;
                        failure->set("index", static_cast<int>(i));
                        failure->set("sku", product.sku);
                        failure->set("error", "SKU already exists");
                        failures.add(failure);
                    }
                }
                else
                {
                    failureCount++;
                    Poco::JSON::Object::Ptr failure = new Poco::JSON::Object;
                    failure->set("index", static_cast<int>(i));
                    failure->set("sku", product.sku);
                    failure->set("error", errorMessage);
                    failures.add(failure);
                }
            }
        }
        catch (...)
        {
            failureCount++;
            Poco::JSON::Object::Ptr failure = new Poco::JSON::Object;
            failure->set("index", static_cast<int>(i));
            failure->set("error", "Invalid product data");
            failures.add(failure);
        }
    }
    
    Poco::JSON::Object::Ptr responseObj = new Poco::JSON::Object;
    responseObj->set("successCount", successCount);
    responseObj->set("failureCount", failureCount);
    responseObj->set("failures", failures);
    
    std::string ipAddress = getClientIpAddress(request);
    std::string userAgent = getUserAgent(request);
    
    logProductEvent(importedBy, "IMPORT_PRODUCTS", 0, ipAddress, userAgent, 
                   true, "Imported " + std::to_string(successCount) + " products, " + 
                   std::to_string(failureCount) + " failed");
    
    sendSuccessResponse(response, "Products import completed", responseObj);
}

void ProductController::handleExportProducts(Poco::Net::HTTPServerRequest& request, 
                                           Poco::Net::HTTPServerResponse& response)
{
    auto currentUserRole = getCurrentUserRole(request);
    if (currentUserRole != database::models::UserRole::ADMIN && 
        currentUserRole != database::models::UserRole::MANAGER)
    {
        sendForbiddenResponse(response, "Only admin and manager can export products");
        return;
    }
    
    auto filters = getFilterParameters(request);
    
    if (filters.find("is_active") == filters.end())
    {
        filters["is_active"] = "true";
    }
    
    Poco::JSON::Array productsArray = productService->searchProducts("", 1, 1000);
    
    Poco::JSON::Object::Ptr responseObj = new Poco::JSON::Object;
    responseObj->set("products", productsArray);
    responseObj->set("exportDate", getCurrentTimestamp());
    responseObj->set("totalProducts", productsArray.size());
    
    long long exportedBy = getCurrentUserId(request);
    
    std::string ipAddress = getClientIpAddress(request);
    std::string userAgent = getUserAgent(request);
    
    logProductEvent(exportedBy, "EXPORT_PRODUCTS", 0, ipAddress, userAgent, 
                   true, "Exported " + std::to_string(productsArray.size()) + " products");
    
    sendSuccessResponse(response, "Products export completed", responseObj);
}

bool ProductController::validateRequest(Poco::Net::HTTPServerRequest& request, 
                                       Poco::Net::HTTPServerResponse& response,
                                       std::string& errorMessage)
{
    if (!BaseController::validateRequest(request, response, errorMessage))
    {
        return false;
    }
    
    return true;
}

bool ProductController::authorizeRequest(Poco::Net::HTTPServerRequest& request, 
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

bool ProductController::validateProductAccess(Poco::Net::HTTPServerRequest& request, 
                                            long long targetProductId,
                                            std::string& errorMessage)
{
    return canViewProduct(request, targetProductId, errorMessage);
}

long long ProductController::getCurrentUserId(Poco::Net::HTTPServerRequest& request)
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

database::models::UserRole ProductController::getCurrentUserRole(Poco::Net::HTTPServerRequest& request)
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

bool ProductController::validateCreateProductData(const Poco::JSON::Object::Ptr& json, 
                                                std::vector<std::string>& errors)
{
    errors.clear();
    
    std::vector<std::string> requiredFields = {"sku", "name", "category_id", "supplier_id", "unit_price"};
    std::vector<std::string> missingFields;
    if (!validateRequiredFields(json, requiredFields, missingFields))
    {
        for (const auto& field : missingFields)
        {
            errors.push_back(field + " is required");
        }
    }
    
    if (json->has("sku"))
    {
        std::string sku = utils::JsonUtils::getString(*json, "sku");
        if (sku.empty() || sku.length() > database::models::Product::MAX_SKU_LENGTH)
        {
            errors.push_back("SKU must be between 1 and " + 
                           std::to_string(database::models::Product::MAX_SKU_LENGTH) + " characters");
        }
    }
    
    if (json->has("name"))
    {
        std::string name = utils::JsonUtils::getString(*json, "name");
        if (name.empty() || name.length() > database::models::Product::MAX_NAME_LENGTH)
        {
            errors.push_back("Name must be between 1 and " + 
                           std::to_string(database::models::Product::MAX_NAME_LENGTH) + " characters");
        }
    }
    
    if (json->has("unit_price"))
    {
        double price = utils::JsonUtils::getDouble(*json, "unit_price");
        if (price < database::models::Product::MIN_PRICE)
        {
            errors.push_back("Price must be greater than or equal to " + 
                           std::to_string(database::models::Product::MIN_PRICE));
        }
    }
    
    if (json->has("weight"))
    {
        double weight = utils::JsonUtils::getDouble(*json, "weight");
        if (weight < database::models::Product::MIN_WEIGHT)
        {
            errors.push_back("Weight must be greater than or equal to " + 
                           std::to_string(database::models::Product::MIN_WEIGHT));
        }
    }
    
    if (json->has("min_stock_level"))
    {
        int minStockLevel = utils::JsonUtils::getInt(*json, "min_stock_level");
        if (minStockLevel < database::models::Product::MIN_STOCK_LEVEL)
        {
            errors.push_back("Minimum stock level must be greater than or equal to " + 
                           std::to_string(database::models::Product::MIN_STOCK_LEVEL));
        }
    }
    
    if (json->has("max_stock_level"))
    {
        int maxStockLevel = utils::JsonUtils::getInt(*json, "max_stock_level");
        if (maxStockLevel < database::models::Product::MIN_STOCK_LEVEL)
        {
            errors.push_back("Maximum stock level must be greater than or equal to " + 
                           std::to_string(database::models::Product::MIN_STOCK_LEVEL));
        }
    }
    
    if (json->has("category_id"))
    {
        long long categoryId = utils::JsonUtils::getInt(*json, "category_id");
        if (categoryId <= 0)
        {
            errors.push_back("Invalid category ID");
        }
    }
    
    if (json->has("supplier_id"))
    {
        long long supplierId = utils::JsonUtils::getInt(*json, "supplier_id");
        if (supplierId <= 0)
        {
            errors.push_back("Invalid supplier ID");
        }
    }
    
    return errors.empty();
}

bool ProductController::validateUpdateProductData(const Poco::JSON::Object::Ptr& json, 
                                                std::vector<std::string>& errors)
{
    errors.clear();
    
    bool hasSku = json->has("sku") && !json->get("sku").isEmpty();
    bool hasName = json->has("name") && !json->get("name").isEmpty();
    bool hasDescription = json->has("description") && !json->get("description").isEmpty();
    bool hasCategoryId = json->has("category_id") && !json->get("category_id").isEmpty();
    bool hasSupplierId = json->has("supplier_id") && !json->get("supplier_id").isEmpty();
    bool hasUnitPrice = json->has("unit_price") && !json->get("unit_price").isEmpty();
    bool hasWeight = json->has("weight") && !json->get("weight").isEmpty();
    bool hasDimensions = json->has("dimensions") && !json->get("dimensions").isEmpty();
    
    if (!hasSku && !hasName && !hasDescription && !hasCategoryId && 
        !hasSupplierId && !hasUnitPrice && !hasWeight && !hasDimensions)
    {
        errors.push_back("At least one field must be provided for update");
    }
    
    if (hasSku)
    {
        std::string sku = utils::JsonUtils::getString(*json, "sku");
        if (sku.empty() || sku.length() > database::models::Product::MAX_SKU_LENGTH)
        {
            errors.push_back("SKU must be between 1 and " + 
                           std::to_string(database::models::Product::MAX_SKU_LENGTH) + " characters");
        }
    }
    
    if (hasName)
    {
        std::string name = utils::JsonUtils::getString(*json, "name");
        if (name.empty() || name.length() > database::models::Product::MAX_NAME_LENGTH)
        {
            errors.push_back("Name must be between 1 and " + 
                           std::to_string(database::models::Product::MAX_NAME_LENGTH) + " characters");
        }
    }
    
    if (hasUnitPrice)
    {
        double price = utils::JsonUtils::getDouble(*json, "unit_price");
        if (price < database::models::Product::MIN_PRICE)
        {
            errors.push_back("Price must be greater than or equal to " + 
                           std::to_string(database::models::Product::MIN_PRICE));
        }
    }
    
    if (hasWeight)
    {
        double weight = utils::JsonUtils::getDouble(*json, "weight");
        if (weight < database::models::Product::MIN_WEIGHT)
        {
            errors.push_back("Weight must be greater than or equal to " + 
                           std::to_string(database::models::Product::MIN_WEIGHT));
        }
    }
    
    return errors.empty();
}

bool ProductController::validateStockLevelsData(const Poco::JSON::Object::Ptr& json, 
                                               std::vector<std::string>& errors)
{
    errors.clear();
    
    std::vector<std::string> requiredFields = {"min_stock_level", "max_stock_level"};
    std::vector<std::string> missingFields;
    if (!validateRequiredFields(json, requiredFields, missingFields))
    {
        for (const auto& field : missingFields)
        {
            errors.push_back(field + " is required");
        }
    }
    
    if (json->has("min_stock_level"))
    {
        int minStockLevel = utils::JsonUtils::getInt(*json, "min_stock_level");
        if (minStockLevel < database::models::Product::MIN_STOCK_LEVEL)
        {
            errors.push_back("Minimum stock level must be greater than or equal to " + 
                           std::to_string(database::models::Product::MIN_STOCK_LEVEL));
        }
    }
    
    if (json->has("max_stock_level"))
    {
        int maxStockLevel = utils::JsonUtils::getInt(*json, "max_stock_level");
        if (maxStockLevel < database::models::Product::MIN_STOCK_LEVEL)
        {
            errors.push_back("Maximum stock level must be greater than or equal to " + 
                           std::to_string(database::models::Product::MIN_STOCK_LEVEL));
        }
    }
    
    if (json->has("min_stock_level") && json->has("max_stock_level"))
    {
        int minStockLevel = utils::JsonUtils::getInt(*json, "min_stock_level");
        int maxStockLevel = utils::JsonUtils::getInt(*json, "max_stock_level");
        if (maxStockLevel < minStockLevel)
        {
            errors.push_back("Maximum stock level must be greater than or equal to minimum stock level");
        }
    }
    
    return errors.empty();
}

bool ProductController::validatePriceUpdateData(const Poco::JSON::Object::Ptr& json, 
                                              std::vector<std::string>& errors)
{
    errors.clear();
    
    std::vector<std::string> requiredFields = {"new_price"};
    std::vector<std::string> missingFields;
    if (!validateRequiredFields(json, requiredFields, missingFields))
    {
        for (const auto& field : missingFields)
        {
            errors.push_back(field + " is required");
        }
    }
    
    if (json->has("new_price"))
    {
        double price = utils::JsonUtils::getDouble(*json, "new_price");
        if (price < database::models::Product::MIN_PRICE)
        {
            errors.push_back("Price must be greater than or equal to " + 
                           std::to_string(database::models::Product::MIN_PRICE));
        }
    }
    
    return errors.empty();
}

bool ProductController::validateSearchParameters(const std::map<std::string, std::string>& filters,
                                                std::vector<std::string>& errors)
{
    errors.clear();
    
    for (const auto& [key, value] : filters)
    {
        if (key == "is_active")
        {
            if (value != "true" && value != "false")
            {
                errors.push_back("isActive must be 'true' or 'false'");
            }
        }
        else if (key == "category_id")
        {
            try
            {
                long long categoryId = std::stoll(value);
                if (categoryId <= 0)
                {
                    errors.push_back("categoryId must be a positive integer");
                }
            }
            catch (...)
            {
                errors.push_back("Invalid categoryId value");
            }
        }
        else if (key == "supplier_id")
        {
            try
            {
                long long supplierId = std::stoll(value);
                if (supplierId <= 0)
                {
                    errors.push_back("supplierId must be a positive integer");
                }
            }
            catch (...)
            {
                errors.push_back("Invalid supplierId value");
            }
        }
        else if (key == "min_price" || key == "max_price")
        {
            try
            {
                double price = std::stod(value);
                if (price < 0)
                {
                    errors.push_back(key + " must be a non-negative number");
                }
            }
            catch (...)
            {
                errors.push_back("Invalid " + key + " value");
            }
        }
    }
    
    return errors.empty();
}

void ProductController::logProductEvent(long long userId, 
                                      const std::string& action,
                                      long long targetProductId,
                                      const std::string& ipAddress,
                                      const std::string& userAgent,
                                      bool success,
                                      const std::string& details)
{
    std::cout << "[" << getCurrentTimestamp() << "] "
              << "PRODUCT " << action << " "
              << "UserID: " << userId << " "
              << "ProductID: " << targetProductId << " "
              << "IP: " << ipAddress << " "
              << "Success: " << (success ? "Yes" : "No") << " "
              << "Details: " << details << std::endl;
}

std::unique_ptr<database::models::Product> ProductController::extractProductFromJson(const Poco::JSON::Object::Ptr& json)
{
    if (!json)
    {
        return nullptr;
    }
    
    try
    {
        auto product = std::make_unique<database::models::Product>(*json);
        return product;
    }
    catch (...)
    {
        return nullptr;
    }
}

bool ProductController::canCreateProduct(Poco::Net::HTTPServerRequest& request, 
                                        const database::models::Product& productData,
                                        std::string& errorMessage)
{
    auto currentUserRole = getCurrentUserRole(request);
    
    if (currentUserRole == database::models::UserRole::ADMIN || 
        currentUserRole == database::models::UserRole::MANAGER)
    {
        return true;
    }
    
    errorMessage = "Only admin and manager can create products";
    return false;
}

bool ProductController::canUpdateProduct(Poco::Net::HTTPServerRequest& request, 
                                        long long targetProductId,
                                        const database::models::Product& productData,
                                        std::string& errorMessage)
{
    auto currentUserRole = getCurrentUserRole(request);
    
    if (currentUserRole == database::models::UserRole::ADMIN || 
        currentUserRole == database::models::UserRole::MANAGER)
    {
        return true;
    }
    
    errorMessage = "Only admin and manager can update products";
    return false;
}

bool ProductController::canDeleteProduct(Poco::Net::HTTPServerRequest& request, 
                                        long long targetProductId,
                                        std::string& errorMessage)
{
    auto currentUserRole = getCurrentUserRole(request);
    
    if (currentUserRole == database::models::UserRole::ADMIN || 
        currentUserRole == database::models::UserRole::MANAGER)
    {
        return true;
    }
    
    errorMessage = "Only admin and manager can delete products";
    return false;
}

bool ProductController::canViewProduct(Poco::Net::HTTPServerRequest& request, 
                                      long long targetProductId,
                                      std::string& errorMessage)
{
    auto currentUserRole = getCurrentUserRole(request);
    
    if (currentUserRole == database::models::UserRole::ADMIN || 
        currentUserRole == database::models::UserRole::MANAGER)
    {
        return true;
    }
    
    auto productResult = productService->getProductById(targetProductId);
    if (productResult.success)
    {
        if (productResult.data.has("is_active"))
        {
            bool isActive = productResult.data.getValue<bool>("is_active");
            if (!isActive)
            {
                errorMessage = "Product is not active";
                return false;
            }
        }
        return true;
    }
    
    errorMessage = "Product not found";
    return false;
}

Poco::JSON::Object ProductController::buildPaginationResponse(int page, int pageSize, int totalItems, 
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

long long ProductController::extractProductIdFromPath(const std::string& uri)
{
    std::string productIdStr = getPathParameter(uri, "/api/v1/products/", 0);
    if (productIdStr.empty())
    {
        productIdStr = getPathParameter(uri, "/api/products/", 0);
    }
    
    size_t slashPos = productIdStr.find('/');
    if (slashPos != std::string::npos)
    {
        productIdStr = productIdStr.substr(0, slashPos);
    }
    
    try
    {
        return std::stoll(productIdStr);
    }
    catch (...)
    {
        return 0;
    }
}

long long ProductController::extractCategoryIdFromQuery(const std::string& uri)
{
    std::string categoryIdStr = getQueryParameter(uri, "category_id");
    if (!categoryIdStr.empty())
    {
        try
        {
            return std::stoll(categoryIdStr);
        }
        catch (...)
        {
            return 0;
        }
    }
    return 0;
}

long long ProductController::extractSupplierIdFromQuery(const std::string& uri)
{
    std::string supplierIdStr = getQueryParameter(uri, "supplier_id");
    if (!supplierIdStr.empty())
    {
        try
        {
            return std::stoll(supplierIdStr);
        }
        catch (...)
        {
            return 0;
        }
    }
    return 0;
}

} // namespace controllers
