#pragma once

#include "BaseRepository.hpp"
#include "../models/Supplier.hpp"
#include <memory>
#include <string>
#include <vector>
#include <Poco/JSON/Object.h>
#include <Poco/JSON/Array.h>
 
namespace warehouse_backend::database::repositories
{

class SupplierRepository : public BaseRepository<models::Supplier>
{
public:
    SupplierRepository();
    ~SupplierRepository() override = default;
    
    std::unique_ptr<models::Supplier> findById(long long id) override;
    std::vector<std::unique_ptr<models::Supplier>> findAll() override;
    std::vector<std::unique_ptr<models::Supplier>> findPaginated(int page, int pageSize) override;
    long long create(const models::Supplier& supplier) override;
    bool update(long long id, const models::Supplier& supplier) override;
    bool remove(long long id) override;
    bool softDelete(long long id) override;
    int count() override;
    
    Poco::JSON::Array findAllAsJson() override;
    Poco::JSON::Object findByIdAsJson(long long id) override;
    
    std::vector<std::unique_ptr<models::Supplier>> findByField(const std::string& fieldName, 
                                                               const std::string& fieldValue) override;
    std::vector<std::unique_ptr<models::Supplier>> search(const std::string& query, 
                                                          const std::vector<std::string>& fields) override;
    
    std::vector<std::unique_ptr<models::Supplier>> findActiveSuppliers();
    std::vector<std::unique_ptr<models::Supplier>> findInactiveSuppliers();
    std::vector<std::unique_ptr<models::Supplier>> findSuppliersByRating(double minRating, double maxRating = 5.0);
    std::vector<std::unique_ptr<models::Supplier>> findSuppliersWithProducts();
    
    bool updateRating(long long id, double newRating);
    bool updateStatus(long long id, bool isActive);
    bool updateContactInfo(long long id, const std::string& contactPerson, 
                          const std::string& email, const std::string& phone);
    
    int countActiveSuppliers();
    double getAverageRating();
    
    Poco::JSON::Array getSupplierStatistics();
    Poco::JSON::Array getSupplierPerformanceReport();
    
    std::vector<std::pair<long long, std::string>> getSupplierNames();
    
private:
    models::Supplier mapRowToSupplier(Poco::Data::Row& row) const;
    
    static const std::string TABLE_NAME;
    static const std::vector<std::string> SEARCH_FIELDS;
};

} // namespace database::repositories
