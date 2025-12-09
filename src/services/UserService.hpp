#pragma once

#include "../database/repositories/UserRepository.hpp"
#include "../database/repositories/AuditLogRepository.hpp"
#include "../config/ConfigManager.hpp"
#include "../utils/Validator.hpp"
#include "../utils/JsonUtils.hpp"
#include "../utils/DateUtils.hpp"
#include <memory>
#include <string>
#include <vector>
#include <Poco/JSON/Object.h>
#include <Poco/JSON/Array.h>

namespace warehouse_backend::services
{

struct UserServiceResult
{
    bool success;
    std::string message;
    long long userId;
    std::unique_ptr<database::models::User> user;
    Poco::JSON::Object data;
    
    UserServiceResult();
    
    Poco::JSON::Object toJson() const;
};

class UserService
{
public:
    UserService();
    ~UserService() = default;
    
    UserServiceResult createUser(const database::models::User& user,
                                 long long createdBy = 0,
                                 const std::string& ipAddress = "",
                                 const std::string& userAgent = "");
    
    UserServiceResult getUserById(long long userId);
    UserServiceResult getUserByUsername(const std::string& username);
    UserServiceResult getUserByEmail(const std::string& email);
    
    UserServiceResult updateUser(long long userId, 
                                const database::models::User& updatedUser,
                                long long updatedBy = 0,
                                const std::string& ipAddress = "",
                                const std::string& userAgent = "");
    
    UserServiceResult deleteUser(long long userId,
                                long long deletedBy = 0,
                                const std::string& reason = "",
                                const std::string& ipAddress = "",
                                const std::string& userAgent = "");
    
    UserServiceResult softDeleteUser(long long userId,
                                    long long deletedBy = 0,
                                    const std::string& reason = "",
                                    const std::string& ipAddress = "",
                                    const std::string& userAgent = "");
    
    UserServiceResult activateUser(long long userId,
                                  long long activatedBy = 0,
                                  const std::string& ipAddress = "",
                                  const std::string& userAgent = "");
    
    UserServiceResult deactivateUser(long long userId,
                                    long long deactivatedBy = 0,
                                    const std::string& reason = "",
                                    const std::string& ipAddress = "",
                                    const std::string& userAgent = "");
    
    Poco::JSON::Array findUsersByRole(const std::string& role, 
                                     int page = 1, 
                                     int pageSize = 20);
    
    Poco::JSON::Array findActiveUsers(int page = 1, int pageSize = 20);
    Poco::JSON::Array findInactiveUsers(int page = 1, int pageSize = 20);
    Poco::JSON::Array searchUsers(const std::string& query, int page = 1, int pageSize = 20);
    
    Poco::JSON::Object getUserStatistics();
    Poco::JSON::Array getRoleStatistics();
    Poco::JSON::Array getUserActivityReport(const std::string& startDate, 
                                           const std::string& endDate);
    Poco::JSON::Array getInactiveUsersReport(int daysThreshold = 30);
    
    UserServiceResult changeUserRole(long long userId, 
                                    const std::string& newRole,
                                    long long changedBy = 0,
                                    const std::string& reason = "",
                                    const std::string& ipAddress = "",
                                    const std::string& userAgent = "");
    
    UserServiceResult updateUserProfile(long long userId,
                                       const std::string& fullName,
                                       const std::string& email,
                                       const std::string& phoneNumber,
                                       long long updatedBy = 0,
                                       const std::string& ipAddress = "",
                                       const std::string& userAgent = "");
    
    UserServiceResult updateLastLogin(long long userId,
                                     const std::string& ipAddress = "",
                                     const std::string& userAgent = "");
    
    bool isUsernameAvailable(const std::string& username);
    bool isEmailAvailable(const std::string& email);
    bool hasPermission(long long userId, const std::string& permission);
    bool hasRole(long long userId, const std::string& role);
    bool canModifyUser(long long requesterId, long long targetUserId);
    
    Poco::JSON::Array importUsers(const Poco::JSON::Array& usersArray,
                                 long long importedBy = 0,
                                 const std::string& ipAddress = "",
                                 const std::string& userAgent = "");
    
    UserServiceResult exportUsers(const std::vector<long long>& userIds,
                                 long long exportedBy = 0,
                                 const std::string& ipAddress = "",
                                 const std::string& userAgent = "");
    
    Poco::JSON::Array getAllUsersAsJson(int page = 1, int pageSize = 20);
    Poco::JSON::Object getPaginatedUsers(int page = 1, int pageSize = 20);
    std::vector<std::string> getUserPermissions(long long userId);
    std::vector<std::pair<long long, std::string>> getUserList();
    
    UserServiceResult createUserWithTransaction(const database::models::User& user,
                                               long long createdBy = 0,
                                               const std::string& ipAddress = "",
                                               const std::string& userAgent = "");
    
    UserServiceResult updateUserWithTransaction(long long userId,
                                               const database::models::User& updatedUser,
                                               long long updatedBy = 0,
                                               const std::string& ipAddress = "",
                                               const std::string& userAgent = "");
    
private:
    void logAuditEvent(const std::string& tableName,
                      long long recordId,
                      database::models::AuditAction action,
                      const std::string& oldValues,
                      const std::string& newValues,
                      long long changedBy,
                      const std::string& ipAddress = "",
                      const std::string& userAgent = "",
                      const std::string& description = "");
    
    std::string userToJsonString(const database::models::User& user) const;
    std::string getCurrentDateTime() const;
    
    bool validateUserForCreation(const database::models::User& user, std::string& errorMessage);
    bool validateUserForUpdate(const database::models::User& user, std::string& errorMessage);
    
    std::unique_ptr<database::models::User> enrichUserWithDetails(
        std::unique_ptr<database::models::User> user);
    
    Poco::JSON::Object createPaginationMetadata(int page, int pageSize, int totalItems);
    
    static const std::map<std::string, std::vector<std::string>> ROLE_PERMISSIONS;
    
    std::unique_ptr<database::repositories::UserRepository> userRepository;
    std::unique_ptr<database::repositories::AuditLogRepository> auditRepository;
};

} // namespace services
