#include "AuthController.hpp"
#include "../utils/JsonUtils.hpp"
#include "../utils/Validator.hpp"
#include <Poco/Net/HTTPRequestHandler.h>
#include <Poco/Net/HTTPServerRequest.h>
#include <Poco/Net/HTTPServerResponse.h>
#include <Poco/JSON/Parser.h>
#include <Poco/StreamCopier.h>
#include <iostream>

namespace warehouse_backend::controllers
{

AuthController::AuthController()
    : authService(std::make_unique<services::AuthService>())
{
}

void AuthController::handleRequest(Poco::Net::HTTPServerRequest& request, 
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
        
        std::string endpoint = request.getURI();
        
        if ((endpoint == "/api/v1/auth/register" || endpoint == "/api/auth/register") && request.getMethod() == "POST")
        {
            handleRegister(request, response);
        }
        else if ((endpoint == "/api/v1/auth/login" || endpoint == "/api/auth/login") && request.getMethod() == "POST")
        {
            handleLogin(request, response);
        }
        else if ((endpoint == "/api/v1/auth/refresh" || endpoint == "/api/auth/refresh") && request.getMethod() == "POST")
        {
            handleRefreshToken(request, response);
        }
        else if ((endpoint == "/api/v1/auth/logout" || endpoint == "/api/auth/logout") && request.getMethod() == "POST")
        {
            handleLogout(request, response);
        }
        else if ((endpoint == "/api/v1/auth/forgot-password" || endpoint == "/api/auth/forgot-password") && request.getMethod() == "POST")
        {
            handleForgotPassword(request, response);
        }
        else if ((endpoint == "/api/v1/auth/reset-password" || endpoint == "/api/auth/reset-password") && request.getMethod() == "POST")
        {
            handleResetPassword(request, response);
        }
        else if ((endpoint == "/api/v1/auth/change-password" || endpoint == "/api/auth/change-password") && request.getMethod() == "POST")
        {
            handleChangePassword(request, response);
        }
        else if ((endpoint == "/api/v1/auth/verify-email" || endpoint == "/api/auth/verify-email") && request.getMethod() == "POST")
        {
            handleVerifyEmail(request, response);
        }
        else if ((endpoint == "/api/v1/auth/profile" || endpoint == "/api/auth/profile") && request.getMethod() == "GET")
        {
            handleGetProfile(request, response);
        }
        else if ((endpoint == "/api/v1/auth/profile" || endpoint == "/api/auth/profile") && request.getMethod() == "PUT")
        {
            handleUpdateProfile(request, response);
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

void AuthController::handleRegister(Poco::Net::HTTPServerRequest& request, 
                                   Poco::Net::HTTPServerResponse& response)
{
    if (!checkAuthRateLimit(request, "register"))
    {
        sendErrorResponse(response, "Too many registration attempts. Please try again later.", 
                        Poco::Net::HTTPResponse::HTTP_TOO_MANY_REQUESTS);
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
    if (!validateRegistrationData(json, errors))
    {
        sendValidationErrorResponse(response, errors);
        return;
    }

    std::string username = utils::JsonUtils::getString(*json, "username");
    std::string password = utils::JsonUtils::getString(*json, "password");
    std::string email = utils::JsonUtils::getString(*json, "email");
    std::string fullName = utils::JsonUtils::getString(*json, "fullName");
    std::string role = utils::JsonUtils::getString(*json, "role", "worker");
    std::string phoneNumber = utils::JsonUtils::getString(*json, "phoneNumber", "");
    
    std::string ipAddress = getClientIpAddress(request);
    std::string userAgent = getUserAgent(request);
    
    auto result = authService->registerUser(username, password, email, fullName, role, phoneNumber);
    
    logAuthEvent(result.userId, "REGISTER", ipAddress, userAgent, result.success, 
                result.success ? "User registered successfully" : result.message);
    
    if (result.success)
    {
        Poco::JSON::Object::Ptr data = new Poco::JSON::Object;
        data->set("userId", result.userId);
        data->set("username", result.user->username);
        data->set("email", result.user->email);
        data->set("fullName", result.user->fullName);
        data->set("role", database::models::User::roleToString(result.user->role));
        data->set("createdAt", result.user->createdAt);
        
        sendSuccessResponse(response, "User registered successfully", data);
    }
    else
    {
        sendErrorResponse(response, result.message, 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
    }
}

void AuthController::handleLogin(Poco::Net::HTTPServerRequest& request, 
                                Poco::Net::HTTPServerResponse& response)
{
    if (!checkAuthRateLimit(request, "login"))
    {
        sendErrorResponse(response, "Too many login attempts. Please try again later.", 
                        Poco::Net::HTTPResponse::HTTP_TOO_MANY_REQUESTS);
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
    if (!validateLoginData(json, errors))
    {
        sendValidationErrorResponse(response, errors);
        return;
    }
    
    std::string username = utils::JsonUtils::getString(*json, "username", "");
    std::string email = utils::JsonUtils::getString(*json, "email", "");
    std::string password = utils::JsonUtils::getString(*json, "password");
    
    std::string ipAddress = getClientIpAddress(request);
    std::string userAgent = getUserAgent(request);
    
    services::LoginResult result;
    if (!email.empty())
    {
        result = authService->loginWithEmail(email, password);
    }
    else
    {
        result = authService->login(username, password);
    }
    
    long long userId = result.user ? result.user->id : 0;
    logAuthEvent(userId, "LOGIN", ipAddress, userAgent, result.success, 
                result.success ? "Login successful" : result.message);
    
    if (result.success)
    {
        Poco::JSON::Object::Ptr data = new Poco::JSON::Object;
        data->set("accessToken", result.accessToken);
        data->set("refreshToken", result.refreshToken);
        data->set("tokenType", result.tokenType);
        data->set("expiresIn", result.expiresIn);
        
        Poco::JSON::Object::Ptr userData = new Poco::JSON::Object;
        userData->set("id", result.user->id);
        userData->set("username", result.user->username);
        userData->set("email", result.user->email);
        userData->set("fullName", result.user->fullName);
        userData->set("role", database::models::User::roleToString(result.user->role));
        userData->set("lastLogin", result.user->lastLogin);
        
        data->set("user", userData);
        
        sendSuccessResponse(response, "Login successful", data);
    }
    else
    {
        sendErrorResponse(response, result.message, 
                        Poco::Net::HTTPResponse::HTTP_UNAUTHORIZED);
    }
}

void AuthController::handleRefreshToken(Poco::Net::HTTPServerRequest& request, 
                                       Poco::Net::HTTPServerResponse& response)
{
    auto json = parseJsonBody(request);
    if (!json)
    {
        sendErrorResponse(response, "Invalid JSON body", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    std::string refreshToken = utils::JsonUtils::getString(*json, "refreshToken");
    if (refreshToken.empty())
    {
        sendErrorResponse(response, "Refresh token is required", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    auto result = authService->refreshToken(refreshToken);
    
    if (result.success)
    {
        Poco::JSON::Object::Ptr data = new Poco::JSON::Object;
        data->set("accessToken", result.accessToken);
        data->set("refreshToken", result.refreshToken);
        data->set("tokenType", result.tokenType);
        data->set("expiresIn", result.expiresIn);
        
        sendSuccessResponse(response, "Token refreshed successfully", data);
    }
    else
    {
        sendErrorResponse(response, result.message, 
                        Poco::Net::HTTPResponse::HTTP_UNAUTHORIZED);
    }
}

void AuthController::handleLogout(Poco::Net::HTTPServerRequest& request, 
                                 Poco::Net::HTTPServerResponse& response)
{
    auto tokenResult = validateAuthToken(request);
    if (!tokenResult.isValid)
    {
        sendUnauthorizedResponse(response, tokenResult.message);
        return;
    }
    
    std::string authHeader = getAuthorizationHeader(request);
    std::string token = extractBearerToken(authHeader);
    
    bool success = authService->logout(tokenResult.user->id, token);
    
    if (success)
    {
        sendSuccessResponse(response, "Logged out successfully");
    }
    else
    {
        sendErrorResponse(response, "Logout failed", 
                        Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
    }
}

void AuthController::handleForgotPassword(Poco::Net::HTTPServerRequest& request, 
                                         Poco::Net::HTTPServerResponse& response)
{
    if (!checkAuthRateLimit(request, "forgot-password"))
    {
        sendErrorResponse(response, "Too many password reset requests. Please try again later.", 
                        Poco::Net::HTTPResponse::HTTP_TOO_MANY_REQUESTS);
        return;
    }
    
    auto json = parseJsonBody(request);
    if (!json)
    {
        sendErrorResponse(response, "Invalid JSON body", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    std::string email = utils::JsonUtils::getString(*json, "email");
    if (email.empty())
    {
        sendErrorResponse(response, "Email is required", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    auto result = authService->requestPasswordReset(email);
    
    if (result.success)
    {
        Poco::JSON::Object::Ptr data = new Poco::JSON::Object;
        data->set("resetToken", result.resetToken);
        data->set("expiresAt", utils::JsonUtils::dateTimeToISOString(result.expiresAt));
        
        sendSuccessResponse(response, "Password reset email sent", data);
    }
    else
    {
        sendSuccessResponse(response, "If the email exists, a reset link has been sent");
    }
}

void AuthController::handleResetPassword(Poco::Net::HTTPServerRequest& request, 
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
    if (!validatePasswordResetData(json, errors))
    {
        sendValidationErrorResponse(response, errors);
        return;
    }
    
    std::string resetToken = utils::JsonUtils::getString(*json, "resetToken");
    std::string newPassword = utils::JsonUtils::getString(*json, "newPassword");
    
    bool success = authService->resetPassword(resetToken, newPassword);
    
    if (success)
    {
        sendSuccessResponse(response, "Password reset successful");
    }
    else
    {
        sendErrorResponse(response, "Invalid or expired reset token", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
    }
}

void AuthController::handleChangePassword(Poco::Net::HTTPServerRequest& request, 
                                         Poco::Net::HTTPServerResponse& response)
{
    auto tokenResult = validateAuthToken(request);
    if (!tokenResult.isValid)
    {
        sendUnauthorizedResponse(response, tokenResult.message);
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
    if (!validateChangePasswordData(json, errors))
    {
        sendValidationErrorResponse(response, errors);
        return;
    }
    
    std::string currentPassword = utils::JsonUtils::getString(*json, "currentPassword");
    std::string newPassword = utils::JsonUtils::getString(*json, "newPassword");
    
    bool success = authService->changePassword(tokenResult.user->id, currentPassword, newPassword);
    
    if (success)
    {
        sendSuccessResponse(response, "Password changed successfully");
    }
    else
    {
        sendErrorResponse(response, "Current password is incorrect", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
    }
}

void AuthController::handleVerifyEmail(Poco::Net::HTTPServerRequest& request, 
                                      Poco::Net::HTTPServerResponse& response)
{
    auto tokenResult = validateAuthToken(request);
    if (!tokenResult.isValid)
    {
        sendUnauthorizedResponse(response, tokenResult.message);
        return;
    }
    
    auto json = parseJsonBody(request);
    if (!json)
    {
        sendErrorResponse(response, "Invalid JSON body", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    std::string verificationCode = utils::JsonUtils::getString(*json, "verificationCode");
    if (verificationCode.empty())
    {
        sendErrorResponse(response, "Verification code is required", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        return;
    }
    
    bool success = authService->verifyEmail(tokenResult.user->id, verificationCode);
    
    if (success)
    {
        sendSuccessResponse(response, "Email verified successfully");
    }
    else
    {
        sendErrorResponse(response, "Invalid verification code", 
                        Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
    }
}

void AuthController::handleGetProfile(Poco::Net::HTTPServerRequest& request, 
                                     Poco::Net::HTTPServerResponse& response)
{
    auto tokenResult = validateAuthToken(request);
    if (!tokenResult.isValid)
    {
        sendUnauthorizedResponse(response, tokenResult.message);
        return;
    }
    
    Poco::JSON::Object::Ptr userData = new Poco::JSON::Object(tokenResult.user->toJson());
    
    auto permissions = authService->getUserPermissions(tokenResult.user->id);
    Poco::JSON::Array::Ptr permissionsArray = new Poco::JSON::Array;
    for (const auto& permission : permissions)
    {
        permissionsArray->add(permission);
    }
    userData->set("permissions", permissionsArray);
    
    sendSuccessResponse(response, "Profile retrieved successfully", userData);
}

void AuthController::handleUpdateProfile(Poco::Net::HTTPServerRequest& request, 
                                        Poco::Net::HTTPServerResponse& response)
{
    auto tokenResult = validateAuthToken(request);
    if (!tokenResult.isValid)
    {
        sendUnauthorizedResponse(response, tokenResult.message);
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
    if (!validateProfileUpdateData(json, errors))
    {
        sendValidationErrorResponse(response, errors);
        return;
    }
    
    std::string fullName = utils::JsonUtils::getString(*json, "fullName", "");
    std::string email = utils::JsonUtils::getString(*json, "email", "");
    std::string phoneNumber = utils::JsonUtils::getString(*json, "phoneNumber", "");
    
    std::string ipAddress = getClientIpAddress(request);
    std::string userAgent = getUserAgent(request);
    
    sendSuccessResponse(response, "Profile updated successfully");
}

bool AuthController::validateRequest(Poco::Net::HTTPServerRequest& request, 
                                    Poco::Net::HTTPServerResponse& response,
                                    std::string& errorMessage)
{
    if (!BaseController::validateRequest(request, response, errorMessage))
    {
        return false;
    }
    
    return true;
}

bool AuthController::authorizeRequest(Poco::Net::HTTPServerRequest& request, 
                                     Poco::Net::HTTPServerResponse& response,
                                     std::string& errorMessage)
{
    std::string endpoint = request.getURI();
    
    if ((endpoint == "/api/v1/auth/register" || endpoint == "/api/auth/register") && request.getMethod() == "POST")
    {
        return true;
    }
    else if ((endpoint == "/api/v1/auth/login" || endpoint == "/api/auth/login") && request.getMethod() == "POST")
    {
        return true;
    }
    else if ((endpoint == "/api/v1/auth/refresh" || endpoint == "/api/auth/refresh") && request.getMethod() == "POST")
    {
        return true;
    }
    else if ((endpoint == "/api/v1/auth/forgot-password" || endpoint == "/api/auth/forgot-password") && request.getMethod() == "POST")
    {
        return true;
    }
    else if ((endpoint == "/api/v1/auth/reset-password" || endpoint == "/api/auth/reset-password") && request.getMethod() == "POST")
    {
        return true;
    }
    
    auto tokenResult = validateAuthToken(request);
    if (!tokenResult.isValid)
    {
        errorMessage = tokenResult.message;
        return false;
    }
    
    return true;
}

std::string AuthController::getCurrentUserIdFromToken(Poco::Net::HTTPServerRequest& request)
{
    auto tokenResult = validateAuthToken(request);
    if (tokenResult.isValid && tokenResult.user)
    {
        return std::to_string(tokenResult.user->id);
    }
    return "";
}

bool AuthController::validateUserAccess(Poco::Net::HTTPServerRequest& request, 
                                       long long targetUserId,
                                       std::string& errorMessage)
{
    auto tokenResult = validateAuthToken(request);
    if (!tokenResult.isValid)
    {
        errorMessage = tokenResult.message;
        return false;
    }
    
    if (tokenResult.user->id != targetUserId && 
        tokenResult.user->role != database::models::UserRole::ADMIN)
    {
        errorMessage = "Access denied. You can only access your own data.";
        return false;
    }
    
    return true;
}

bool AuthController::validateRegistrationData(const Poco::JSON::Object::Ptr& json, 
                                            std::vector<std::string>& errors)
{
    errors.clear();
    
    std::vector<std::string> requiredFields = {"username", "password", "email", "fullName"};
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
        std::string password = utils::JsonUtils::getString(*json, "password");
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

bool AuthController::validateLoginData(const Poco::JSON::Object::Ptr& json, 
                                      std::vector<std::string>& errors)
{
    errors.clear();
    
    bool hasUsername = json->has("username") && !json->get("username").isEmpty();
    bool hasEmail = json->has("email") && !json->get("email").isEmpty();
    bool hasPassword = json->has("password") && !json->get("password").isEmpty();
    
    if (!hasPassword)
    {
        errors.push_back("Password is required");
    }
    
    if (!hasUsername && !hasEmail)
    {
        errors.push_back("Either username or email is required");
    }
    
    return errors.empty();
}

bool AuthController::validatePasswordResetData(const Poco::JSON::Object::Ptr& json, 
                                              std::vector<std::string>& errors)
{
    errors.clear();
    
    std::vector<std::string> requiredFields = {"resetToken", "newPassword"};
    std::vector<std::string> missingFields;
    if (!validateRequiredFields(json, requiredFields, missingFields))
    {
        for (const auto& field : missingFields)
        {
            errors.push_back(field + " is required");
        }
    }
    
    if (json->has("newPassword"))
    {
        std::string password = utils::JsonUtils::getString(*json, "newPassword");
        if (!services::AuthService::isValidPassword(password))
        {
            errors.push_back("Password must be at least 8 characters long");
        }
    }
    
    return errors.empty();
}

bool AuthController::validateChangePasswordData(const Poco::JSON::Object::Ptr& json, 
                                               std::vector<std::string>& errors)
{
    errors.clear();
    
    std::vector<std::string> requiredFields = {"currentPassword", "newPassword"};
    std::vector<std::string> missingFields;
    if (!validateRequiredFields(json, requiredFields, missingFields))
    {
        for (const auto& field : missingFields)
        {
            errors.push_back(field + " is required");
        }
    }
    
    if (json->has("newPassword"))
    {
        std::string password = utils::JsonUtils::getString(*json, "newPassword");
        if (!services::AuthService::isValidPassword(password))
        {
            errors.push_back("New password must be at least 8 characters long");
        }
    }
    
    return errors.empty();
}

bool AuthController::validateProfileUpdateData(const Poco::JSON::Object::Ptr& json, 
                                              std::vector<std::string>& errors)
{
    errors.clear();
    
    bool hasFullName = json->has("fullName") && !json->get("fullName").isEmpty();
    bool hasEmail = json->has("email") && !json->get("email").isEmpty();
    bool hasPhoneNumber = json->has("phoneNumber") && !json->get("phoneNumber").isEmpty();
    
    if (!hasFullName && !hasEmail && !hasPhoneNumber)
    {
        errors.push_back("At least one field must be provided for update");
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

services::TokenValidationResult AuthController::validateAuthToken(Poco::Net::HTTPServerRequest& request)
{
    std::string authHeader = getAuthorizationHeader(request);
    std::string token = extractBearerToken(authHeader);
    
    if (token.empty())
    {
        return {false, "Authorization token is required", nullptr, services::TokenType::ACCESS_TOKEN};
    }
    
    return authService->validateToken(token);
}

bool AuthController::checkAuthRateLimit(Poco::Net::HTTPServerRequest& request, 
                                       const std::string& endpoint)
{
    std::string clientId = getClientIpAddress(request);
    
    int maxRequests = 10;
    int timeWindow = 60;
    
    if (endpoint == "register")
    {
        maxRequests = 5;
        timeWindow = 3600;
    }
    else if (endpoint == "login" || endpoint == "forgot-password")
    {
        maxRequests = 10;
        timeWindow = 300;
    }
    
    return checkRateLimit(clientId, endpoint, maxRequests, timeWindow);
}

void AuthController::logAuthEvent(long long userId, 
                                 const std::string& action,
                                 const std::string& ipAddress,
                                 const std::string& userAgent,
                                 bool success,
                                 const std::string& details)
{
    std::cout << "[" << getCurrentTimestamp() << "] "
              << "AUTH " << action << " "
              << "UserID: " << userId << " "
              << "IP: " << ipAddress << " "
              << "Success: " << (success ? "Yes" : "No") << " "
              << "Details: " << details << std::endl;
}

} // namespace controllers
