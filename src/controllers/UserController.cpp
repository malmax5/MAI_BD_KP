#include "UserController.hpp"
#include "../utils/JsonUtils.hpp"
#include "../utils/Validator.hpp"
#include <Poco/Net/HTTPRequestHandler.h>
#include <Poco/Net/HTTPServerRequest.h>
#include <Poco/Net/HTTPServerResponse.h>
#include <Poco/JSON/Parser.h>
#include <Poco/StreamCopier.h>
#include <Poco/URI.h>
#include <iostream>

namespace warehouse_backend::controllers
{

UserController::UserController()
    : userService(std::make_unique<services::UserService>()),
      authService(std::make_unique<services::AuthService>())
{
}

void UserController::handleRequest(Poco::Net::HTTPServerRequest& request, 
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
        
        std::string userIdStr = getPathParameter(uri, "/api/v1/users/", 0);
        if (userIdStr.empty())
        {
            userIdStr = getPathParameter(uri, "/api/users/", 0);
        }
        
        if ((endpoint == "/api/v1/users/statistics" || endpoint == "/api/users/statistics") && request.getMethod() == "GET")
        {
            handleGetUserStatistics(request, response);
        }
        else if ((endpoint == "/api/v1/users" || endpoint == "/api/users") && request.getMethod() == "GET")
        {
            handleGetUsers(request, response);
        }
        else if ((endpoint == "/api/v1/users" || endpoint == "/api/users") && request.getMethod() == "POST")
        {
            handleCreateUser(request, response);
        }
        else if ((endpoint.find("/api/v1/users/") == 0 || endpoint.find("/api/users/") == 0) && 
                 !userIdStr.empty() && request.getMethod() == "GET")
        {
            handleGetUserById(request, response);
        }
        else if ((endpoint.find("/api/v1/users/") == 0 || endpoint.find("/api/users/") == 0) && 
                 !userIdStr.empty() && request.getMethod() == "PUT")
        {
            handleUpdateUser(request, response);
        }
        else if ((endpoint.find("/api/v1/users/") == 0 || endpoint.find("/api/users/") == 0) && 
                 !userIdStr.empty() && request.getMethod() == "DELETE")
        {
            handleDeleteUser(request, response);
        }
        else if ((endpoint.find("/api/v1/users/") == 0 || endpoint.find("/api/users/") == 0) && 
                 endpoint.find("/soft-delete") != std::string::npos && request.getMethod() == "POST")
        {
            handleSoftDeleteUser(request, response);
        }
        else if ((endpoint.find("/api/v1/users/") == 0 || endpoint.find("/api/users/") == 0) && 
                 endpoint.find("/activate") != std::string::npos && request.getMethod() == "POST")
        {
            handleActivateUser(request, response);
        }
        else if ((endpoint.find("/api/v1/users/") == 0 || endpoint.find("/api/users/") == 0) && 
                 endpoint.find("/deactivate") != std::string::npos && request.getMethod() == "POST")
        {
            handleDeactivateUser(request, response);
        }
        else if ((endpoint.find("/api/v1/users/") == 0 || endpoint.find("/api/users/") == 0) && 
                 endpoint.find("/change-role") != std::string::npos && request.getMethod() == "POST")
        {
            handleChangeUserRole(request, response);
        }
        else if ((endpoint.find("/api/v1/users/search") == 0 || endpoint.find("/api/users/search") == 0) && request.getMethod() == "POST")
        {
            handleSearchUsers(request, response);
        }
        else if ((endpoint == "/api/v1/users/role-statistics" || endpoint == "/api/users/role-statistics") && request.getMethod() == "GET")
        {
            handleGetRoleStatistics(request, response);
        }
        else if ((endpoint == "/api/v1/users/activity-report" || endpoint == "/api/users/activity-report") && request.getMethod() == "GET")
        {
            handleGetUserActivityReport(request, response);
        }
        else if ((endpoint == "/api/v1/users/import" || endpoint == "/api/users/import") && request.getMethod() == "POST")
        {
            handleImportUsers(request, response);
        }
        else if ((endpoint == "/api/v1/users/export" || endpoint == "/api/users/export") && request.getMethod() == "GET")
        {
            handleExportUsers(request, response);
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

void UserController::handleGetUsers(Poco::Net::HTTPServerRequest& request, 
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
    
    long long currentUserId = getCurrentUserId(request);
    auto currentUserRole = getCurrentUserRole(request);
    
    if (currentUserRole != database::models::UserRole::ADMIN)
    {
        filters["isActive"] = "true";
    }
    
    Poco::JSON::Object usersJson = userService->getPaginatedUsers(page, pageSize);

    Poco::JSON::Object::Ptr usersJsonPtr = new Poco::JSON::Object(usersJson);
    
    sendSuccessResponse(response, "Users retrieved successfully", usersJsonPtr);
}

void UserController::handleGetUserById(Poco::Net::HTTPServerRequest& request, 
                                      Poco::Net::HTTPServerResponse& response)
{
    std::string userIdStr = getPathParameter(request.getURI(), "/api/v1/users/", 0);
    if (userIdStr.empty())
    {
        sendErrorResponse(response, "User ID is required", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    long long userId;
    try
    {
        userId = std::stoll(userIdStr);
    }
    catch (...)
    {
        sendErrorResponse(response, "Invalid user ID", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    std::string accessError;
    if (!validateUserAccess(request, userId, accessError))
    {
        sendForbiddenResponse(response, accessError);
        return;
    }
    
    auto result = userService->getUserById(userId);
    
    if (result.success)
    {
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object(result.user->toJson());
        sendSuccessResponse(response, "User retrieved successfully", dataPtr);
    }
    else
    {
        sendNotFoundResponse(response, "User");
    }
}

void UserController::handleCreateUser(Poco::Net::HTTPServerRequest& request, 
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
    if (!validateCreateUserData(json, errors))
    {
        sendValidationErrorResponse(response, errors);
        return;
    }
    
    auto userData = extractUserFromJson(json);
    if (!userData)
    {
        sendErrorResponse(response, "Invalid user data", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    std::string authError;
    if (!canCreateUser(request, *userData, authError))
    {
        sendForbiddenResponse(response, authError);
        return;
    }
    
    long long createdBy = getCurrentUserId(request);
    
    std::string ipAddress = getClientIpAddress(request);
    std::string userAgent = getUserAgent(request);
    
    auto result = userService->createUser(*userData, createdBy, ipAddress, userAgent);
    
    logUserEvent(createdBy, "CREATE_USER", result.userId, ipAddress, userAgent, 
                result.success, result.success ? "User created successfully" : result.message);
    
    if (result.success)
    {
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object(result.user->toJson());
        sendSuccessResponse(response, "User created successfully", dataPtr);
    }
    else
    {
        sendErrorResponse(response, result.message, 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
    }
}

void UserController::handleUpdateUser(Poco::Net::HTTPServerRequest& request, 
                                     Poco::Net::HTTPServerResponse& response)
{
    std::string userIdStr = getPathParameter(request.getURI(), "/api/v1/users/", 0);
    if (userIdStr.empty())
    {
        sendErrorResponse(response, "User ID is required", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    long long userId;
    try
    {
        userId = std::stoll(userIdStr);
    }
    catch (...)
    {
        sendErrorResponse(response, "Invalid user ID", 
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
    if (!validateUpdateUserData(json, errors))
    {
        sendValidationErrorResponse(response, errors);
        return;
    }
    
    auto userData = extractUserFromJson(json);
    if (!userData)
    {
        sendErrorResponse(response, "Invalid user data", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    std::string authError;
    if (!canUpdateUser(request, userId, *userData, authError))
    {
        sendForbiddenResponse(response, authError);
        return;
    }
    
    long long updatedBy = getCurrentUserId(request);
    
    std::string ipAddress = getClientIpAddress(request);
    std::string userAgent = getUserAgent(request);
    
    auto result = userService->updateUser(userId, *userData, updatedBy, ipAddress, userAgent);
    
    logUserEvent(updatedBy, "UPDATE_USER", userId, ipAddress, userAgent, 
                result.success, result.success ? "User updated successfully" : result.message);
    
    if (result.success)
    {
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object(result.user->toJson());
        sendSuccessResponse(response, "User updated successfully", dataPtr);
    }
    else
    {
        sendErrorResponse(response, result.message, 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
    }
}

void UserController::handleDeleteUser(Poco::Net::HTTPServerRequest& request, 
                                     Poco::Net::HTTPServerResponse& response)
{
    std::string userIdStr = getPathParameter(request.getURI(), "/api/v1/users/", 0);
    if (userIdStr.empty())
    {
        sendErrorResponse(response, "User ID is required", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    long long userId;
    try
    {
        userId = std::stoll(userIdStr);
    }
    catch (...)
    {
        sendErrorResponse(response, "Invalid user ID", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    std::string authError;
    if (!canDeleteUser(request, userId, authError))
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
    
    std::string ipAddress = getClientIpAddress(request);
    std::string userAgent = getUserAgent(request);
    
    auto result = userService->deleteUser(userId, deletedBy, reason, ipAddress, userAgent);
    
    logUserEvent(deletedBy, "DELETE_USER", userId, ipAddress, userAgent, 
                result.success, result.success ? "User deleted successfully" : result.message);
    
    if (result.success)
    {
        sendSuccessResponse(response, "User deleted successfully");
    }
    else
    {
        sendErrorResponse(response, result.message, 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
    }
}

void UserController::handleSoftDeleteUser(Poco::Net::HTTPServerRequest& request, 
                                         Poco::Net::HTTPServerResponse& response)
{
    std::string userIdStr = getPathParameter(request.getURI(), "/api/users/", 0);
    userIdStr = userIdStr.substr(0, userIdStr.find("/soft-delete"));
    
    if (userIdStr.empty())
    {
        sendErrorResponse(response, "User ID is required", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    long long userId;
    try
    {
        userId = std::stoll(userIdStr);
    }
    catch (...)
    {
        sendErrorResponse(response, "Invalid user ID", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    std::string authError;
    if (!canDeleteUser(request, userId, authError))
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
    
    std::string ipAddress = getClientIpAddress(request);
    std::string userAgent = getUserAgent(request);
    
    auto result = userService->softDeleteUser(userId, deletedBy, reason, ipAddress, userAgent);
    
    logUserEvent(deletedBy, "SOFT_DELETE_USER", userId, ipAddress, userAgent, 
                result.success, result.success ? "User soft-deleted successfully" : result.message);
    
    if (result.success)
    {
        sendSuccessResponse(response, "User soft-deleted successfully");
    }
    else
    {
        sendErrorResponse(response, result.message, 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
    }
}

void UserController::handleActivateUser(Poco::Net::HTTPServerRequest& request, 
                                       Poco::Net::HTTPServerResponse& response)
{
    std::string userIdStr = getPathParameter(request.getURI(), "/api/users/", 0);
    userIdStr = userIdStr.substr(0, userIdStr.find("/activate"));
    
    if (userIdStr.empty())
    {
        sendErrorResponse(response, "User ID is required", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    long long userId;
    try
    {
        userId = std::stoll(userIdStr);
    }
    catch (...)
    {
        sendErrorResponse(response, "Invalid user ID", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    auto currentUserRole = getCurrentUserRole(request);
    if (currentUserRole != database::models::UserRole::ADMIN)
    {
        sendForbiddenResponse(response, "Only admin can activate users");
        return;
    }
    
    long long activatedBy = getCurrentUserId(request);
    
    std::string ipAddress = getClientIpAddress(request);
    std::string userAgent = getUserAgent(request);
    
    auto result = userService->activateUser(userId, activatedBy, ipAddress, userAgent);
    
    logUserEvent(activatedBy, "ACTIVATE_USER", userId, ipAddress, userAgent, 
                result.success, result.success ? "User activated successfully" : result.message);
    
    if (result.success)
    {
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object(result.data);
        sendSuccessResponse(response, "User activated successfully", dataPtr);
    }
    else
    {
        sendErrorResponse(response, result.message, 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
    }
}

void UserController::handleDeactivateUser(Poco::Net::HTTPServerRequest& request, 
                                         Poco::Net::HTTPServerResponse& response)
{
    std::string userIdStr = getPathParameter(request.getURI(), "/api/users/", 0);
    userIdStr = userIdStr.substr(0, userIdStr.find("/deactivate"));
    
    if (userIdStr.empty())
    {
        sendErrorResponse(response, "User ID is required", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    long long userId;
    try
    {
        userId = std::stoll(userIdStr);
    }
    catch (...)
    {
        sendErrorResponse(response, "Invalid user ID", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    auto currentUserRole = getCurrentUserRole(request);
    auto targetUserResult = userService->getUserById(userId);
    
    if (!targetUserResult.success)
    {
        sendNotFoundResponse(response, "User");
        return;
    }
    
    bool canDeactivate = false;
    if (currentUserRole == database::models::UserRole::ADMIN)
    {
        canDeactivate = true;
    }
    else if (currentUserRole == database::models::UserRole::MANAGER)
    {
        if (targetUserResult.data.has("role"))
        {
            std::string targetRoleStr = targetUserResult.data.getValue<std::string>("role");
            auto targetRole = database::models::User::stringToRole(targetRoleStr);
            if (targetRole == database::models::UserRole::WORKER)
            {
                canDeactivate = true;
            }
        }
    }
    
    if (!canDeactivate)
    {
        sendForbiddenResponse(response, "You don't have permission to deactivate this user");
        return;
    }
    
    long long deactivatedBy = getCurrentUserId(request);
    
    std::string reason;
    auto json = parseJsonBody(request);
    if (json && json->has("reason"))
    {
        reason = utils::JsonUtils::getString(*json, "reason");
    }
    
    std::string ipAddress = getClientIpAddress(request);
    std::string userAgent = getUserAgent(request);
    
    auto result = userService->deactivateUser(userId, deactivatedBy, reason, ipAddress, userAgent);
    
    logUserEvent(deactivatedBy, "DEACTIVATE_USER", userId, ipAddress, userAgent, 
                result.success, result.success ? "User deactivated successfully" : result.message);
    
    if (result.success)
    {
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object(result.data);
        sendSuccessResponse(response, "User deactivated successfully", dataPtr);
    }
    else
    {
        sendErrorResponse(response, result.message, 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
    }
}

void UserController::handleChangeUserRole(Poco::Net::HTTPServerRequest& request, 
                                         Poco::Net::HTTPServerResponse& response)
{
    std::string userIdStr = getPathParameter(request.getURI(), "/api/users/", 0);
    userIdStr = userIdStr.substr(0, userIdStr.find("/change-role"));
    
    if (userIdStr.empty())
    {
        sendErrorResponse(response, "User ID is required", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    long long userId;
    try
    {
        userId = std::stoll(userIdStr);
    }
    catch (...)
    {
        sendErrorResponse(response, "Invalid user ID", 
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
    if (!validateChangeRoleData(json, errors))
    {
        sendValidationErrorResponse(response, errors);
        return;
    }
    
    std::string newRole = utils::JsonUtils::getString(*json, "newRole");
    std::string reason = utils::JsonUtils::getString(*json, "reason", "");
    
    std::string authError;
    if (!canChangeUserRole(request, userId, newRole, authError))
    {
        sendForbiddenResponse(response, authError);
        return;
    }
    
    long long changedBy = getCurrentUserId(request);
    
    std::string ipAddress = getClientIpAddress(request);
    std::string userAgent = getUserAgent(request);
    
    auto result = userService->changeUserRole(userId, newRole, changedBy, reason, ipAddress, userAgent);
    
    logUserEvent(changedBy, "CHANGE_USER_ROLE", userId, ipAddress, userAgent, 
                result.success, result.success ? "User role changed successfully" : result.message);
    
    if (result.success)
    {
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object(result.data);
        sendSuccessResponse(response, "User role changed successfully", dataPtr);
    }
    else
    {
        sendErrorResponse(response, result.message, 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
    }
}

void UserController::handleSearchUsers(Poco::Net::HTTPServerRequest& request, 
                                      Poco::Net::HTTPServerResponse& response)
{
    std::string query = getQueryParameter(request.getURI(), "q");
    if (query.empty())
    {
        sendErrorResponse(response, "Search query is required", 
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
    
    std::map<std::string, std::string> filters;
    if (currentUserRole != database::models::UserRole::ADMIN)
    {
        filters["isActive"] = "true";
    }
    
    Poco::JSON::Array usersArray = userService->searchUsers(query, page, pageSize);

    Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object;
    dataPtr->set("items", usersArray);
    
    sendSuccessResponse(response, "Users search completed", dataPtr);
}

void UserController::handleGetUserStatistics(Poco::Net::HTTPServerRequest& request, 
                                            Poco::Net::HTTPServerResponse& response)
{
    auto currentUserRole = getCurrentUserRole(request);
    if (currentUserRole != database::models::UserRole::ADMIN)
    {
        sendForbiddenResponse(response, "Only admin can view user statistics");
        return;
    }
    
    Poco::JSON::Object statistics = userService->getUserStatistics();
    Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object(statistics);
    
    sendSuccessResponse(response, "User statistics retrieved", dataPtr);
}

void UserController::handleGetRoleStatistics(Poco::Net::HTTPServerRequest& request, 
                                            Poco::Net::HTTPServerResponse& response)
{
    auto currentUserRole = getCurrentUserRole(request);
    if (currentUserRole != database::models::UserRole::ADMIN && 
        currentUserRole != database::models::UserRole::MANAGER)
    {
        sendForbiddenResponse(response, "Only admin and manager can view role statistics");
        return;
    }
    
    Poco::JSON::Array statistics = userService->getRoleStatistics();
    Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object;
    dataPtr->set("statistics", statistics);
    
    sendSuccessResponse(response, "Role statistics retrieved", dataPtr);
}

void UserController::handleGetUserActivityReport(Poco::Net::HTTPServerRequest& request, 
                                                Poco::Net::HTTPServerResponse& response)
{
    auto currentUserRole = getCurrentUserRole(request);
    if (currentUserRole != database::models::UserRole::ADMIN)
    {
        sendForbiddenResponse(response, "Only admin can view user activity reports");
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
    
    Poco::JSON::Array report = userService->getUserActivityReport(startDate, endDate);
    Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object;
    dataPtr->set("report", report);
    
    sendSuccessResponse(response, "User activity report generated", dataPtr);
}

void UserController::handleImportUsers(Poco::Net::HTTPServerRequest& request, 
                                      Poco::Net::HTTPServerResponse& response)
{
    auto currentUserRole = getCurrentUserRole(request);
    if (currentUserRole != database::models::UserRole::ADMIN)
    {
        sendForbiddenResponse(response, "Only admin can import users");
        return;
    }
    
    auto json = parseJsonBody(request);
    if (!json)
    {
        sendErrorResponse(response, "Invalid JSON body", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    if (!json->has("users"))
    {
        sendErrorResponse(response, "Missing 'users' array in request body", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    auto usersArray = json->getArray("users");
    if (!usersArray || usersArray->size() == 0)
    {
        sendErrorResponse(response, "Empty users array", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    long long importedBy = getCurrentUserId(request);
    
    std::string ipAddress = getClientIpAddress(request);
    std::string userAgent = getUserAgent(request);
    
    Poco::JSON::Array importResult = userService->importUsers(*usersArray, importedBy, ipAddress, userAgent);
    Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object;
    dataPtr->set("result", importResult);
    
    sendSuccessResponse(response, "Users import completed", dataPtr);
}

void UserController::handleExportUsers(Poco::Net::HTTPServerRequest& request, 
                                      Poco::Net::HTTPServerResponse& response)
{
    auto currentUserRole = getCurrentUserRole(request);
    if (currentUserRole != database::models::UserRole::ADMIN)
    {
        sendForbiddenResponse(response, "Only admin can export users");
        return;
    }
    
    std::string userIdsParam = getQueryParameter(request.getURI(), "userIds");
    std::vector<long long> userIds;
    
    if (!userIdsParam.empty())
    {
        std::stringstream ss(userIdsParam);
        std::string id;
        while (std::getline(ss, id, ','))
        {
            try
            {
                userIds.push_back(std::stoll(id));
            }
            catch (...)
            {
            }
        }
    }
    
    long long exportedBy = getCurrentUserId(request);
    
    std::string ipAddress = getClientIpAddress(request);
    std::string userAgent = getUserAgent(request);
    
    auto result = userService->exportUsers(userIds, exportedBy, ipAddress, userAgent);
    
    if (result.success)
    {
        Poco::JSON::Object::Ptr dataPtr = new Poco::JSON::Object(result.data);
        sendSuccessResponse(response, "Users export completed", dataPtr);
    }
    else
    {
        sendErrorResponse(response, result.message, 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
    }
}

bool UserController::validateRequest(Poco::Net::HTTPServerRequest& request, 
                                    Poco::Net::HTTPServerResponse& response,
                                    std::string& errorMessage)
{
    if (!BaseController::validateRequest(request, response, errorMessage))
    {
        return false;
    }
    
    return true;
}

bool UserController::authorizeRequest(Poco::Net::HTTPServerRequest& request, 
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

bool UserController::validateUserAccess(Poco::Net::HTTPServerRequest& request, 
                                       long long targetUserId,
                                       std::string& errorMessage)
{
    auto currentUserRole = getCurrentUserRole(request);
    
    if (currentUserRole == database::models::UserRole::ADMIN)
    {
        return true;
    }
    
    long long currentUserId = getCurrentUserId(request);
    if (currentUserId == targetUserId)
    {
        return true;
    }
    
    if (currentUserRole == database::models::UserRole::MANAGER)
    {
        auto targetUserResult = userService->getUserById(targetUserId);
        if (targetUserResult.success && targetUserResult.data.has("role"))
        {
            std::string targetRoleStr = targetUserResult.data.getValue<std::string>("role");
            auto targetRole = database::models::User::stringToRole(targetRoleStr);
            if (targetRole == database::models::UserRole::WORKER || 
                targetRole == database::models::UserRole::AUDITOR)
            {
                return true;
            }
        }
    }
    
    errorMessage = "Access denied. You don't have permission to access this user's data.";
    return false;
}

long long UserController::getCurrentUserId(Poco::Net::HTTPServerRequest& request)
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

database::models::UserRole UserController::getCurrentUserRole(Poco::Net::HTTPServerRequest& request)
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

bool UserController::validateCreateUserData(const Poco::JSON::Object::Ptr& json, 
                                           std::vector<std::string>& errors)
{
    errors.clear();
    
    std::vector<std::string> requiredFields = {"username", "password_hash", "email", "full_name", "role"};
    std::vector<std::string> missingFields;
    if (!validateRequiredFields(json, requiredFields, missingFields))
    {
        for (const auto& field : missingFields)
        {
            errors.push_back(field + " is required");
        }
    }
    
    if (json->has("username"))
    {
        std::string username = utils::JsonUtils::getString(*json, "username");
        if (!services::AuthService::isValidUsername(username))
        {
            errors.push_back("Invalid username format");
        }
    }
    
    if (json->has("email"))
    {
        std::string email = utils::JsonUtils::getString(*json, "email");
        if (!services::AuthService::isValidEmailFormat(email))
        {
            errors.push_back("Invalid email format");
        }
    }
    
    if (json->has("password"))
    {
        std::string password = utils::JsonUtils::getString(*json, "password_hash");
        if (!services::AuthService::isValidPassword(password))
        {
            errors.push_back("Password must be at least 8 characters long");
        }
    }
    
    if (json->has("role"))
    {
        std::string role = utils::JsonUtils::getString(*json, "role");
        try
        {
            auto userRole = database::models::User::stringToRole(role);
        }
        catch (...)
        {
            errors.push_back("Invalid role");
        }
    }
    
    return errors.empty();
}

bool UserController::validateUpdateUserData(const Poco::JSON::Object::Ptr& json, 
                                           std::vector<std::string>& errors)
{
    errors.clear();
    
    bool hasUsername = json->has("username") && !json->get("username").isEmpty();
    bool hasEmail = json->has("email") && !json->get("email").isEmpty();
    bool hasFullName = json->has("fullName") && !json->get("fullName").isEmpty();
    bool hasPhoneNumber = json->has("phoneNumber") && !json->get("phoneNumber").isEmpty();
    
    if (!hasUsername && !hasEmail && !hasFullName && !hasPhoneNumber)
    {
        errors.push_back("At least one field must be provided for update");
    }
    
    if (hasUsername)
    {
        std::string username = utils::JsonUtils::getString(*json, "username");
        if (!services::AuthService::isValidUsername(username))
        {
            errors.push_back("Invalid username format");
        }
    }
    
    if (hasEmail)
    {
        std::string email = utils::JsonUtils::getString(*json, "email");
        if (!services::AuthService::isValidEmailFormat(email))
        {
            errors.push_back("Invalid email format");
        }
    }
    
    return errors.empty();
}

bool UserController::validateChangeRoleData(const Poco::JSON::Object::Ptr& json, 
                                           std::vector<std::string>& errors)
{
    errors.clear();
    
    std::vector<std::string> requiredFields = {"newRole"};
    std::vector<std::string> missingFields;
    if (!validateRequiredFields(json, requiredFields, missingFields))
    {
        for (const auto& field : missingFields)
        {
            errors.push_back(field + " is required");
        }
    }
    
    if (json->has("newRole"))
    {
        std::string role = utils::JsonUtils::getString(*json, "newRole");
        try
        {
            auto userRole = database::models::User::stringToRole(role);
        }
        catch (...)
        {
            errors.push_back("Invalid role");
        }
    }
    
    return errors.empty();
}

bool UserController::validateSearchParameters(const std::map<std::string, std::string>& filters,
                                             std::vector<std::string>& errors)
{
    errors.clear();
    
    for (const auto& [key, value] : filters)
    {
        if (key == "isActive")
        {
            if (value != "true" && value != "false")
            {
                errors.push_back("isActive must be 'true' or 'false'");
            }
        }
        else if (key == "role")
        {
            try
            {
                auto role = database::models::User::stringToRole(value);
            }
            catch (...)
            {
                errors.push_back("Invalid role value");
            }
        }
    }
    
    return errors.empty();
}

void UserController::logUserEvent(long long userId, 
                                 const std::string& action,
                                 long long targetUserId,
                                 const std::string& ipAddress,
                                 const std::string& userAgent,
                                 bool success,
                                 const std::string& details)
{
    std::cout << "[" << getCurrentTimestamp() << "] "
              << "USER " << action << " "
              << "UserID: " << userId << " "
              << "TargetUserID: " << targetUserId << " "
              << "IP: " << ipAddress << " "
              << "Success: " << (success ? "Yes" : "No") << " "
              << "Details: " << details << std::endl;
}

std::unique_ptr<database::models::User> UserController::extractUserFromJson(const Poco::JSON::Object::Ptr& json)
{
    if (!json)
    {
        return nullptr;
    }
    
    try
    {
        auto user = std::make_unique<database::models::User>(*json);
        return user;
    }
    catch (...)
    {
        return nullptr;
    }
}

bool UserController::canCreateUser(Poco::Net::HTTPServerRequest& request, 
                                  const database::models::User& userData,
                                  std::string& errorMessage)
{
    auto currentUserRole = getCurrentUserRole(request);
    
    if (currentUserRole == database::models::UserRole::ADMIN)
    {
        return true;
    }
    
    if (currentUserRole == database::models::UserRole::MANAGER)
    {
        if (userData.role == database::models::UserRole::WORKER || 
            userData.role == database::models::UserRole::AUDITOR)
        {
            return true;
        }
    }
    
    errorMessage = "You don't have permission to create users with this role";
    return false;
}

bool UserController::canUpdateUser(Poco::Net::HTTPServerRequest& request, 
                                  long long targetUserId,
                                  const database::models::User& userData,
                                  std::string& errorMessage)
{
    auto currentUserRole = getCurrentUserRole(request);
    long long currentUserId = getCurrentUserId(request);
    
    if (currentUserId == targetUserId)
    {
        if (userData.role != database::models::UserRole::UNKNOWN)
        {
            errorMessage = "You cannot change your own role";
            return false;
        }
        return true;
    }
    
    if (currentUserRole == database::models::UserRole::ADMIN)
    {
        return true;
    }
    
    if (currentUserRole == database::models::UserRole::MANAGER)
    {
        auto targetUserResult = userService->getUserById(targetUserId);
        if (targetUserResult.success && targetUserResult.data.has("role"))
        {
            std::string targetRoleStr = targetUserResult.data.getValue<std::string>("role");
            auto targetRole = database::models::User::stringToRole(targetRoleStr);
            if (targetRole == database::models::UserRole::WORKER || 
                targetRole == database::models::UserRole::AUDITOR)
            {
                if (userData.role == database::models::UserRole::ADMIN || 
                    userData.role == database::models::UserRole::MANAGER)
                {
                    errorMessage = "Manager cannot change user role to admin or manager";
                    return false;
                }
                return true;
            }
        }
    }
    
    errorMessage = "You don't have permission to update this user";
    return false;
}

bool UserController::canDeleteUser(Poco::Net::HTTPServerRequest& request, 
                                  long long targetUserId,
                                  std::string& errorMessage)
{
    auto currentUserRole = getCurrentUserRole(request);
    long long currentUserId = getCurrentUserId(request);
    
    if (currentUserId == targetUserId)
    {
        errorMessage = "You cannot delete your own account";
        return false;
    }
    
    if (currentUserRole == database::models::UserRole::ADMIN)
    {
        return true;
    }
    
    if (currentUserRole == database::models::UserRole::MANAGER)
    {
        auto targetUserResult = userService->getUserById(targetUserId);
        if (targetUserResult.success && targetUserResult.data.has("role"))
        {
            std::string targetRoleStr = targetUserResult.data.getValue<std::string>("role");
            auto targetRole = database::models::User::stringToRole(targetRoleStr);
            if (targetRole == database::models::UserRole::WORKER || 
                targetRole == database::models::UserRole::AUDITOR)
            {
                return true;
            }
        }
    }
    
    errorMessage = "You don't have permission to delete this user";
    return false;
}

bool UserController::canChangeUserRole(Poco::Net::HTTPServerRequest& request, 
                                      long long targetUserId,
                                      const std::string& newRole,
                                      std::string& errorMessage)
{
    auto currentUserRole = getCurrentUserRole(request);
    long long currentUserId = getCurrentUserId(request);
    
    if (currentUserId == targetUserId)
    {
        errorMessage = "You cannot change your own role";
        return false;
    }
    
    if (currentUserRole == database::models::UserRole::ADMIN)
    {
        return true;
    }
    
    if (currentUserRole == database::models::UserRole::MANAGER)
    {
        auto targetUserResult = userService->getUserById(targetUserId);
        if (targetUserResult.success && targetUserResult.data.has("role"))
        {
            std::string targetRoleStr = targetUserResult.data.getValue<std::string>("role");
            auto targetRole = database::models::User::stringToRole(targetRoleStr);
            
            if (targetRole == database::models::UserRole::WORKER || 
                targetRole == database::models::UserRole::AUDITOR)
            {
                auto newUserRole = database::models::User::stringToRole(newRole);
                if (newUserRole != database::models::UserRole::ADMIN && 
                    newUserRole != database::models::UserRole::MANAGER)
                {
                    return true;
                }
            }
        }
    }
    
    errorMessage = "You don't have permission to change this user's role";
    return false;
}

Poco::JSON::Object UserController::buildPaginationResponse(int page, int pageSize, int totalItems, 
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
