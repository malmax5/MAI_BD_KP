#include "AuthService.hpp"
#include <Poco/JWT/Signer.h>
#include <Poco/JWT/JWTException.h>
#include <Poco/Crypto/DigestEngine.h>
#include <Poco/Random.h>
#include <Poco/Base64Encoder.h>
#include <Poco/Base64Decoder.h>
#include <Poco/StreamCopier.h>
#include <Poco/DateTimeFormatter.h>
#include <Poco/DateTimeParser.h>
#include <sodium/crypto_pwhash.h>
#include <sstream>
#include <algorithm>
#include <chrono>

namespace warehouse_backend::services
{

const std::map<std::string, std::vector<std::string>> AuthService::ROLE_PERMISSIONS = {
    {"admin", {"users:read", "users:write", "products:read", "products:write", 
               "orders:read", "orders:write", "inventory:read", "inventory:write",
               "reports:read", "settings:write", "audit:read"}},
    {"manager", {"users:read", "products:read", "products:write", "orders:read", 
                 "orders:write", "inventory:read", "inventory:write", "reports:read"}},
    {"worker", {"products:read", "orders:read", "inventory:read", "orders:write"}},
    {"auditor", {"audit:read", "reports:read", "users:read", "products:read", 
                 "orders:read", "inventory:read"}}
};


Poco::JSON::Object LoginResult::toJson() const
{
    Poco::JSON::Object json;
    json.set("success", success);
    json.set("message", message);
    
    if (success)
    {
        json.set("access_token", accessToken);
        json.set("refresh_token", refreshToken);
        json.set("token_type", tokenType);
        json.set("expires_in", expiresIn);
        
        if (user)
        {
            json.set("user", user->toJson());
        }
    }
    
    return json;
}

Poco::JSON::Object RegistrationResult::toJson() const
{
    Poco::JSON::Object json;
    json.set("success", success);
    json.set("message", message);
    json.set("user_id", userId);
    
    if (user)
    {
        json.set("user", user->toJson());
    }
    
    return json;
}

Poco::JSON::Object TokenValidationResult::toJson() const
{
    Poco::JSON::Object json;
    json.set("is_valid", isValid);
    json.set("message", message);
    json.set("token_type", tokenType);
    
    if (user)
    {
        json.set("user", user->toJson());
    }
    
    return json;
}

Poco::JSON::Object PasswordResetResult::toJson() const
{
    Poco::JSON::Object json;
    json.set("success", success);
    json.set("message", message);
    json.set("reset_token", resetToken);
    json.set("expires_at", Poco::DateTimeFormatter::format(expiresAt, "%Y-%m-%d %H:%M:%S"));
    
    return json;
}


AuthService::AuthService()
    : userRepository(std::make_unique<database::repositories::UserRepository>())
{

}

RegistrationResult AuthService::registerUser(const std::string& username,
                                             const std::string& password,
                                             const std::string& email,
                                             const std::string& fullName,
                                             const std::string& role,
                                             const std::string& phoneNumber)
{
    RegistrationResult result;
    
    try
    {
        if (!isValidUsername(username))
        {
            result.success = false;
            result.message = "Invalid username format";
            return result;
        }
        
        if (!isValidPassword(password))
        {
            result.success = false;
            result.message = "Password does not meet security requirements";
            return result;
        }
        
        if (!isValidEmailFormat(email))
        {
            result.success = false;
            result.message = "Invalid email format";
            return result;
        }
        
        auto existingUser = userRepository->findByUsername(username);
        if (existingUser)
        {
            result.success = false;
            result.message = "Username already exists";
            return result;
        }
        
        existingUser = userRepository->findByEmail(email);
        if (existingUser)
        {
            result.success = false;
            result.message = "Email already registered";
            return result;
        }
        
        database::models::User user;
        user.username = username;
        user.passwordHash = hashPassword(password);
        user.email = email;
        user.fullName = fullName;
        user.role = database::models::User::stringToRole(role);
        user.phoneNumber = phoneNumber;
        user.isActive = true;

        if (!user.validate())
        {
            result.success = false;
            result.message = "User data validation failed";
            return result;
        }
        
        result.userId = userRepository->create(user);
        
        if (result.userId > 0)
        {
            result.success = true;
            result.message = "User registered successfully";
            result.user = userRepository->findById(result.userId);
            
            logAuthEvent(result.userId, "REGISTRATION");
        }
        else
        {
            result.success = false;
            result.message = "Failed to create user in database";
        }
    }
    catch (const std::exception& e)
    {
        result.success = false;
        result.message = std::string("Registration failed: ") + e.what();
    }
    
    return result;
}

LoginResult AuthService::login(const std::string& username, const std::string& password)
{
    LoginResult result;
    
    try
    {
        if (isAccountLocked(username))
        {
            result.success = false;
            result.message = "Account is temporarily locked due to too many failed attempts";
            return result;
        }
        
        auto user = userRepository->findByUsername(username);
        if (!user)
        {
            recordLoginAttempt(username, false);
            result.success = false;
            result.message = "Invalid username or password";
            return result;
        }
        
        if (!user->isActive)
        {
            recordLoginAttempt(username, false);
            result.success = false;
            result.message = "User account is deactivated";
            return result;
        }
        
        if (!verifyPassword(password, user->passwordHash))
        {
            recordLoginAttempt(username, false);
            result.success = false;
            result.message = "Invalid username or password";
            return result;
        }
        
        recordLoginAttempt(username, true);
        
        result.accessToken = generateAccessToken(*user);
        result.refreshToken = generateRefreshToken(*user);
        result.expiresIn = getTokenExpiryHours() * 3600;
        result.tokenType = "Bearer";
        
        Poco::DateTime now;
        user->lastLogin = Poco::DateTimeFormatter::format(now, "%Y-%m-%d %H:%M:%S");
        userRepository->update(user->id, *user);
        
        result.user = std::move(user);
        result.success = true;
        result.message = "Login successful";
        
        addToTokenCache(result.accessToken, result.user->id, TokenType::ACCESS_TOKEN);
        addToTokenCache(result.refreshToken, result.user->id, TokenType::REFRESH_TOKEN);
        
        logAuthEvent(result.user->id, "LOGIN");
    }
    catch (const std::exception& e)
    {
        result.success = false;
        result.message = std::string("Login failed: ") + e.what();
    }
    
    return result;
}

LoginResult AuthService::loginWithEmail(const std::string& email, const std::string& password)
{
    LoginResult result;
    
    try
    {
        if (isAccountLocked(email))
        {
            result.success = false;
            result.message = "Account is temporarily locked due to too many failed attempts";
            return result;
        }
        
        auto user = userRepository->findByEmail(email);
        if (!user)
        {
            recordLoginAttempt(email, false);
            result.success = false;
            result.message = "Invalid email or password";
            return result;
        }
        
        if (!user->isActive)
        {
            recordLoginAttempt(email, false);
            result.success = false;
            result.message = "User account is deactivated";
            return result;
        }
        
        if (!verifyPassword(password, user->passwordHash))
        {
            recordLoginAttempt(email, false);
            result.success = false;
            result.message = "Invalid email or password";
            return result;
        }
        
        recordLoginAttempt(email, true);
        
        result.accessToken = generateAccessToken(*user);
        result.refreshToken = generateRefreshToken(*user);
        result.tokenType = "Bearer";
        result.expiresIn = getTokenExpiryHours() * 3600;
        
        Poco::DateTime now;
        user->lastLogin = Poco::DateTimeFormatter::format(now, "%Y-%m-%d %H:%M:%S");
        userRepository->update(user->id, *user);
        
        result.user = std::move(user);
        result.success = true;
        result.message = "Login successful";
        
        addToTokenCache(result.accessToken, result.user->id, TokenType::ACCESS_TOKEN);
        addToTokenCache(result.refreshToken, result.user->id, TokenType::REFRESH_TOKEN);
        
        logAuthEvent(result.user->id, "LOGIN_WITH_EMAIL");
    }
    catch (const std::exception& e)
    {
        result.success = false;
        result.message = std::string("Login failed: ") + e.what();
    }
    
    return result;
}

TokenValidationResult AuthService::validateToken(const std::string& token)
{
    TokenValidationResult result;
    
    try
    {
        {
            std::lock_guard<std::mutex> lock(tokenCacheMutex);
            auto it = tokenCache.find(token);
            if (it != tokenCache.end())
            {
                if (it->second.validUntil > Poco::Timestamp())
                {
                    auto user = userRepository->findById(it->second.userId);
                    if (user && user->isActive)
                    {
                        result.isValid = true;
                        result.message = "Token is valid";
                        result.user = std::move(user);
                        result.tokenType = it->second.tokenType;
                        return result;
                    }
                }
                else
                {
                    tokenCache.erase(it);
                }
            }
        }
        
        auto jwtToken = decodeToken(token);
        
        if (!jwtToken.payload().has("type") || !jwtToken.payload().has("user_id"))
        {
            result.isValid = false;
            result.message = "Invalid token structure";
            return result;
        }
        
        TokenType tokenType = (TokenType)jwtToken.payload().getValue<int>("type");
        long long userId = jwtToken.payload().getValue<long long>("user_id");
        
        if (!(tokenType == TokenType::ACCESS_TOKEN || 
              tokenType == TokenType::REFRESH_TOKEN || 
              tokenType == TokenType::RESET_TOKEN))
        {
            result.isValid = false;
            result.message = "Invalid token type";
            return result;
        }
        
        if (isTokenExpired(jwtToken))
        {
            result.isValid = false;
            result.message = "Token has expired";
            return result;
        }
        
        Poco::JWT::Signer signer(getJwtSecret());
        try
        {
            signer.verify(jwtToken.toString());
        }
        catch (const Poco::JWT::JWTException& e)
        {
            result.isValid = false;
            result.message = std::string("Token signature verification failed: ") + e.what();
            return result;
        }
        
        auto user = userRepository->findById(userId);
        if (!user)
        {
            result.isValid = false;
            result.message = "User not found";
            return result;
        }
        
        if (!user->isActive)
        {
            result.isValid = false;
            result.message = "User account is deactivated";
            return result;
        }
        
        addToTokenCache(token, userId, tokenType);
        
        result.isValid = true;
        result.message = "Token is valid";
        result.user = std::move(user);
        result.tokenType = tokenType;
    }
    catch (const Poco::JWT::JWTException& e)
    {
        result.isValid = false;
        result.message = std::string("JWT error: ") + e.what();
    }
    catch (const std::exception& e)
    {
        result.isValid = false;
        result.message = std::string("Token validation failed: ") + e.what();
    }
    
    return result;
}

LoginResult AuthService::refreshToken(const std::string& refreshToken)
{
    LoginResult result;
    
    try
    {
        auto validationResult = validateToken(refreshToken);
        
        if (!validationResult.isValid || validationResult.tokenType != TokenType::REFRESH_TOKEN)
        {
            result.success = false;
            result.message = "Invalid refresh token";
            return result;
        }
        
        if (!validationResult.user)
        {
            result.success = false;
            result.message = "User not found";
            return result;
        }
        
        result.accessToken = generateAccessToken(*validationResult.user);
        result.refreshToken = generateRefreshToken(*validationResult.user);
        result.tokenType = "Bearer";
        result.expiresIn = getTokenExpiryHours() * 3600;
        result.user = std::move(validationResult.user);
        result.success = true;
        result.message = "Token refreshed successfully";
        
        addToTokenCache(result.accessToken, result.user->id, TokenType::ACCESS_TOKEN);
        addToTokenCache(result.refreshToken, result.user->id, TokenType::REFRESH_TOKEN);
        
        removeFromTokenCache(refreshToken);
        
        logAuthEvent(result.user->id, "TOKEN_REFRESH");
    }
    catch (const std::exception& e)
    {
        result.success = false;
        result.message = std::string("Token refresh failed: ") + e.what();
    }
    
    return result;
}

bool AuthService::logout(long long userId, const std::string& token)
{
    try
    {
        removeFromTokenCache(token);
        
        
        logAuthEvent(userId, "LOGOUT");
        return true;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

bool AuthService::changePassword(long long userId, 
                                 const std::string& currentPassword, 
                                 const std::string& newPassword)
{
    try
    {
        auto user = userRepository->findById(userId);
        if (!user)
            return false;
        
        if (!verifyPassword(currentPassword, user->passwordHash))
            return false;
        
        if (!isValidPassword(newPassword))
            return false;
        
        user->passwordHash = hashPassword(newPassword);
        bool updated = userRepository->update(userId, *user);
        
        if (updated)
        {
            invalidateAllSessions(userId);
            logAuthEvent(userId, "PASSWORD_CHANGE");
        }
        
        return updated;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

PasswordResetResult AuthService::requestPasswordReset(const std::string& email)
{
    PasswordResetResult result;
    
    try
    {
        auto user = userRepository->findByEmail(email);
        if (!user)
        {
            result.success = false;
            result.message = "No user found with this email";
            return result;
        }
        
        if (!user->isActive)
        {
            result.success = false;
            result.message = "User account is deactivated";
            return result;
        }
        
        result.resetToken = generateResetToken(*user);
        result.success = true;
        result.message = "Password reset token generated";
        
        addToTokenCache(result.resetToken, user->id, TokenType::RESET_TOKEN);
        
        logAuthEvent(user->id, "PASSWORD_RESET_REQUEST");
    }
    catch (const std::exception& e)
    {
        result.success = false;
        result.message = std::string("Password reset request failed: ") + e.what();
    }
    
    return result;
}

bool AuthService::resetPassword(const std::string& resetToken, const std::string& newPassword)
{
    try
    {
        auto validationResult = validateToken(resetToken);
        
        if (!validationResult.isValid || validationResult.tokenType != TokenType::RESET_TOKEN)
            return false;
        
        if (!validationResult.user)
            return false;
        
        if (!isValidPassword(newPassword))
            return false;
        
        validationResult.user->passwordHash = hashPassword(newPassword);
        bool updated = userRepository->update(validationResult.user->id, *validationResult.user);
        
        if (updated)
        {
            removeFromTokenCache(resetToken);
            invalidateAllSessions(validationResult.user->id);
            
            logAuthEvent(validationResult.user->id, "PASSWORD_RESET_COMPLETE");
        }
        
        return updated;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

bool AuthService::verifyEmail(long long userId, const std::string& verificationCode)
{
    try
    {
        logAuthEvent(userId, "EMAIL_VERIFICATION");
        return true;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

bool AuthService::hasPermission(long long userId, const std::string& permission)
{
    try
    {
        auto user = userRepository->findById(userId);
        if (!user || !user->isActive)
            return false;
        
        std::string roleStr = database::models::User::roleToString(user->role);
        auto it = ROLE_PERMISSIONS.find(roleStr);
        if (it == ROLE_PERMISSIONS.end())
            return false;
        
        const auto& permissions = it->second;
        return std::find(permissions.begin(), permissions.end(), permission) != permissions.end();
    }
    catch (const std::exception&)
    {
        return false;
    }
}

bool AuthService::hasRole(long long userId, const std::string& role)
{
    try
    {
        auto user = userRepository->findById(userId);
        if (!user || !user->isActive)
            return false;
        
        std::string userRoleStr = database::models::User::roleToString(user->role);
        return userRoleStr == role;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

bool AuthService::isUserActive(long long userId)
{
    try
    {
        auto user = userRepository->findById(userId);
        return user && user->isActive;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

int AuthService::getActiveSessionsCount(long long userId)
{
    std::lock_guard<std::mutex> lock(tokenCacheMutex);
    
    int count = 0;
    for (const auto& entry : tokenCache)
    {
        if (entry.second.userId == userId && 
            entry.second.validUntil > Poco::Timestamp())
        {
            count++;
        }
    }
    
    return count;
}

bool AuthService::invalidateAllSessions(long long userId)
{
    try
    {
        std::lock_guard<std::mutex> lock(tokenCacheMutex);
        
        for (auto it = tokenCache.begin(); it != tokenCache.end();)
        {
            if (it->second.userId == userId)
            {
                it = tokenCache.erase(it);
            }
            else
            {
                ++it;
            }
        }
        
        logAuthEvent(userId, "ALL_SESSIONS_INVALIDATED");
        return true;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

bool AuthService::invalidateSession(long long userId, const std::string& sessionId)
{
    try
    {
        removeFromTokenCache(sessionId);
        logAuthEvent(userId, "SESSION_INVALIDATED");
        return true;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

bool AuthService::deactivateUser(long long userId, long long adminId, const std::string& reason)
{
    try
    {
        auto user = userRepository->findById(userId);
        if (!user)
            return false;
        
        user->isActive = false;
        bool updated = userRepository->update(userId, *user);
        
        if (updated)
        {
            invalidateAllSessions(userId);
            
            logAuthEvent(adminId, "USER_DEACTIVATED", "", 
                        "Deactivated user " + std::to_string(userId) + ": " + reason);
            logAuthEvent(userId, "ACCOUNT_DEACTIVATED");
        }
        
        return updated;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

bool AuthService::activateUser(long long userId, long long adminId)
{
    try
    {
        auto user = userRepository->findById(userId);
        if (!user)
            return false;
        
        user->isActive = true;
        bool updated = userRepository->update(userId, *user);
        
        if (updated)
        {
            logAuthEvent(adminId, "USER_ACTIVATED", "", 
                        "Activated user " + std::to_string(userId));
            logAuthEvent(userId, "ACCOUNT_ACTIVATED");
        }
        
        return updated;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

bool AuthService::updateUserRole(long long userId, long long adminId, const std::string& newRole)
{
    try
    {
        auto user = userRepository->findById(userId);
        if (!user)
            return false;
        
        auto newRoleEnum = database::models::User::stringToRole(newRole);
        user->role = newRoleEnum;
        bool updated = userRepository->update(userId, *user);
        
        if (updated)
        {
            invalidateAllSessions(userId);
            
            logAuthEvent(adminId, "USER_ROLE_UPDATED", "", 
                        "Updated role for user " + std::to_string(userId) + " to " + newRole);
            logAuthEvent(userId, "ROLE_UPDATED");
        }
        
        return updated;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

std::unique_ptr<database::models::User> AuthService::getUserFromToken(const std::string& token)
{
    auto validationResult = validateToken(token);
    if (validationResult.isValid)
    {
        return std::move(validationResult.user);
    }
    return nullptr;
}

std::vector<std::string> AuthService::getUserPermissions(long long userId)
{
    std::vector<std::string> permissions;
    
    try
    {
        auto user = userRepository->findById(userId);
        if (!user || !user->isActive)
            return permissions;
        
        std::string roleStr = database::models::User::roleToString(user->role);
        auto it = ROLE_PERMISSIONS.find(roleStr);
        if (it != ROLE_PERMISSIONS.end())
        {
            permissions = it->second;
        }
    }
    catch (const std::exception&)
    {
    }
    
    return permissions;
}


std::string AuthService::generateAccessToken(const database::models::User& user)
{
    Poco::JWT::Token token;
    
    token.setType("JWT");
    token.setSubject(std::to_string(user.id));
    token.setIssuer("WarehouseBackend");
    
    Poco::Timestamp now;
    token.setIssuedAt(now);
    token.setExpiration(now + (getTokenExpiryHours() * 3600LL * 1000000LL));
    
    token.payload().set("user_id", user.id);
    token.payload().set("username", user.username);
    token.payload().set("email", user.email);
    token.payload().set("role", database::models::User::roleToString(user.role));
    token.payload().set("type", (int)TokenType::ACCESS_TOKEN);
    
    Poco::JWT::Signer signer(getJwtSecret());
    return signer.sign(token, Poco::JWT::Signer::ALGO_HS256);
}

std::string AuthService::generateRefreshToken(const database::models::User& user)
{
    Poco::JWT::Token token;
    
    token.setType("JWT");
    token.setSubject(std::to_string(user.id));
    token.setIssuer("WarehouseBackend");
    
    Poco::Timestamp now;
    token.setIssuedAt(now);
    token.setExpiration(now + (getRefreshTokenExpiryDays() * 24 * 3600 * 1000000));
    
    token.payload().set("user_id", user.id);
    token.payload().set("type", (int)TokenType::REFRESH_TOKEN);
    
    Poco::Random random;
    random.seed();
    token.payload().set("jti", std::to_string(random.next()));
    
    Poco::JWT::Signer signer(getJwtSecret());
    return signer.sign(token, Poco::JWT::Signer::ALGO_HS256);
}

std::string AuthService::generateResetToken(const database::models::User& user)
{
    Poco::JWT::Token token;
    
    token.setType("JWT");
    token.setSubject(std::to_string(user.id));
    token.setIssuer("WarehouseBackend");
    
    Poco::Timestamp now;
    token.setIssuedAt(now);
    token.setExpiration(now + (getRefreshTokenExpiryDays() * 24LL * 3600LL * 1000000LL));
    
    token.payload().set("user_id", user.id);
    token.payload().set("type", (int)TokenType::RESET_TOKEN);
    token.payload().set("purpose", "password_reset");
    
    Poco::Random random;
    random.seed();
    token.payload().set("jti", std::to_string(random.next()));
    
    Poco::JWT::Signer signer(getJwtSecret());
    return signer.sign(token, Poco::JWT::Signer::ALGO_HS256);
}

bool AuthService::isValidPassword(const std::string& password)
{
    return utils::Validator::isValidPassword(password, 8, true, true, true, true);
}

bool AuthService::isValidUsername(const std::string& username)
{
    if (username.length() < 3 || username.length() > 50)
        return false;
    
    for (char c : username)
    {
        if (!std::isalnum(c) && c != '_')
            return false;
    }
    
    return true;
}

bool AuthService::isValidEmailFormat(const std::string& email)
{
    return utils::Validator::isValidEmail(email);
}

std::string AuthService::hashPassword(const std::string& password)
{
    if (password.length() > crypto_pwhash_PASSWD_MAX)
    {
        throw std::runtime_error("Password is too long");
    }

    char hash[crypto_pwhash_STRBYTES];

    if (crypto_pwhash_str(
            hash,
            password.c_str(), 
            password.length(),
            crypto_pwhash_OPSLIMIT_INTERACTIVE,
            crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0)
    {
        throw std::runtime_error("Failed to hash password: out of memory or other error");
    }

    return std::string(hash);
}

bool AuthService::verifyPassword(const std::string& password, const std::string& hash)
{
    if (crypto_pwhash_str_verify(
            hash.c_str(),
            password.c_str(),
            password.size()) == 0)
    {
        return true;
    }
    else
    {
        return false;
    }
}

Poco::JWT::Token AuthService::decodeToken(const std::string& token)
{
    Poco::JWT::Signer signer(getJwtSecret());
    return signer.verify(token);
}

bool AuthService::isTokenExpired(const Poco::JWT::Token& token)
{
    Poco::Timestamp expiration = token.getExpiration();

    if (expiration == 0)
        return false;

    Poco::Timestamp now;
    return now > expiration;
}

bool AuthService::isTokenValid(const Poco::JWT::Token& token, const TokenType& expectedType)
{
    if (!token.payload().has("type"))
        return false;
    
    TokenType tokenType = (TokenType)token.payload().getValue<int>("type");
    return tokenType == expectedType && !isTokenExpired(token);
}

void AuthService::logAuthEvent(long long userId, 
                               const std::string& action,
                               const std::string& ipAddress,
                               const std::string& userAgent)
{
    std::cout << "Auth Event - User: " << userId 
              << ", Action: " << action 
              << ", IP: " << ipAddress 
              << ", Agent: " << userAgent << std::endl;
}

std::string AuthService::getJwtSecret()
{
    auto& configManager = config::ConfigManager::getInstance();
    return configManager.getSecurityConfig().jwtSecret;
}

int AuthService::getTokenExpiryHours()
{
    auto& configManager = config::ConfigManager::getInstance();
    return configManager.getSecurityConfig().tokenExpiryHours;
}

int AuthService::getRefreshTokenExpiryDays()
{
    auto& configManager = config::ConfigManager::getInstance();
    return configManager.getSecurityConfig().refreshTokenExpiryDays;
}

int AuthService::getBcryptCost()
{
    auto& configManager = config::ConfigManager::getInstance();
    return configManager.getSecurityConfig().bcryptCost;
}

int AuthService::getMaxLoginAttempts()
{
    auto& configManager = config::ConfigManager::getInstance();
    return configManager.getSecurityConfig().maxLoginAttempts;
}

int AuthService::getLockoutDurationMinutes()
{
    auto& configManager = config::ConfigManager::getInstance();
    return configManager.getSecurityConfig().lockoutDurationMinutes;
}

void AuthService::clearExpiredLoginAttempts()
{
    Poco::Timestamp now;
    
    for (auto it = loginAttemptsCache.begin(); it != loginAttemptsCache.end();)
    {
        if (it->second.locked && 
            it->second.lockoutUntil <= now)
        {
            it = loginAttemptsCache.erase(it);
        }
        else if (!it->second.locked &&
                 (now - it->second.lastAttempt) > (getLockoutDurationMinutes() * 60 * 1000000))
        {
            it = loginAttemptsCache.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

bool AuthService::isAccountLocked(const std::string& identifier)
{
    std::lock_guard<std::mutex> lock(cacheMutex);
    clearExpiredLoginAttempts();
    
    auto it = loginAttemptsCache.find(identifier);
    if (it != loginAttemptsCache.end() && it->second.locked)
    {
        if (it->second.lockoutUntil > Poco::Timestamp())
        {
            return true;
        }
        else
        {
            loginAttemptsCache.erase(it);
            return false;
        }
    }
    
    return false;
}

void AuthService::recordLoginAttempt(const std::string& identifier, bool successful)
{
    std::lock_guard<std::mutex> lock(cacheMutex);
    clearExpiredLoginAttempts();
    
    auto& attempt = loginAttemptsCache[identifier];
    attempt.identifier = identifier;
    
    if (successful)
    {
        attempt.attempts = 0;
        attempt.locked = false;
    }
    else
    {
        attempt.attempts++;
        attempt.lastAttempt = Poco::Timestamp();
        
        if (attempt.attempts >= getMaxLoginAttempts())
        {
            attempt.locked = true;
            attempt.lockoutUntil = Poco::Timestamp() + 
                (getLockoutDurationMinutes() * 60 * 1000000);
        }
    }
}

void AuthService::resetLoginAttempts(const std::string& identifier)
{
    std::lock_guard<std::mutex> lock(cacheMutex);
    loginAttemptsCache.erase(identifier);
}

void AuthService::addToTokenCache(const std::string& token, long long userId, TokenType tokenType)
{
    std::lock_guard<std::mutex> lock(tokenCacheMutex);
    
    TokenCacheEntry entry;
    entry.token = token;
    entry.userId = userId;
    entry.tokenType = tokenType;
    
    Poco::Timestamp expiry;
    if (tokenType == TokenType::REFRESH_TOKEN)
    {
        expiry += (getRefreshTokenExpiryDays() * 24LL * 3600LL * 1000000LL);
    }
    else if (tokenType == TokenType::RESET_TOKEN)
    {
        expiry += (24LL * 3600LL * 1000000LL);
    }
    else
    {
        expiry += (getTokenExpiryHours() * 3600LL * 1000000LL);
    }
    
    entry.validUntil = expiry;
    tokenCache[token] = entry;

    clearExpiredTokensFromCache();
}

void AuthService::removeFromTokenCache(const std::string& token)
{
    std::lock_guard<std::mutex> lock(tokenCacheMutex);
    tokenCache.erase(token);
}

void AuthService::clearExpiredTokensFromCache()
{
    Poco::Timestamp now;
    
    for (auto it = tokenCache.begin(); it != tokenCache.end();)
    {
        if (it->second.validUntil <= now)
        {
            it = tokenCache.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

} // namespace services
