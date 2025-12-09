#pragma once

#include <string>
#include <Poco/JSON/Object.h>
#include <Poco/Nullable.h>

namespace warehouse_backend::database::models
{

enum class AuditAction
{
    INSERT,
    UPDATE,
    DELETE,
    SOFT_DELETE
};

class AuditLog
{
public:
    Poco::Int64 id;
    std::string tableName;
    Poco::Int64 recordId;
    AuditAction action;
    Poco::Nullable<std::string> oldValues;
    Poco::Nullable<std::string> newValues;
    Poco::Int64 changedBy;
    Poco::Nullable<std::string> changedAt;
    Poco::Nullable<std::string> ipAddress;
    Poco::Nullable<std::string> userAgent;
    Poco::Nullable<std::string> description;

    std::string changedByName;

    AuditLog();
    explicit AuditLog(const Poco::JSON::Object& json);

    Poco::JSON::Object toJson() const;
    static AuditLog fromJson(const Poco::JSON::Object& json);

    bool validate() const;
    
    Poco::JSON::Object::Ptr getOldValuesJson() const;
    Poco::JSON::Object::Ptr getNewValuesJson() const;
    bool hasChanges() const;
    std::vector<std::string> getChangedFields() const;
    
    static std::string auditActionToString(AuditAction action);
    static AuditAction stringToAuditAction(const std::string& actionStr);
    
    static constexpr int MAX_TABLE_NAME = 100;
    static constexpr int MAX_IP_ADDRESS = 45;
};

} // namespace database::models
