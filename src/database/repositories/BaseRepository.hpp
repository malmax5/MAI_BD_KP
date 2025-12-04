#pragma once

#include <memory>
#include <string>
#include <vector>
#include <Poco/JSON/Object.h>
#include "../DatabaseConnection.hpp"
#include "../ConnectionPool.hpp"

#include <Poco/Data/TypeHandler.h>
#include <Poco/Data/AbstractBinder.h>
#include <Poco/Data/AbstractExtractor.h>

namespace warehouse_backend::database::repositories
{

template<typename T>
class BaseRepository
{
public:
    BaseRepository() 
        : pool(ConnectionPool::getInstance())
    {
        if (!pool.isInitialized())
        {
            throw std::runtime_error("Connection pool is not initialized");
        }
    }
    
    virtual ~BaseRepository() = default;
    
    virtual std::unique_ptr<T> findById(long long id) = 0;
    virtual std::vector<std::unique_ptr<T>> findAll() = 0;
    virtual std::vector<std::unique_ptr<T>> findPaginated(int page, int pageSize) = 0;
    virtual long long create(const T& entity) = 0;
    virtual bool update(long long id, const T& entity) = 0;
    virtual bool remove(long long id) = 0;
    virtual bool softDelete(long long id) = 0;
    virtual int count() = 0;
    
    virtual Poco::JSON::Array findAllAsJson() = 0;
    virtual Poco::JSON::Object findByIdAsJson(long long id) = 0;
    
    virtual std::vector<std::unique_ptr<T>> findByField(const std::string& fieldName, 
                                                        const std::string& fieldValue) = 0;
    virtual std::vector<std::unique_ptr<T>> search(const std::string& query, 
                                                   const std::vector<std::string>& fields) = 0;
    
protected:
    ConnectionPool& pool;
    
    std::shared_ptr<DatabaseConnection> acquireConnection()
    {
        return pool.acquireConnection();
    }
    
    void beginTransaction(DatabaseConnection& connection)
    {
        connection.beginTransaction();
    }
    
    void commitTransaction(DatabaseConnection& connection)
    {
        connection.commitTransaction();
    }
    
    void rollbackTransaction(DatabaseConnection& connection)
    {
        connection.rollbackTransaction();
    }
    
    std::string escapeSql(const std::string& input) const
    {
        std::string escaped = input;
        size_t pos = 0;
        while ((pos = escaped.find("'", pos)) != std::string::npos)
        {
            escaped.replace(pos, 1, "''");
            pos += 2;
        }

        return escaped;
    }
    
    std::string buildSearchQuery(const std::string& query, 
                                 const std::vector<std::string>& fields) const
    {
        std::string sql = "(";
        for (size_t i = 0; i < fields.size(); ++i)
        {
            if (i > 0) sql += " OR ";
            sql += fields[i] + " ILIKE '%" + escapeSql(query) + "%'";
        }

        sql += ")";

        return sql;
    }
    
    std::string buildWhereClause(const std::map<std::string, std::string>& filters) const
    {
        if (filters.empty())
        {
            return "";
        }
        
        std::string where = " WHERE ";
        bool first = true;

        for (const auto& [field, value] : filters)
        {
            if (!first) where += " AND ";
            where += field + " = '" + escapeSql(value) + "'";
            first = false;
        }

        return where;
    }
    
    std::string buildOrderClause(const std::string& orderBy, 
                                 const std::string& orderDir) const
    {
        if (orderBy.empty())
        {
            return "";
        }

        return " ORDER BY " + orderBy + " " + (orderDir == "desc" ? "DESC" : "ASC");
    }
};

} // namespace database::repositories


namespace Poco
{
namespace Data
{

template <>
class TypeHandler<long long>
{
public:
    static void bind(std::size_t pos, const long long& obj, AbstractBinder::Ptr pBinder, AbstractBinder::Direction dir)
    {
        pBinder->bind(pos, static_cast<Poco::Int64>(obj), dir);
    }
    
    static void prepare(std::size_t pos, const long long& obj, AbstractPreparator::Ptr pPreparator)
    {
        pPreparator->prepare(pos, static_cast<Poco::Int64>(obj));
    }
    
    static void extract(std::size_t pos, long long& obj, const long long& defVal, AbstractExtractor::Ptr pExt)
    {
        Poco::Int64 val;
        if (!pExt->extract(pos, val)) {
            obj = defVal;
        } else {
            obj = static_cast<long long>(val);
        }
    }
    
    static std::size_t size()
    {
        return 1;
    }
};

} // namespace Data
} // namespace Poco
