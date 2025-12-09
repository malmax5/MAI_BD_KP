#pragma once

#include "../database/models/User.hpp"
#include "../database/repositories/UserRepository.hpp"
#include "../config/ConfigManager.hpp"
#include "../utils/JsonUtils.hpp"
#include "../utils/Validator.hpp"
#include <Poco/JWT/Token.h>
#include <Poco/JWT/Signer.h>
#include <Poco/Timestamp.h>
#include <Poco/DateTime.h>
#include <memory>
#include <string>
#include <optional>

namespace warehouse_backend::services
{

struct LoginResult
{
    bool success;
    std::string message;
    std::string accessToken;
    std::string refreshToken;
    std::string tokenType;
    int expiresIn;
    std::unique_ptr<database::models::User> user;
    
    Poco::JSON::Object toJson() const;
};

struct RegistrationResult
{
    bool success;
    std::string message;
    long long userId;
    std::unique_ptr<database::models::User> user;
    
    Poco::JSON::Object toJson() const;
};

enum TokenType
{
    ACCESS_TOKEN = 0,
    REFRESH_TOKEN = 1,
    RESET_TOKEN = 2
};

struct TokenValidationResult
{
    bool isValid;
    std::string message;
    std::unique_ptr<database::models::User> user;
    TokenType tokenType;
    
    Poco::JSON::Object toJson() const;
};

struct PasswordResetResult
{
    bool success;
    std::string message;
    std::string resetToken;
    Poco::DateTime expiresAt;
    
    Poco::JSON::Object toJson() const;
};

class AuthService
{
public:
    AuthService();
    ~AuthService() = default;
    
    RegistrationResult registerUser(const std::string& username,
                                    const std::string& password,
                                    const std::string& email,
                                    const std::string& fullName,
                                    const std::string& role = "worker",
                                    const std::string& phoneNumber = "");
    
    LoginResult login(const std::string& username, const std::string& password);
    LoginResult loginWithEmail(const std::string& email, const std::string& password);
    
    TokenValidationResult validateToken(const std::string& token);
    LoginResult refreshToken(const std::string& refreshToken);
    
    bool logout(long long userId, const std::string& token);
    
    bool changePassword(long long userId, 
                        const std::string& currentPassword, 
                        const std::string& newPassword);
    
    PasswordResetResult requestPasswordReset(const std::string& email);
    bool resetPassword(const std::string& resetToken, const std::string& newPassword);
    
    bool verifyEmail(long long userId, const std::string& verificationCode);
    
    bool hasPermission(long long userId, const std::string& permission);
    bool hasRole(long long userId, const std::string& role);
    bool isUserActive(long long userId);
    
    int getActiveSessionsCount(long long userId);
    bool invalidateAllSessions(long long userId);
    bool invalidateSession(long long userId, const std::string& sessionId);
    
    bool deactivateUser(long long userId, long long adminId, const std::string& reason);
    bool activateUser(long long userId, long long adminId);
    bool updateUserRole(long long userId, long long adminId, const std::string& newRole);
    
    std::unique_ptr<database::models::User> getUserFromToken(const std::string& token);
    std::vector<std::string> getUserPermissions(long long userId);
    
    std::string generateAccessToken(const database::models::User& user);
    std::string generateRefreshToken(const database::models::User& user);
    std::string generateResetToken(const database::models::User& user);
    
    static bool isValidPassword(const std::string& password);
    static bool isValidUsername(const std::string& username);
    static bool isValidEmailFormat(const std::string& email);
    
private:
    struct LoginAttempt
    {
        std::string identifier;
        int attempts;
        Poco::Timestamp lastAttempt;
        bool locked;
        Poco::Timestamp lockoutUntil;
    };

    struct TokenCacheEntry
    {
        std::string token;
        long long userId;
        Poco::Timestamp validUntil;
        TokenType tokenType;
    };

private:
    std::string hashPassword(const std::string& password);
    bool verifyPassword(const std::string& password, const std::string& hash);
    
    Poco::JWT::Token decodeToken(const std::string& token);
    bool isTokenExpired(const Poco::JWT::Token& token);
    bool isTokenValid(const Poco::JWT::Token& token, const TokenType& expectedType);
    
    void logAuthEvent(long long userId, 
                      const std::string& action,
                      const std::string& ipAddress = "",
                      const std::string& userAgent = "");
    
    std::string getJwtSecret();
    int getTokenExpiryHours();
    int getRefreshTokenExpiryDays();
    int getBcryptCost();
    int getMaxLoginAttempts();
    int getLockoutDurationMinutes();
    
    std::unique_ptr<database::repositories::UserRepository> userRepository;
    
    std::map<std::string, LoginAttempt> loginAttemptsCache;
    std::mutex cacheMutex;
    
    void clearExpiredLoginAttempts();
    bool isAccountLocked(const std::string& identifier);
    void recordLoginAttempt(const std::string& identifier, bool successful);
    void resetLoginAttempts(const std::string& identifier);
    
    std::map<std::string, TokenCacheEntry> tokenCache;
    std::mutex tokenCacheMutex;
    
    void addToTokenCache(const std::string& token, long long userId, TokenType tokenType);
    void removeFromTokenCache(const std::string& token);
    void clearExpiredTokensFromCache();
    
    static const std::map<std::string, std::vector<std::string>> ROLE_PERMISSIONS;
};

} // namespace services
