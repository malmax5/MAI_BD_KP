#include "UserService.hpp"
#include "../database/ConnectionPool.hpp"
#include <Poco/JSON/Parser.h>
#include <Poco/JSON/Stringifier.h>
#include <Poco/DateTimeFormatter.h>
#include <Poco/DateTimeFormat.h>
#include <algorithm>
#include <sstream>

namespace warehouse_backend::services
{

const std::map<std::string, std::vector<std::string>> UserService::ROLE_PERMISSIONS = {
    {"admin", {"users.create", "users.read", "users.update", "users.delete", 
               "users.manage_roles", "audit.read", "reports.generate", 
               "system.manage", "inventory.full_access", "orders.full_access"}},
    {"manager", {"users.read", "users.update", "inventory.manage", 
                "orders.manage", "reports.generate", "audit.read"}},
    {"worker", {"inventory.read", "inventory.update", "orders.read", 
               "orders.update", "orders.pick", "orders.pack"}},
    {"auditor", {"audit.read", "audit.export", "reports.generate", 
                "users.read", "inventory.read", "orders.read"}}
};

UserServiceResult::UserServiceResult()
    : success(false), userId(0), user(nullptr)
{
    
}

Poco::JSON::Object UserServiceResult::toJson() const
{
    Poco::JSON::Object result;
    result.set("success", success);
    result.set("message", message);
    result.set("userId", static_cast<Poco::Int64>(userId));
    
    if (user)
    {
        result.set("user", user->toJson());
    }
    
    if (!data.size())
    {
        result.set("data", data);
    }
    
    return result;
}

UserService::UserService()
    : userRepository(std::make_unique<database::repositories::UserRepository>()),
      auditRepository(std::make_unique<database::repositories::AuditLogRepository>())
{
}

UserServiceResult UserService::createUser(const database::models::User& user,
                                         long long createdBy,
                                         const std::string& ipAddress,
                                         const std::string& userAgent)
{
    UserServiceResult result;
    
    std::string validationError;
    if (!validateUserForCreation(user, validationError))
    {
        result.success = false;
        result.message = "Validation failed: " + validationError;
        return result;
    }
    
    if (!isUsernameAvailable(user.username))
    {
        result.success = false;
        result.message = "Username '" + user.username + "' is already taken";
        return result;
    }
    
    if (!isEmailAvailable(user.email))
    {
        result.success = false;
        result.message = "Email '" + user.email + "' is already registered";
        return result;
    }
    
    try
    {
        long long userId = userRepository->create(user);
        
        if (userId > 0)
        {
            result.success = true;
            result.userId = userId;
            result.message = "User created successfully";
            
            result.user = userRepository->findById(userId);
            
            logAuditEvent("users", userId, 
                         database::models::AuditAction::INSERT,
                         "", 
                         userToJsonString(*result.user),
                         createdBy,
                         ipAddress,
                         userAgent,
                         "User created");
        }
        else
        {
            result.success = false;
            result.message = "Failed to create user";
        }
    }
    catch (const std::exception& e)
    {
        result.success = false;
        result.message = "Error creating user: " + std::string(e.what());
    }
    
    return result;
}

UserServiceResult UserService::getUserById(long long userId)
{
    UserServiceResult result;
    
    try
    {
        auto user = userRepository->findById(userId);
        if (user)
        {
            result.success = true;
            result.userId = userId;
            result.message = "User found";
            result.user = enrichUserWithDetails(std::move(user));
        }
        else
        {
            result.success = false;
            result.message = "User not found with ID: " + std::to_string(userId);
        }
    }
    catch (const std::exception& e)
    {
        result.success = false;
        result.message = "Error retrieving user: " + std::string(e.what());
    }
    
    return result;
}

UserServiceResult UserService::getUserByUsername(const std::string& username)
{
    UserServiceResult result;
    
    try
    {
        auto user = userRepository->findByUsername(username);
        if (user)
        {
            result.success = true;
            result.userId = user->id;
            result.message = "User found";
            result.user = enrichUserWithDetails(std::move(user));
        }
        else
        {
            result.success = false;
            result.message = "User not found with username: " + username;
        }
    }
    catch (const std::exception& e)
    {
        result.success = false;
        result.message = "Error retrieving user: " + std::string(e.what());
    }
    
    return result;
}

UserServiceResult UserService::getUserByEmail(const std::string& email)
{
    UserServiceResult result;
    
    try
    {
        auto user = userRepository->findByEmail(email);
        if (user)
        {
            result.success = true;
            result.userId = user->id;
            result.message = "User found";
            result.user = enrichUserWithDetails(std::move(user));
        }
        else
        {
            result.success = false;
            result.message = "User not found with email: " + email;
        }
    }
    catch (const std::exception& e)
    {
        result.success = false;
        result.message = "Error retrieving user: " + std::string(e.what());
    }
    
    return result;
}

UserServiceResult UserService::updateUser(long long userId, 
                                         const database::models::User& updatedUser,
                                         long long updatedBy,
                                         const std::string& ipAddress,
                                         const std::string& userAgent)
{
    UserServiceResult result;
    
    try
    {
        auto currentUser = userRepository->findById(userId);
        if (!currentUser)
        {
            result.success = false;
            result.message = "User not found with ID: " + std::to_string(userId);
            return result;
        }
        
        std::string validationError;
        if (!validateUserForUpdate(updatedUser, validationError))
        {
            result.success = false;
            result.message = "Validation failed: " + validationError;
            return result;
        }
        
        if (currentUser->email != updatedUser.email && !isEmailAvailable(updatedUser.email))
        {
            result.success = false;
            result.message = "Email '" + updatedUser.email + "' is already registered";
            return result;
        }
        
        if (currentUser->username != updatedUser.username && !isUsernameAvailable(updatedUser.username))
        {
            result.success = false;
            result.message = "Username '" + updatedUser.username + "' is already taken";
            return result;
        }
        
        std::string oldValues = userToJsonString(*currentUser);
        
        bool updateSuccess = userRepository->update(userId, updatedUser);
        
        if (updateSuccess)
        {
            result.success = true;
            result.userId = userId;
            result.message = "User updated successfully";
            
            result.user = userRepository->findById(userId);
            
            std::string newValues = userToJsonString(*result.user);
            logAuditEvent("users", userId, 
                         database::models::AuditAction::UPDATE,
                         oldValues, 
                         newValues,
                         updatedBy,
                         ipAddress,
                         userAgent,
                         "User updated");
        }
        else
        {
            result.success = false;
            result.message = "Failed to update user";
        }
    }
    catch (const std::exception& e)
    {
        result.success = false;
        result.message = "Error updating user: " + std::string(e.what());
    }
    
    return result;
}

UserServiceResult UserService::deleteUser(long long userId,
                                         long long deletedBy,
                                         const std::string& reason,
                                         const std::string& ipAddress,
                                         const std::string& userAgent)
{
    UserServiceResult result;
    
    try
    {
        auto user = userRepository->findById(userId);
        if (!user)
        {
            result.success = false;
            result.message = "User not found with ID: " + std::to_string(userId);
            return result;
        }
        
        std::string oldValues = userToJsonString(*user);
        
        bool deleteSuccess = userRepository->remove(userId);
        
        if (deleteSuccess)
        {
            result.success = true;
            result.userId = userId;
            result.message = "User deleted successfully";
            
            std::string description = "User deleted";
            if (!reason.empty())
            {
                description += ": " + reason;
            }
            
            logAuditEvent("users", userId, 
                         database::models::AuditAction::DELETE,
                         oldValues, 
                         "",
                         deletedBy,
                         ipAddress,
                         userAgent,
                         description);
        }
        else
        {
            result.success = false;
            result.message = "Failed to delete user";
        }
    }
    catch (const std::exception& e)
    {
        result.success = false;
        result.message = "Error deleting user: " + std::string(e.what());
    }
    
    return result;
}

UserServiceResult UserService::softDeleteUser(long long userId,
                                            long long deletedBy,
                                            const std::string& reason,
                                            const std::string& ipAddress,
                                            const std::string& userAgent)
{
    UserServiceResult result;
    
    try
    {
        auto user = userRepository->findById(userId);
        if (!user)
        {
            result.success = false;
            result.message = "User not found with ID: " + std::to_string(userId);
            return result;
        }
        
        std::string oldValues = userToJsonString(*user);
        
        bool deleteSuccess = userRepository->softDelete(userId);
        
        if (deleteSuccess)
        {
            result.success = true;
            result.userId = userId;
            result.message = "User soft deleted successfully";
            
            result.user = userRepository->findById(userId);
            
            std::string description = "User soft deleted";
            if (!reason.empty())
            {
                description += ": " + reason;
            }
            
            std::string newValues = userToJsonString(*result.user);
            logAuditEvent("users", userId, 
                         database::models::AuditAction::SOFT_DELETE,
                         oldValues, 
                         newValues,
                         deletedBy,
                         ipAddress,
                         userAgent,
                         description);
        }
        else
        {
            result.success = false;
            result.message = "Failed to soft delete user";
        }
    }
    catch (const std::exception& e)
    {
        result.success = false;
        result.message = "Error soft deleting user: " + std::string(e.what());
    }
    
    return result;
}

UserServiceResult UserService::activateUser(long long userId,
                                          long long activatedBy,
                                          const std::string& ipAddress,
                                          const std::string& userAgent)
{
    UserServiceResult result;
    
    try
    {
        auto currentUser = userRepository->findById(userId);
        if (!currentUser)
        {
            result.success = false;
            result.message = "User not found with ID: " + std::to_string(userId);
            return result;
        }
        
        std::string oldValues = userToJsonString(*currentUser);
        
        database::models::User updatedUser = *currentUser;
        updatedUser.isActive = true;
        
        bool updateSuccess = userRepository->update(userId, updatedUser);
        
        if (updateSuccess)
        {
            result.success = true;
            result.userId = userId;
            result.message = "User activated successfully";
            
            result.user = userRepository->findById(userId);
            
            std::string newValues = userToJsonString(*result.user);
            logAuditEvent("users", userId, 
                         database::models::AuditAction::UPDATE,
                         oldValues, 
                         newValues,
                         activatedBy,
                         ipAddress,
                         userAgent,
                         "User activated");
        }
        else
        {
            result.success = false;
            result.message = "Failed to activate user";
        }
    }
    catch (const std::exception& e)
    {
        result.success = false;
        result.message = "Error activating user: " + std::string(e.what());
    }
    
    return result;
}

UserServiceResult UserService::deactivateUser(long long userId,
                                            long long deactivatedBy,
                                            const std::string& reason,
                                            const std::string& ipAddress,
                                            const std::string& userAgent)
{
    UserServiceResult result;
    
    try
    {
        auto currentUser = userRepository->findById(userId);
        if (!currentUser)
        {
            result.success = false;
            result.message = "User not found with ID: " + std::to_string(userId);
            return result;
        }
        
        std::string oldValues = userToJsonString(*currentUser);
        
        database::models::User updatedUser = *currentUser;
        updatedUser.isActive = false;
        
        bool updateSuccess = userRepository->update(userId, updatedUser);
        
        if (updateSuccess)
        {
            result.success = true;
            result.userId = userId;
            result.message = "User deactivated successfully";
            
            result.user = userRepository->findById(userId);
            
            std::string description = "User deactivated";
            if (!reason.empty())
            {
                description += ": " + reason;
            }
            
            std::string newValues = userToJsonString(*result.user);
            logAuditEvent("users", userId, 
                         database::models::AuditAction::UPDATE,
                         oldValues, 
                         newValues,
                         deactivatedBy,
                         ipAddress,
                         userAgent,
                         description);
        }
        else
        {
            result.success = false;
            result.message = "Failed to deactivate user";
        }
    }
    catch (const std::exception& e)
    {
        result.success = false;
        result.message = "Error deactivating user: " + std::string(e.what());
    }
    
    return result;
}

Poco::JSON::Array UserService::findUsersByRole(const std::string& role, 
                                              int page, 
                                              int pageSize)
{
    Poco::JSON::Array result;
    
    try
    {
        auto users = userRepository->findByRole(role);
        int startIndex = (page - 1) * pageSize;
        int endIndex = std::min(static_cast<int>(users.size()), startIndex + pageSize);
        
        for (int i = startIndex; i < endIndex && i < static_cast<int>(users.size()); ++i)
        {
            auto enrichedUser = enrichUserWithDetails(std::move(users[i]));
            result.add(enrichedUser->toJson());
        }
    }
    catch (const std::exception& e)
    {
        Poco::JSON::Object error;
        error.set("error", "Error retrieving users: " + std::string(e.what()));
        result.add(error);
    }
    
    return result;
}

Poco::JSON::Array UserService::findActiveUsers(int page, int pageSize)
{
    Poco::JSON::Array result;
    
    try
    {
        auto users = userRepository->findActiveUsers();
        int startIndex = (page - 1) * pageSize;
        int endIndex = std::min(static_cast<int>(users.size()), startIndex + pageSize);
        
        for (int i = startIndex; i < endIndex && i < static_cast<int>(users.size()); ++i)
        {
            auto enrichedUser = enrichUserWithDetails(std::move(users[i]));
            result.add(enrichedUser->toJson());
        }
    }
    catch (const std::exception& e)
    {
        Poco::JSON::Object error;
        error.set("error", "Error retrieving active users: " + std::string(e.what()));
        result.add(error);
    }
    
    return result;
}

Poco::JSON::Array UserService::findInactiveUsers(int page, int pageSize)
{
    Poco::JSON::Array result;
    
    try
    {
        auto users = userRepository->findInactiveUsers();
        int startIndex = (page - 1) * pageSize;
        int endIndex = std::min(static_cast<int>(users.size()), startIndex + pageSize);
        
        for (int i = startIndex; i < endIndex && i < static_cast<int>(users.size()); ++i)
        {
            auto enrichedUser = enrichUserWithDetails(std::move(users[i]));
            result.add(enrichedUser->toJson());
        }
    }
    catch (const std::exception& e)
    {
        Poco::JSON::Object error;
        error.set("error", "Error retrieving inactive users: " + std::string(e.what()));
        result.add(error);
    }
    
    return result;
}

Poco::JSON::Array UserService::searchUsers(const std::string& query, 
                                          int page, 
                                          int pageSize)
{
    Poco::JSON::Array result;
    
    try
    {
        std::vector<std::string> searchFields = {"username", "email", "full_name", "phone_number"};
        auto users = userRepository->search(query, searchFields);
        
        int startIndex = (page - 1) * pageSize;
        int endIndex = std::min(static_cast<int>(users.size()), startIndex + pageSize);
        
        for (int i = startIndex; i < endIndex && i < static_cast<int>(users.size()); ++i)
        {
            auto enrichedUser = enrichUserWithDetails(std::move(users[i]));
            result.add(enrichedUser->toJson());
        }
    }
    catch (const std::exception& e)
    {
        Poco::JSON::Object error;
        error.set("error", "Error searching users: " + std::string(e.what()));
        result.add(error);
    }
    
    return result;
}

Poco::JSON::Object UserService::getUserStatistics()
{
    Poco::JSON::Object stats;
    
    try
    {
        int totalUsers = userRepository->count();
        int activeUsers = userRepository->countActiveUsers();
        int inactiveUsers = totalUsers - activeUsers;
        
        auto adminCount = userRepository->countByRole("admin");
        auto managerCount = userRepository->countByRole("manager");
        auto workerCount = userRepository->countByRole("worker");
        auto auditorCount = userRepository->countByRole("auditor");
        
        stats.set("totalUsers", totalUsers);
        stats.set("activeUsers", activeUsers);
        stats.set("inactiveUsers", inactiveUsers);
        stats.set("adminCount", adminCount);
        stats.set("managerCount", managerCount);
        stats.set("workerCount", workerCount);
        stats.set("auditorCount", auditorCount);
        stats.set("lastUpdated", getCurrentDateTime());
    }
    catch (const std::exception& e)
    {
        stats.set("error", "Error calculating statistics: " + std::string(e.what()));
    }
    
    return stats;
}

Poco::JSON::Array UserService::getRoleStatistics()
{
    Poco::JSON::Array result;
    
    try
    {
        std::vector<std::string> roles = {"admin", "manager", "worker", "auditor"};
        
        for (const auto& role : roles)
        {
            Poco::JSON::Object roleStat;
            roleStat.set("role", role);
            
            int count = userRepository->countByRole(role);
            roleStat.set("count", count);
            
            auto users = userRepository->findByRole(role);
            int exampleCount = std::min(3, static_cast<int>(users.size()));
            
            Poco::JSON::Array examples;
            for (int i = 0; i < exampleCount; ++i)
            {
                Poco::JSON::Object userExample;
                userExample.set("id", static_cast<Poco::Int64>(users[i]->id));
                userExample.set("username", users[i]->username);
                userExample.set("fullName", users[i]->fullName);
                examples.add(userExample);
            }
            
            roleStat.set("examples", examples);
            result.add(roleStat);
        }
    }
    catch (const std::exception& e)
    {
        Poco::JSON::Object error;
        error.set("error", "Error retrieving role statistics: " + std::string(e.what()));
        result.add(error);
    }
    
    return result;
}

Poco::JSON::Array UserService::getUserActivityReport(const std::string& startDate, 
                                                    const std::string& endDate)
{
    Poco::JSON::Array result;
    
    try
    {
        auto auditLogs = auditRepository->findByDateRange(startDate, endDate);
        
        std::map<long long, Poco::JSON::Object> userActivities;
        
        for (const auto& log : auditLogs)
        {
            long long userId = log->changedBy;
            
            if (userActivities.find(userId) == userActivities.end())
            {
                auto user = userRepository->findById(userId);
                if (user)
                {
                    Poco::JSON::Object userActivity;
                    userActivity.set("userId", static_cast<Poco::Int64>(userId));
                    userActivity.set("username", user->username);
                    userActivity.set("fullName", user->fullName);
                    userActivity.set("role", database::models::User::roleToString(user->role));
                    userActivity.set("actionCount", 0);
                    
                    Poco::JSON::Array actions;
                    userActivity.set("actions", actions);
                    
                    userActivities[userId] = userActivity;
                }
            }
            
            Poco::JSON::Object action;
            action.set("timestamp", log->changedAt);
            action.set("actionType", database::models::AuditLog::auditActionToString(log->action));
            action.set("tableName", log->tableName);
            action.set("recordId", static_cast<Poco::Int64>(log->recordId));
            action.set("description", log->description);
            
            auto& userActivity = userActivities[userId];
            int actionCount = userActivity.get("actionCount");
            userActivity.set("actionCount", actionCount + 1);
            
            Poco::JSON::Array actions = *userActivity.get("actions").extract<Poco::JSON::Array::Ptr>();
            
            actions.add(action);
            userActivity.set("actions", actions);
        }
        
        for (const auto& [userId, activity] : userActivities)
        {
            result.add(activity);
        }
    }
    catch (const std::exception& e)
    {
        Poco::JSON::Object error;
        error.set("error", "Error generating activity report: " + std::string(e.what()));
        result.add(error);
    }
    
    return result;
}

Poco::JSON::Array UserService::getInactiveUsersReport(int daysThreshold)
{
    Poco::JSON::Array result;
    
    try
    {
        auto users = userRepository->findUsersWithLastLoginBefore(
            utils::DateUtils::formatDate(
                utils::DateUtils::addDays(utils::DateUtils::now(), -daysThreshold)
            )
        );
        
        for (auto& user : users)
        {
            auto enrichedUser = enrichUserWithDetails(std::move(user));
            result.add(enrichedUser->toJson());
        }
    }
    catch (const std::exception& e)
    {
        Poco::JSON::Object error;
        error.set("error", "Error generating inactive users report: " + std::string(e.what()));
        result.add(error);
    }
    
    return result;
}

UserServiceResult UserService::changeUserRole(long long userId, 
                                             const std::string& newRole,
                                             long long changedBy,
                                             const std::string& reason,
                                             const std::string& ipAddress,
                                             const std::string& userAgent)
{
    UserServiceResult result;
    
    try
    {
        auto currentUser = userRepository->findById(userId);
        if (!currentUser)
        {
            result.success = false;
            result.message = "User not found with ID: " + std::to_string(userId);
            return result;
        }
        
        std::vector<std::string> validRoles = {"admin", "manager", "worker", "auditor"};
        if (std::find(validRoles.begin(), validRoles.end(), newRole) == validRoles.end())
        {
            result.success = false;
            result.message = "Invalid role: " + newRole;
            return result;
        }
        
        std::string oldValues = userToJsonString(*currentUser);
        
        database::models::User updatedUser = *currentUser;
        updatedUser.role = database::models::User::stringToRole(newRole);
        
        bool updateSuccess = userRepository->updateRole(userId, newRole);
        
        if (updateSuccess)
        {
            result.success = true;
            result.userId = userId;
            result.message = "User role changed successfully";
            
            result.user = userRepository->findById(userId);
            
            std::string description = "User role changed to " + newRole;
            if (!reason.empty())
            {
                description += ": " + reason;
            }
            
            std::string newValues = userToJsonString(*result.user);
            logAuditEvent("users", userId, 
                         database::models::AuditAction::UPDATE,
                         oldValues, 
                         newValues,
                         changedBy,
                         ipAddress,
                         userAgent,
                         description);
        }
        else
        {
            result.success = false;
            result.message = "Failed to change user role";
        }
    }
    catch (const std::exception& e)
    {
        result.success = false;
        result.message = "Error changing user role: " + std::string(e.what());
    }
    
    return result;
}

UserServiceResult UserService::updateUserProfile(long long userId,
                                                const std::string& fullName,
                                                const std::string& email,
                                                const std::string& phoneNumber,
                                                long long updatedBy,
                                                const std::string& ipAddress,
                                                const std::string& userAgent)
{
    UserServiceResult result;
    
    try
    {
        auto currentUser = userRepository->findById(userId);
        if (!currentUser)
        {
            result.success = false;
            result.message = "User not found with ID: " + std::to_string(userId);
            return result;
        }
        
        std::string oldValues = userToJsonString(*currentUser);
        
        database::models::User updatedUser = *currentUser;
        updatedUser.fullName = fullName;
        updatedUser.email = email;
        updatedUser.phoneNumber = phoneNumber;
        
        if (!utils::Validator::isValidEmail(email))
        {
            result.success = false;
            result.message = "Invalid email format";
            return result;
        }
        
        if (currentUser->email != email && !isEmailAvailable(email))
        {
            result.success = false;
            result.message = "Email '" + email + "' is already registered";
            return result;
        }
        
        bool updateSuccess = userRepository->update(userId, updatedUser);
        
        if (updateSuccess)
        {
            result.success = true;
            result.userId = userId;
            result.message = "User profile updated successfully";
            
            result.user = userRepository->findById(userId);
            
            std::string newValues = userToJsonString(*result.user);
            logAuditEvent("users", userId, 
                         database::models::AuditAction::UPDATE,
                         oldValues, 
                         newValues,
                         updatedBy,
                         ipAddress,
                         userAgent,
                         "User profile updated");
        }
        else
        {
            result.success = false;
            result.message = "Failed to update user profile";
        }
    }
    catch (const std::exception& e)
    {
        result.success = false;
        result.message = "Error updating user profile: " + std::string(e.what());
    }
    
    return result;
}

UserServiceResult UserService::updateLastLogin(long long userId,
                                              const std::string& ipAddress,
                                              const std::string& userAgent)
{
    UserServiceResult result;
    
    try
    {
        bool updateSuccess = userRepository->updateLastLogin(userId, getCurrentDateTime());
        
        if (updateSuccess)
        {
            result.success = true;
            result.userId = userId;
            result.message = "Last login updated";
            
            if (!ipAddress.empty())
            {
                logAuditEvent("users", userId, 
                            database::models::AuditAction::UPDATE,
                            "", 
                            "",
                            userId,
                            ipAddress,
                            userAgent,
                            "User login");
            }
        }
        else
        {
            result.success = false;
            result.message = "Failed to update last login";
        }
    }
    catch (const std::exception& e)
    {
        result.success = false;
        result.message = "Error updating last login: " + std::string(e.what());
    }
    
    return result;
}

bool UserService::isUsernameAvailable(const std::string& username)
{
    try
    {
        return !userRepository->usernameExists(username);
    }
    catch (const std::exception&)
    {
        return false;
    }
}

bool UserService::isEmailAvailable(const std::string& email)
{
    try
    {
        return !userRepository->emailExists(email);
    }
    catch (const std::exception&)
    {
        return false;
    }
}

bool UserService::hasPermission(long long userId, const std::string& permission)
{
    try
    {
        auto user = userRepository->findById(userId);
        if (!user)
        {
            return false;
        }
        
        std::string role = database::models::User::roleToString(user->role);
        
        auto it = ROLE_PERMISSIONS.find(role);
        if (it != ROLE_PERMISSIONS.end())
        {
            const auto& permissions = it->second;
            return std::find(permissions.begin(), permissions.end(), permission) != permissions.end();
        }
        
        return false;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

bool UserService::hasRole(long long userId, const std::string& role)
{
    try
    {
        auto user = userRepository->findById(userId);
        if (!user)
        {
            return false;
        }
        
        std::string userRole = database::models::User::roleToString(user->role);
        return userRole == role;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

bool UserService::canModifyUser(long long requesterId, long long targetUserId)
{
    try
    {
        if (requesterId == targetUserId)
        {
            return true;
        }
        
        auto requester = userRepository->findById(requesterId);
        if (!requester)
        {
            return false;
        }
        
        std::string requesterRole = database::models::User::roleToString(requester->role);
        
        return requesterRole == "admin" || requesterRole == "manager";
    }
    catch (const std::exception&)
    {
        return false;
    }
}

Poco::JSON::Array UserService::importUsers(const Poco::JSON::Array& usersArray,
                                          long long importedBy,
                                          const std::string& ipAddress,
                                          const std::string& userAgent)
{
    Poco::JSON::Array result;
    
    try
    {
        int successCount = 0;
        int failureCount = 0;
        
        for (size_t i = 0; i < usersArray.size(); ++i)
        {
            Poco::JSON::Object itemResult;
            
            try
            {
                auto userJson = usersArray.getObject(i);
                database::models::User user = database::models::User::fromJson(*userJson);
                
                auto createResult = createUser(user, importedBy, ipAddress, userAgent);
                
                itemResult.set("index", static_cast<int>(i));
                itemResult.set("success", createResult.success);
                itemResult.set("message", createResult.message);
                
                if (createResult.success)
                {
                    successCount++;
                    itemResult.set("userId", static_cast<Poco::Int64>(createResult.userId));
                }
                else
                {
                    failureCount++;
                }
            }
            catch (const std::exception& e)
            {
                itemResult.set("index", static_cast<int>(i));
                itemResult.set("success", false);
                itemResult.set("message", "Error importing user: " + std::string(e.what()));
                failureCount++;
            }
            
            result.add(itemResult);
        }
        
        Poco::JSON::Object summary;
        summary.set("total", static_cast<int>(usersArray.size()));
        summary.set("success", successCount);
        summary.set("failure", failureCount);
        summary.set("importedBy", static_cast<Poco::Int64>(importedBy));
        summary.set("timestamp", getCurrentDateTime());
        
        result.add(summary);
    }
    catch (const std::exception& e)
    {
        Poco::JSON::Object error;
        error.set("error", "Error during import: " + std::string(e.what()));
        result.add(error);
    }
    
    return result;
}

UserServiceResult UserService::exportUsers(const std::vector<long long>& userIds,
                                          long long exportedBy,
                                          const std::string& ipAddress,
                                          const std::string& userAgent)
{
    UserServiceResult result;
    
    try
    {
        Poco::JSON::Array usersArray;
        
        for (long long userId : userIds)
        {
            auto user = userRepository->findById(userId);
            if (user)
            {
                auto enrichedUser = enrichUserWithDetails(std::move(user));
                usersArray.add(enrichedUser->toJson());
            }
        }
        
        result.success = true;
        result.message = "Users exported successfully";
        result.data.set("users", usersArray);
        result.data.set("count", static_cast<int>(usersArray.size()));
        result.data.set("exportedBy", static_cast<Poco::Int64>(exportedBy));
        result.data.set("timestamp", getCurrentDateTime());
        
        logAuditEvent("users", 0, 
                     database::models::AuditAction::UPDATE,
                     "", 
                     "",
                     exportedBy,
                     ipAddress,
                     userAgent,
                     "Users export: " + std::to_string(userIds.size()) + " users");
    }
    catch (const std::exception& e)
    {
        result.success = false;
        result.message = "Error exporting users: " + std::string(e.what());
    }
    
    return result;
}

Poco::JSON::Array UserService::getAllUsersAsJson(int page, int pageSize)
{
    Poco::JSON::Array result;
    
    try
    {
        auto users = userRepository->findPaginated(page, pageSize);
        
        for (auto& user : users)
        {
            auto enrichedUser = enrichUserWithDetails(std::move(user));
            result.add(enrichedUser->toJson());
        }
    }
    catch (const std::exception& e)
    {
        Poco::JSON::Object error;
        error.set("error", "Error retrieving users: " + std::string(e.what()));
        result.add(error);
    }
    
    return result;
}

Poco::JSON::Object UserService::getPaginatedUsers(int page, int pageSize)
{
    Poco::JSON::Object result;
    
    try
    {
        auto users = userRepository->findPaginated(page, pageSize);
        int totalUsers = userRepository->count();
        
        Poco::JSON::Array usersArray;
        for (auto& user : users)
        {
            auto enrichedUser = enrichUserWithDetails(std::move(user));
            usersArray.add(enrichedUser->toJson());
        }
        
        result.set("users", usersArray);
        result.set("pagination", createPaginationMetadata(page, pageSize, totalUsers));
    }
    catch (const std::exception& e)
    {
        result.set("error", "Error retrieving paginated users: " + std::string(e.what()));
    }
    
    return result;
}

std::vector<std::string> UserService::getUserPermissions(long long userId)
{
    std::vector<std::string> permissions;
    
    try
    {
        auto user = userRepository->findById(userId);
        if (user)
        {
            std::string role = database::models::User::roleToString(user->role);
            
            auto it = ROLE_PERMISSIONS.find(role);
            if (it != ROLE_PERMISSIONS.end())
            {
                permissions = it->second;
            }
        }
    }
    catch (const std::exception&)
    {
    }
    
    return permissions;
}

std::vector<std::pair<long long, std::string>> UserService::getUserList()
{
    std::vector<std::pair<long long, std::string>> userList;
    
    try
    {
        auto users = userRepository->search("", {"username"});
        
        for (auto& user : users)
        {
            userList.emplace_back(user->id, user->username + " (" + user->fullName + ")");
        }
    }
    catch (const std::exception&)
    {
    }
    
    return userList;
}

UserServiceResult UserService::createUserWithTransaction(const database::models::User& user,
                                                        long long createdBy,
                                                        const std::string& ipAddress,
                                                        const std::string& userAgent)
{
    UserServiceResult result;
    
    auto connection = database::ConnectionPool::getInstance().acquireConnection();
    if (!connection)
    {
        result.success = false;
        result.message = "Failed to acquire database connection";
        return result;
    }
    
    try
    {
        connection->beginTransaction();
        
        std::string validationError;
        if (!validateUserForCreation(user, validationError))
        {
            connection->rollbackTransaction();
            result.success = false;
            result.message = "Validation failed: " + validationError;
            return result;
        }
        
        if (!isUsernameAvailable(user.username))
        {
            connection->rollbackTransaction();
            result.success = false;
            result.message = "Username '" + user.username + "' is already taken";
            return result;
        }
        
        if (!isEmailAvailable(user.email))
        {
            connection->rollbackTransaction();
            result.success = false;
            result.message = "Email '" + user.email + "' is already registered";
            return result;
        }
        
        long long userId = userRepository->create(user);
        
        if (userId <= 0)
        {
            connection->rollbackTransaction();
            result.success = false;
            result.message = "Failed to create user";
            return result;
        }
        
        auto createdUser = userRepository->findById(userId);
        if (!createdUser)
        {
            connection->rollbackTransaction();
            result.success = false;
            result.message = "Failed to retrieve created user";
            return result;
        }
        
        logAuditEvent("users", userId, 
                     database::models::AuditAction::INSERT,
                     "", 
                     userToJsonString(*createdUser),
                     createdBy,
                     ipAddress,
                     userAgent,
                     "User created (transactional)");
        
        connection->commitTransaction();
        
        result.success = true;
        result.userId = userId;
        result.message = "User created successfully (transactional)";
        result.user = enrichUserWithDetails(std::move(createdUser));
    }
    catch (const std::exception& e)
    {
        connection->rollbackTransaction();
        result.success = false;
        result.message = "Error creating user (transactional): " + std::string(e.what());
    }
    
    return result;
}

UserServiceResult UserService::updateUserWithTransaction(long long userId,
                                                        const database::models::User& updatedUser,
                                                        long long updatedBy,
                                                        const std::string& ipAddress,
                                                        const std::string& userAgent)
{
    UserServiceResult result;
    
    auto connection = database::ConnectionPool::getInstance().acquireConnection();
    if (!connection)
    {
        result.success = false;
        result.message = "Failed to acquire database connection";
        return result;
    }
    
    try
    {
        connection->beginTransaction();
        
        auto currentUser = userRepository->findById(userId);
        if (!currentUser)
        {
            connection->rollbackTransaction();
            result.success = false;
            result.message = "User not found with ID: " + std::to_string(userId);
            return result;
        }
        
        std::string validationError;
        if (!validateUserForUpdate(updatedUser, validationError))
        {
            connection->rollbackTransaction();
            result.success = false;
            result.message = "Validation failed: " + validationError;
            return result;
        }
        
        if (currentUser->email != updatedUser.email && !isEmailAvailable(updatedUser.email))
        {
            connection->rollbackTransaction();
            result.success = false;
            result.message = "Email '" + updatedUser.email + "' is already registered";
            return result;
        }
        
        if (currentUser->username != updatedUser.username && !isUsernameAvailable(updatedUser.username))
        {
            connection->rollbackTransaction();
            result.success = false;
            result.message = "Username '" + updatedUser.username + "' is already taken";
            return result;
        }
        
        std::string oldValues = userToJsonString(*currentUser);
        
        bool updateSuccess = userRepository->update(userId, updatedUser);
        
        if (!updateSuccess)
        {
            connection->rollbackTransaction();
            result.success = false;
            result.message = "Failed to update user";
            return result;
        }
        
        auto updatedUserResult = userRepository->findById(userId);
        if (!updatedUserResult)
        {
            connection->rollbackTransaction();
            result.success = false;
            result.message = "Failed to retrieve updated user";
            return result;
        }
        
        std::string newValues = userToJsonString(*updatedUserResult);
        logAuditEvent("users", userId, 
                     database::models::AuditAction::UPDATE,
                     oldValues, 
                     newValues,
                     updatedBy,
                     ipAddress,
                     userAgent,
                     "User updated (transactional)");
        
        connection->commitTransaction();
        
        result.success = true;
        result.userId = userId;
        result.message = "User updated successfully (transactional)";
        result.user = enrichUserWithDetails(std::move(updatedUserResult));
    }
    catch (const std::exception& e)
    {
        connection->rollbackTransaction();
        result.success = false;
        result.message = "Error updating user (transactional): " + std::string(e.what());
    }
    
    return result;
}


void UserService::logAuditEvent(const std::string& tableName,
                               long long recordId,
                               database::models::AuditAction action,
                               const std::string& oldValues,
                               const std::string& newValues,
                               long long changedBy,
                               const std::string& ipAddress,
                               const std::string& userAgent,
                               const std::string& description)
{
    try
    {
        database::models::AuditLog auditLog;
        auditLog.tableName = tableName;
        auditLog.recordId = recordId;
        auditLog.action = action;
        auditLog.oldValues = oldValues;
        auditLog.newValues = newValues;
        auditLog.changedBy = changedBy;
        auditLog.ipAddress = ipAddress;
        auditLog.userAgent = userAgent;
        auditLog.description = description;
        
        auditRepository->create(auditLog);
    }
    catch (const std::exception&)
    {
    }
}

std::string UserService::userToJsonString(const database::models::User& user) const
{
    try
    {
        auto json = user.toJson();
        return utils::JsonUtils::objectToString(json, false);
    }
    catch (const std::exception&)
    {
        return "{}";
    }
}

std::string UserService::getCurrentDateTime() const
{
    return Poco::DateTimeFormatter::format(Poco::DateTime(), 
                                          Poco::DateTimeFormat::ISO8601_FORMAT);
}

bool UserService::validateUserForCreation(const database::models::User& user, std::string& errorMessage)
{
    if (user.username.empty())
    {
        errorMessage = "Username is required";
        return false;
    }
    
    if (user.email.empty())
    {
        errorMessage = "Email is required";
        return false;
    }
    
    if (user.fullName.empty())
    {
        errorMessage = "Full name is required";
        return false;
    }
    
    if (user.passwordHash.empty())
    {
        errorMessage = "Password is required";
        return false;
    }
    
    if (!utils::Validator::isValidEmail(user.email))
    {
        errorMessage = "Invalid email format";
        return false;
    }
    
    if (user.username.length() < 3 || user.username.length() > 50)
    {
        errorMessage = "Username must be between 3 and 50 characters";
        return false;
    }
    
    if (user.fullName.length() > 100)
    {
        errorMessage = "Full name cannot exceed 100 characters";
        return false;
    }
    
    return true;
}

bool UserService::validateUserForUpdate(const database::models::User& user, std::string& errorMessage)
{
    if (user.username.empty())
    {
        errorMessage = "Username is required";
        return false;
    }
    
    if (user.email.empty())
    {
        errorMessage = "Email is required";
        return false;
    }
    
    if (user.fullName.empty())
    {
        errorMessage = "Full name is required";
        return false;
    }
    
    if (!utils::Validator::isValidEmail(user.email))
    {
        errorMessage = "Invalid email format";
        return false;
    }
    
    if (user.username.length() < 3 || user.username.length() > 50)
    {
        errorMessage = "Username must be between 3 and 50 characters";
        return false;
    }
    
    if (user.fullName.length() > 100)
    {
        errorMessage = "Full name cannot exceed 100 characters";
        return false;
    }
    
    return true;
}

std::unique_ptr<database::models::User> UserService::enrichUserWithDetails(
    std::unique_ptr<database::models::User> user)
{
    if (!user)
    {
        return nullptr;
    }
    
    
    return user;
}

Poco::JSON::Object UserService::createPaginationMetadata(int page, int pageSize, int totalItems)
{
    Poco::JSON::Object pagination;
    
    pagination.set("page", page);
    pagination.set("pageSize", pageSize);
    pagination.set("totalItems", totalItems);
    
    int totalPages = (totalItems + pageSize - 1) / pageSize;
    pagination.set("totalPages", totalPages);
    
    pagination.set("hasPrevious", page > 1);
    pagination.set("hasNext", page < totalPages);
    
    if (page > 1)
    {
        pagination.set("previousPage", page - 1);
    }
    
    if (page < totalPages)
    {
        pagination.set("nextPage", page + 1);
    }
    
    return pagination;
}

}
