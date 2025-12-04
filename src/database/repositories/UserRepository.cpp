#include "UserRepository.hpp"
#include "../models/User.hpp"
#include <Poco/Data/RecordSet.h>
#include <Poco/Data/Row.h>
#include <Poco/DateTime.h>
#include <Poco/DateTimeFormat.h>
#include <Poco/DateTimeFormatter.h>
#include <Poco/Exception.h>
#include <Poco/Dynamic/Var.h>

using namespace Poco::Data;
using namespace Poco::Data::Keywords;
using namespace Poco;

namespace warehouse_backend::database::repositories
{

const std::string UserRepository::TABLE_NAME = "users";
const std::vector<std::string> UserRepository::SEARCH_FIELDS = {"username", "full_name", "email"};

UserRepository::UserRepository() : BaseRepository<models::User>()
{

}

std::unique_ptr<models::User> UserRepository::findById(long long id)
{
    auto connection = acquireConnection();
    
    try
    {
        Poco::Int64 pocoId = static_cast<Poco::Int64>(id);
        Statement select(connection->getSession());
        select << "SELECT u.* FROM " + TABLE_NAME + " u WHERE u.id = $1",
            use(pocoId),
            now;
        
        RecordSet rs(select);
        
        if (rs.rowCount() == 0)
        {
            return nullptr;
        }
        
        Row row = rs.row(0);
        auto user = std::make_unique<models::User>(mapRowToUser(row));
        
        return user;
    }
    catch (const Poco::Exception& e)
    {
        throw database::DatabaseException(e.displayText(), database::DatabaseException::ErrorCode::QUERY_FAILED);
    }
    catch (const std::exception& e)
    {
        throw database::DatabaseException(e.what(), database::DatabaseException::ErrorCode::QUERY_FAILED);
    }
}

std::vector<std::unique_ptr<models::User>> UserRepository::findAll()
{
    auto connection = acquireConnection();
    
    try
    {
        Statement select(connection->getSession());
        select << "SELECT u.* FROM " + TABLE_NAME + " u ORDER BY u.id",
            now;
        
        RecordSet rs(select);
        std::vector<std::unique_ptr<models::User>> users;
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            Row row = rs.row(i);
            users.push_back(std::make_unique<models::User>(mapRowToUser(row)));
        }
        
        return users;
    }
    catch (const Poco::Exception& e)
    {
        throw database::DatabaseException(e.displayText(), database::DatabaseException::ErrorCode::QUERY_FAILED);
    }
    catch (const std::exception& e)
    {
        throw database::DatabaseException(e.what(), database::DatabaseException::ErrorCode::QUERY_FAILED);
    }
}

std::vector<std::unique_ptr<models::User>> UserRepository::findPaginated(int page, int pageSize)
{
    if (page < 1) page = 1;
    if (pageSize < 1) pageSize = 10;
    
    int offset = (page - 1) * pageSize;
    
    auto connection = acquireConnection();
    
    try
    {
        int usePageSize = pageSize;
        int useOffset = offset;
        Statement select(connection->getSession());
        select << "SELECT u.* FROM " + TABLE_NAME + " u ORDER BY u.id LIMIT $1 OFFSET $2",
            use(usePageSize),
            use(useOffset),
            now;
        
        RecordSet rs(select);
        std::vector<std::unique_ptr<models::User>> users;
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            Row row = rs.row(i);
            users.push_back(std::make_unique<models::User>(mapRowToUser(row)));
        }
        
        return users;
    }
    catch (const Poco::Exception& e)
    {
        throw database::DatabaseException(e.displayText(), database::DatabaseException::ErrorCode::QUERY_FAILED);
    }
    catch (const std::exception& e)
    {
        throw database::DatabaseException(e.what(), database::DatabaseException::ErrorCode::QUERY_FAILED);
    }
}

long long UserRepository::create(const models::User& user)
{
    auto connection = acquireConnection();

    try
    {
        std::string username = user.username;
        std::string passwordHash = user.passwordHash;
        std::string fullName = user.fullName;
        std::string email = user.email;
        std::string roleStr = models::User::roleToString(user.role);
        bool isActive = user.isActive;
        std::string phoneNumber = user.phoneNumber;
        
        {
            Statement check(connection->getSession());
            check << "SELECT COUNT(*) FROM " + TABLE_NAME + " WHERE username = $1 OR email = $2",
                use(username),
                use(email),
                now;
            
            RecordSet rs(check);
            int count = rs.row(0)[0].convert<int>();
            
            if (count > 0)
            {
                throw database::DatabaseException("Username or email already exists", 
                    database::DatabaseException::ErrorCode::QUERY_FAILED);
            }
        }
        
        connection->beginTransaction();
        
        Statement insert(connection->getSession());
        Poco::Int64 id = 0;
        
        insert << "INSERT INTO " + TABLE_NAME + " (username, password_hash, full_name, email, role, is_active, phone_number) "
               << "VALUES ($1, $2, $3, $4, $5, $6, $7) RETURNING id",
            use(username),
            use(passwordHash),
            use(fullName),
            use(email),
            use(roleStr),
            use(isActive),
            use(phoneNumber),
            into(id),
            now;
        
        connection->commitTransaction();
        return static_cast<long long>(id);
    }
    catch (const Poco::Exception& e)
    {
        connection->rollbackTransaction();
        throw database::DatabaseException(e.displayText(), database::DatabaseException::ErrorCode::QUERY_FAILED);
    }
    catch (const std::exception& e)
    {
        connection->rollbackTransaction();
        throw database::DatabaseException(e.what(), database::DatabaseException::ErrorCode::QUERY_FAILED);
    }
}

bool UserRepository::update(long long id, const models::User& user)
{
    auto connection = acquireConnection();
    
    try
    {
        std::string username = user.username;
        std::string passwordHash = user.passwordHash;
        std::string fullName = user.fullName;
        std::string email = user.email;
        std::string roleStr = models::User::roleToString(user.role);
        bool isActive = user.isActive;
        std::string phoneNumber = user.phoneNumber;
        Poco::Int64 pocoId = static_cast<Poco::Int64>(id);
        
        {
            Statement check(connection->getSession());
            check << "SELECT COUNT(*) FROM " + TABLE_NAME + " WHERE id = $1",
                use(pocoId),
                now;
            
            RecordSet rs(check);
            int count = rs.row(0)[0].convert<int>();
            
            if (count == 0)
            {
                return false;
            }
        }
        
        {
            Statement check(connection->getSession());
            check << "SELECT COUNT(*) FROM " + TABLE_NAME + " WHERE (username = $1 OR email = $2) AND id != $3",
                use(username),
                use(email),
                use(pocoId),
                now;
            
            RecordSet rs(check);
            int count = rs.row(0)[0].convert<int>();
            
            if (count > 0)
            {
                throw database::DatabaseException("Username or email already exists for another user", 
                    database::DatabaseException::ErrorCode::QUERY_FAILED);
            }
        }
        
        connection->beginTransaction();
        
        Statement updateStmt(connection->getSession());
        updateStmt << "UPDATE " + TABLE_NAME + " SET username = $1, password_hash = $2, full_name = $3, "
                   << "email = $4, role = $5, is_active = $6, phone_number = $7 WHERE id = $8",
            use(username),
            use(passwordHash),
            use(fullName),
            use(email),
            use(roleStr),
            use(isActive),
            use(phoneNumber),
            use(pocoId),
            now;
        
        int rowsAffected = updateStmt.execute();
        
        connection->commitTransaction();
        return rowsAffected > 0;
    }
    catch (const Poco::Exception& e)
    {
        connection->rollbackTransaction();
        throw database::DatabaseException(e.displayText(), database::DatabaseException::ErrorCode::QUERY_FAILED);
    }
    catch (const std::exception& e)
    {
        connection->rollbackTransaction();
        throw database::DatabaseException(e.what(), database::DatabaseException::ErrorCode::QUERY_FAILED);
    }
}

bool UserRepository::remove(long long id)
{
    auto connection = acquireConnection();
    
    try
    {
        Poco::Int64 pocoId = static_cast<Poco::Int64>(id);
        connection->beginTransaction();
        
        Statement deleteStmt(connection->getSession());
        deleteStmt << "DELETE FROM " + TABLE_NAME + " WHERE id = $1",
            use(pocoId),
            now;
        
        connection->commitTransaction();
        return true;
    }
    catch (const Poco::Exception& e)
    {
        connection->rollbackTransaction();
        throw database::DatabaseException(e.displayText(), database::DatabaseException::ErrorCode::QUERY_FAILED);
    }
    catch (const std::exception& e)
    {
        connection->rollbackTransaction();
        throw database::DatabaseException(e.what(), database::DatabaseException::ErrorCode::QUERY_FAILED);
    }
}

bool UserRepository::softDelete(long long id)
{
    auto connection = acquireConnection();
    
    try
    {
        Poco::Int64 pocoId = static_cast<Poco::Int64>(id);
        connection->beginTransaction();
        
        Statement updateStmt(connection->getSession());
        updateStmt << "UPDATE " + TABLE_NAME + " SET is_active = false WHERE id = $1",
            use(pocoId),
            now;
        
        int rowsAffected = updateStmt.execute();
        
        connection->commitTransaction();
        return rowsAffected > 0;
    }
    catch (const Poco::Exception& e)
    {
        connection->rollbackTransaction();
        throw database::DatabaseException(e.displayText(), database::DatabaseException::ErrorCode::QUERY_FAILED);
    }
    catch (const std::exception& e)
    {
        connection->rollbackTransaction();
        throw database::DatabaseException(e.what(), database::DatabaseException::ErrorCode::QUERY_FAILED);
    }
}

int UserRepository::count()
{
    auto connection = acquireConnection();
    
    try
    {
        Statement select(connection->getSession());
        select << "SELECT COUNT(*) FROM " + TABLE_NAME,
            now;
        
        RecordSet rs(select);
        return rs.row(0)[0].convert<int>();
    }
    catch (const Poco::Exception& e)
    {
        throw database::DatabaseException(e.displayText(), database::DatabaseException::ErrorCode::QUERY_FAILED);
    }
    catch (const std::exception& e)
    {
        throw database::DatabaseException(e.what(), database::DatabaseException::ErrorCode::QUERY_FAILED);
    }
}

Poco::JSON::Array UserRepository::findAllAsJson()
{
    auto users = findAll();
    Poco::JSON::Array jsonArray;
    
    for (const auto& user : users)
    {
        jsonArray.add(user->toJson());
    }
    
    return jsonArray;
}

Poco::JSON::Object UserRepository::findByIdAsJson(long long id)
{
    auto user = findById(id);
    
    if (!user)
    {
        throw database::DatabaseException("User not found", database::DatabaseException::ErrorCode::QUERY_FAILED);
    }
    
    return user->toJson();
}

std::vector<std::unique_ptr<models::User>> UserRepository::findByField(const std::string& fieldName, const std::string& fieldValue)
{
    auto connection = acquireConnection();
    
    try
    {
        std::string sql = "SELECT u.* FROM " + TABLE_NAME + " u WHERE " + fieldName + " = $1 ORDER BY u.id";
        
        std::string useFieldValue = fieldValue;
        Statement select(connection->getSession());
        select << sql,
            use(useFieldValue),
            now;
        
        RecordSet rs(select);
        std::vector<std::unique_ptr<models::User>> users;
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            Row row = rs.row(i);
            users.push_back(std::make_unique<models::User>(mapRowToUser(row)));
        }
        
        return users;
    }
    catch (const Poco::Exception& e)
    {
        throw database::DatabaseException(e.displayText(), database::DatabaseException::ErrorCode::QUERY_FAILED);
    }
    catch (const std::exception& e)
    {
        throw database::DatabaseException(e.what(), database::DatabaseException::ErrorCode::QUERY_FAILED);
    }
}

std::vector<std::unique_ptr<models::User>> UserRepository::search(const std::string& query, const std::vector<std::string>& fields)
{
    auto connection = acquireConnection();
    
    try
    {
        if (fields.empty())
        {
            return std::vector<std::unique_ptr<models::User>>();
        }
        
        std::string sql = "SELECT u.* FROM " + TABLE_NAME + " u WHERE ";
        std::string searchQuery = "%" + query + "%";
        
        for (size_t i = 0; i < fields.size(); ++i)
        {
            if (i > 0) sql += " OR ";
            sql += fields[i] + " ILIKE $" + std::to_string(i + 1);
        }
        
        sql += " ORDER BY u.id";
        
        Statement select(connection->getSession());
        select << sql;
        
        for (size_t i = 0; i < fields.size(); ++i)
        {
            select, use(searchQuery);
        }
        
        select, now;
        
        RecordSet rs(select);
        std::vector<std::unique_ptr<models::User>> users;
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            Row row = rs.row(i);
            users.push_back(std::make_unique<models::User>(mapRowToUser(row)));
        }
        
        return users;
    }
    catch (const Poco::Exception& e)
    {
        throw database::DatabaseException(e.displayText(), database::DatabaseException::ErrorCode::QUERY_FAILED);
    }
    catch (const std::exception& e)
    {
        throw database::DatabaseException(e.what(), database::DatabaseException::ErrorCode::QUERY_FAILED);
    }
}

std::unique_ptr<models::User> UserRepository::findByUsername(const std::string& username)
{
    auto users = findByField("username", username);
    
    if (users.empty())
    {
        return nullptr;
    }
    
    return std::move(users[0]);
}

std::unique_ptr<models::User> UserRepository::findByEmail(const std::string& email)
{
    auto users = findByField("email", email);
    
    if (users.empty())
    {
        return nullptr;
    }
    
    return std::move(users[0]);
}

std::vector<std::unique_ptr<models::User>> UserRepository::findByRole(const std::string& role)
{
    return findByField("role", role);
}

std::vector<std::unique_ptr<models::User>> UserRepository::findActiveUsers()
{
    auto connection = acquireConnection();
    
    try
    {
        Statement select(connection->getSession());
        select << "SELECT u.* FROM " + TABLE_NAME + " u WHERE u.is_active = true ORDER BY u.id",
            now;
        
        RecordSet rs(select);
        std::vector<std::unique_ptr<models::User>> users;
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            Row row = rs.row(i);
            users.push_back(std::make_unique<models::User>(mapRowToUser(row)));
        }
        
        return users;
    }
    catch (const Poco::Exception& e)
    {
        throw database::DatabaseException(e.displayText(), database::DatabaseException::ErrorCode::QUERY_FAILED);
    }
    catch (const std::exception& e)
    {
        throw database::DatabaseException(e.what(), database::DatabaseException::ErrorCode::QUERY_FAILED);
    }
}

std::vector<std::unique_ptr<models::User>> UserRepository::findInactiveUsers()
{
    auto connection = acquireConnection();
    
    try
    {
        Statement select(connection->getSession());
        select << "SELECT u.* FROM " + TABLE_NAME + " u WHERE u.is_active = false ORDER BY u.id",
            now;
        
        RecordSet rs(select);
        std::vector<std::unique_ptr<models::User>> users;
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            Row row = rs.row(i);
            users.push_back(std::make_unique<models::User>(mapRowToUser(row)));
        }
        
        return users;
    }
    catch (const Poco::Exception& e)
    {
        throw database::DatabaseException(e.displayText(), database::DatabaseException::ErrorCode::QUERY_FAILED);
    }
    catch (const std::exception& e)
    {
        throw database::DatabaseException(e.what(), database::DatabaseException::ErrorCode::QUERY_FAILED);
    }
}

std::vector<std::unique_ptr<models::User>> UserRepository::findUsersWithLastLoginBefore(const std::string& date)
{
    auto connection = acquireConnection();
    
    try
    {
        std::string useDate = date;
        Statement select(connection->getSession());
        select << "SELECT u.* FROM " + TABLE_NAME + " u WHERE u.last_login < $1 ORDER BY u.last_login",
            use(useDate),
            now;
        
        RecordSet rs(select);
        std::vector<std::unique_ptr<models::User>> users;
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            Row row = rs.row(i);
            users.push_back(std::make_unique<models::User>(mapRowToUser(row)));
        }
        
        return users;
    }
    catch (const Poco::Exception& e)
    {
        throw database::DatabaseException(e.displayText(), database::DatabaseException::ErrorCode::QUERY_FAILED);
    }
    catch (const std::exception& e)
    {
        throw database::DatabaseException(e.what(), database::DatabaseException::ErrorCode::QUERY_FAILED);
    }
}

bool UserRepository::updatePassword(long long id, const std::string& newPasswordHash)
{
    auto connection = acquireConnection();
    
    try
    {
        Poco::Int64 pocoId = static_cast<Poco::Int64>(id);
        std::string passwordHashCopy = newPasswordHash;
        
        connection->beginTransaction();
        
        Statement updateStmt(connection->getSession());
        updateStmt << "UPDATE " + TABLE_NAME + " SET password_hash = $1 WHERE id = $2",
            use(passwordHashCopy),
            use(pocoId),
            now;
        
        int rowsAffected = updateStmt.execute();
        
        connection->commitTransaction();
        return rowsAffected > 0;
    }
    catch (const Poco::Exception& e)
    {
        connection->rollbackTransaction();
        throw database::DatabaseException(e.displayText(), database::DatabaseException::ErrorCode::QUERY_FAILED);
    }
    catch (const std::exception& e)
    {
        connection->rollbackTransaction();
        throw database::DatabaseException(e.what(), database::DatabaseException::ErrorCode::QUERY_FAILED);
    }
}

bool UserRepository::updateLastLogin(long long id, const std::string& loginTime)
{
    auto connection = acquireConnection();
    
    try
    {
        Poco::Int64 pocoId = static_cast<Poco::Int64>(id);
        std::string loginTimeCopy = loginTime;
        
        connection->beginTransaction();
        
        Statement updateStmt(connection->getSession());
        updateStmt << "UPDATE " + TABLE_NAME + " SET last_login = $1 WHERE id = $2",
            use(loginTimeCopy),
            use(pocoId),
            now;
        
        int rowsAffected = updateStmt.execute();
        
        connection->commitTransaction();
        return rowsAffected > 0;
    }
    catch (const Poco::Exception& e)
    {
        connection->rollbackTransaction();
        throw database::DatabaseException(e.displayText(), database::DatabaseException::ErrorCode::QUERY_FAILED);
    }
    catch (const std::exception& e)
    {
        connection->rollbackTransaction();
        throw database::DatabaseException(e.what(), database::DatabaseException::ErrorCode::QUERY_FAILED);
    }
}

bool UserRepository::updateStatus(long long id, bool isActive)
{
    auto connection = acquireConnection();
    
    try
    {
        Poco::Int64 pocoId = static_cast<Poco::Int64>(id);
        bool useIsActive = isActive;
        
        connection->beginTransaction();
        
        Statement updateStmt(connection->getSession());
        updateStmt << "UPDATE " + TABLE_NAME + " SET is_active = $1 WHERE id = $2",
            use(useIsActive),
            use(pocoId),
            now;
        
        int rowsAffected = updateStmt.execute();
        
        connection->commitTransaction();
        return rowsAffected > 0;
    }
    catch (const Poco::Exception& e)
    {
        connection->rollbackTransaction();
        throw database::DatabaseException(e.displayText(), database::DatabaseException::ErrorCode::QUERY_FAILED);
    }
    catch (const std::exception& e)
    {
        connection->rollbackTransaction();
        throw database::DatabaseException(e.what(), database::DatabaseException::ErrorCode::QUERY_FAILED);
    }
}

bool UserRepository::updateRole(long long id, const std::string& role)
{
    auto connection = acquireConnection();
    
    try
    {
        Poco::Int64 pocoId = static_cast<Poco::Int64>(id);
        std::string roleCopy = role;
        
        connection->beginTransaction();
        
        Statement updateStmt(connection->getSession());
        updateStmt << "UPDATE " + TABLE_NAME + " SET role = $1 WHERE id = $2",
            use(roleCopy),
            use(pocoId),
            now;
        
        int rowsAffected = updateStmt.execute();
        
        connection->commitTransaction();
        return rowsAffected > 0;
    }
    catch (const Poco::Exception& e)
    {
        connection->rollbackTransaction();
        throw database::DatabaseException(e.displayText(), database::DatabaseException::ErrorCode::QUERY_FAILED);
    }
    catch (const std::exception& e)
    {
        connection->rollbackTransaction();
        throw database::DatabaseException(e.what(), database::DatabaseException::ErrorCode::QUERY_FAILED);
    }
}

int UserRepository::countByRole(const std::string& role)
{
    auto connection = acquireConnection();
    
    try
    {
        std::string useRole = role;
        Statement select(connection->getSession());
        select << "SELECT COUNT(*) FROM " + TABLE_NAME + " WHERE role = $1",
            use(useRole),
            now;
        
        RecordSet rs(select);
        return rs.row(0)[0].convert<int>();
    }
    catch (const Poco::Exception& e)
    {
        throw database::DatabaseException(e.displayText(), database::DatabaseException::ErrorCode::QUERY_FAILED);
    }
    catch (const std::exception& e)
    {
        throw database::DatabaseException(e.what(), database::DatabaseException::ErrorCode::QUERY_FAILED);
    }
}

int UserRepository::countActiveUsers()
{
    auto connection = acquireConnection();
    
    try
    {
        Statement select(connection->getSession());
        select << "SELECT COUNT(*) FROM " + TABLE_NAME + " WHERE is_active = true",
            now;
        
        RecordSet rs(select);
        return rs.row(0)[0].convert<int>();
    }
    catch (const Poco::Exception& e)
    {
        throw database::DatabaseException(e.displayText(), database::DatabaseException::ErrorCode::QUERY_FAILED);
    }
    catch (const std::exception& e)
    {
        throw database::DatabaseException(e.what(), database::DatabaseException::ErrorCode::QUERY_FAILED);
    }
}

bool UserRepository::usernameExists(const std::string& username)
{
    auto connection = acquireConnection();
    
    try
    {
        std::string useUsername = username;
        Statement select(connection->getSession());
        select << "SELECT COUNT(*) FROM " + TABLE_NAME + " WHERE username = $1",
            use(useUsername),
            now;
        
        RecordSet rs(select);
        int count = rs.row(0)[0].convert<int>();
        
        return count > 0;
    }
    catch (const Poco::Exception& e)
    {
        throw database::DatabaseException(e.displayText(), database::DatabaseException::ErrorCode::QUERY_FAILED);
    }
    catch (const std::exception& e)
    {
        throw database::DatabaseException(e.what(), database::DatabaseException::ErrorCode::QUERY_FAILED);
    }
}

bool UserRepository::emailExists(const std::string& email)
{
    auto connection = acquireConnection();
    
    try
    {
        std::string useEmail = email;
        Statement select(connection->getSession());
        select << "SELECT COUNT(*) FROM " + TABLE_NAME + " WHERE email = $1",
            use(useEmail),
            now;
        
        RecordSet rs(select);
        int count = rs.row(0)[0].convert<int>();
        
        return count > 0;
    }
    catch (const Poco::Exception& e)
    {
        throw database::DatabaseException(e.displayText(), database::DatabaseException::ErrorCode::QUERY_FAILED);
    }
    catch (const std::exception& e)
    {
        throw database::DatabaseException(e.what(), database::DatabaseException::ErrorCode::QUERY_FAILED);
    }
}

Poco::JSON::Array UserRepository::getUserStatistics()
{
    auto connection = acquireConnection();
    
    try
    {
        Statement select(connection->getSession());
        select << "SELECT role, COUNT(*) as count, "
               << "SUM(CASE WHEN is_active THEN 1 ELSE 0 END) as active_count, "
               << "SUM(CASE WHEN NOT is_active THEN 1 ELSE 0 END) as inactive_count "
               << "FROM " + TABLE_NAME + " GROUP BY role ORDER BY role",
            now;
        
        RecordSet rs(select);
        Poco::JSON::Array jsonArray;
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            Row row = rs.row(i);
            Poco::JSON::Object obj;
            
            obj.set("role", row["role"].convert<std::string>());
            obj.set("count", row["count"].convert<int>());
            obj.set("active_count", row["active_count"].convert<int>());
            obj.set("inactive_count", row["inactive_count"].convert<int>());
            
            jsonArray.add(obj);
        }
        
        return jsonArray;
    }
    catch (const Poco::Exception& e)
    {
        throw database::DatabaseException(e.displayText(), database::DatabaseException::ErrorCode::QUERY_FAILED);
    }
    catch (const std::exception& e)
    {
        throw database::DatabaseException(e.what(), database::DatabaseException::ErrorCode::QUERY_FAILED);
    }
}

models::User UserRepository::mapRowToUser(Poco::Data::Row& row) const
{
    models::User user;
    
    user.id = row["id"].convert<long long>();
    user.username = row["username"].convert<std::string>();
    user.passwordHash = row["password_hash"].convert<std::string>();
    user.fullName = row["full_name"].convert<std::string>();
    user.email = row["email"].convert<std::string>();
    
    std::string roleStr = row["role"].convert<std::string>();
    user.role = models::User::stringToRole(roleStr);
    
    if (!row["created_at"].isEmpty())
    {
        DateTime dt = row["created_at"].extract<DateTime>();
        user.createdAt = DateTimeFormatter::format(dt, DateTimeFormat::ISO8601_FORMAT);
    }
    
    if (!row["last_login"].isEmpty())
    {
        DateTime dt = row["last_login"].extract<DateTime>();
        user.lastLogin = DateTimeFormatter::format(dt, DateTimeFormat::ISO8601_FORMAT);
    }
    
    user.isActive = row["is_active"].convert<bool>();
    
    if (!row["phone_number"].isEmpty())
    {
        user.phoneNumber = row["phone_number"].convert<std::string>();
    }
    
    return user;
}

} // namespace database::repositories
