#pragma once

#include <string>
#include <Poco/JSON/Object.h>

namespace warehouse_backend::database::models
{

enum class UserRole
{
    ADMIN,
    MANAGER,
    WORKER,
    AUDITOR
};

class User
{
public:
    long long id;
    std::string username;
    std::string passwordHash;
    std::string fullName;
    std::string email;
    UserRole role;
    std::string createdAt;
    std::string lastLogin;
    bool isActive;
    std::string phoneNumber;

    User();
    explicit User(const Poco::JSON::Object& json);

    Poco::JSON::Object toJson() const;
    static User fromJson(const Poco::JSON::Object& json);

    static std::string roleToString(UserRole role);
    static UserRole stringToRole(const std::string& roleStr);

    bool validate() const;
};

} // namespace database::models