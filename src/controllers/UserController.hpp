#pragma once

#include "BaseController.hpp"
#include "../services/UserService.hpp"
#include "../services/AuthService.hpp"
#include <memory>

namespace warehouse_backend::controllers
{

class UserController : public BaseController
{
public:
    UserController();
    virtual ~UserController() = default;
    
    void handleRequest(Poco::Net::HTTPServerRequest& request, 
                      Poco::Net::HTTPServerResponse& response) override;
    
    void handleGetUsers(Poco::Net::HTTPServerRequest& request, 
                       Poco::Net::HTTPServerResponse& response);
    
    void handleGetUserById(Poco::Net::HTTPServerRequest& request, 
                          Poco::Net::HTTPServerResponse& response);
    
    void handleCreateUser(Poco::Net::HTTPServerRequest& request, 
                         Poco::Net::HTTPServerResponse& response);
    
    void handleUpdateUser(Poco::Net::HTTPServerRequest& request, 
                         Poco::Net::HTTPServerResponse& response);
    
    void handleDeleteUser(Poco::Net::HTTPServerRequest& request, 
                         Poco::Net::HTTPServerResponse& response);
    
    void handleSoftDeleteUser(Poco::Net::HTTPServerRequest& request, 
                             Poco::Net::HTTPServerResponse& response);
    
    void handleActivateUser(Poco::Net::HTTPServerRequest& request, 
                           Poco::Net::HTTPServerResponse& response);
    
    void handleDeactivateUser(Poco::Net::HTTPServerRequest& request, 
                             Poco::Net::HTTPServerResponse& response);
    
    void handleChangeUserRole(Poco::Net::HTTPServerRequest& request, 
                             Poco::Net::HTTPServerResponse& response);
    
    void handleSearchUsers(Poco::Net::HTTPServerRequest& request, 
                          Poco::Net::HTTPServerResponse& response);
    
    void handleGetUserStatistics(Poco::Net::HTTPServerRequest& request, 
                                Poco::Net::HTTPServerResponse& response);
    
    void handleGetRoleStatistics(Poco::Net::HTTPServerRequest& request, 
                                Poco::Net::HTTPServerResponse& response);
    
    void handleGetUserActivityReport(Poco::Net::HTTPServerRequest& request, 
                                    Poco::Net::HTTPServerResponse& response);
    
    void handleImportUsers(Poco::Net::HTTPServerRequest& request, 
                          Poco::Net::HTTPServerResponse& response);
    
    void handleExportUsers(Poco::Net::HTTPServerRequest& request, 
                          Poco::Net::HTTPServerResponse& response);
    
    bool validateRequest(Poco::Net::HTTPServerRequest& request, 
                        Poco::Net::HTTPServerResponse& response,
                        std::string& errorMessage) override;
    
    bool authorizeRequest(Poco::Net::HTTPServerRequest& request, 
                         Poco::Net::HTTPServerResponse& response,
                         std::string& errorMessage) override;
    
    bool validateUserAccess(Poco::Net::HTTPServerRequest& request, 
                           long long targetUserId,
                           std::string& errorMessage);
    
    long long getCurrentUserId(Poco::Net::HTTPServerRequest& request);
    database::models::UserRole getCurrentUserRole(Poco::Net::HTTPServerRequest& request);
    
private:
    std::unique_ptr<services::UserService> userService;
    std::unique_ptr<services::AuthService> authService;
    
    bool validateCreateUserData(const Poco::JSON::Object::Ptr& json, 
                               std::vector<std::string>& errors);
    
    bool validateUpdateUserData(const Poco::JSON::Object::Ptr& json, 
                               std::vector<std::string>& errors);
    
    bool validateChangeRoleData(const Poco::JSON::Object::Ptr& json, 
                               std::vector<std::string>& errors);
    
    bool validateSearchParameters(const std::map<std::string, std::string>& filters,
                                 std::vector<std::string>& errors);
    
    void logUserEvent(long long userId, 
                     const std::string& action,
                     long long targetUserId,
                     const std::string& ipAddress,
                     const std::string& userAgent,
                     bool success,
                     const std::string& details = "");
    
    std::unique_ptr<database::models::User> extractUserFromJson(const Poco::JSON::Object::Ptr& json);
    
    bool canCreateUser(Poco::Net::HTTPServerRequest& request, 
                      const database::models::User& userData,
                      std::string& errorMessage);
    
    bool canUpdateUser(Poco::Net::HTTPServerRequest& request, 
                      long long targetUserId,
                      const database::models::User& userData,
                      std::string& errorMessage);
    
    bool canDeleteUser(Poco::Net::HTTPServerRequest& request, 
                      long long targetUserId,
                      std::string& errorMessage);
    
    bool canChangeUserRole(Poco::Net::HTTPServerRequest& request, 
                          long long targetUserId,
                          const std::string& newRole,
                          std::string& errorMessage);
    
    Poco::JSON::Object buildPaginationResponse(int page, int pageSize, int totalItems, 
                                              const Poco::JSON::Array& data);
};

} // namespace controllers
