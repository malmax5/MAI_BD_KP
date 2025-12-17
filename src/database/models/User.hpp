#pragma once

#include <string>
#include <Poco/JSON/Object.h>
#include <Poco/Nullable.h>

namespace warehouse_backend::database::models
{

enum class UserRole
{
    UNKNOWN,
    ADMIN,
    MANAGER,
    WORKER,
    AUDITOR
};

class User
{
public:
    Poco::Int64 id;
    std::string username;
    std::string passwordHash;
    std::string fullName;
    std::string email;
    UserRole role;
    std::string createdAt;
    Poco::Nullable<std::string> lastLogin;
    bool isActive;
    Poco::Nullable<std::string> phoneNumber;

    User();
    explicit User(const Poco::JSON::Object& json);

    Poco::JSON::Object toJson() const;
    static User fromJson(const Poco::JSON::Object& json);

    static std::string roleToString(UserRole role);
    static UserRole stringToRole(const std::string& roleStr);

    bool validate() const;
};

} // namespace database::models
