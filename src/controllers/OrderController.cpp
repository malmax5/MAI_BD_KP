#include "OrderController.hpp"
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

OrderController::OrderController()
    : orderService(std::make_unique<services::OrderService>()),
      authService(std::make_unique<services::AuthService>())
{
}

void OrderController::handleRequest(Poco::Net::HTTPServerRequest& request, 
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
        
        std::string basePath = "/api/v1/orders";
        if (endpoint.find("/api/v1/") != 0)
        {
            basePath = "/api/orders";
        }
        
        std::string orderIdStr = getPathParameter(uri, basePath + "/", 0);
        
        if ((endpoint == "/api/v1/orders/statistics" || endpoint == "/api/orders/statistics") && 
            request.getMethod() == "GET")
        {
            handleGetOrderStatistics(request, response);
        }
        else if ((endpoint.find("/api/v1/orders/revenue-report") == 0 || endpoint.find("/api/orders/revenue-report") == 0) && 
                 request.getMethod() == "GET")
        {
            handleGetRevenueReport(request, response);
        }
        else if ((endpoint.find("/api/v1/orders/customer-history") == 0 || 
                  endpoint.find("/api/orders/customer-history") == 0) && 
                 request.getMethod() == "GET")
        {
            handleGetCustomerOrderHistory(request, response);
        }
        else if ((endpoint.find("/api/v1/orders/pending") == 0 || 
                  endpoint.find("/api/orders/pending") == 0) && 
                 request.getMethod() == "GET")
        {
            handleGetPendingOrders(request, response);
        }
        else if ((endpoint.find("/api/v1/orders/urgent") == 0 || 
                  endpoint.find("/api/orders/urgent") == 0) && 
                 request.getMethod() == "GET")
        {
            handleGetUrgentOrders(request, response);
        }
        else if ((endpoint.find("/api/v1/orders/customer") == 0 || 
                  endpoint.find("/api/orders/customer") == 0) && 
                 request.getMethod() == "GET")
        {
            handleGetOrdersByCustomer(request, response);
        }
        else if ((endpoint.find("/api/v1/orders/status") == 0 || 
                  endpoint.find("/api/orders/status") == 0) && 
                 request.getMethod() == "GET")
        {
            handleGetOrdersByStatus(request, response);
        }
        else if ((endpoint.find("/api/v1/orders/search") == 0 || 
                  endpoint.find("/api/orders/search") == 0) && 
                 request.getMethod() == "GET")
        {
            handleSearchOrders(request, response);
        }
        else if ((endpoint == "/api/v1/orders" || endpoint == "/api/orders") && 
                 request.getMethod() == "GET")
        {
            handleGetOrders(request, response);
        }
        else if ((endpoint == "/api/v1/orders" || endpoint == "/api/orders") && 
                 request.getMethod() == "POST")
        {
            handleCreateOrder(request, response);
        }
        else if ((endpoint.find("/api/v1/orders/") == 0 || endpoint.find("/api/orders/") == 0) && 
                 !orderIdStr.empty())
        {
            if (endpoint.find("/items") != std::string::npos)
            {
                std::string itemIdStr = getPathParameter(uri, basePath + "/" + orderIdStr + "/items/", 0);
                
                if (!itemIdStr.empty() && request.getMethod() == "PUT")
                {
                    handleUpdateOrderItem(request, response);
                }
                else if (!itemIdStr.empty() && request.getMethod() == "DELETE")
                {
                    handleRemoveOrderItem(request, response);
                }
                else if (request.getMethod() == "GET")
                {
                    handleGetOrderItems(request, response);
                }
                else if (request.getMethod() == "POST")
                {
                    handleAddOrderItem(request, response);
                }
            }
            else if (endpoint.find("/cancel") != std::string::npos && request.getMethod() == "POST")
            {
                handleCancelOrder(request, response);
            }
            else if (endpoint.find("/status") != std::string::npos && request.getMethod() == "PUT")
            {
                handleUpdateOrderStatus(request, response);
            }
            else if (endpoint.find("/priority") != std::string::npos && request.getMethod() == "PUT")
            {
                handleUpdateOrderPriority(request, response);
            }
            else if (endpoint.find("/ship") != std::string::npos && request.getMethod() == "POST")
            {
                handleMarkOrderAsShipped(request, response);
            }
            else if (endpoint.find("/deliver") != std::string::npos && request.getMethod() == "POST")
            {
                handleMarkOrderAsDelivered(request, response);
            }
            else if (request.getMethod() == "GET")
            {
                handleGetOrderById(request, response);
            }
            else if (request.getMethod() == "PUT")
            {
                sendErrorResponse(response, "Method not supported. Use specific endpoints for updating order status or priority.", 
                                Poco::Net::HTTPResponse::HTTP_METHOD_NOT_ALLOWED);
            }
            else if (request.getMethod() == "DELETE")
            {
                handleDeleteOrder(request, response);
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

void OrderController::handleGetOrders(Poco::Net::HTTPServerRequest& request, 
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
    
    long long currentUserId = getCurrentUserId(request);
    auto currentUserRole = getCurrentUserRole(request);
    
    Poco::JSON::Array ordersArray;
    
    try
    {
        if (filters.find("customerEmail") != filters.end())
        {
            std::string customerEmail = filters["customerEmail"];
            ordersArray = orderService->findOrdersByCustomer(customerEmail, page, pageSize);
        }
        else if (filters.find("status") != filters.end())
        {
            std::string status = filters["status"];
            ordersArray = orderService->findOrdersByStatus(status, page, pageSize);
        }
        else if (filters.find("startDate") != filters.end() && filters.find("endDate") != filters.end())
        {
            std::string startDate = filters["startDate"];
            std::string endDate = filters["endDate"];
            ordersArray = orderService->findOrdersByDateRange(startDate, endDate, page, pageSize);
        }
        else if (filters.find("priority") != filters.end() && filters["priority"] == "high")
        {
            ordersArray = orderService->getUrgentOrders(page, pageSize);
        }
        else
        {
            if (currentUserRole != database::models::UserRole::ADMIN && 
                currentUserRole != database::models::UserRole::MANAGER)
            {
                ordersArray = orderService->getPendingOrders(page, pageSize);
            }
            else
            {
                auto now = utils::DateUtils::now();
                std::string defaultStartDate = Poco::DateTimeFormatter::format(now, "%Y-01-01");
                std::string defaultEndDate = Poco::DateTimeFormatter::format(now, "%Y-%m-%d");
                ordersArray = orderService->findOrdersByDateRange(defaultStartDate, defaultEndDate, page, pageSize);
            }
        }
        
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object;
        dataPtr->set("orders", ordersArray);
        sendSuccessResponse(response, "Orders retrieved successfully", dataPtr);
    }
    catch (const std::exception& e)
    {
        sendErrorResponse(response, "Error retrieving orders: " + std::string(e.what()), 
                        Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
}

void OrderController::handleGetOrderById(Poco::Net::HTTPServerRequest& request, 
                                        Poco::Net::HTTPServerResponse& response)
{
    std::string orderIdStr = getPathParameter(request.getURI(), "/api/v1/orders/", 0);
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
    
    std::string accessError;
    if (!canViewOrder(request, orderId, accessError))
    {
        sendForbiddenResponse(response, accessError);
        return;
    }
    
    Poco::JSON::Object orderDetails = orderService->getOrderWithItems(orderId);
    
    if (orderDetails.has("error"))
    {
        std::string error = orderDetails.getValue<std::string>("error");
        if (error.find("Order not found") != std::string::npos)
        {
            sendNotFoundResponse(response, "Order");
        }
        else
        {
            sendErrorResponse(response, error, Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
        }
        return;
    }
    
    Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object(orderDetails);
    sendSuccessResponse(response, "Order retrieved successfully", dataPtr);
}

void OrderController::handleCreateOrder(Poco::Net::HTTPServerRequest& request, 
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
    if (!validateCreateOrderData(json, errors))
    {
        sendValidationErrorResponse(response, errors);
        return;
    }
    
    auto orderData = extractOrderFromJson(json);
    if (!orderData)
    {
        sendErrorResponse(response, "Invalid order data", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    std::vector<database::models::OrderItem> orderItems;
    if (json->has("items"))
    {
        auto itemsArray = json->getArray("items");
        if (itemsArray)
        {
            for (size_t i = 0; i < itemsArray->size(); ++i)
            {
                try
                {
                    auto itemJson = itemsArray->getObject(i);
                    if (itemJson)
                    {
                        database::models::OrderItem item(*itemJson);
                        orderItems.push_back(item);
                    }
                }
                catch (...)
                {
                    sendErrorResponse(response, "Invalid order item at index " + std::to_string(i), 
                                    Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
                    return;
                }
            }
        }
    }
    
    std::string authError;
    if (!canCreateOrder(request, *orderData, authError))
    {
        sendForbiddenResponse(response, authError);
        return;
    }
    
    std::string itemsError;
    if (!orderService->validateOrderItems(orderItems, itemsError))
    {
        sendErrorResponse(response, itemsError, 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    long long createdBy = getCurrentUserId(request);
    
    auto result = orderService->createOrder(*orderData, orderItems, createdBy);
    
    logOrderEvent(createdBy, "CREATE_ORDER", result.orderId, 
                 getClientIpAddress(request), getUserAgent(request), 
                 result.success, result.success ? "Order created successfully" : result.message);
    
    if (result.success)
    {
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object(result.orderDetails);
        sendSuccessResponse(response, "Order created successfully", dataPtr);
    }
    else
    {
        sendErrorResponse(response, result.message, 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
    }
}

void OrderController::handleUpdateOrder(Poco::Net::HTTPServerRequest& request, 
                                       Poco::Net::HTTPServerResponse& response)
{
    sendErrorResponse(response, "Method not supported. Use specific endpoints for updating order status or priority.", 
                    Poco::Net::HTTPResponse::HTTP_METHOD_NOT_ALLOWED);
}

void OrderController::handleDeleteOrder(Poco::Net::HTTPServerRequest& request, 
                                       Poco::Net::HTTPServerResponse& response)
{
    std::string orderIdStr = getPathParameter(request.getURI(), "/api/v1/orders/", 0);
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
    if (!canDeleteOrder(request, orderId, authError))
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
    
    bool success = orderService->cancelOrder(orderId, deletedBy, reason);
    
    logOrderEvent(deletedBy, "DELETE_ORDER", orderId, 
                 getClientIpAddress(request), getUserAgent(request), 
                 success, success ? "Order cancelled/deleted successfully" : "Failed to cancel/delete order");
    
    if (success)
    {
        sendSuccessResponse(response, "Order cancelled/deleted successfully");
    }
    else
    {
        sendErrorResponse(response, "Failed to cancel/delete order. Order may already be shipped.", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
    }
}

void OrderController::handleCancelOrder(Poco::Net::HTTPServerRequest& request, 
                                       Poco::Net::HTTPServerResponse& response)
{
    std::string orderIdStr = getPathParameter(request.getURI(), "/api/v1/orders/", 0);
    orderIdStr = orderIdStr.substr(0, orderIdStr.find("/cancel"));
    
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
    if (!canCancelOrder(request, orderId, authError))
    {
        sendForbiddenResponse(response, authError);
        return;
    }
    
    long long cancelledBy = getCurrentUserId(request);
    
    std::string reason;
    auto json = parseJsonBody(request);
    if (json && json->has("reason"))
    {
        reason = utils::JsonUtils::getString(*json, "reason");
    }
    
    bool success = orderService->cancelOrder(orderId, cancelledBy, reason);
    
    logOrderEvent(cancelledBy, "CANCEL_ORDER", orderId, 
                 getClientIpAddress(request), getUserAgent(request), 
                 success, success ? "Order cancelled successfully" : "Failed to cancel order");
    
    if (success)
    {
        sendSuccessResponse(response, "Order cancelled successfully");
    }
    else
    {
        sendErrorResponse(response, "Failed to cancel order. Order may already be shipped.", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
    }
}

void OrderController::handleUpdateOrderStatus(Poco::Net::HTTPServerRequest& request, 
                                            Poco::Net::HTTPServerResponse& response)
{
    std::string orderIdStr = getPathParameter(request.getURI(), "/api/v1/orders/", 0);
    orderIdStr = orderIdStr.substr(0, orderIdStr.find("/status"));
    
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
    
    auto json = parseJsonBody(request);
    if (!json)
    {
        sendErrorResponse(response, "Invalid JSON body", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    std::vector<std::string> errors;
    if (!validateUpdateStatusData(json, errors))
    {
        sendValidationErrorResponse(response, errors);
        return;
    }
    
    std::string newStatus = utils::JsonUtils::getString(*json, "status");
    std::string notes = utils::JsonUtils::getString(*json, "notes", "");
    
    std::string authError;
    if (!canUpdateOrderStatus(request, orderId, newStatus, authError))
    {
        sendForbiddenResponse(response, authError);
        return;
    }
    
    long long updatedBy = getCurrentUserId(request);
    
    auto result = orderService->updateOrderStatus(orderId, newStatus, updatedBy, notes);
    
    logOrderEvent(updatedBy, "UPDATE_ORDER_STATUS", orderId, 
                 getClientIpAddress(request), getUserAgent(request), 
                 result.success, result.success ? "Order status updated successfully" : result.message);
    
    if (result.success)
    {
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object(result.updatedOrder->toJson());
        sendSuccessResponse(response, "Order status updated successfully", dataPtr);
    }
    else
    {
        sendErrorResponse(response, result.message, 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
    }
}

void OrderController::handleUpdateOrderPriority(Poco::Net::HTTPServerRequest& request, 
                                              Poco::Net::HTTPServerResponse& response)
{
    std::string orderIdStr = getPathParameter(request.getURI(), "/api/v1/orders/", 0);
    orderIdStr = orderIdStr.substr(0, orderIdStr.find("/priority"));
    
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
    
    auto json = parseJsonBody(request);
    if (!json)
    {
        sendErrorResponse(response, "Invalid JSON body", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    std::vector<std::string> errors;
    if (!validateUpdatePriorityData(json, errors))
    {
        sendValidationErrorResponse(response, errors);
        return;
    }
    
    std::string newPriority = utils::JsonUtils::getString(*json, "priority");
    
    std::string authError;
    if (!canUpdateOrderPriority(request, orderId, newPriority, authError))
    {
        sendForbiddenResponse(response, authError);
        return;
    }
    
    long long updatedBy = getCurrentUserId(request);
    
    auto result = orderService->updateOrderPriority(orderId, newPriority, updatedBy);
    
    logOrderEvent(updatedBy, "UPDATE_ORDER_PRIORITY", orderId, 
                 getClientIpAddress(request), getUserAgent(request), 
                 result.success, result.success ? "Order priority updated successfully" : result.message);
    
    if (result.success)
    {
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object(result.updatedOrder->toJson());
        sendSuccessResponse(response, "Order priority updated successfully", dataPtr);
    }
    else
    {
        sendErrorResponse(response, result.message, 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
    }
}

void OrderController::handleGetOrderItems(Poco::Net::HTTPServerRequest& request, 
                                         Poco::Net::HTTPServerResponse& response)
{
    std::string orderIdStr = getPathParameter(request.getURI(), "/api/v1/orders/", 0);
    orderIdStr = orderIdStr.substr(0, orderIdStr.find("/items"));
    
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
    
    std::string accessError;
    if (!canViewOrder(request, orderId, accessError))
    {
        sendForbiddenResponse(response, accessError);
        return;
    }
    
    Poco::JSON::Object orderDetails = orderService->getOrderWithItems(orderId);

    if (orderDetails.has("error"))
    {
        std::string error = orderDetails.getValue<std::string>("error");
        if (error.find("Order not found") != std::string::npos)
        {
            sendNotFoundResponse(response, "Order");
        }
        else
        {
            sendErrorResponse(response, error, Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
        }
        return;
    }
    
    Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object(orderDetails);
    
    sendSuccessResponse(response, "Order items retrieved successfully", dataPtr);
}

void OrderController::handleAddOrderItem(Poco::Net::HTTPServerRequest& request, 
                                        Poco::Net::HTTPServerResponse& response)
{
    std::string orderIdStr = getPathParameter(request.getURI(), "/api/v1/orders/", 0);
    orderIdStr = orderIdStr.substr(0, orderIdStr.find("/items"));
    
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
    
    auto json = parseJsonBody(request);
    if (!json)
    {
        sendErrorResponse(response, "Invalid JSON body", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    std::vector<std::string> errors;
    if (!validateAddOrderItemData(json, errors))
    {
        sendValidationErrorResponse(response, errors);
        return;
    }
    
    auto itemData = extractOrderItemFromJson(json);
    if (!itemData)
    {
        sendErrorResponse(response, "Invalid order item data", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    std::string authError;
    if (!canAddOrderItem(request, orderId, authError))
    {
        sendForbiddenResponse(response, authError);
        return;
    }
    
    long long addedBy = getCurrentUserId(request);
    
    auto result = orderService->addOrderItem(orderId, *itemData, addedBy);
    
    logOrderEvent(addedBy, "ADD_ORDER_ITEM", orderId, 
                 getClientIpAddress(request), getUserAgent(request), 
                 result.success, result.success ? "Order item added successfully" : result.message);
    
    if (result.success)
    {
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object(result.updatedOrder->toJson());
        sendSuccessResponse(response, "Order item added successfully", dataPtr);
    }
    else
    {
        sendErrorResponse(response, result.message, 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
    }
}

void OrderController::handleUpdateOrderItem(Poco::Net::HTTPServerRequest& request, 
                                           Poco::Net::HTTPServerResponse& response)
{
    std::string path = request.getURI();
    std::string orderIdStr = getPathParameter(path, "/api/v1/orders/", 0);
    std::string itemIdStr = getPathParameter(path, "/api/v1/orders/" + orderIdStr + "/items/", 0);
    
    if (orderIdStr.empty() || itemIdStr.empty())
    {
        sendErrorResponse(response, "Order ID and Item ID are required", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    long long orderId, itemId;
    try
    {
        orderId = std::stoll(orderIdStr);
        itemId = std::stoll(itemIdStr);
    }
    catch (...)
    {
        sendErrorResponse(response, "Invalid order ID or item ID", 
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
    if (!validateUpdateOrderItemData(json, errors))
    {
        sendValidationErrorResponse(response, errors);
        return;
    }
    
    auto itemData = extractOrderItemFromJson(json);
    if (!itemData)
    {
        sendErrorResponse(response, "Invalid order item data", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    std::string authError;
    if (!canUpdateOrderItem(request, orderId, itemId, authError))
    {
        sendForbiddenResponse(response, authError);
        return;
    }
    
    long long updatedBy = getCurrentUserId(request);
    
    auto result = orderService->updateOrderItem(orderId, itemId, *itemData, updatedBy);
    
    logOrderEvent(updatedBy, "UPDATE_ORDER_ITEM", orderId, 
                 getClientIpAddress(request), getUserAgent(request), 
                 result.success, result.success ? "Order item updated successfully" : result.message);
    
    if (result.success)
    {
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object(result.updatedOrder->toJson());
        sendSuccessResponse(response, "Order item updated successfully", dataPtr);
    }
    else
    {
        sendErrorResponse(response, result.message, 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
    }
}

void OrderController::handleRemoveOrderItem(Poco::Net::HTTPServerRequest& request, 
                                           Poco::Net::HTTPServerResponse& response)
{
    std::string path = request.getURI();
    std::string orderIdStr = getPathParameter(path, "/api/v1/orders/", 0);
    std::string itemIdStr = getPathParameter(path, "/api/v1/orders/" + orderIdStr + "/items/", 0);
    
    if (orderIdStr.empty() || itemIdStr.empty())
    {
        sendErrorResponse(response, "Order ID and Item ID are required", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    long long orderId, itemId;
    try
    {
        orderId = std::stoll(orderIdStr);
        itemId = std::stoll(itemIdStr);
    }
    catch (...)
    {
        sendErrorResponse(response, "Invalid order ID or item ID", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    std::string authError;
    if (!canRemoveOrderItem(request, orderId, itemId, authError))
    {
        sendForbiddenResponse(response, authError);
        return;
    }
    
    long long removedBy = getCurrentUserId(request);
    
    auto result = orderService->removeOrderItem(orderId, itemId, removedBy);
    
    logOrderEvent(removedBy, "REMOVE_ORDER_ITEM", orderId, 
                 getClientIpAddress(request), getUserAgent(request), 
                 result.success, result.success ? "Order item removed successfully" : result.message);
    
    if (result.success)
    {
        sendSuccessResponse(response, "Order item removed successfully");
    }
    else
    {
        sendErrorResponse(response, result.message, 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
    }
}

void OrderController::handleSearchOrders(Poco::Net::HTTPServerRequest& request, 
                                        Poco::Net::HTTPServerResponse& response)
{
    auto filters = getFilterParameters(request);
    
    int page, pageSize;
    if (!getPaginationParameters(request, page, pageSize, 20))
    {
        sendErrorResponse(response, "Invalid pagination parameters", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    std::vector<std::string> errors;
    if (!validateSearchParameters(filters, errors))
    {
        sendValidationErrorResponse(response, errors);
        return;
    }
    
    auto currentUserRole = getCurrentUserRole(request);
    long long currentUserId = getCurrentUserId(request);
    
    Poco::JSON::Array ordersArray;
    
    try
    {
        if (filters.find("customerEmail") != filters.end())
        {
            std::string customerEmail = filters["customerEmail"];
            ordersArray = orderService->findOrdersByCustomer(customerEmail, page, pageSize);
        }
        else if (filters.find("status") != filters.end())
        {
            std::string status = filters["status"];
            ordersArray = orderService->findOrdersByStatus(status, page, pageSize);
        }
        else if (filters.find("startDate") != filters.end() && filters.find("endDate") != filters.end())
        {
            std::string startDate = filters["startDate"];
            std::string endDate = filters["endDate"];
            ordersArray = orderService->findOrdersByDateRange(startDate, endDate, page, pageSize);
        }
        else
        {
            sendErrorResponse(response, "Invalid search parameters. Use customerEmail, status, or date range.", 
                            Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
            return;
        }
        
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object();
        Poco::JSON::Array::Ptr ordersArrayPtr = new Poco::JSON::Array(ordersArray);
        dataPtr->set("orders", ordersArrayPtr);
        sendSuccessResponse(response, "Orders search completed", dataPtr);
    }
    catch (const std::exception& e)
    {
        sendErrorResponse(response, "Error searching orders: " + std::string(e.what()), 
                        Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
}

void OrderController::handleGetOrdersByCustomer(Poco::Net::HTTPServerRequest& request, 
                                               Poco::Net::HTTPServerResponse& response)
{
    std::string customerEmail = getQueryParameter(request.getURI(), "email");
    if (customerEmail.empty())
    {
        sendErrorResponse(response, "Customer email is required", 
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
    if (currentUserRole != database::models::UserRole::ADMIN && 
        currentUserRole != database::models::UserRole::MANAGER && 
        currentUserRole != database::models::UserRole::AUDITOR)
    {
        sendForbiddenResponse(response, "You don't have permission to view customer orders");
        return;
    }
    
    Poco::JSON::Array ordersArray = orderService->findOrdersByCustomer(customerEmail, page, pageSize);
    Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object;
    dataPtr->set("orders", ordersArray);
    
    sendSuccessResponse(response, "Customer orders retrieved successfully", dataPtr);
}

void OrderController::handleGetOrdersByStatus(Poco::Net::HTTPServerRequest& request, 
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
    
    Poco::JSON::Array ordersArray = orderService->findOrdersByStatus(status, page, pageSize);
    Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object;
    dataPtr->set("orders", ordersArray);
    
    sendSuccessResponse(response, "Orders by status retrieved successfully", dataPtr);
}

void OrderController::handleGetPendingOrders(Poco::Net::HTTPServerRequest& request, 
                                            Poco::Net::HTTPServerResponse& response)
{
    int page, pageSize;
    if (!getPaginationParameters(request, page, pageSize, 20))
    {
        sendErrorResponse(response, "Invalid pagination parameters", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    auto currentUserRole = getCurrentUserRole(request);
    
    Poco::JSON::Array ordersArray = orderService->getPendingOrders(page, pageSize);
    Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object;
    dataPtr->set("orders", ordersArray);
    
    sendSuccessResponse(response, "Pending orders retrieved successfully", dataPtr);
}

void OrderController::handleGetUrgentOrders(Poco::Net::HTTPServerRequest& request, 
                                           Poco::Net::HTTPServerResponse& response)
{
    int page, pageSize;
    if (!getPaginationParameters(request, page, pageSize, 20))
    {
        sendErrorResponse(response, "Invalid pagination parameters", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    Poco::JSON::Array ordersArray = orderService->getUrgentOrders(page, pageSize);
    Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object;
    dataPtr->set("orders", ordersArray);
    
    sendSuccessResponse(response, "Urgent orders retrieved successfully", dataPtr);
}

void OrderController::handleGetOrderStatistics(Poco::Net::HTTPServerRequest& request, 
                                              Poco::Net::HTTPServerResponse& response)
{
    auto currentUserRole = getCurrentUserRole(request);
    if (currentUserRole != database::models::UserRole::ADMIN && 
        currentUserRole != database::models::UserRole::MANAGER && 
        currentUserRole != database::models::UserRole::AUDITOR)
    {
        sendForbiddenResponse(response, "You don't have permission to view order statistics");
        return;
    }
    
    Poco::JSON::Object statistics = orderService->getOrderStatistics();
    Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object(statistics);
    
    sendSuccessResponse(response, "Order statistics retrieved", dataPtr);
}

void OrderController::handleGetRevenueReport(Poco::Net::HTTPServerRequest& request, 
                                            Poco::Net::HTTPServerResponse& response)
{
    auto currentUserRole = getCurrentUserRole(request);
    if (currentUserRole != database::models::UserRole::ADMIN && 
        currentUserRole != database::models::UserRole::MANAGER && 
        currentUserRole != database::models::UserRole::AUDITOR)
    {
        sendForbiddenResponse(response, "You don't have permission to view revenue reports");
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
    
    Poco::JSON::Array report = orderService->getRevenueReport(startDate, endDate);
    Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object;
    dataPtr->set("report", report);
    
    sendSuccessResponse(response, "Revenue report generated", dataPtr);
}

void OrderController::handleGetCustomerOrderHistory(Poco::Net::HTTPServerRequest& request, 
                                                   Poco::Net::HTTPServerResponse& response)
{
    std::string customerEmail = getQueryParameter(request.getURI(), "email");
    if (customerEmail.empty())
    {
        sendErrorResponse(response, "Customer email is required", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    auto currentUserRole = getCurrentUserRole(request);
    if (currentUserRole != database::models::UserRole::ADMIN && 
        currentUserRole != database::models::UserRole::MANAGER && 
        currentUserRole != database::models::UserRole::AUDITOR)
    {
        sendForbiddenResponse(response, "You don't have permission to view customer order history");
        return;
    }
    
    Poco::JSON::Array history = orderService->getCustomerOrderHistory(customerEmail);
    Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object;
    dataPtr->set("history", history);
    
    sendSuccessResponse(response, "Customer order history retrieved", dataPtr);
}

void OrderController::handleMarkOrderAsShipped(Poco::Net::HTTPServerRequest& request, 
                                              Poco::Net::HTTPServerResponse& response)
{
    std::string orderIdStr = getPathParameter(request.getURI(), "/api/v1/orders/", 0);
    orderIdStr = orderIdStr.substr(0, orderIdStr.find("/ship"));
    
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
    
    auto currentUserRole = getCurrentUserRole(request);
    if (currentUserRole != database::models::UserRole::ADMIN && 
        currentUserRole != database::models::UserRole::MANAGER && 
        currentUserRole != database::models::UserRole::WORKER)
    {
        sendForbiddenResponse(response, "You don't have permission to mark orders as shipped");
        return;
    }
    
    long long shippedBy = getCurrentUserId(request);
    
    bool success = orderService->markOrderAsShipped(orderId, shippedBy);
    
    logOrderEvent(shippedBy, "MARK_ORDER_SHIPPED", orderId, 
                 getClientIpAddress(request), getUserAgent(request), 
                 success, success ? "Order marked as shipped successfully" : "Failed to mark order as shipped");
    
    if (success)
    {
        sendSuccessResponse(response, "Order marked as shipped successfully");
    }
    else
    {
        sendErrorResponse(response, "Failed to mark order as shipped", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
    }
}

void OrderController::handleMarkOrderAsDelivered(Poco::Net::HTTPServerRequest& request, 
                                                Poco::Net::HTTPServerResponse& response)
{
    std::string orderIdStr = getPathParameter(request.getURI(), "/api/v1/orders/", 0);
    orderIdStr = orderIdStr.substr(0, orderIdStr.find("/deliver"));
    
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
    
    auto currentUserRole = getCurrentUserRole(request);
    if (currentUserRole != database::models::UserRole::ADMIN && 
        currentUserRole != database::models::UserRole::MANAGER && 
        currentUserRole != database::models::UserRole::WORKER)
    {
        sendForbiddenResponse(response, "You don't have permission to mark orders as delivered");
        return;
    }
    
    long long deliveredBy = getCurrentUserId(request);
    
    std::string actualDeliveryDate;
    auto json = parseJsonBody(request);
    if (json && json->has("actualDeliveryDate"))
    {
        actualDeliveryDate = utils::JsonUtils::getString(*json, "actualDeliveryDate");
    }
    
    bool success = orderService->markOrderAsDelivered(orderId, actualDeliveryDate);
    
    logOrderEvent(deliveredBy, "MARK_ORDER_DELIVERED", orderId, 
                 getClientIpAddress(request), getUserAgent(request), 
                 success, success ? "Order marked as delivered successfully" : "Failed to mark order as delivered");
    
    if (success)
    {
        sendSuccessResponse(response, "Order marked as delivered successfully");
    }
    else
    {
        sendErrorResponse(response, "Failed to mark order as delivered", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
    }
}

bool OrderController::validateRequest(Poco::Net::HTTPServerRequest& request, 
                                    Poco::Net::HTTPServerResponse& response,
                                    std::string& errorMessage)
{
    if (!BaseController::validateRequest(request, response, errorMessage))
    {
        return false;
    }
    
    return true;
}

bool OrderController::authorizeRequest(Poco::Net::HTTPServerRequest& request, 
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

bool OrderController::validateOrderAccess(Poco::Net::HTTPServerRequest& request, 
                                         long long orderId,
                                         std::string& errorMessage)
{
    auto currentUserRole = getCurrentUserRole(request);
    long long currentUserId = getCurrentUserId(request);
    
    if (currentUserRole == database::models::UserRole::ADMIN || 
        currentUserRole == database::models::UserRole::MANAGER || 
        currentUserRole == database::models::UserRole::AUDITOR)
    {
        return true;
    }
    
    return true;
}

long long OrderController::getCurrentUserId(Poco::Net::HTTPServerRequest& request)
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

database::models::UserRole OrderController::getCurrentUserRole(Poco::Net::HTTPServerRequest& request)
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

bool OrderController::validateCreateOrderData(const Poco::JSON::Object::Ptr& json, 
                                             std::vector<std::string>& errors)
{
    errors.clear();
    
    std::vector<std::string> requiredFields = {"customer_name", "shipping_address", "items"};
    std::vector<std::string> missingFields;
    if (!validateRequiredFields(json, requiredFields, missingFields))
    {
        for (const auto& field : missingFields)
        {
            errors.push_back(field + " is required");
        }
    }
    
    if (json->has("customer_name"))
    {
        std::string customerName = utils::JsonUtils::getString(*json, "customer_name");
        if (customerName.empty() || customerName.length() > 150)
        {
            errors.push_back("Customer name must be between 1 and 150 characters");
        }
    }
    
    if (json->has("shipping_address"))
    {
        std::string shippingAddress = utils::JsonUtils::getString(*json, "shipping_address");
        if (shippingAddress.empty())
        {
            errors.push_back("Shipping address is required");
        }
    }
    
    if (json->has("customer_email"))
    {
        std::string email = utils::JsonUtils::getString(*json, "customer_email");
        if (!email.empty() && !utils::Validator::isValidEmail(email))
        {
            errors.push_back("Invalid email format");
        }
    }
    
    if (json->has("customer_phone"))
    {
        std::string phone = utils::JsonUtils::getString(*json, "customer_phone");
        if (!phone.empty() && !utils::Validator::isValidPhone(phone))
        {
            errors.push_back("Invalid phone format");
        }
    }
    
    if (json->has("items"))
    {
        auto itemsArray = json->getArray("items");
        if (!itemsArray || itemsArray->size() == 0)
        {
            errors.push_back("Order must have at least one item");
        }
    }
    
    if (json->has("priority"))
    {
        std::string priority = utils::JsonUtils::getString(*json, "priority");
        try
        {
            auto orderPriority = database::models::CustomerOrder::stringToPriority(priority);
        }
        catch (...)
        {
            errors.push_back("Invalid priority value");
        }
    }
    
    return errors.empty();
}

bool OrderController::validateUpdateOrderData(const Poco::JSON::Object::Ptr& json, 
                                             std::vector<std::string>& errors)
{
    errors.clear();
    
    bool hasCustomerName = json->has("customer_name") && !json->get("customer_name").isEmpty();
    bool hasShippingAddress = json->has("shipping_address") && !json->get("shipping_address").isEmpty();
    bool hasCustomerEmail = json->has("customer_email") && !json->get("customer_email").isEmpty();
    bool hasCustomerPhone = json->has("customer_phone") && !json->get("customer_phone").isEmpty();
    bool hasNotes = json->has("notes") && !json->get("notes").isEmpty();
    
    if (!hasCustomerName && !hasShippingAddress && !hasCustomerEmail && 
        !hasCustomerPhone && !hasNotes)
    {
        errors.push_back("At least one field must be provided for update");
    }
    
    if (hasCustomerName)
    {
        std::string customerName = utils::JsonUtils::getString(*json, "customer_name");
        if (customerName.empty() || customerName.length() > 150)
        {
            errors.push_back("Customer name must be between 1 and 150 characters");
        }
    }
    
    if (hasCustomerEmail)
    {
        std::string email = utils::JsonUtils::getString(*json, "customer_email");
        if (!email.empty() && !utils::Validator::isValidEmail(email))
        {
            errors.push_back("Invalid email format");
        }
    }
    
    if (hasCustomerPhone)
    {
        std::string phone = utils::JsonUtils::getString(*json, "customer_phone  ");
        if (!phone.empty() && !utils::Validator::isValidPhone(phone))
        {
            errors.push_back("Invalid phone format");
        }
    }
    
    return errors.empty();
}

bool OrderController::validateUpdateStatusData(const Poco::JSON::Object::Ptr& json, 
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
            auto orderStatus = database::models::CustomerOrder::stringToStatus(status);
        }
        catch (...)
        {
            errors.push_back("Invalid status value");
        }
    }
    
    return errors.empty();
}

bool OrderController::validateUpdatePriorityData(const Poco::JSON::Object::Ptr& json, 
                                                std::vector<std::string>& errors)
{
    errors.clear();
    
    std::vector<std::string> requiredFields = {"priority"};
    std::vector<std::string> missingFields;
    if (!validateRequiredFields(json, requiredFields, missingFields))
    {
        for (const auto& field : missingFields)
        {
            errors.push_back(field + " is required");
        }
    }
    
    if (json->has("priority"))
    {
        std::string priority = utils::JsonUtils::getString(*json, "priority");
        try
        {
            auto orderPriority = database::models::CustomerOrder::stringToPriority(priority);
        }
        catch (...)
        {
            errors.push_back("Invalid priority value");
        }
    }
    
    return errors.empty();
}

bool OrderController::validateAddOrderItemData(const Poco::JSON::Object::Ptr& json, 
                                              std::vector<std::string>& errors)
{
    errors.clear();
    
    std::vector<std::string> requiredFields = {"product_id", "quantity_ordered", "unit_price"};
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
    
    if (json->has("quantity_ordered"))
    {
        int quantity = utils::JsonUtils::getInt(*json, "quantity_ordered");
        if (quantity <= 0)
        {
            errors.push_back("Quantity must be greater than 0");
        }
    }
    
    if (json->has("unit_price"))
    {
        double unitPrice = utils::JsonUtils::getDouble(*json, "unit_price");
        if (unitPrice < 0)
        {
            errors.push_back("Unit price must be greater than or equal to 0");
        }
    }
    
    if (json->has("discount_percent"))
    {
        double discount = utils::JsonUtils::getDouble(*json, "discount_percent");
        if (discount < 0 || discount > 100)
        {
            errors.push_back("Discount percent must be between 0 and 100");
        }
    }
    
    return errors.empty();
}

bool OrderController::validateUpdateOrderItemData(const Poco::JSON::Object::Ptr& json, 
                                                 std::vector<std::string>& errors)
{
    errors.clear();
    
    bool hasQuantity = json->has("quantity_ordered") && !json->get("quantity_ordered").isEmpty();
    bool hasUnitPrice = json->has("unit_price") && !json->get("unit_price").isEmpty();
    bool hasDiscount = json->has("discount_percent") && !json->get("discount_percent").isEmpty();
    bool hasBatchId = json->has("batch_id") && !json->get("batch_id").isEmpty();
    
    if (!hasQuantity && !hasUnitPrice && !hasDiscount && !hasBatchId)
    {
        errors.push_back("At least one field must be provided for update");
    }
    
    if (hasQuantity)
    {
        int quantity = utils::JsonUtils::getInt(*json, "quantity_ordered");
        if (quantity <= 0)
        {
            errors.push_back("Quantity must be greater than 0");
        }
    }
    
    if (hasUnitPrice)
    {
        double unitPrice = utils::JsonUtils::getDouble(*json, "unit_price");
        if (unitPrice < 0)
        {
            errors.push_back("Unit price must be greater than or equal to 0");
        }
    }
    
    if (hasDiscount)
    {
        double discount = utils::JsonUtils::getDouble(*json, "discount_percent");
        if (discount < 0 || discount > 100)
        {
            errors.push_back("Discount percent must be between 0 and 100");
        }
    }
    
    if (hasBatchId)
    {
        long long batchId = utils::JsonUtils::getInt(*json, "batch_id");
        if (batchId <= 0)
        {
            errors.push_back("Invalid batch ID");
        }
    }
    
    return errors.empty();
}

bool OrderController::validateSearchParameters(const std::map<std::string, std::string>& filters,
                                              std::vector<std::string>& errors)
{
    errors.clear();
    
    for (const auto& [key, value] : filters)
    {
        if (key == "status")
        {
            try
            {
                auto status = database::models::CustomerOrder::stringToStatus(value);
            }
            catch (...)
            {
                errors.push_back("Invalid status value");
            }
        }
        else if (key == "priority")
        {
            try
            {
                auto priority = database::models::CustomerOrder::stringToPriority(value);
            }
            catch (...)
            {
                errors.push_back("Invalid priority value");
            }
        }
        else if (key == "startDate" || key == "endDate")
        {
            if (!utils::Validator::isValidDate(value))
            {
                errors.push_back(key + " must be in YYYY-MM-DD format");
            }
        }
        else if (key == "minAmount")
        {
            try
            {
                double amount = std::stod(value);
                if (amount < 0)
                {
                    errors.push_back("Minimum amount must be greater than or equal to 0");
                }
            }
            catch (...)
            {
                errors.push_back("Invalid minimum amount");
            }
        }
        else if (key == "maxAmount")
        {
            try
            {
                double amount = std::stod(value);
                if (amount < 0)
                {
                    errors.push_back("Maximum amount must be greater than or equal to 0");
                }
            }
            catch (...)
            {
                errors.push_back("Invalid maximum amount");
            }
        }
        else if (key == "customerEmail")
        {
            if (!utils::Validator::isValidEmail(value))
            {
                errors.push_back("Invalid email format for customerEmail");
            }
        }
    }
    
    bool hasCustomerEmail = filters.find("customerEmail") != filters.end();
    bool hasStatus = filters.find("status") != filters.end();
    bool hasDateRange = (filters.find("startDate") != filters.end() && filters.find("endDate") != filters.end());
    
    int filterCount = 0;
    if (hasCustomerEmail) filterCount++;
    if (hasStatus) filterCount++;
    if (hasDateRange) filterCount++;
    
    if (filterCount > 1)
    {
        errors.push_back("Only one filter type (customerEmail, status, or date range) is allowed at a time");
    }
    
    return errors.empty();
}

void OrderController::logOrderEvent(long long userId, 
                                   const std::string& action,
                                   long long orderId,
                                   const std::string& ipAddress,
                                   const std::string& userAgent,
                                   bool success,
                                   const std::string& details)
{
    std::cout << "[" << getCurrentTimestamp() << "] "
              << "ORDER " << action << " "
              << "UserID: " << userId << " "
              << "OrderID: " << orderId << " "
              << "IP: " << ipAddress << " "
              << "Success: " << (success ? "Yes" : "No") << " "
              << "Details: " << details << std::endl;
}

std::unique_ptr<database::models::CustomerOrder> OrderController::extractOrderFromJson(const Poco::JSON::Object::Ptr& json)
{
    if (!json)
    {
        return nullptr;
    }
    
    try
    {
        auto order = std::make_unique<database::models::CustomerOrder>(*json);
        return order;
    }
    catch (...)
    {
        return nullptr;
    }
}

std::unique_ptr<database::models::OrderItem> OrderController::extractOrderItemFromJson(const Poco::JSON::Object::Ptr& json)
{
    if (!json)
    {
        return nullptr;
    }
    
    try
    {
        auto item = std::make_unique<database::models::OrderItem>(*json);
        return item;
    }
    catch (...)
    {
        return nullptr;
    }
}

bool OrderController::canCreateOrder(Poco::Net::HTTPServerRequest& request, 
                                    const database::models::CustomerOrder& orderData,
                                    std::string& errorMessage)
{
    auto currentUserRole = getCurrentUserRole(request);
    
    if (currentUserRole == database::models::UserRole::ADMIN || 
        currentUserRole == database::models::UserRole::MANAGER || 
        currentUserRole == database::models::UserRole::WORKER)
    {
        return true;
    }
    
    errorMessage = "You don't have permission to create orders";
    return false;
}

bool OrderController::canUpdateOrder(Poco::Net::HTTPServerRequest& request, 
                                    long long orderId,
                                    const database::models::CustomerOrder& orderData,
                                    std::string& errorMessage)
{
    auto currentUserRole = getCurrentUserRole(request);
    
    if (currentUserRole == database::models::UserRole::ADMIN || 
        currentUserRole == database::models::UserRole::MANAGER)
    {
        return true;
    }
    
    long long currentUserId = getCurrentUserId(request);
    
    errorMessage = "You don't have permission to update this order";
    return false;
}

bool OrderController::canDeleteOrder(Poco::Net::HTTPServerRequest& request, 
                                    long long orderId,
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
    
    errorMessage = "You don't have permission to delete this order";
    return false;
}

bool OrderController::canCancelOrder(Poco::Net::HTTPServerRequest& request, 
                                    long long orderId,
                                    std::string& errorMessage)
{
    auto currentUserRole = getCurrentUserRole(request);
    
    if (currentUserRole == database::models::UserRole::ADMIN || 
        currentUserRole == database::models::UserRole::MANAGER)
    {
        return true;
    }
    
    long long currentUserId = getCurrentUserId(request);
    
    errorMessage = "You don't have permission to cancel this order";
    return false;
}

bool OrderController::canUpdateOrderStatus(Poco::Net::HTTPServerRequest& request, 
                                          long long orderId,
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
        if (newStatus == "picked" || newStatus == "packed")
        {
            return true;
        }
    }
    
    errorMessage = "You don't have permission to update order status";
    return false;
}

bool OrderController::canUpdateOrderPriority(Poco::Net::HTTPServerRequest& request, 
                                            long long orderId,
                                            const std::string& newPriority,
                                            std::string& errorMessage)
{
    auto currentUserRole = getCurrentUserRole(request);
    
    if (currentUserRole == database::models::UserRole::ADMIN || 
        currentUserRole == database::models::UserRole::MANAGER)
    {
        return true;
    }
    
    errorMessage = "You don't have permission to update order priority";
    return false;
}

bool OrderController::canAddOrderItem(Poco::Net::HTTPServerRequest& request, 
                                     long long orderId,
                                     std::string& errorMessage)
{
    auto currentUserRole = getCurrentUserRole(request);
    
    if (currentUserRole == database::models::UserRole::ADMIN || 
        currentUserRole == database::models::UserRole::MANAGER)
    {
        return true;
    }
    
    long long currentUserId = getCurrentUserId(request);
    
    errorMessage = "You don't have permission to add items to this order";
    return false;
}

bool OrderController::canUpdateOrderItem(Poco::Net::HTTPServerRequest& request, 
                                        long long orderId,
                                        long long itemId,
                                        std::string& errorMessage)
{
    auto currentUserRole = getCurrentUserRole(request);
    
    if (currentUserRole == database::models::UserRole::ADMIN || 
        currentUserRole == database::models::UserRole::MANAGER)
    {
        return true;
    }
    
    long long currentUserId = getCurrentUserId(request);
    
    errorMessage = "You don't have permission to update this order item";
    return false;
}

bool OrderController::canRemoveOrderItem(Poco::Net::HTTPServerRequest& request, 
                                        long long orderId,
                                        long long itemId,
                                        std::string& errorMessage)
{
    auto currentUserRole = getCurrentUserRole(request);
    
    if (currentUserRole == database::models::UserRole::ADMIN || 
        currentUserRole == database::models::UserRole::MANAGER)
    {
        return true;
    }
    
    long long currentUserId = getCurrentUserId(request);
    
    errorMessage = "You don't have permission to remove this order item";
    return false;
}

bool OrderController::canViewOrder(Poco::Net::HTTPServerRequest& request, 
                                  long long orderId,
                                  std::string& errorMessage)
{
    auto currentUserRole = getCurrentUserRole(request);
    
    if (currentUserRole == database::models::UserRole::ADMIN || 
        currentUserRole == database::models::UserRole::MANAGER || 
        currentUserRole == database::models::UserRole::AUDITOR)
    {
        return true;
    }
    
    long long currentUserId = getCurrentUserId(request);
    
    return true;
}

Poco::JSON::Object OrderController::buildPaginationResponse(int page, int pageSize, int totalItems, 
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
