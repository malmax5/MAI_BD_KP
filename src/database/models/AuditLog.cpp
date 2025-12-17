#include "AuditLog.hpp"
#include "../../utils/JsonUtils.hpp"
#include "../../utils/Validator.hpp"
#include "../../utils/DateUtils.hpp"
#include <Poco/JSON/Parser.h>
#include <Poco/JSON/Stringifier.h>
#include <Poco/JSON/Query.h>
#include <sstream>
#include <iomanip>
#include <set>

namespace warehouse_backend::database::models
{

using namespace warehouse_backend::utils;

AuditLog::AuditLog()
    : id(0),
      recordId(0),
      action(AuditAction::INSERT),
      changedBy(0)
{

}

AuditLog::AuditLog(const Poco::JSON::Object& json)
{
    tableName = JsonUtils::getString(json, "table_name", "");
    recordId = static_cast<Poco::Int64>(JsonUtils::getInt(json, "record_id", 0));

    std::string actionStr = JsonUtils::getString(json, "action", "INSERT");
    action = stringToAuditAction(actionStr);

    changedBy = static_cast<Poco::Int64>(JsonUtils::getInt(json, "changed_by", 0));

    changedByName = JsonUtils::getString(json, "changed_by_name", "");

    if (json.has("old_values") && !json.isNull("old_values"))
        oldValues = JsonUtils::getString(json, "old_values", "");

    if (json.has("new_values") && !json.isNull("new_values"))
        newValues = JsonUtils::getString(json, "new_values", "");

    if (json.has("changed_at") && !json.isNull("changed_at"))
        changedAt = JsonUtils::getString(json, "changed_at", "");

    if (json.has("ip_address") && !json.isNull("ip_address"))
        ipAddress = JsonUtils::getString(json, "ip_address", "");

    if (json.has("user_agent") && !json.isNull("user_agent"))
        userAgent = JsonUtils::getString(json, "user_agent", "");

    if (json.has("description") && !json.isNull("description"))
        description = JsonUtils::getString(json, "description", "");

    if (json.has("id") && !json.isNull("id"))
        id = static_cast<Poco::Int64>(JsonUtils::getInt(json, "id", 0));
    else
        id = 0;
}

Poco::JSON::Object AuditLog::toJson() const
{
    Poco::JSON::Object json;
    
    if (id > 0)
    {
        json.set("id", id);
    }
    
    json.set("table_name", tableName);
    json.set("record_id", recordId);
    json.set("action", auditActionToString(action));
    
    if (!oldValues.isNull())
    {
        json.set("old_values", oldValues.value());
    }
    
    if (!newValues.isNull())
    {
        json.set("new_values", newValues.value());
    }
    
    json.set("changed_by", changedBy);
    json.set("changed_at", changedAt);
    
    if (!ipAddress.isNull())
    {
        json.set("ip_address", ipAddress.value());
    }
    
    if (!userAgent.isNull())
    {
        json.set("user_agent", userAgent.value());
    }
    
    if (!description.isNull())
    {
        json.set("description", description.value());
    }
    
    if (!changedByName.empty())
    {
        json.set("changed_by_name", changedByName);
    }
    
    json.set("has_changes", hasChanges());
    
    auto changedFields = getChangedFields();
    if (!changedFields.empty())
    {
        Poco::JSON::Array fieldsArray;
        for (const auto& field : changedFields)
        {
            fieldsArray.add(field);
        }
        json.set("changed_fields", fieldsArray);
    }
    
    return json;
}

AuditLog AuditLog::fromJson(const Poco::JSON::Object& json)
{
    return AuditLog(json);
}

bool AuditLog::validate() const
{
    if (tableName.empty() || tableName.length() > MAX_TABLE_NAME)
    {
        return false;
    }
    
    if (recordId <= 0)
    {
        return false;
    }
    
    if (changedBy <= 0)
    {
        return false;
    }
    
    if (!changedAt.isNull() && !Validator::isValidDateTime(changedAt.value()))
    {
        return false;
    }
    
    if (!ipAddress.isNull() && ipAddress.value().length() > MAX_IP_ADDRESS)
    {
        return false;
    }
    
    return true;
}

Poco::JSON::Object::Ptr AuditLog::getOldValuesJson() const
{
    try
    {
        if (oldValues.isNull())
        {
            return nullptr;
        }
        
        Poco::JSON::Parser parser;
        return parser.parse(oldValues).extract<Poco::JSON::Object::Ptr>();
    }
    catch (...)
    {
        return nullptr;
    }
}

Poco::JSON::Object::Ptr AuditLog::getNewValuesJson() const
{
    try
    {
        if (newValues.isNull())
        {
            return nullptr;
        }
        
        Poco::JSON::Parser parser;
        return parser.parse(newValues).extract<Poco::JSON::Object::Ptr>();
    }
    catch (...)
    {
        return nullptr;
    }
}

bool AuditLog::hasChanges() const
{
    return !oldValues.isNull() || !newValues.isNull();
}

std::vector<std::string> AuditLog::getChangedFields() const
{
    std::vector<std::string> fields;
    std::set<std::string> fieldSet;
    
    try
    {
        auto oldJson = getOldValuesJson();
        auto newJson = getNewValuesJson();
        
        if (oldJson)
        {
            for (const auto& name : oldJson->getNames())
            {
                fieldSet.insert(name);
            }
        }
        
        if (newJson)
        {
            for (const auto& name : newJson->getNames())
            {
                fieldSet.insert(name);
            }
        }
        
        fields.assign(fieldSet.begin(), fieldSet.end());
    }
    catch (...)
    {

    }
    
    return fields;
}

std::string AuditLog::auditActionToString(AuditAction action)
{
    switch (action)
    {
        case AuditAction::INSERT: return "INSERT";
        case AuditAction::UPDATE: return "UPDATE";
        case AuditAction::DELETE: return "DELETE";
        case AuditAction::SOFT_DELETE: return "SOFT_DELETE";
        default: return "INSERT";
    }
}

AuditAction AuditLog::stringToAuditAction(const std::string& actionStr)
{
    if (actionStr == "UPDATE") return AuditAction::UPDATE;
    if (actionStr == "DELETE") return AuditAction::DELETE;
    if (actionStr == "SOFT_DELETE") return AuditAction::SOFT_DELETE;
    return AuditAction::INSERT;
}

} // namespace database::models
