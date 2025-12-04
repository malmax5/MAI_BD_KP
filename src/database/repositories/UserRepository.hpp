#pragma once

#include "BaseRepository.hpp"
#include "../models/User.hpp"
#include <memory>
#include <string>
#include <vector>
#include <Poco/JSON/Object.h>
#include <Poco/JSON/Array.h>

namespace warehouse_backend::database::repositories
{

class UserRepository : public BaseRepository<models::User>
{
public:
    UserRepository();
    ~UserRepository() override = default;
    
    std::unique_ptr<models::User> findById(long long id) override;
    std::vector<std::unique_ptr<models::User>> findAll() override;
    std::vector<std::unique_ptr<models::User>> findPaginated(int page, int pageSize) override;
    long long create(const models::User& user) override;
    bool update(long long id, const models::User& user) override;
    bool remove(long long id) override;
    bool softDelete(long long id) override;
    int count() override;
    
    Poco::JSON::Array findAllAsJson() override;
    Poco::JSON::Object findByIdAsJson(long long id) override;
    
    std::vector<std::unique_ptr<models::User>> findByField(const std::string& fieldName, 
                                                           const std::string& fieldValue) override;
    std::vector<std::unique_ptr<models::User>> search(const std::string& query, 
                                                      const std::vector<std::string>& fields) override;
    
    std::unique_ptr<models::User> findByUsername(const std::string& username);
    std::unique_ptr<models::User> findByEmail(const std::string& email);
    std::vector<std::unique_ptr<models::User>> findByRole(const std::string& role);
    std::vector<std::unique_ptr<models::User>> findActiveUsers();
    std::vector<std::unique_ptr<models::User>> findInactiveUsers();
    std::vector<std::unique_ptr<models::User>> findUsersWithLastLoginBefore(const std::string& date);
    
    bool updatePassword(long long id, const std::string& newPasswordHash);
    bool updateLastLogin(long long id, const std::string& loginTime);
    bool updateStatus(long long id, bool isActive);
    bool updateRole(long long id, const std::string& role);
    
    int countByRole(const std::string& role);
    int countActiveUsers();
    
    bool usernameExists(const std::string& username);
    bool emailExists(const std::string& email);
    
    Poco::JSON::Array getUserStatistics();
    
private:
    models::User mapRowToUser(Poco::Data::Row& row) const;
    
    static const std::string TABLE_NAME;
    static const std::vector<std::string> SEARCH_FIELDS;
};

} // namespace database::repositories
