#pragma once

#include "BaseController.hpp"
#include "../services/AuthService.hpp"
#include <memory>

namespace warehouse_backend::controllers
{

class AuthController : public BaseController
{
public:
    AuthController();
    virtual ~AuthController() = default;
    
    void handleRequest(Poco::Net::HTTPServerRequest& request, 
                      Poco::Net::HTTPServerResponse& response) override;
    
    void handleRegister(Poco::Net::HTTPServerRequest& request, 
                       Poco::Net::HTTPServerResponse& response);
    
    void handleLogin(Poco::Net::HTTPServerRequest& request, 
                    Poco::Net::HTTPServerResponse& response);
    
    void handleRefreshToken(Poco::Net::HTTPServerRequest& request, 
                           Poco::Net::HTTPServerResponse& response);
    
    void handleLogout(Poco::Net::HTTPServerRequest& request, 
                     Poco::Net::HTTPServerResponse& response);
    
    void handleForgotPassword(Poco::Net::HTTPServerRequest& request, 
                             Poco::Net::HTTPServerResponse& response);
    
    void handleResetPassword(Poco::Net::HTTPServerRequest& request, 
                            Poco::Net::HTTPServerResponse& response);
    
    void handleChangePassword(Poco::Net::HTTPServerRequest& request, 
                             Poco::Net::HTTPServerResponse& response);
    
    void handleVerifyEmail(Poco::Net::HTTPServerRequest& request, 
                          Poco::Net::HTTPServerResponse& response);
    
    void handleGetProfile(Poco::Net::HTTPServerRequest& request, 
                         Poco::Net::HTTPServerResponse& response);
    
    void handleUpdateProfile(Poco::Net::HTTPServerRequest& request, 
                            Poco::Net::HTTPServerResponse& response);
    
    bool validateRequest(Poco::Net::HTTPServerRequest& request, 
                        Poco::Net::HTTPServerResponse& response,
                        std::string& errorMessage) override;
    
    bool authorizeRequest(Poco::Net::HTTPServerRequest& request, 
                         Poco::Net::HTTPServerResponse& response,
                         std::string& errorMessage) override;
    
    std::string getCurrentUserIdFromToken(Poco::Net::HTTPServerRequest& request);
    bool validateUserAccess(Poco::Net::HTTPServerRequest& request, 
                           long long targetUserId,
                           std::string& errorMessage);
    
private:
    std::unique_ptr<services::AuthService> authService;
    
    bool validateRegistrationData(const Poco::JSON::Object::Ptr& json, 
                                 std::vector<std::string>& errors);
    
    bool validateLoginData(const Poco::JSON::Object::Ptr& json, 
                          std::vector<std::string>& errors);
    
    bool validatePasswordResetData(const Poco::JSON::Object::Ptr& json, 
                                  std::vector<std::string>& errors);
    
    bool validateChangePasswordData(const Poco::JSON::Object::Ptr& json, 
                                   std::vector<std::string>& errors);
    
    bool validateProfileUpdateData(const Poco::JSON::Object::Ptr& json, 
                                  std::vector<std::string>& errors);
    
    services::TokenValidationResult validateAuthToken(Poco::Net::HTTPServerRequest& request);
    
    bool checkAuthRateLimit(Poco::Net::HTTPServerRequest& request, 
                           const std::string& endpoint);
    
    void logAuthEvent(long long userId, 
                     const std::string& action,
                     const std::string& ipAddress,
                     const std::string& userAgent,
                     bool success,
                     const std::string& details = "");
};

} // namespace controllers