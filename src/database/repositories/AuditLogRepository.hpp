#pragma once

#include "BaseRepository.hpp"
#include "../models/AuditLog.hpp"
#include <memory>
#include <string>
#include <vector>
#include <Poco/JSON/Object.h>
#include <Poco/JSON/Array.h>

namespace warehouse_backend::database::repositories
{

class AuditLogRepository : public BaseRepository<models::AuditLog>
{
public:
    AuditLogRepository();
    ~AuditLogRepository() override = default;
    
    std::unique_ptr<models::AuditLog> findById(long long id) override;
    std::vector<std::unique_ptr<models::AuditLog>> findAll() override;
    std::vector<std::unique_ptr<models::AuditLog>> findPaginated(int page, int pageSize) override;
    long long create(const models::AuditLog& auditLog) override;
    bool update(long long id, const models::AuditLog& auditLog) override;
    bool remove(long long id) override;
    bool softDelete(long long id) override;
    int count() override;
    
    Poco::JSON::Array findAllAsJson() override;
    Poco::JSON::Object findByIdAsJson(long long id) override;
    
    std::vector<std::unique_ptr<models::AuditLog>> findByField(const std::string& fieldName, 
                                                               const std::string& fieldValue) override;
    std::vector<std::unique_ptr<models::AuditLog>> search(const std::string& query, 
                                                          const std::vector<std::string>& fields) override;
    
    std::vector<std::unique_ptr<models::AuditLog>> findByTableName(const std::string& tableName);
    std::vector<std::unique_ptr<models::AuditLog>> findByRecordId(long long recordId);
    std::vector<std::unique_ptr<models::AuditLog>> findByAction(const std::string& action);
    std::vector<std::unique_ptr<models::AuditLog>> findByChangedBy(long long userId);
    std::vector<std::unique_ptr<models::AuditLog>> findByDateRange(const std::string& startDate, const std::string& endDate);
    std::vector<std::unique_ptr<models::AuditLog>> findByTableAndRecord(const std::string& tableName, long long recordId);
    
    std::vector<std::unique_ptr<models::AuditLog>> findRecentLogs(int limit = 100);
    std::vector<std::unique_ptr<models::AuditLog>> findLogsByUserAction(long long userId, const std::string& action);
    
    bool deleteOldLogs(int daysOld);
    bool deleteLogsByTable(const std::string& tableName);
    
    int countByTable(const std::string& tableName);
    int countByAction(const std::string& action);
    int countByUser(long long userId);
    
    Poco::JSON::Array getAuditStatistics();
    Poco::JSON::Array getActivityReport(const std::string& startDate, const std::string& endDate);
    Poco::JSON::Array getUserActivityReport(long long userId, const std::string& startDate, const std::string& endDate);
    Poco::JSON::Array getTableActivityReport(const std::string& tableName, const std::string& startDate, const std::string& endDate);
    
    std::vector<std::string> getAuditedTables();
    std::vector<std::pair<long long, std::string>> getUsersWithActivity();
    
private:
    models::AuditLog mapRowToAuditLog(Poco::Data::Row& row) const;
    void enrichAuditLogWithUserDetails(models::AuditLog& auditLog);
    
    static const std::string TABLE_NAME;
    static const std::vector<std::string> SEARCH_FIELDS;
};

} // namespace database::repositories
