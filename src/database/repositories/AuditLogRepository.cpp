#include "AuditLogRepository.hpp"
#include "../models/AuditLog.hpp"
#include "../models/User.hpp"
#include <Poco/Data/Session.h>
#include <Poco/Data/RecordSet.h>
#include <Poco/Data/Statement.h>
#include <Poco/Data/Row.h>
#include <Poco/DateTime.h>
#include <Poco/DateTimeFormat.h>
#include <Poco/DateTimeFormatter.h>
#include <Poco/DateTimeParser.h>
#include <Poco/Dynamic/Var.h>
#include <Poco/JSON/Parser.h>
#include "../../utils/DateUtils.hpp"
#include "../../utils/JsonUtils.hpp"

using namespace Poco::Data;
using namespace Poco::Data::Keywords;
using namespace Poco;

using namespace warehouse_backend::utils;

namespace warehouse_backend::database::repositories
{

const std::string AuditLogRepository::TABLE_NAME = "audit_log";
const std::vector<std::string> AuditLogRepository::SEARCH_FIELDS = {
    "table_name", "description", "ip_address", "user_agent"
};

AuditLogRepository::AuditLogRepository() : BaseRepository<models::AuditLog>()
{
}

std::unique_ptr<models::AuditLog> AuditLogRepository::findById(long long id)
{
    auto connection = acquireConnection();
    
    try
    {
        Poco::Int64 pocoId = static_cast<Poco::Int64>(id);
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT al.id, al.table_name, al.record_id, al.action, "
                  "al.old_values::text, al.new_values::text, al.changed_by, "
                  "al.changed_at, al.ip_address, al.user_agent, al.description, "
                  "u.full_name as changed_by_name "
                  "FROM " << TABLE_NAME << " al "
                  "LEFT JOIN users u ON u.id = al.changed_by "
                  "WHERE al.id = $1",
            Poco::Data::Keywords::use(pocoId),
            now;
        
        Poco::Data::RecordSet rs(select);
        
        if (rs.rowCount() > 0)
        {
            auto auditLog = std::make_unique<models::AuditLog>();
            auditLog->id = rs.value("id", 0).convert<long long>();
            auditLog->tableName = rs.value("table_name").convert<std::string>();
            auditLog->recordId = rs.value("record_id", 0).convert<long long>();
            
            std::string actionStr = rs.value("action").convert<std::string>();
            auditLog->action = models::AuditLog::stringToAuditAction(actionStr);
            
            auditLog->oldValues = rs.value("old_values").isEmpty() ? "" : rs.value("old_values").convert<std::string>();
            auditLog->newValues = rs.value("new_values").isEmpty() ? "" : rs.value("new_values").convert<std::string>();
            auditLog->changedBy = rs.value("changed_by", 0).convert<long long>();
            auditLog->changedAt = rs.value("changed_at").convert<std::string>();
            auditLog->ipAddress = rs.value("ip_address").isEmpty() ? "" : rs.value("ip_address").convert<std::string>();
            auditLog->userAgent = rs.value("user_agent").isEmpty() ? "" : rs.value("user_agent").convert<std::string>();
            auditLog->description = rs.value("description").isEmpty() ? "" : rs.value("description").convert<std::string>();
            auditLog->changedByName = rs.value("changed_by_name").isEmpty() ? "" : rs.value("changed_by_name").convert<std::string>();
            
            return auditLog;
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findById: " + e.displayText());
    }
    
    return nullptr;
}

std::vector<std::unique_ptr<models::AuditLog>> AuditLogRepository::findAll()
{
    std::vector<std::unique_ptr<models::AuditLog>> logs;
    auto connection = acquireConnection();
    
    try
    {
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT al.id, al.table_name, al.record_id, al.action, "
                  "al.old_values::text, al.new_values::text, al.changed_by, "
                  "al.changed_at, al.ip_address, al.user_agent, al.description, "
                  "u.full_name as changed_by_name "
                  "FROM " << TABLE_NAME << " al "
                  "LEFT JOIN users u ON u.id = al.changed_by "
                  "ORDER BY al.changed_at DESC, al.id DESC",
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            auto auditLog = std::make_unique<models::AuditLog>();
            auditLog->id = rs.value("id", 0).convert<long long>();
            auditLog->tableName = rs.value("table_name").convert<std::string>();
            auditLog->recordId = rs.value("record_id", 0).convert<long long>();
            
            std::string actionStr = rs.value("action").convert<std::string>();
            auditLog->action = models::AuditLog::stringToAuditAction(actionStr);
            
            auditLog->oldValues = rs.value("old_values").isEmpty() ? "" : rs.value("old_values").convert<std::string>();
            auditLog->newValues = rs.value("new_values").isEmpty() ? "" : rs.value("new_values").convert<std::string>();
            auditLog->changedBy = rs.value("changed_by", 0).convert<long long>();
            auditLog->changedAt = rs.value("changed_at").convert<std::string>();
            auditLog->ipAddress = rs.value("ip_address").isEmpty() ? "" : rs.value("ip_address").convert<std::string>();
            auditLog->userAgent = rs.value("user_agent").isEmpty() ? "" : rs.value("user_agent").convert<std::string>();
            auditLog->description = rs.value("description").isEmpty() ? "" : rs.value("description").convert<std::string>();
            auditLog->changedByName = rs.value("changed_by_name").isEmpty() ? "" : rs.value("changed_by_name").convert<std::string>();
            
            logs.push_back(std::move(auditLog));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findAll: " + e.displayText());
    }
    
    return logs;
}

std::vector<std::unique_ptr<models::AuditLog>> AuditLogRepository::findPaginated(int page, int pageSize)
{
    std::vector<std::unique_ptr<models::AuditLog>> logs;
    auto connection = acquireConnection();
    
    if (page < 1) page = 1;
    int offset = (page - 1) * pageSize;
    
    try
    {
        int usePageSize = pageSize;
        int useOffset = offset;
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT al.id, al.table_name, al.record_id, al.action, "
                  "al.old_values::text, al.new_values::text, al.changed_by, "
                  "al.changed_at, al.ip_address, al.user_agent, al.description, "
                  "u.full_name as changed_by_name "
                  "FROM " << TABLE_NAME << " al "
                  "LEFT JOIN users u ON u.id = al.changed_by "
                  "ORDER BY al.changed_at DESC, al.id DESC LIMIT $1 OFFSET $2",
            Poco::Data::Keywords::use(usePageSize),
            Poco::Data::Keywords::use(useOffset),
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            auto auditLog = std::make_unique<models::AuditLog>();
            auditLog->id = rs.value("id", 0).convert<long long>();
            auditLog->tableName = rs.value("table_name").convert<std::string>();
            auditLog->recordId = rs.value("record_id", 0).convert<long long>();
            
            std::string actionStr = rs.value("action").convert<std::string>();
            auditLog->action = models::AuditLog::stringToAuditAction(actionStr);
            
            auditLog->oldValues = rs.value("old_values").isEmpty() ? "" : rs.value("old_values").convert<std::string>();
            auditLog->newValues = rs.value("new_values").isEmpty() ? "" : rs.value("new_values").convert<std::string>();
            auditLog->changedBy = rs.value("changed_by", 0).convert<long long>();
            auditLog->changedAt = rs.value("changed_at").convert<std::string>();
            auditLog->ipAddress = rs.value("ip_address").isEmpty() ? "" : rs.value("ip_address").convert<std::string>();
            auditLog->userAgent = rs.value("user_agent").isEmpty() ? "" : rs.value("user_agent").convert<std::string>();
            auditLog->description = rs.value("description").isEmpty() ? "" : rs.value("description").convert<std::string>();
            auditLog->changedByName = rs.value("changed_by_name").isEmpty() ? "" : rs.value("changed_by_name").convert<std::string>();
            
            logs.push_back(std::move(auditLog));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findPaginated: " + e.displayText());
    }
    
    return logs;
}

long long AuditLogRepository::create(const models::AuditLog& auditLog)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        std::string actionStr = models::AuditLog::auditActionToString(auditLog.action);
        
        models::AuditLog auditLogCopy = auditLog;
        
        Poco::Data::Statement insert(connection->getSession());
        Poco::Int64 newId = 0;
        
        insert << "INSERT INTO " << TABLE_NAME << " "
                  "(table_name, record_id, action, old_values, new_values, "
                  "changed_by, changed_at, ip_address, user_agent, description) "
                  "VALUES ($1, $2, $3::audit_action, $4::jsonb, $5::jsonb, "
                  "$6, $7, $8, $9, $10) "
                  "RETURNING id",
            Poco::Data::Keywords::use(auditLogCopy.tableName),
            Poco::Data::Keywords::use(auditLogCopy.recordId),
            Poco::Data::Keywords::use(actionStr),
            Poco::Data::Keywords::use(auditLogCopy.oldValues),
            Poco::Data::Keywords::use(auditLogCopy.newValues),
            Poco::Data::Keywords::use(auditLogCopy.changedBy),
            Poco::Data::Keywords::use(auditLogCopy.changedAt),
            Poco::Data::Keywords::use(auditLogCopy.ipAddress),
            Poco::Data::Keywords::use(auditLogCopy.userAgent),
            Poco::Data::Keywords::use(auditLogCopy.description),
            Poco::Data::Keywords::into(newId),
            now;
        
        commitTransaction(*connection);
        return static_cast<long long>(newId);
    }
    catch (const Poco::Exception& e)
    {
        rollbackTransaction(*connection);
        throw std::runtime_error("Database error in create: " + e.displayText());
    }
}

bool AuditLogRepository::update(long long id, const models::AuditLog& auditLog)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        std::string actionStr = models::AuditLog::auditActionToString(auditLog.action);
        
        models::AuditLog auditLogCopy = auditLog;
        
        Poco::Int64 idCopy = id;
        
        Poco::Data::Statement update(connection->getSession());
        update << "UPDATE " << TABLE_NAME << " SET "
                  "table_name = $1, record_id = $2, action = $3::audit_action, "
                  "old_values = $4::jsonb, new_values = $5::jsonb, "
                  "changed_by = $6, changed_at = $7, "
                  "ip_address = $8, user_agent = $9, description = $10 "
                  "WHERE id = $11",
            Poco::Data::Keywords::use(auditLogCopy.tableName),
            Poco::Data::Keywords::use(auditLogCopy.recordId),
            Poco::Data::Keywords::use(actionStr),
            Poco::Data::Keywords::use(auditLogCopy.oldValues),
            Poco::Data::Keywords::use(auditLogCopy.newValues),
            Poco::Data::Keywords::use(auditLogCopy.changedBy),
            Poco::Data::Keywords::use(auditLogCopy.changedAt),
            Poco::Data::Keywords::use(auditLogCopy.ipAddress),
            Poco::Data::Keywords::use(auditLogCopy.userAgent),
            Poco::Data::Keywords::use(auditLogCopy.description),
            Poco::Data::Keywords::use(idCopy);
        
        int rowsAffected = update.execute();
        
        commitTransaction(*connection);
        return rowsAffected > 0;
    }
    catch (const Poco::Exception& e)
    {
        rollbackTransaction(*connection);
        throw std::runtime_error("Database error in update: " + e.displayText());
    }
}

bool AuditLogRepository::remove(long long id)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        long long idCopy = id;
        
        Poco::Data::Statement del(connection->getSession());
        del << "DELETE FROM " << TABLE_NAME << " WHERE id = $1",
            Poco::Data::Keywords::use(idCopy);
        
        int rowsAffected = del.execute();
        
        commitTransaction(*connection);
        return rowsAffected > 0;
    }
    catch (const Poco::Exception& e)
    {
        rollbackTransaction(*connection);
        throw std::runtime_error("Database error in remove: " + e.displayText());
    }
}

bool AuditLogRepository::softDelete(long long id)
{
    return remove(id);
}

int AuditLogRepository::count()
{
    auto connection = acquireConnection();
    
    try
    {
        Poco::Data::Statement countStmt(connection->getSession());
        countStmt << "SELECT COUNT(*) FROM " << TABLE_NAME,
            now;
        
        Poco::Data::RecordSet rs(countStmt);
        if (rs.rowCount() > 0)
        {
            return rs.value(0, 0).convert<int>();
        }
        
        return 0;
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in count: " + e.displayText());
    }
}

Poco::JSON::Array AuditLogRepository::findAllAsJson()
{
    auto logs = findAll();
    Poco::JSON::Array jsonArray;
    
    for (const auto& log : logs)
    {
        if (log)
        {
            jsonArray.add(log->toJson());
        }
    }
    
    return jsonArray;
}

Poco::JSON::Object AuditLogRepository::findByIdAsJson(long long id)
{
    auto log = findById(id);
    if (log)
    {
        return log->toJson();
    }
    
    return Poco::JSON::Object();
}

std::vector<std::unique_ptr<models::AuditLog>> AuditLogRepository::findByField(
    const std::string& fieldName, const std::string& fieldValue)
{
    std::vector<std::unique_ptr<models::AuditLog>> logs;
    auto connection = acquireConnection();
    
    try
    {
        std::string sql = "SELECT al.id, al.table_name, al.record_id, al.action, "
                          "al.old_values::text, al.new_values::text, al.changed_by, "
                          "al.changed_at, al.ip_address, al.user_agent, al.description, "
                          "u.full_name as changed_by_name "
                          "FROM " + TABLE_NAME + " al "
                          "LEFT JOIN users u ON u.id = al.changed_by "
                          "WHERE al." + fieldName + " = $1 "
                          "ORDER BY al.changed_at DESC, al.id DESC";
        
        std::string fieldValueCopy = fieldValue;
        Poco::Data::Statement select(connection->getSession());
        select << sql,
            Poco::Data::Keywords::use(fieldValueCopy),
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            auto auditLog = std::make_unique<models::AuditLog>();
            auditLog->id = rs.value("id", 0).convert<long long>();
            auditLog->tableName = rs.value("table_name").convert<std::string>();
            auditLog->recordId = rs.value("record_id", 0).convert<long long>();
            
            std::string actionStr = rs.value("action").convert<std::string>();
            auditLog->action = models::AuditLog::stringToAuditAction(actionStr);
            
            auditLog->oldValues = rs.value("old_values").isEmpty() ? "" : rs.value("old_values").convert<std::string>();
            auditLog->newValues = rs.value("new_values").isEmpty() ? "" : rs.value("new_values").convert<std::string>();
            auditLog->changedBy = rs.value("changed_by", 0).convert<long long>();
            auditLog->changedAt = rs.value("changed_at").convert<std::string>();
            auditLog->ipAddress = rs.value("ip_address").isEmpty() ? "" : rs.value("ip_address").convert<std::string>();
            auditLog->userAgent = rs.value("user_agent").isEmpty() ? "" : rs.value("user_agent").convert<std::string>();
            auditLog->description = rs.value("description").isEmpty() ? "" : rs.value("description").convert<std::string>();
            auditLog->changedByName = rs.value("changed_by_name").isEmpty() ? "" : rs.value("changed_by_name").convert<std::string>();
            
            logs.push_back(std::move(auditLog));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findByField: " + e.displayText());
    }
    
    return logs;
}

std::vector<std::unique_ptr<models::AuditLog>> AuditLogRepository::search(
    const std::string& query, const std::vector<std::string>& fields)
{
    std::vector<std::unique_ptr<models::AuditLog>> logs;
    auto connection = acquireConnection();
    
    try
    {
        std::string searchCondition = buildSearchQuery(query, SEARCH_FIELDS);
        
        std::string sql = "SELECT al.id, al.table_name, al.record_id, al.action, "
                          "al.old_values::text, al.new_values::text, al.changed_by, "
                          "al.changed_at, al.ip_address, al.user_agent, al.description, "
                          "u.full_name as changed_by_name "
                          "FROM " + TABLE_NAME + " al "
                          "LEFT JOIN users u ON u.id = al.changed_by "
                          "WHERE " + searchCondition + " "
                          "ORDER BY al.changed_at DESC, al.id DESC";
        
        Poco::Data::Statement select(connection->getSession());
        select << sql,
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            auto auditLog = std::make_unique<models::AuditLog>();
            auditLog->id = rs.value("id", 0).convert<long long>();
            auditLog->tableName = rs.value("table_name").convert<std::string>();
            auditLog->recordId = rs.value("record_id", 0).convert<long long>();
            
            std::string actionStr = rs.value("action").convert<std::string>();
            auditLog->action = models::AuditLog::stringToAuditAction(actionStr);
            
            auditLog->oldValues = rs.value("old_values").isEmpty() ? "" : rs.value("old_values").convert<std::string>();
            auditLog->newValues = rs.value("new_values").isEmpty() ? "" : rs.value("new_values").convert<std::string>();
            auditLog->changedBy = rs.value("changed_by", 0).convert<long long>();
            auditLog->changedAt = rs.value("changed_at").convert<std::string>();
            auditLog->ipAddress = rs.value("ip_address").isEmpty() ? "" : rs.value("ip_address").convert<std::string>();
            auditLog->userAgent = rs.value("user_agent").isEmpty() ? "" : rs.value("user_agent").convert<std::string>();
            auditLog->description = rs.value("description").isEmpty() ? "" : rs.value("description").convert<std::string>();
            auditLog->changedByName = rs.value("changed_by_name").isEmpty() ? "" : rs.value("changed_by_name").convert<std::string>();
            
            logs.push_back(std::move(auditLog));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in search: " + e.displayText());
    }
    
    return logs;
}

std::vector<std::unique_ptr<models::AuditLog>> AuditLogRepository::findByTableName(const std::string& tableName)
{
    return findByField("table_name", tableName);
}

std::vector<std::unique_ptr<models::AuditLog>> AuditLogRepository::findByRecordId(long long recordId)
{
    return findByField("record_id", std::to_string(recordId));
}

std::vector<std::unique_ptr<models::AuditLog>> AuditLogRepository::findByAction(const std::string& action)
{
    return findByField("action", action);
}

std::vector<std::unique_ptr<models::AuditLog>> AuditLogRepository::findByChangedBy(long long userId)
{
    return findByField("changed_by", std::to_string(userId));
}

std::vector<std::unique_ptr<models::AuditLog>> AuditLogRepository::findByDateRange(const std::string& startDate, const std::string& endDate)
{
    std::vector<std::unique_ptr<models::AuditLog>> logs;
    auto connection = acquireConnection();
    
    try
    {
        std::string startDateCopy = startDate;
        std::string endDateCopy = endDate;
        
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT al.id, al.table_name, al.record_id, al.action, "
                  "al.old_values::text, al.new_values::text, al.changed_by, "
                  "al.changed_at, al.ip_address, al.user_agent, al.description, "
                  "u.full_name as changed_by_name "
                  "FROM " << TABLE_NAME << " al "
                  "LEFT JOIN users u ON u.id = al.changed_by "
                  "WHERE al.changed_at >= $1 AND al.changed_at <= $2 "
                  "ORDER BY al.changed_at DESC, al.id DESC",
            Poco::Data::Keywords::use(startDateCopy),
            Poco::Data::Keywords::use(endDateCopy),
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            auto auditLog = std::make_unique<models::AuditLog>();
            auditLog->id = rs.value("id", 0).convert<long long>();
            auditLog->tableName = rs.value("table_name").convert<std::string>();
            auditLog->recordId = rs.value("record_id", 0).convert<long long>();
            
            std::string actionStr = rs.value("action").convert<std::string>();
            auditLog->action = models::AuditLog::stringToAuditAction(actionStr);
            
            auditLog->oldValues = rs.value("old_values").isEmpty() ? "" : rs.value("old_values").convert<std::string>();
            auditLog->newValues = rs.value("new_values").isEmpty() ? "" : rs.value("new_values").convert<std::string>();
            auditLog->changedBy = rs.value("changed_by", 0).convert<long long>();
            auditLog->changedAt = rs.value("changed_at").convert<std::string>();
            auditLog->ipAddress = rs.value("ip_address").isEmpty() ? "" : rs.value("ip_address").convert<std::string>();
            auditLog->userAgent = rs.value("user_agent").isEmpty() ? "" : rs.value("user_agent").convert<std::string>();
            auditLog->description = rs.value("description").isEmpty() ? "" : rs.value("description").convert<std::string>();
            auditLog->changedByName = rs.value("changed_by_name").isEmpty() ? "" : rs.value("changed_by_name").convert<std::string>();
            
            logs.push_back(std::move(auditLog));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findByDateRange: " + e.displayText());
    }
    
    return logs;
}

std::vector<std::unique_ptr<models::AuditLog>> AuditLogRepository::findByTableAndRecord(const std::string& tableName, long long recordId)
{
    std::vector<std::unique_ptr<models::AuditLog>> logs;
    auto connection = acquireConnection();
    
    try
    {
        std::string tableNameCopy = tableName;
        Poco::Int64 recordIdCopy = recordId;
        
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT al.id, al.table_name, al.record_id, al.action, "
                  "al.old_values::text, al.new_values::text, al.changed_by, "
                  "al.changed_at, al.ip_address, al.user_agent, al.description, "
                  "u.full_name as changed_by_name "
                  "FROM " << TABLE_NAME << " al "
                  "LEFT JOIN users u ON u.id = al.changed_by "
                  "WHERE al.table_name = $1 AND al.record_id = $2 "
                  "ORDER BY al.changed_at DESC, al.id DESC",
            Poco::Data::Keywords::use(tableNameCopy),
            Poco::Data::Keywords::use(recordIdCopy),
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            auto auditLog = std::make_unique<models::AuditLog>();
            auditLog->id = rs.value("id", 0).convert<long long>();
            auditLog->tableName = rs.value("table_name").convert<std::string>();
            auditLog->recordId = rs.value("record_id", 0).convert<long long>();
            
            std::string actionStr = rs.value("action").convert<std::string>();
            auditLog->action = models::AuditLog::stringToAuditAction(actionStr);
            
            auditLog->oldValues = rs.value("old_values").isEmpty() ? "" : rs.value("old_values").convert<std::string>();
            auditLog->newValues = rs.value("new_values").isEmpty() ? "" : rs.value("new_values").convert<std::string>();
            auditLog->changedBy = rs.value("changed_by", 0).convert<long long>();
            auditLog->changedAt = rs.value("changed_at").convert<std::string>();
            auditLog->ipAddress = rs.value("ip_address").isEmpty() ? "" : rs.value("ip_address").convert<std::string>();
            auditLog->userAgent = rs.value("user_agent").isEmpty() ? "" : rs.value("user_agent").convert<std::string>();
            auditLog->description = rs.value("description").isEmpty() ? "" : rs.value("description").convert<std::string>();
            auditLog->changedByName = rs.value("changed_by_name").isEmpty() ? "" : rs.value("changed_by_name").convert<std::string>();
            
            logs.push_back(std::move(auditLog));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findByTableAndRecord: " + e.displayText());
    }
    
    return logs;
}

std::vector<std::unique_ptr<models::AuditLog>> AuditLogRepository::findRecentLogs(int limit)
{
    std::vector<std::unique_ptr<models::AuditLog>> logs;
    auto connection = acquireConnection();
    
    try
    {
        int useLimit = limit;
        
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT al.id, al.table_name, al.record_id, al.action, "
                  "al.old_values::text, al.new_values::text, al.changed_by, "
                  "al.changed_at, al.ip_address, al.user_agent, al.description, "
                  "u.full_name as changed_by_name "
                  "FROM " << TABLE_NAME << " al "
                  "LEFT JOIN users u ON u.id = al.changed_by "
                  "ORDER BY al.changed_at DESC, al.id DESC LIMIT $1",
            Poco::Data::Keywords::use(useLimit),
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            auto auditLog = std::make_unique<models::AuditLog>();
            auditLog->id = rs.value("id", 0).convert<long long>();
            auditLog->tableName = rs.value("table_name").convert<std::string>();
            auditLog->recordId = rs.value("record_id", 0).convert<long long>();
            
            std::string actionStr = rs.value("action").convert<std::string>();
            auditLog->action = models::AuditLog::stringToAuditAction(actionStr);
            
            auditLog->oldValues = rs.value("old_values").isEmpty() ? "" : rs.value("old_values").convert<std::string>();
            auditLog->newValues = rs.value("new_values").isEmpty() ? "" : rs.value("new_values").convert<std::string>();
            auditLog->changedBy = rs.value("changed_by", 0).convert<long long>();
            auditLog->changedAt = rs.value("changed_at").convert<std::string>();
            auditLog->ipAddress = rs.value("ip_address").isEmpty() ? "" : rs.value("ip_address").convert<std::string>();
            auditLog->userAgent = rs.value("user_agent").isEmpty() ? "" : rs.value("user_agent").convert<std::string>();
            auditLog->description = rs.value("description").isEmpty() ? "" : rs.value("description").convert<std::string>();
            auditLog->changedByName = rs.value("changed_by_name").isEmpty() ? "" : rs.value("changed_by_name").convert<std::string>();
            
            logs.push_back(std::move(auditLog));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findRecentLogs: " + e.displayText());
    }
    
    return logs;
}

std::vector<std::unique_ptr<models::AuditLog>> AuditLogRepository::findLogsByUserAction(long long userId, const std::string& action)
{
    std::vector<std::unique_ptr<models::AuditLog>> logs;
    auto connection = acquireConnection();
    
    try
    {
        Poco::Int64 userIdCopy = userId;
        std::string actionCopy = action;
        
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT al.id, al.table_name, al.record_id, al.action, "
                  "al.old_values::text, al.new_values::text, al.changed_by, "
                  "al.changed_at, al.ip_address, al.user_agent, al.description, "
                  "u.full_name as changed_by_name "
                  "FROM " << TABLE_NAME << " al "
                  "LEFT JOIN users u ON u.id = al.changed_by "
                  "WHERE al.changed_by = $1 AND al.action = $2::audit_action "
                  "ORDER BY al.changed_at DESC, al.id DESC",
            Poco::Data::Keywords::use(userIdCopy),
            Poco::Data::Keywords::use(actionCopy),
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            auto auditLog = std::make_unique<models::AuditLog>();
            auditLog->id = rs.value("id", 0).convert<long long>();
            auditLog->tableName = rs.value("table_name").convert<std::string>();
            auditLog->recordId = rs.value("record_id", 0).convert<long long>();
            
            auditLog->action = models::AuditLog::stringToAuditAction(action);
            auditLog->oldValues = rs.value("old_values").isEmpty() ? "" : rs.value("old_values").convert<std::string>();
            auditLog->newValues = rs.value("new_values").isEmpty() ? "" : rs.value("new_values").convert<std::string>();
            auditLog->changedBy = userId;
            auditLog->changedAt = rs.value("changed_at").convert<std::string>();
            auditLog->ipAddress = rs.value("ip_address").isEmpty() ? "" : rs.value("ip_address").convert<std::string>();
            auditLog->userAgent = rs.value("user_agent").isEmpty() ? "" : rs.value("user_agent").convert<std::string>();
            auditLog->description = rs.value("description").isEmpty() ? "" : rs.value("description").convert<std::string>();
            auditLog->changedByName = rs.value("changed_by_name").isEmpty() ? "" : rs.value("changed_by_name").convert<std::string>();
            
            logs.push_back(std::move(auditLog));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findLogsByUserAction: " + e.displayText());
    }
    
    return logs;
}

bool AuditLogRepository::deleteOldLogs(int daysOld)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        std::string cutoffDate = DateUtils::formatDate(DateUtils::addDays(DateUtils::now(), -daysOld));
        
        Poco::Data::Statement del(connection->getSession());
        del << "DELETE FROM " << TABLE_NAME << " WHERE changed_at < $1",
            Poco::Data::Keywords::use(cutoffDate);
        
        int rowsAffected = del.execute();
        
        commitTransaction(*connection);
        return rowsAffected > 0;
    }
    catch (const Poco::Exception& e)
    {
        rollbackTransaction(*connection);
        throw std::runtime_error("Database error in deleteOldLogs: " + e.displayText());
    }
}

bool AuditLogRepository::deleteLogsByTable(const std::string& tableName)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        std::string tableNameCopy = tableName;
        
        Poco::Data::Statement del(connection->getSession());
        del << "DELETE FROM " << TABLE_NAME << " WHERE table_name = $1",
            Poco::Data::Keywords::use(tableNameCopy);
        
        int rowsAffected = del.execute();
        
        commitTransaction(*connection);
        return rowsAffected > 0;
    }
    catch (const Poco::Exception& e)
    {
        rollbackTransaction(*connection);
        throw std::runtime_error("Database error in deleteLogsByTable: " + e.displayText());
    }
}

int AuditLogRepository::countByTable(const std::string& tableName)
{
    auto connection = acquireConnection();
    
    try
    {
        std::string tableNameCopy = tableName;
        
        Poco::Data::Statement countStmt(connection->getSession());
        countStmt << "SELECT COUNT(*) FROM " << TABLE_NAME << " WHERE table_name = $1",
            Poco::Data::Keywords::use(tableNameCopy),
            now;
        
        Poco::Data::RecordSet rs(countStmt);
        if (rs.rowCount() > 0)
        {
            return rs.value(0, 0).convert<int>();
        }
        
        return 0;
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in countByTable: " + e.displayText());
    }
}

int AuditLogRepository::countByAction(const std::string& action)
{
    auto connection = acquireConnection();
    
    try
    {
        std::string actionCopy = action;
        
        Poco::Data::Statement countStmt(connection->getSession());
        countStmt << "SELECT COUNT(*) FROM " << TABLE_NAME << " WHERE action = $1::audit_action",
            Poco::Data::Keywords::use(actionCopy),
            now;
        
        Poco::Data::RecordSet rs(countStmt);
        if (rs.rowCount() > 0)
        {
            return rs.value(0, 0).convert<int>();
        }
        
        return 0;
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in countByAction: " + e.displayText());
    }
}

int AuditLogRepository::countByUser(long long userId)
{
    auto connection = acquireConnection();
    
    try
    {
        Poco::Int64 userIdCopy = userId;
        
        Poco::Data::Statement countStmt(connection->getSession());
        countStmt << "SELECT COUNT(*) FROM " << TABLE_NAME << " WHERE changed_by = $1",
            Poco::Data::Keywords::use(userIdCopy),
            now;
        
        Poco::Data::RecordSet rs(countStmt);
        if (rs.rowCount() > 0)
        {
            return rs.value(0, 0).convert<int>();
        }
        
        return 0;
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in countByUser: " + e.displayText());
    }
}

Poco::JSON::Array AuditLogRepository::getAuditStatistics()
{
    auto connection = acquireConnection();
    Poco::JSON::Array jsonArray;
    
    try
    {
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT "
                  "table_name, "
                  "action, "
                  "COUNT(*) as count, "
                  "MIN(changed_at) as first_activity, "
                  "MAX(changed_at) as last_activity "
                  "FROM " << TABLE_NAME << " "
                  "GROUP BY table_name, action "
                  "ORDER BY table_name, action",
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            Poco::JSON::Object stats;
            stats.set("table_name", rs.value("table_name").convert<std::string>());
            stats.set("action", rs.value("action").convert<std::string>());
            stats.set("count", rs.value("count", 0).convert<int>());
            stats.set("first_activity", rs.value("first_activity").convert<std::string>());
            stats.set("last_activity", rs.value("last_activity").convert<std::string>());
            
            jsonArray.add(stats);
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in getAuditStatistics: " + e.displayText());
    }
    
    return jsonArray;
}

Poco::JSON::Array AuditLogRepository::getActivityReport(const std::string& startDate, const std::string& endDate)
{
    auto connection = acquireConnection();
    Poco::JSON::Array jsonArray;
    
    try
    {
        std::string startDateCopy = startDate;
        std::string endDateCopy = endDate;
        
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT "
                  "DATE(changed_at) as activity_date, "
                  "COUNT(*) as total_activities, "
                  "SUM(CASE WHEN action = 'INSERT' THEN 1 ELSE 0 END) as inserts, "
                  "SUM(CASE WHEN action = 'UPDATE' THEN 1 ELSE 0 END) as updates, "
                  "SUM(CASE WHEN action = 'DELETE' THEN 1 ELSE 0 END) as deletes, "
                  "SUM(CASE WHEN action = 'SOFT_DELETE' THEN 1 ELSE 0 END) as soft_deletes, "
                  "COUNT(DISTINCT changed_by) as unique_users, "
                  "COUNT(DISTINCT table_name) as unique_tables "
                  "FROM " << TABLE_NAME << " "
                  "WHERE changed_at >= $1 AND changed_at <= $2 "
                  "GROUP BY DATE(changed_at) "
                  "ORDER BY activity_date DESC",
            Poco::Data::Keywords::use(startDateCopy),
            Poco::Data::Keywords::use(endDateCopy),
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            Poco::JSON::Object report;
            report.set("activity_date", rs.value("activity_date").convert<std::string>());
            report.set("total_activities", rs.value("total_activities", 0).convert<int>());
            report.set("inserts", rs.value("inserts", 0).convert<int>());
            report.set("updates", rs.value("updates", 0).convert<int>());
            report.set("deletes", rs.value("deletes", 0).convert<int>());
            report.set("soft_deletes", rs.value("soft_deletes", 0).convert<int>());
            report.set("unique_users", rs.value("unique_users", 0).convert<int>());
            report.set("unique_tables", rs.value("unique_tables", 0).convert<int>());
            
            jsonArray.add(report);
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in getActivityReport: " + e.displayText());
    }
    
    return jsonArray;
}

Poco::JSON::Array AuditLogRepository::getUserActivityReport(long long userId, const std::string& startDate, const std::string& endDate)
{
    auto connection = acquireConnection();
    Poco::JSON::Array jsonArray;
    
    try
    {
        Poco::Int64 userIdCopy = userId;
        std::string startDateCopy = startDate;
        std::string endDateCopy = endDate;
        
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT "
                  "DATE(changed_at) as activity_date, "
                  "table_name, "
                  "action, "
                  "COUNT(*) as count, "
                  "MIN(changed_at) as first_activity, "
                  "MAX(changed_at) as last_activity "
                  "FROM " << TABLE_NAME << " "
                  "WHERE changed_by = $1 "
                  "AND changed_at >= $2 AND changed_at <= $3 "
                  "GROUP BY DATE(changed_at), table_name, action "
                  "ORDER BY activity_date DESC, table_name, action",
            Poco::Data::Keywords::use(userIdCopy),
            Poco::Data::Keywords::use(startDateCopy),
            Poco::Data::Keywords::use(endDateCopy),
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            Poco::JSON::Object report;
            report.set("activity_date", rs.value("activity_date").convert<std::string>());
            report.set("table_name", rs.value("table_name").convert<std::string>());
            report.set("action", rs.value("action").convert<std::string>());
            report.set("count", rs.value("count", 0).convert<int>());
            report.set("first_activity", rs.value("first_activity").convert<std::string>());
            report.set("last_activity", rs.value("last_activity").convert<std::string>());
            
            jsonArray.add(report);
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in getUserActivityReport: " + e.displayText());
    }
    
    return jsonArray;
}

Poco::JSON::Array AuditLogRepository::getTableActivityReport(const std::string& tableName, const std::string& startDate, const std::string& endDate)
{
    auto connection = acquireConnection();
    Poco::JSON::Array jsonArray;
    
    try
    {
        std::string tableNameCopy = tableName;
        std::string startDateCopy = startDate;
        std::string endDateCopy = endDate;
        
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT "
                  "DATE(changed_at) as activity_date, "
                  "action, "
                  "COUNT(*) as count, "
                  "COUNT(DISTINCT changed_by) as unique_users, "
                  "MIN(changed_at) as first_activity, "
                  "MAX(changed_at) as last_activity "
                  "FROM " << TABLE_NAME << " "
                  "WHERE table_name = $1 "
                  "AND changed_at >= $2 AND changed_at <= $3 "
                  "GROUP BY DATE(changed_at), action "
                  "ORDER BY activity_date DESC, action",
            Poco::Data::Keywords::use(tableNameCopy),
            Poco::Data::Keywords::use(startDateCopy),
            Poco::Data::Keywords::use(endDateCopy),
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            Poco::JSON::Object report;
            report.set("activity_date", rs.value("activity_date").convert<std::string>());
            report.set("action", rs.value("action").convert<std::string>());
            report.set("count", rs.value("count", 0).convert<int>());
            report.set("unique_users", rs.value("unique_users", 0).convert<int>());
            report.set("first_activity", rs.value("first_activity").convert<std::string>());
            report.set("last_activity", rs.value("last_activity").convert<std::string>());
            
            jsonArray.add(report);
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in getTableActivityReport: " + e.displayText());
    }
    
    return jsonArray;
}

std::vector<std::string> AuditLogRepository::getAuditedTables()
{
    std::vector<std::string> tables;
    auto connection = acquireConnection();
    
    try
    {
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT DISTINCT table_name FROM " << TABLE_NAME << " ORDER BY table_name",
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            tables.push_back(rs.value("table_name").convert<std::string>());
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in getAuditedTables: " + e.displayText());
    }
    
    return tables;
}

std::vector<std::pair<long long, std::string>> AuditLogRepository::getUsersWithActivity()
{
    std::vector<std::pair<long long, std::string>> users;
    auto connection = acquireConnection();
    
    try
    {
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT DISTINCT al.changed_by, u.full_name "
                  "FROM " << TABLE_NAME << " al "
                  "JOIN users u ON u.id = al.changed_by "
                  "ORDER BY u.full_name",
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            long long userId = rs.value("changed_by", 0).convert<long long>();
            std::string userName = rs.value("full_name").convert<std::string>();
            users.emplace_back(userId, userName);
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in getUsersWithActivity: " + e.displayText());
    }
    
    return users;
}

models::AuditLog AuditLogRepository::mapRowToAuditLog(Poco::Data::Row& row) const
{
    models::AuditLog auditLog;
    auditLog.id = row.get(0).convert<long long>();
    auditLog.tableName = row.get(1).convert<std::string>();
    auditLog.recordId = row.get(2).convert<long long>();
    
    std::string actionStr = row.get(3).convert<std::string>();
    auditLog.action = models::AuditLog::stringToAuditAction(actionStr);
    
    auditLog.oldValues = row.get(4).convert<std::string>();
    auditLog.newValues = row.get(5).convert<std::string>();
    auditLog.changedBy = row.get(6).convert<long long>();
    auditLog.changedAt = row.get(7).convert<std::string>();
    auditLog.ipAddress = row.get(8).convert<std::string>();
    auditLog.userAgent = row.get(9).convert<std::string>();
    auditLog.description = row.get(10).convert<std::string>();
    
    return auditLog;
}

void AuditLogRepository::enrichAuditLogWithUserDetails(models::AuditLog& auditLog)
{
    auto connection = acquireConnection();
    
    try
    {
        Poco::Int64 userId = auditLog.changedBy;
        
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT full_name FROM users WHERE id = $1",
            Poco::Data::Keywords::use(userId),
            now;
        
        Poco::Data::RecordSet rs(select);
        
        if (rs.rowCount() > 0)
        {
            auditLog.changedByName = rs.value("full_name").convert<std::string>();
        }
        else
        {
            auditLog.changedByName = "Unknown User";
        }
    }
    catch (const Poco::Exception& e)
    {
        auditLog.changedByName = "Error loading user";
    }
}

} // namespace database::repositories
