#include "OrderPaymentController.hpp"
#include "../services/OrderPaymentService.hpp"
#include "../services/AuthService.hpp"
#include "../utils/JsonUtils.hpp"
#include "../utils/Validator.hpp"
#include "../utils/DateUtils.hpp"
#include <Poco/Net/HTTPRequestHandler.h>
#include <Poco/Net/HTTPServerRequest.h>
#include <Poco/Net/HTTPServerResponse.h>
#include <Poco/JSON/Parser.h>
#include <Poco/JSON/Array.h>
#include <Poco/JSON/Stringifier.h>
#include <Poco/DateTimeFormatter.h>
#include <Poco/DateTimeFormat.h>
#include <Poco/URI.h>
#include <Poco/Exception.h>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <ctime>

namespace warehouse_backend::controllers
{

OrderPaymentController::OrderPaymentController()
    : orderPaymentService(std::make_unique<services::OrderPaymentService>()),
      authService(std::make_unique<services::AuthService>())
{
}

void OrderPaymentController::handleRequest(Poco::Net::HTTPServerRequest& request, 
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
        
        std::string basePath = "/api/v1/payments";
        if (endpoint.find("/api/v1/") != 0)
        {
            basePath = "/api/payments";
        }
        
        std::string paymentIdStr = getPathParameter(uri, basePath + "/", 0);
        
        if ((endpoint == "/api/v1/payments/statistics" || endpoint == "/api/payments/statistics") && 
            request.getMethod() == "GET")
        {
            handleGetPaymentStatistics(request, response);
        }
        else if ((endpoint.find("/api/v1/payments/report") == 0 || endpoint.find("/api/payments/report") == 0) && 
                request.getMethod() == "GET")
        {
            handleGetPaymentReport(request, response);
        }
        else if ((endpoint.find("/api/v1/payments/pending") == 0 || 
                 endpoint.find("/api/payments/pending") == 0) && 
                request.getMethod() == "GET")
        {
            handleGetPendingPayments(request, response);
        }
        else if ((endpoint.find("/api/v1/payments/paid") == 0 || 
                 endpoint.find("/api/payments/paid") == 0) && 
                request.getMethod() == "GET")
        {
            handleGetPaidPayments(request, response);
        }
        else if ((endpoint.find("/api/v1/payments/failed") == 0 || 
                 endpoint.find("/api/payments/failed") == 0) && 
                request.getMethod() == "GET")
        {
            handleGetFailedPayments(request, response);
        }
        else if ((endpoint.find("/api/v1/payments/refunded") == 0 || 
                 endpoint.find("/api/payments/refunded") == 0) && 
                request.getMethod() == "GET")
        {
            handleGetRefundedPayments(request, response);
        }
        else if ((endpoint.find("/api/v1/payments/overdue") == 0 || 
                 endpoint.find("/api/payments/overdue") == 0) && 
                request.getMethod() == "GET")
        {
            handleGetOverduePayments(request, response);
        }
        else if ((endpoint.find("/api/v1/payments/search") == 0 || 
                 endpoint.find("/api/payments/search") == 0) && 
                request.getMethod() == "GET")
        {
            handleSearchPayments(request, response);
        }
        else if ((endpoint == "/api/v1/payments" || endpoint == "/api/payments") && 
                request.getMethod() == "GET")
        {
            handleGetPayments(request, response);
        }
        else if ((endpoint == "/api/v1/payments" || endpoint == "/api/payments") && 
                request.getMethod() == "POST")
        {
            handleCreatePayment(request, response);
        }
        else if ((endpoint.find("/api/v1/payments/order/") == 0 || 
                 endpoint.find("/api/payments/order/") == 0) && 
                request.getMethod() == "GET")
        {
            handleGetPaymentByOrderId(request, response);
        }
        else if ((endpoint.find("/api/v1/payments/") == 0 || endpoint.find("/api/payments/") == 0) && 
                !paymentIdStr.empty())
        {
            if (endpoint.find("/status") != std::string::npos && request.getMethod() == "PUT")
            {
                handleUpdatePaymentStatus(request, response);
            }
            else if (endpoint.find("/paid") != std::string::npos && request.getMethod() == "POST")
            {
                handleMarkAsPaid(request, response);
            }
            else if (endpoint.find("/failed") != std::string::npos && request.getMethod() == "POST")
            {
                handleMarkAsFailed(request, response);
            }
            else if (endpoint.find("/refunded") != std::string::npos && request.getMethod() == "POST")
            {
                handleMarkAsRefunded(request, response);
            }
            else if (endpoint.find("/pending") != std::string::npos && request.getMethod() == "POST")
            {
                handleMarkAsPending(request, response);
            }
            else if (endpoint.find("/process") != std::string::npos && request.getMethod() == "POST")
            {
                handleProcessPayment(request, response);
            }
            else if (endpoint.find("/refund") != std::string::npos && request.getMethod() == "POST")
            {
                handleRefundPayment(request, response);
            }
            else if (request.getMethod() == "GET")
            {
                handleGetPaymentById(request, response);
            }
            else if (request.getMethod() == "PUT")
            {
                handleUpdatePayment(request, response);
            }
            else if (request.getMethod() == "DELETE")
            {
                handleDeletePayment(request, response);
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

void OrderPaymentController::handleGetPayments(Poco::Net::HTTPServerRequest& request, 
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
    long long currentUserId = getCurrentUserId(request);
    
    if (currentUserRole != database::models::UserRole::ADMIN && 
        currentUserRole != database::models::UserRole::MANAGER && 
        currentUserRole != database::models::UserRole::AUDITOR)
    {
        sendForbiddenResponse(response, "You don't have permission to view all payments");
        return;
    }
    
    try
    {
        auto payments = orderPaymentService->getPaginatedPayments(page, pageSize);
        int totalPayments = orderPaymentService->countPayments();
        
        Poco::JSON::Array paymentsArray;
        for (const auto& payment : payments)
        {
            if (payment)
            {
                paymentsArray.add(payment->toJson());
            }
        }
        
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object;
        dataPtr->set("payments", paymentsArray);
        dataPtr->set("page", page);
        dataPtr->set("pageSize", pageSize);
        dataPtr->set("totalPayments", totalPayments);
        dataPtr->set("totalPages", (totalPayments + pageSize - 1) / pageSize);
        
        sendSuccessResponse(response, "Payments retrieved successfully", dataPtr);
    }
    catch (const std::exception& e)
    {
        sendErrorResponse(response, "Failed to retrieve payments: " + std::string(e.what()), 
                        Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
}

void OrderPaymentController::handleGetPaymentById(Poco::Net::HTTPServerRequest& request, 
                                                Poco::Net::HTTPServerResponse& response)
{
    std::string paymentIdStr = getPathParameter(request.getURI(), "/api/v1/payments/", 0);
    if (paymentIdStr.empty())
    {
        sendErrorResponse(response, "Payment ID is required", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    long long paymentId;
    try
    {
        paymentId = std::stoll(paymentIdStr);
    }
    catch (...)
    {
        sendErrorResponse(response, "Invalid payment ID", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    std::string accessError;
    if (!canViewPayment(request, paymentId, accessError))
    {
        sendForbiddenResponse(response, accessError);
        return;
    }
    
    try
    {
        auto payment = orderPaymentService->getPaymentById(paymentId);
        if (!payment)
        {
            sendErrorResponse(response, "Payment not found", 
                            Poco::Net::HTTPResponse::HTTP_NOT_FOUND);
            return;
        }
        
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object(payment->toJson());
        sendSuccessResponse(response, "Payment retrieved successfully", dataPtr);
    }
    catch (const std::exception& e)
    {
        sendErrorResponse(response, "Failed to retrieve payment: " + std::string(e.what()), 
                        Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
}

void OrderPaymentController::handleGetPaymentByOrderId(Poco::Net::HTTPServerRequest& request, 
                                                     Poco::Net::HTTPServerResponse& response)
{
    std::string orderIdStr = getPathParameter(request.getURI(), 
        request.getURI().find("/api/v1/") == 0 ? "/api/v1/payments/order/" : "/api/payments/order/", 
        0);
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
    
    try
    {
        auto payment = orderPaymentService->getPaymentByOrderId(orderId);
        
        Poco::JSON::Array paymentsArray;
        if (payment)
        {
            paymentsArray.add(payment->toJson());
        }
        
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object;
        dataPtr->set("payments", paymentsArray);
        dataPtr->set("orderId", orderId);
        dataPtr->set("hasPayment", payment != nullptr);
        
        sendSuccessResponse(response, "Payment for order retrieved successfully", dataPtr);
    }
    catch (const std::exception& e)
    {
        sendErrorResponse(response, "Failed to retrieve payment for order: " + std::string(e.what()), 
                        Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
}

void OrderPaymentController::handleCreatePayment(Poco::Net::HTTPServerRequest& request, 
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
    if (!validateCreatePaymentData(json, errors))
    {
        sendValidationErrorResponse(response, errors);
        return;
    }
    
    auto paymentData = extractPaymentFromJson(json);
    if (!paymentData)
    {
        sendErrorResponse(response, "Invalid payment data", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    std::string authError;
    long long currentUserId = getCurrentUserId(request);
    auto currentUserRole = getCurrentUserRole(request);
    
    if (!orderPaymentService->canUserCreatePayment(currentUserId, currentUserRole, *paymentData))
    {
        sendForbiddenResponse(response, "You don't have permission to create this payment");
        return;
    }
    
    try
    {
        long long paymentId = orderPaymentService->createPayment(*paymentData, currentUserId);
        
        auto createdPayment = orderPaymentService->getPaymentById(paymentId);
        if (!createdPayment)
        {
            sendErrorResponse(response, "Payment created but not found", 
                            Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
            return;
        }
        
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object(createdPayment->toJson());
        sendSuccessResponse(response, "Payment created successfully", dataPtr);
    }
    catch (const std::exception& e)
    {
        sendErrorResponse(response, "Failed to create payment: " + std::string(e.what()), 
                        Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
}

void OrderPaymentController::handleUpdatePayment(Poco::Net::HTTPServerRequest& request, 
                                               Poco::Net::HTTPServerResponse& response)
{
    std::string paymentIdStr = getPathParameter(request.getURI(), "/api/v1/payments/", 0);
    if (paymentIdStr.empty())
    {
        sendErrorResponse(response, "Payment ID is required", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    long long paymentId;
    try
    {
        paymentId = std::stoll(paymentIdStr);
    }
    catch (...)
    {
        sendErrorResponse(response, "Invalid payment ID", 
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
    if (!validateUpdatePaymentData(json, errors))
    {
        sendValidationErrorResponse(response, errors);
        return;
    }
    
    auto paymentData = extractPaymentFromJson(json);
    if (!paymentData)
    {
        sendErrorResponse(response, "Invalid payment data", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    std::string authError;
    long long currentUserId = getCurrentUserId(request);
    auto currentUserRole = getCurrentUserRole(request);
    
    if (!orderPaymentService->canUserUpdatePayment(currentUserId, currentUserRole, paymentId))
    {
        sendForbiddenResponse(response, "You don't have permission to update this payment");
        return;
    }
    
    try
    {
        bool success = orderPaymentService->updatePayment(paymentId, *paymentData, currentUserId);
        if (!success)
        {
            sendErrorResponse(response, "Failed to update payment", 
                            Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
            return;
        }
        
        auto updatedPayment = orderPaymentService->getPaymentById(paymentId);
        if (!updatedPayment)
        {
            sendErrorResponse(response, "Payment updated but not found", 
                            Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
            return;
        }
        
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object(updatedPayment->toJson());
        sendSuccessResponse(response, "Payment updated successfully", dataPtr);
    }
    catch (const std::exception& e)
    {
        sendErrorResponse(response, "Failed to update payment: " + std::string(e.what()), 
                        Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
}

void OrderPaymentController::handleDeletePayment(Poco::Net::HTTPServerRequest& request, 
                                               Poco::Net::HTTPServerResponse& response)
{
    std::string paymentIdStr = getPathParameter(request.getURI(), "/api/v1/payments/", 0);
    if (paymentIdStr.empty())
    {
        sendErrorResponse(response, "Payment ID is required", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    long long paymentId;
    try
    {
        paymentId = std::stoll(paymentIdStr);
    }
    catch (...)
    {
        sendErrorResponse(response, "Invalid payment ID", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    std::string authError;
    long long currentUserId = getCurrentUserId(request);
    auto currentUserRole = getCurrentUserRole(request);
    
    if (!orderPaymentService->canUserDeletePayment(currentUserId, currentUserRole, paymentId))
    {
        sendForbiddenResponse(response, "You don't have permission to delete this payment");
        return;
    }
    
    try
    {
        bool success = orderPaymentService->deletePayment(paymentId, currentUserId);
        if (!success)
        {
            sendErrorResponse(response, "Failed to delete payment", 
                            Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
            return;
        }
        
        sendSuccessResponse(response, "Payment deleted successfully");
    }
    catch (const std::exception& e)
    {
        sendErrorResponse(response, "Failed to delete payment: " + std::string(e.what()), 
                        Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
}

void OrderPaymentController::handleUpdatePaymentStatus(Poco::Net::HTTPServerRequest& request, 
                                                     Poco::Net::HTTPServerResponse& response)
{
    std::string paymentIdStr = getPathParameter(request.getURI(), "/api/v1/payments/", 0);
    paymentIdStr = paymentIdStr.substr(0, paymentIdStr.find("/status"));
    
    if (paymentIdStr.empty())
    {
        sendErrorResponse(response, "Payment ID is required", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    long long paymentId;
    try
    {
        paymentId = std::stoll(paymentIdStr);
    }
    catch (...)
    {
        sendErrorResponse(response, "Invalid payment ID", 
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
    if (!validateUpdatePaymentStatusData(json, errors))
    {
        sendValidationErrorResponse(response, errors);
        return;
    }
    
    std::string newStatus = utils::JsonUtils::getString(*json, "status");
    std::string transactionId = utils::JsonUtils::getString(*json, "transactionId", "");
    std::string paymentDate = utils::JsonUtils::getString(*json, "paymentDate", "");
    
    std::string authError;
    long long currentUserId = getCurrentUserId(request);
    auto currentUserRole = getCurrentUserRole(request);
    
    if (!orderPaymentService->canUserUpdatePaymentStatus(currentUserId, currentUserRole, paymentId))
    {
        sendForbiddenResponse(response, "You don't have permission to update payment status");
        return;
    }
    
    try
    {
        bool success = orderPaymentService->updatePaymentStatus(paymentId, newStatus, 
                                                              transactionId, paymentDate, 
                                                              currentUserId);
        if (!success)
        {
            sendErrorResponse(response, "Failed to update payment status", 
                            Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
            return;
        }
        
        auto updatedPayment = orderPaymentService->getPaymentById(paymentId);
        if (!updatedPayment)
        {
            sendErrorResponse(response, "Payment status updated but payment not found", 
                            Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
            return;
        }
        
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object(updatedPayment->toJson());
        sendSuccessResponse(response, "Payment status updated successfully", dataPtr);
    }
    catch (const std::exception& e)
    {
        sendErrorResponse(response, "Failed to update payment status: " + std::string(e.what()), 
                        Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
}

void OrderPaymentController::handleSearchPayments(Poco::Net::HTTPServerRequest& request, 
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
    
    if (currentUserRole != database::models::UserRole::ADMIN && 
        currentUserRole != database::models::UserRole::MANAGER && 
        currentUserRole != database::models::UserRole::AUDITOR)
    {
        sendForbiddenResponse(response, "You don't have permission to search payments");
        return;
    }
    
    try
    {
        std::string query;
        std::vector<std::string> fields;
        
        if (filters.find("query") != filters.end())
        {
            query = filters["query"];
            fields = {"payment_method", "payment_status", "transaction_id"};
        }
        
        auto payments = orderPaymentService->searchPayments(query, fields);
        
        int totalPayments = static_cast<int>(payments.size());
        int startIdx = (page - 1) * pageSize;
        int endIdx = std::min(startIdx + pageSize, totalPayments);
        
        Poco::JSON::Array paymentsArray;
        for (int i = startIdx; i < endIdx; ++i)
        {
            if (payments[i])
            {
                paymentsArray.add(payments[i]->toJson());
            }
        }
        
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object();
        dataPtr->set("payments", paymentsArray);
        dataPtr->set("page", page);
        dataPtr->set("pageSize", pageSize);
        dataPtr->set("totalPayments", totalPayments);
        dataPtr->set("totalPages", (totalPayments + pageSize - 1) / pageSize);
        
        sendSuccessResponse(response, "Payments search completed", dataPtr);
    }
    catch (const std::exception& e)
    {
        sendErrorResponse(response, "Failed to search payments: " + std::string(e.what()), 
                        Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
}

void OrderPaymentController::handleGetPaymentStatistics(Poco::Net::HTTPServerRequest& request, 
                                                      Poco::Net::HTTPServerResponse& response)
{
    auto currentUserRole = getCurrentUserRole(request);
    if (currentUserRole != database::models::UserRole::ADMIN && 
        currentUserRole != database::models::UserRole::MANAGER && 
        currentUserRole != database::models::UserRole::AUDITOR)
    {
        sendForbiddenResponse(response, "You don't have permission to view payment statistics");
        return;
    }
    
    try
    {
        auto statistics = orderPaymentService->getPaymentStatistics();
        
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object(statistics.toJson());
        sendSuccessResponse(response, "Payment statistics retrieved", dataPtr);
    }
    catch (const std::exception& e)
    {
        sendErrorResponse(response, "Failed to get payment statistics: " + std::string(e.what()), 
                        Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
}

void OrderPaymentController::handleGetPaymentReport(Poco::Net::HTTPServerRequest& request, 
                                                  Poco::Net::HTTPServerResponse& response)
{
    auto currentUserRole = getCurrentUserRole(request);
    if (currentUserRole != database::models::UserRole::ADMIN && 
        currentUserRole != database::models::UserRole::MANAGER && 
        currentUserRole != database::models::UserRole::AUDITOR)
    {
        sendForbiddenResponse(response, "You don't have permission to view payment reports");
        return;
    }
    
    std::string startDate = getQueryParameter(request.getURI(), "startDate");
    std::string endDate = getQueryParameter(request.getURI(), "endDate");
    
    if (startDate.empty())
    {
        startDate = utils::DateUtils::formatDate(utils::DateUtils::addDays(utils::DateUtils::now(), -30));
    }
    
    if (endDate.empty())
    {
        endDate = utils::DateUtils::formatDate(utils::DateUtils::now());
    }
    
    if ((!startDate.empty() && !utils::Validator::isValidDate(startDate)) ||
        (!endDate.empty() && !utils::Validator::isValidDate(endDate)))
    {
        sendErrorResponse(response, "Invalid date format. Use YYYY-MM-DD", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    try
    {
        auto report = orderPaymentService->getPaymentReport(startDate, endDate);
        
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object;
        dataPtr->set("report", report);
        dataPtr->set("startDate", startDate);
        dataPtr->set("endDate", endDate);
        
        sendSuccessResponse(response, "Payment report generated", dataPtr);
    }
    catch (const std::exception& e)
    {
        sendErrorResponse(response, "Failed to generate payment report: " + std::string(e.what()), 
                        Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
}

void OrderPaymentController::handleGetPendingPayments(Poco::Net::HTTPServerRequest& request, 
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
    if (currentUserRole != database::models::UserRole::ADMIN && 
        currentUserRole != database::models::UserRole::MANAGER && 
        currentUserRole != database::models::UserRole::AUDITOR)
    {
        sendForbiddenResponse(response, "You don't have permission to view pending payments");
        return;
    }
    
    try
    {
        auto payments = orderPaymentService->getPendingPayments();
        
        int totalPayments = static_cast<int>(payments.size());
        int startIdx = (page - 1) * pageSize;
        int endIdx = std::min(startIdx + pageSize, totalPayments);
        
        Poco::JSON::Array paymentsArray;
        for (int i = startIdx; i < endIdx; ++i)
        {
            if (payments[i])
            {
                paymentsArray.add(payments[i]->toJson());
            }
        }
        
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object;
        dataPtr->set("payments", paymentsArray);
        dataPtr->set("page", page);
        dataPtr->set("pageSize", pageSize);
        dataPtr->set("totalPayments", totalPayments);
        dataPtr->set("totalPages", (totalPayments + pageSize - 1) / pageSize);
        
        sendSuccessResponse(response, "Pending payments retrieved successfully", dataPtr);
    }
    catch (const std::exception& e)
    {
        sendErrorResponse(response, "Failed to get pending payments: " + std::string(e.what()), 
                        Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
}

void OrderPaymentController::handleGetPaidPayments(Poco::Net::HTTPServerRequest& request, 
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
    if (currentUserRole != database::models::UserRole::ADMIN && 
        currentUserRole != database::models::UserRole::MANAGER && 
        currentUserRole != database::models::UserRole::AUDITOR)
    {
        sendForbiddenResponse(response, "You don't have permission to view paid payments");
        return;
    }
    
    try
    {
        auto payments = orderPaymentService->getPaidPayments();
        
        int totalPayments = static_cast<int>(payments.size());
        int startIdx = (page - 1) * pageSize;
        int endIdx = std::min(startIdx + pageSize, totalPayments);
        
        Poco::JSON::Array paymentsArray;
        for (int i = startIdx; i < endIdx; ++i)
        {
            if (payments[i])
            {
                paymentsArray.add(payments[i]->toJson());
            }
        }
        
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object;
        dataPtr->set("payments", paymentsArray);
        dataPtr->set("page", page);
        dataPtr->set("pageSize", pageSize);
        dataPtr->set("totalPayments", totalPayments);
        dataPtr->set("totalPages", (totalPayments + pageSize - 1) / pageSize);
        
        sendSuccessResponse(response, "Paid payments retrieved successfully", dataPtr);
    }
    catch (const std::exception& e)
    {
        sendErrorResponse(response, "Failed to get paid payments: " + std::string(e.what()), 
                        Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
}

void OrderPaymentController::handleGetFailedPayments(Poco::Net::HTTPServerRequest& request, 
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
    if (currentUserRole != database::models::UserRole::ADMIN && 
        currentUserRole != database::models::UserRole::MANAGER && 
        currentUserRole != database::models::UserRole::AUDITOR)
    {
        sendForbiddenResponse(response, "You don't have permission to view failed payments");
        return;
    }
    
    try
    {
        auto payments = orderPaymentService->getFailedPayments();
        
        int totalPayments = static_cast<int>(payments.size());
        int startIdx = (page - 1) * pageSize;
        int endIdx = std::min(startIdx + pageSize, totalPayments);
        
        Poco::JSON::Array paymentsArray;
        for (int i = startIdx; i < endIdx; ++i)
        {
            if (payments[i])
            {
                paymentsArray.add(payments[i]->toJson());
            }
        }
        
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object;
        dataPtr->set("payments", paymentsArray);
        dataPtr->set("page", page);
        dataPtr->set("pageSize", pageSize);
        dataPtr->set("totalPayments", totalPayments);
        dataPtr->set("totalPages", (totalPayments + pageSize - 1) / pageSize);
        
        sendSuccessResponse(response, "Failed payments retrieved successfully", dataPtr);
    }
    catch (const std::exception& e)
    {
        sendErrorResponse(response, "Failed to get failed payments: " + std::string(e.what()), 
                        Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
}

void OrderPaymentController::handleGetRefundedPayments(Poco::Net::HTTPServerRequest& request, 
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
    if (currentUserRole != database::models::UserRole::ADMIN && 
        currentUserRole != database::models::UserRole::MANAGER && 
        currentUserRole != database::models::UserRole::AUDITOR)
    {
        sendForbiddenResponse(response, "You don't have permission to view refunded payments");
        return;
    }
    
    try
    {
        auto payments = orderPaymentService->getRefundedPayments();
        
        int totalPayments = static_cast<int>(payments.size());
        int startIdx = (page - 1) * pageSize;
        int endIdx = std::min(startIdx + pageSize, totalPayments);
        
        Poco::JSON::Array paymentsArray;
        for (int i = startIdx; i < endIdx; ++i)
        {
            if (payments[i])
            {
                paymentsArray.add(payments[i]->toJson());
            }
        }
        
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object;
        dataPtr->set("payments", paymentsArray);
        dataPtr->set("page", page);
        dataPtr->set("pageSize", pageSize);
        dataPtr->set("totalPayments", totalPayments);
        dataPtr->set("totalPages", (totalPayments + pageSize - 1) / pageSize);
        
        sendSuccessResponse(response, "Refunded payments retrieved successfully", dataPtr);
    }
    catch (const std::exception& e)
    {
        sendErrorResponse(response, "Failed to get refunded payments: " + std::string(e.what()), 
                        Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
}

void OrderPaymentController::handleGetOverduePayments(Poco::Net::HTTPServerRequest& request, 
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
    if (currentUserRole != database::models::UserRole::ADMIN && 
        currentUserRole != database::models::UserRole::MANAGER && 
        currentUserRole != database::models::UserRole::AUDITOR)
    {
        sendForbiddenResponse(response, "You don't have permission to view overdue payments");
        return;
    }
    
    int daysThreshold = 7;
    std::string daysStr = getQueryParameter(request.getURI(), "days");
    if (!daysStr.empty())
    {
        try
        {
            daysThreshold = std::stoi(daysStr);
            if (daysThreshold <= 0)
            {
                sendErrorResponse(response, "Days threshold must be greater than 0", 
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
    
    try
    {
        auto payments = orderPaymentService->getOverduePayments(daysThreshold);
        
        int totalPayments = static_cast<int>(payments.size());
        int startIdx = (page - 1) * pageSize;
        int endIdx = std::min(startIdx + pageSize, totalPayments);
        
        Poco::JSON::Array paymentsArray;
        for (int i = startIdx; i < endIdx; ++i)
        {
            if (payments[i])
            {
                paymentsArray.add(payments[i]->toJson());
            }
        }
        
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object;
        dataPtr->set("payments", paymentsArray);
        dataPtr->set("page", page);
        dataPtr->set("pageSize", pageSize);
        dataPtr->set("totalPayments", totalPayments);
        dataPtr->set("totalPages", (totalPayments + pageSize - 1) / pageSize);
        dataPtr->set("daysThreshold", daysThreshold);
        
        sendSuccessResponse(response, "Overdue payments retrieved successfully", dataPtr);
    }
    catch (const std::exception& e)
    {
        sendErrorResponse(response, "Failed to get overdue payments: " + std::string(e.what()), 
                        Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
}

void OrderPaymentController::handleMarkAsPaid(Poco::Net::HTTPServerRequest& request, 
                                            Poco::Net::HTTPServerResponse& response)
{
    std::string paymentIdStr = getPathParameter(request.getURI(), "/api/v1/payments/", 0);
    paymentIdStr = paymentIdStr.substr(0, paymentIdStr.find("/paid"));
    
    if (paymentIdStr.empty())
    {
        sendErrorResponse(response, "Payment ID is required", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    long long paymentId;
    try
    {
        paymentId = std::stoll(paymentIdStr);
    }
    catch (...)
    {
        sendErrorResponse(response, "Invalid payment ID", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    std::string authError;
    long long currentUserId = getCurrentUserId(request);
    auto currentUserRole = getCurrentUserRole(request);
    
    if (!orderPaymentService->canUserUpdatePaymentStatus(currentUserId, currentUserRole, paymentId))
    {
        sendForbiddenResponse(response, "You don't have permission to mark payment as paid");
        return;
    }
    
    std::string transactionId;
    auto json = parseJsonBody(request);
    if (json && json->has("transactionId"))
    {
        transactionId = utils::JsonUtils::getString(*json, "transactionId");
    }
    
    try
    {
        bool success = orderPaymentService->markPaymentAsPaid(paymentId, transactionId, currentUserId);
        if (!success)
        {
            sendErrorResponse(response, "Failed to mark payment as paid", 
                            Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
            return;
        }
        
        auto updatedPayment = orderPaymentService->getPaymentById(paymentId);
        if (!updatedPayment)
        {
            sendErrorResponse(response, "Payment marked as paid but not found", 
                            Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
            return;
        }
        
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object(updatedPayment->toJson());
        sendSuccessResponse(response, "Payment marked as paid successfully", dataPtr);
    }
    catch (const std::exception& e)
    {
        sendErrorResponse(response, "Failed to mark payment as paid: " + std::string(e.what()), 
                        Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
}

void OrderPaymentController::handleMarkAsFailed(Poco::Net::HTTPServerRequest& request, 
                                              Poco::Net::HTTPServerResponse& response)
{
    std::string paymentIdStr = getPathParameter(request.getURI(), "/api/v1/payments/", 0);
    paymentIdStr = paymentIdStr.substr(0, paymentIdStr.find("/failed"));
    
    if (paymentIdStr.empty())
    {
        sendErrorResponse(response, "Payment ID is required", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    long long paymentId;
    try
    {
        paymentId = std::stoll(paymentIdStr);
    }
    catch (...)
    {
        sendErrorResponse(response, "Invalid payment ID", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    std::string authError;
    long long currentUserId = getCurrentUserId(request);
    auto currentUserRole = getCurrentUserRole(request);
    
    if (!orderPaymentService->canUserUpdatePaymentStatus(currentUserId, currentUserRole, paymentId))
    {
        sendForbiddenResponse(response, "You don't have permission to mark payment as failed");
        return;
    }
    
    std::string transactionId;
    auto json = parseJsonBody(request);
    if (json && json->has("transactionId"))
    {
        transactionId = utils::JsonUtils::getString(*json, "transactionId");
    }
    
    try
    {
        bool success = orderPaymentService->markPaymentAsFailed(paymentId, transactionId, currentUserId);
        if (!success)
        {
            sendErrorResponse(response, "Failed to mark payment as failed", 
                            Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
            return;
        }
        
        auto updatedPayment = orderPaymentService->getPaymentById(paymentId);
        if (!updatedPayment)
        {
            sendErrorResponse(response, "Payment marked as failed but not found", 
                            Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
            return;
        }
        
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object(updatedPayment->toJson());
        sendSuccessResponse(response, "Payment marked as failed successfully", dataPtr);
    }
    catch (const std::exception& e)
    {
        sendErrorResponse(response, "Failed to mark payment as failed: " + std::string(e.what()), 
                        Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
}

void OrderPaymentController::handleMarkAsRefunded(Poco::Net::HTTPServerRequest& request, 
                                                Poco::Net::HTTPServerResponse& response)
{
    std::string paymentIdStr = getPathParameter(request.getURI(), "/api/v1/payments/", 0);
    paymentIdStr = paymentIdStr.substr(0, paymentIdStr.find("/refunded"));
    
    if (paymentIdStr.empty())
    {
        sendErrorResponse(response, "Payment ID is required", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    long long paymentId;
    try
    {
        paymentId = std::stoll(paymentIdStr);
    }
    catch (...)
    {
        sendErrorResponse(response, "Invalid payment ID", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    std::string authError;
    long long currentUserId = getCurrentUserId(request);
    auto currentUserRole = getCurrentUserRole(request);
    
    if (!orderPaymentService->canUserUpdatePaymentStatus(currentUserId, currentUserRole, paymentId))
    {
        sendForbiddenResponse(response, "You don't have permission to mark payment as refunded");
        return;
    }
    
    std::string transactionId;
    auto json = parseJsonBody(request);
    if (json && json->has("transactionId"))
    {
        transactionId = utils::JsonUtils::getString(*json, "transactionId");
    }
    
    try
    {
        bool success = orderPaymentService->markPaymentAsRefunded(paymentId, transactionId, currentUserId);
        if (!success)
        {
            sendErrorResponse(response, "Failed to mark payment as refunded", 
                            Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
            return;
        }
        
        auto updatedPayment = orderPaymentService->getPaymentById(paymentId);
        if (!updatedPayment)
        {
            sendErrorResponse(response, "Payment marked as refunded but not found", 
                            Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
            return;
        }
        
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object(updatedPayment->toJson());
        sendSuccessResponse(response, "Payment marked as refunded successfully", dataPtr);
    }
    catch (const std::exception& e)
    {
        sendErrorResponse(response, "Failed to mark payment as refunded: " + std::string(e.what()), 
                        Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
}

void OrderPaymentController::handleMarkAsPending(Poco::Net::HTTPServerRequest& request, 
                                               Poco::Net::HTTPServerResponse& response)
{
    std::string paymentIdStr = getPathParameter(request.getURI(), "/api/v1/payments/", 0);
    paymentIdStr = paymentIdStr.substr(0, paymentIdStr.find("/pending"));
    
    if (paymentIdStr.empty())
    {
        sendErrorResponse(response, "Payment ID is required", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    long long paymentId;
    try
    {
        paymentId = std::stoll(paymentIdStr);
    }
    catch (...)
    {
        sendErrorResponse(response, "Invalid payment ID", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    std::string authError;
    long long currentUserId = getCurrentUserId(request);
    auto currentUserRole = getCurrentUserRole(request);
    
    if (!orderPaymentService->canUserUpdatePaymentStatus(currentUserId, currentUserRole, paymentId))
    {
        sendForbiddenResponse(response, "You don't have permission to mark payment as pending");
        return;
    }
    
    try
    {
        bool success = orderPaymentService->markPaymentAsPending(paymentId, currentUserId);
        if (!success)
        {
            sendErrorResponse(response, "Failed to mark payment as pending", 
                            Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
            return;
        }
        
        auto updatedPayment = orderPaymentService->getPaymentById(paymentId);
        if (!updatedPayment)
        {
            sendErrorResponse(response, "Payment marked as pending but not found", 
                            Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
            return;
        }
        
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object(updatedPayment->toJson());
        sendSuccessResponse(response, "Payment marked as pending successfully", dataPtr);
    }
    catch (const std::exception& e)
    {
        sendErrorResponse(response, "Failed to mark payment as pending: " + std::string(e.what()), 
                        Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
}

void OrderPaymentController::handleProcessPayment(Poco::Net::HTTPServerRequest& request, 
                                                Poco::Net::HTTPServerResponse& response)
{
    std::string paymentIdStr = getPathParameter(request.getURI(), "/api/v1/payments/", 0);
    paymentIdStr = paymentIdStr.substr(0, paymentIdStr.find("/process"));
    
    if (paymentIdStr.empty())
    {
        sendErrorResponse(response, "Payment ID is required", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    long long paymentId;
    try
    {
        paymentId = std::stoll(paymentIdStr);
    }
    catch (...)
    {
        sendErrorResponse(response, "Invalid payment ID", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    std::string authError;
    long long currentUserId = getCurrentUserId(request);
    auto currentUserRole = getCurrentUserRole(request);
    
    if (!orderPaymentService->canUserUpdatePaymentStatus(currentUserId, currentUserRole, paymentId))
    {
        sendForbiddenResponse(response, "You don't have permission to process payment");
        return;
    }
    
    try
    {
        bool success = orderPaymentService->processPayment(paymentId, currentUserId);
        if (!success)
        {
            sendErrorResponse(response, "Failed to process payment", 
                            Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
            return;
        }
        
        auto updatedPayment = orderPaymentService->getPaymentById(paymentId);
        if (!updatedPayment)
        {
            sendErrorResponse(response, "Payment processed but not found", 
                            Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
            return;
        }
        
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object(updatedPayment->toJson());
        sendSuccessResponse(response, "Payment processed successfully", dataPtr);
    }
    catch (const std::exception& e)
    {
        sendErrorResponse(response, "Failed to process payment: " + std::string(e.what()), 
                        Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
}

void OrderPaymentController::handleRefundPayment(Poco::Net::HTTPServerRequest& request, 
                                               Poco::Net::HTTPServerResponse& response)
{
    std::string paymentIdStr = getPathParameter(request.getURI(), "/api/v1/payments/", 0);
    paymentIdStr = paymentIdStr.substr(0, paymentIdStr.find("/refund"));
    
    if (paymentIdStr.empty())
    {
        sendErrorResponse(response, "Payment ID is required", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    long long paymentId;
    try
    {
        paymentId = std::stoll(paymentIdStr);
    }
    catch (...)
    {
        sendErrorResponse(response, "Invalid payment ID", 
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
    
    std::string reason = utils::JsonUtils::getString(*json, "reason", "");
    if (reason.empty())
    {
        sendErrorResponse(response, "Refund reason is required", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    std::string authError;
    long long currentUserId = getCurrentUserId(request);
    auto currentUserRole = getCurrentUserRole(request);
    
    if (!orderPaymentService->canUserUpdatePaymentStatus(currentUserId, currentUserRole, paymentId))
    {
        sendForbiddenResponse(response, "You don't have permission to refund payment");
        return;
    }
    
    try
    {
        bool success = orderPaymentService->refundPayment(paymentId, reason, currentUserId);
        if (!success)
        {
            sendErrorResponse(response, "Failed to refund payment", 
                            Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
            return;
        }
        
        auto updatedPayment = orderPaymentService->getPaymentById(paymentId);
        if (!updatedPayment)
        {
            sendErrorResponse(response, "Payment refunded but not found", 
                            Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
            return;
        }
        
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object(updatedPayment->toJson());
        sendSuccessResponse(response, "Payment refunded successfully", dataPtr);
    }
    catch (const std::exception& e)
    {
        sendErrorResponse(response, "Failed to refund payment: " + std::string(e.what()), 
                        Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
}

bool OrderPaymentController::validateRequest(Poco::Net::HTTPServerRequest& request, 
                                           Poco::Net::HTTPServerResponse& response,
                                           std::string& errorMessage)
{
    if (!BaseController::validateRequest(request, response, errorMessage))
    {
        return false;
    }
    
    std::string method = request.getMethod();
    if (method == "POST" || method == "PUT")
    {
        std::string contentType = request.getContentType();
        if (contentType.find("application/json") == std::string::npos)
        {
            errorMessage = "Content-Type must be application/json";
            return false;
        }
    }
    
    return true;
}

bool OrderPaymentController::authorizeRequest(Poco::Net::HTTPServerRequest& request, 
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

bool OrderPaymentController::validatePaymentAccess(Poco::Net::HTTPServerRequest& request, 
                                                 long long paymentId,
                                                 std::string& errorMessage)
{
    long long currentUserId = getCurrentUserId(request);
    auto currentUserRole = getCurrentUserRole(request);
    
    return orderPaymentService->canUserViewPayment(currentUserId, currentUserRole, paymentId);
}

long long OrderPaymentController::getCurrentUserId(Poco::Net::HTTPServerRequest& request)
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

database::models::UserRole OrderPaymentController::getCurrentUserRole(Poco::Net::HTTPServerRequest& request)
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

bool OrderPaymentController::validateCreatePaymentData(const Poco::JSON::Object::Ptr& json, 
                                                     std::vector<std::string>& errors)
{
    errors.clear();
    
    std::vector<std::string> requiredFields = {"order_id", "payment_method", "amount"};
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
    
    if (json->has("payment_method"))
    {
        std::string paymentMethod = utils::JsonUtils::getString(*json, "payment_method");
        if (paymentMethod.empty() || paymentMethod.length() > 50)
        {
            errors.push_back("Payment method must be between 1 and 50 characters");
        }
    }
    
    if (json->has("amount"))
    {
        double amount = utils::JsonUtils::getDouble(*json, "amount");
        if (amount <= 0)
        {
            errors.push_back("Amount must be greater than 0");
        }
    }
    
    if (json->has("payment_status"))
    {
        std::string status = utils::JsonUtils::getString(*json, "payment_status");
        std::vector<std::string> validStatuses = {"pending", "paid", "failed", "refunded", "processing"};
        if (std::find(validStatuses.begin(), validStatuses.end(), status) == validStatuses.end())
        {
            errors.push_back("Invalid payment status. Valid values: pending, paid, failed, refunded, processing");
        }
    }
    
    if (json->has("transaction_id"))
    {
        std::string transactionId = utils::JsonUtils::getString(*json, "transaction_id");
        if (transactionId.length() > 100)
        {
            errors.push_back("Transaction ID must be 100 characters or less");
        }
        
        try
        {
            if (orderPaymentService->transactionIdExists(transactionId))
            {
                errors.push_back("Transaction ID already exists");
            }
        }
        catch (...)
        {
        }
    }
    
    return errors.empty();
}

bool OrderPaymentController::validateUpdatePaymentData(const Poco::JSON::Object::Ptr& json, 
                                                     std::vector<std::string>& errors)
{
    errors.clear();
    
    bool hasPaymentMethod = json->has("payment_method") && !json->get("payment_method").isEmpty();
    bool hasAmount = json->has("amount") && !json->get("amount").isEmpty();
    bool hasTransactionId = json->has("transaction_id") && !json->get("transaction_id").isEmpty();
    bool hasPaymentDate = json->has("payment_date") && !json->get("payment_date").isEmpty();
    bool hasPaymentStatus = json->has("payment_status") && !json->get("payment_status").isEmpty();
    
    if (!hasPaymentMethod && !hasAmount && !hasTransactionId && !hasPaymentDate && !hasPaymentStatus)
    {
        errors.push_back("At least one field must be provided for update");
    }
    
    if (hasPaymentMethod)
    {
        std::string paymentMethod = utils::JsonUtils::getString(*json, "payment_method");
        if (paymentMethod.empty() || paymentMethod.length() > 50)
        {
            errors.push_back("Payment method must be between 1 and 50 characters");
        }
    }
    
    if (hasAmount)
    {
        double amount = utils::JsonUtils::getDouble(*json, "amount");
        if (amount <= 0)
        {
            errors.push_back("Amount must be greater than 0");
        }
    }
    
    if (hasTransactionId)
    {
        std::string transactionId = utils::JsonUtils::getString(*json, "transaction_id");
        if (transactionId.length() > 100)
        {
            errors.push_back("Transaction ID must be 100 characters or less");
        }
        
        try
        {
        }
        catch (...)
        {
        }
    }
    
    if (hasPaymentDate)
    {
        std::string paymentDate = utils::JsonUtils::getString(*json, "payment_date");
        if (!utils::Validator::isValidDateTime(paymentDate))
        {
            errors.push_back("Invalid payment date format. Use YYYY-MM-DD HH:MM:SS");
        }
    }
    
    if (hasPaymentStatus)
    {
        std::string status = utils::JsonUtils::getString(*json, "payment_status");
        std::vector<std::string> validStatuses = {"pending", "paid", "failed", "refunded", "processing"};
        if (std::find(validStatuses.begin(), validStatuses.end(), status) == validStatuses.end())
        {
            errors.push_back("Invalid payment status. Valid values: pending, paid, failed, refunded, processing");
        }
    }
    
    return errors.empty();
}

bool OrderPaymentController::validateUpdatePaymentStatusData(const Poco::JSON::Object::Ptr& json, 
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
        std::vector<std::string> validStatuses = {"pending", "paid", "failed", "refunded", "processing"};
        if (std::find(validStatuses.begin(), validStatuses.end(), status) == validStatuses.end())
        {
            errors.push_back("Invalid payment status. Valid values: pending, paid, failed, refunded, processing");
        }
    }
    
    if (json->has("transaction_id"))
    {
        std::string transactionId = utils::JsonUtils::getString(*json, "transaction_id");
        if (transactionId.length() > 100)
        {
            errors.push_back("Transaction ID must be 100 characters or less");
        }
    }
    
    if (json->has("payment_date"))
    {
        std::string paymentDate = utils::JsonUtils::getString(*json, "payment_date");
        if (!utils::Validator::isValidDateTime(paymentDate))
        {
            errors.push_back("Invalid payment date format. Use YYYY-MM-DD HH:MM:SS");
        }
    }
    
    return errors.empty();
}

bool OrderPaymentController::validateSearchParameters(const std::map<std::string, std::string>& filters,
                                                    std::vector<std::string>& errors)
{
    errors.clear();
    
    for (const auto& [key, value] : filters)
    {
        if (key == "status")
        {
            std::vector<std::string> validStatuses = {"pending", "paid", "failed", "refunded", "processing"};
            if (std::find(validStatuses.begin(), validStatuses.end(), value) == validStatuses.end())
            {
                errors.push_back("Invalid payment status");
            }
        }
        else if (key == "payment_method")
        {
            if (value.empty())
            {
                errors.push_back("Payment method cannot be empty");
            }
        }
        else if (key == "start_date" || key == "end_date")
        {
            if (!utils::Validator::isValidDate(value))
            {
                errors.push_back(key + " must be in YYYY-MM-DD format");
            }
        }
        else if (key == "min_amount")
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
        else if (key == "max_amount")
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
        else if (key == "order_id")
        {
            try
            {
                long long orderId = std::stoll(value);
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
    }
    
    return errors.empty();
}

void OrderPaymentController::logPaymentEvent(long long userId, 
                                           const std::string& action,
                                           long long paymentId,
                                           const std::string& ipAddress,
                                           const std::string& userAgent,
                                           bool success,
                                           const std::string& details)
{
    try
    {
        orderPaymentService->logPaymentEvent(userId, action, paymentId, 
                                           ipAddress, userAgent, success, details);
    }
    catch (...)
    {
        std::string timestamp = utils::DateUtils::formatDateTime(utils::DateUtils::now());
        std::string status = success ? "SUCCESS" : "FAILURE";
        
        std::cout << "[" << timestamp << "] "
                  << "PAYMENT " << action << " "
                  << "UserID: " << userId << " "
                  << "PaymentID: " << paymentId << " "
                  << "IP: " << ipAddress << " "
                  << "Success: " << status << " "
                  << "Details: " << details << std::endl;
    }
}

std::unique_ptr<database::models::OrderPayment> OrderPaymentController::extractPaymentFromJson(const Poco::JSON::Object::Ptr& json)
{
    if (!json)
    {
        return nullptr;
    }
    
    try
    {
        auto payment = std::make_unique<database::models::OrderPayment>(*json);
        return payment;
    }
    catch (...)
    {
        return nullptr;
    }
}

bool OrderPaymentController::canCreatePayment(Poco::Net::HTTPServerRequest& request, 
                                            const database::models::OrderPayment& paymentData,
                                            std::string& errorMessage)
{
    long long currentUserId = getCurrentUserId(request);
    auto currentUserRole = getCurrentUserRole(request);
    
    return orderPaymentService->canUserCreatePayment(currentUserId, currentUserRole, paymentData);
}

bool OrderPaymentController::canUpdatePayment(Poco::Net::HTTPServerRequest& request, 
                                            long long paymentId,
                                            const database::models::OrderPayment& paymentData,
                                            std::string& errorMessage)
{
    long long currentUserId = getCurrentUserId(request);
    auto currentUserRole = getCurrentUserRole(request);
    
    return orderPaymentService->canUserUpdatePayment(currentUserId, currentUserRole, paymentId);
}

bool OrderPaymentController::canDeletePayment(Poco::Net::HTTPServerRequest& request, 
                                            long long paymentId,
                                            std::string& errorMessage)
{
    long long currentUserId = getCurrentUserId(request);
    auto currentUserRole = getCurrentUserRole(request);
    
    return orderPaymentService->canUserDeletePayment(currentUserId, currentUserRole, paymentId);
}

bool OrderPaymentController::canUpdatePaymentStatus(Poco::Net::HTTPServerRequest& request, 
                                                  long long paymentId,
                                                  const std::string& newStatus,
                                                  std::string& errorMessage)
{
    long long currentUserId = getCurrentUserId(request);
    auto currentUserRole = getCurrentUserRole(request);
    
    return orderPaymentService->canUserUpdatePaymentStatus(currentUserId, currentUserRole, paymentId);
}

bool OrderPaymentController::canViewPayment(Poco::Net::HTTPServerRequest& request, 
                                          long long paymentId,
                                          std::string& errorMessage)
{
    long long currentUserId = getCurrentUserId(request);
    auto currentUserRole = getCurrentUserRole(request);
    
    return orderPaymentService->canUserViewPayment(currentUserId, currentUserRole, paymentId);
}

Poco::JSON::Object OrderPaymentController::buildPaginationResponse(int page, int pageSize, int totalItems, 
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
