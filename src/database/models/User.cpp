#include "User.hpp"
#include "../../utils/JsonUtils.hpp"
#include "../../utils/Validator.hpp"
#include <Poco/JSON/Parser.h>
#include <Poco/JSON/Stringifier.h>
#include <Poco/DateTimeFormatter.h>
#include <Poco/DateTimeParser.h>
#include <sstream>

namespace warehouse_backend::database::models
{

using namespace warehouse_backend::utils;

User::User()
    : id(0),
      role(UserRole::WORKER),
      isActive(true)
{

}

User::User(const Poco::JSON::Object& json)
{
    username = JsonUtils::getString(json, "username", "");
    passwordHash = JsonUtils::getString(json, "password_hash", "");
    fullName = JsonUtils::getString(json, "full_name", "");
    email = JsonUtils::getString(json, "email", "");
    
    std::string roleStr = JsonUtils::getString(json, "role", "worker");
    role = stringToRole(roleStr);
    
    createdAt = JsonUtils::getString(json, "created_at", "");
    lastLogin = JsonUtils::getString(json, "last_login", "");
    isActive = JsonUtils::getBool(json, "is_active", true);
    phoneNumber = JsonUtils::getString(json, "phone_number", "");
    
    if (json.has("id"))
    {
        id = JsonUtils::getInt(json, "id", 0);
    }
}

Poco::JSON::Object User::toJson() const
{
    Poco::JSON::Object json;
    
    if (id > 0)
    {
        json.set("id", id);
    }
    
    json.set("username", username);
    json.set("full_name", fullName);
    json.set("email", email);
    json.set("role", roleToString(role));
    json.set("created_at", createdAt);
    
    if (!lastLogin.empty())
    {
        json.set("last_login", lastLogin);
    }
    
    json.set("is_active", isActive);
    
    if (!phoneNumber.empty())
    {
        json.set("phone_number", phoneNumber);
    }
    
    return json;
}

User User::fromJson(const Poco::JSON::Object& json)
{
    return User(json);
}

std::string User::roleToString(UserRole role)
{
    switch (role)
    {
        case UserRole::ADMIN: return "admin";
        case UserRole::MANAGER: return "manager";
        case UserRole::WORKER: return "worker";
        case UserRole::AUDITOR: return "auditor";
        default: return "worker";
    }
}

UserRole User::stringToRole(const std::string& roleStr)
{
    if (roleStr == "admin") return UserRole::ADMIN;
    if (roleStr == "manager") return UserRole::MANAGER;
    if (roleStr == "auditor") return UserRole::AUDITOR;
    return UserRole::WORKER;
}

bool User::validate() const
{
    if (username.empty() || username.length() > 50)
    {
        return false;
    }
    
    if (!Validator::isValidEmail(email))
    {
        return false;
    }
    
    if (fullName.empty() || fullName.length() > 100)
    {
        return false;
    }
    
    return true;
}

} // namespace database::models
