#include "SupplierRepository.hpp"
#include "../models/Supplier.hpp"
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

const std::string SupplierRepository::TABLE_NAME = "suppliers";
const std::vector<std::string> SupplierRepository::SEARCH_FIELDS = {"name", "contact_person", "email", "tax_id"};

SupplierRepository::SupplierRepository() : BaseRepository<models::Supplier>()
{
}

std::unique_ptr<models::Supplier> SupplierRepository::findById(long long id)
{
    auto connection = acquireConnection();
    
    try
    {
        Poco::Int64 pocoId = static_cast<Poco::Int64>(id);
        Statement select(connection->getSession());
        select << "SELECT s.* FROM " + TABLE_NAME + " s WHERE s.id = $1",
            use(pocoId),
            now;
        
        RecordSet rs(select);
        
        if (rs.rowCount() == 0)
        {
            return nullptr;
        }
        
        Row row = rs.row(0);
        auto supplier = std::make_unique<models::Supplier>(mapRowToSupplier(row));
        
        return supplier;
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

std::vector<std::unique_ptr<models::Supplier>> SupplierRepository::findAll()
{
    auto connection = acquireConnection();
    
    try
    {
        Statement select(connection->getSession());
        select << "SELECT s.* FROM " + TABLE_NAME + " s ORDER BY s.name",
            now;
        
        RecordSet rs(select);
        std::vector<std::unique_ptr<models::Supplier>> suppliers;
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            Row row = rs.row(i);
            suppliers.push_back(std::make_unique<models::Supplier>(mapRowToSupplier(row)));
        }
        
        return suppliers;
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

std::vector<std::unique_ptr<models::Supplier>> SupplierRepository::findPaginated(int page, int pageSize)
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
        select << "SELECT s.* FROM " + TABLE_NAME + " s ORDER BY s.name LIMIT $1 OFFSET $2",
            use(usePageSize),
            use(useOffset),
            now;
        
        RecordSet rs(select);
        std::vector<std::unique_ptr<models::Supplier>> suppliers;
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            Row row = rs.row(i);
            suppliers.push_back(std::make_unique<models::Supplier>(mapRowToSupplier(row)));
        }
        
        return suppliers;
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

long long SupplierRepository::create(const models::Supplier& supplier)
{
    auto connection = acquireConnection();
    
    try
    {
        models::Supplier supplierCopy = supplier;

        connection->beginTransaction();
        
        Statement insert(connection->getSession());
        Poco::Int64 id = 0;
        
        insert << "INSERT INTO " + TABLE_NAME + " (name, contact_person, email, phone, address, "
               << "tax_id, payment_terms, rating, created_at, is_active) "
               << "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10) RETURNING id",
            use(supplierCopy.name),
            use(supplierCopy.contactPerson),
            use(supplierCopy.email),
            use(supplierCopy.phone),
            use(supplierCopy.address),
            use(supplierCopy.taxId),
            use(supplierCopy.paymentTerms),
            use(supplierCopy.rating),
            use(supplierCopy.createdAt),
            use(supplierCopy.isActive),
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

bool SupplierRepository::update(long long id, const models::Supplier& supplier)
{
    auto connection = acquireConnection();
    
    try
    {
        models::Supplier supplierCopy = supplier;
        
        connection->beginTransaction();

        Poco::Int64 pocoId = id;
        
        Statement updateStmt(connection->getSession());
        updateStmt << "UPDATE " + TABLE_NAME + " SET "
                   << "name = $1, contact_person = $2, email = $3, phone = $4, address = $5, "
                   << "tax_id = $6, payment_terms = $7, rating = $8, is_active = $9 "
                   << "WHERE id = $10",
            use(supplierCopy.name),
            use(supplierCopy.contactPerson),
            use(supplierCopy.email),
            use(supplierCopy.phone),
            use(supplierCopy.address),
            use(supplierCopy.taxId),
            use(supplierCopy.paymentTerms),
            use(supplierCopy.rating),
            use(supplierCopy.isActive),
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

bool SupplierRepository::remove(long long id)
{
    auto connection = acquireConnection();
    
    try
    {
        Poco::Int64 pocoId = static_cast<Poco::Int64>(id);
        connection->beginTransaction();
        
        Statement deleteStmt(connection->getSession());
        deleteStmt << "DELETE FROM " + TABLE_NAME + " WHERE id = $1",
            use(pocoId);
        
        int rowsAffected = deleteStmt.execute();
        
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

bool SupplierRepository::softDelete(long long id)
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

int SupplierRepository::count()
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

Poco::JSON::Array SupplierRepository::findAllAsJson()
{
    auto suppliers = findAll();
    Poco::JSON::Array jsonArray;
    
    for (const auto& supplier : suppliers)
    {
        jsonArray.add(supplier->toJson());
    }
    
    return jsonArray;
}

Poco::JSON::Object SupplierRepository::findByIdAsJson(long long id)
{
    auto supplier = findById(id);
    
    if (!supplier)
    {
        throw database::DatabaseException("Supplier not found", database::DatabaseException::ErrorCode::QUERY_FAILED);
    }
    
    return supplier->toJson();
}

std::vector<std::unique_ptr<models::Supplier>> SupplierRepository::findByField(
    const std::string& fieldName, const std::string& fieldValue)
{
    auto connection = acquireConnection();
    
    try
    {
        std::string sql = "SELECT s.* FROM " + TABLE_NAME + " s WHERE " + fieldName + " = $1 ORDER BY s.name";
        
        std::string useFieldValue = fieldValue;
        Statement select(connection->getSession());
        select << sql,
            use(useFieldValue),
            now;
        
        RecordSet rs(select);
        std::vector<std::unique_ptr<models::Supplier>> suppliers;
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            Row row = rs.row(i);
            suppliers.push_back(std::make_unique<models::Supplier>(mapRowToSupplier(row)));
        }
        
        return suppliers;
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

std::vector<std::unique_ptr<models::Supplier>> SupplierRepository::search(
    const std::string& query, const std::vector<std::string>& fields)
{
    auto connection = acquireConnection();
    
    try
    {
        if (fields.empty())
        {
            return std::vector<std::unique_ptr<models::Supplier>>();
        }
        
        std::string sql = "SELECT s.* FROM " + TABLE_NAME + " s WHERE ";
        std::string searchQuery = "%" + query + "%";
        
        for (size_t i = 0; i < fields.size(); ++i)
        {
            if (i > 0) sql += " OR ";
            sql += fields[i] + " ILIKE $1";
        }
        
        sql += " ORDER BY s.name";
        
        Statement select(connection->getSession());
        select << sql;
        
        for (size_t i = 0; i < fields.size(); ++i)
        {
            select, use(searchQuery);
        }
        
        select, now;
        
        RecordSet rs(select);
        std::vector<std::unique_ptr<models::Supplier>> suppliers;
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            Row row = rs.row(i);
            suppliers.push_back(std::make_unique<models::Supplier>(mapRowToSupplier(row)));
        }
        
        return suppliers;
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

std::vector<std::unique_ptr<models::Supplier>> SupplierRepository::findActiveSuppliers()
{
    auto connection = acquireConnection();
    
    try
    {
        Statement select(connection->getSession());
        select << "SELECT s.* FROM " + TABLE_NAME + " s WHERE s.is_active = true ORDER BY s.name",
            now;
        
        RecordSet rs(select);
        std::vector<std::unique_ptr<models::Supplier>> suppliers;
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            Row row = rs.row(i);
            suppliers.push_back(std::make_unique<models::Supplier>(mapRowToSupplier(row)));
        }
        
        return suppliers;
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

std::vector<std::unique_ptr<models::Supplier>> SupplierRepository::findInactiveSuppliers()
{
    auto connection = acquireConnection();
    
    try
    {
        Statement select(connection->getSession());
        select << "SELECT s.* FROM " + TABLE_NAME + " s WHERE s.is_active = false ORDER BY s.name",
            now;
        
        RecordSet rs(select);
        std::vector<std::unique_ptr<models::Supplier>> suppliers;
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            Row row = rs.row(i);
            suppliers.push_back(std::make_unique<models::Supplier>(mapRowToSupplier(row)));
        }
        
        return suppliers;
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

std::vector<std::unique_ptr<models::Supplier>> SupplierRepository::findSuppliersByRating(double minRating, double maxRating)
{
    auto connection = acquireConnection();
    
    try
    {
        double useMinRating = minRating;
        double useMaxRating = maxRating;
        Statement select(connection->getSession());
        select << "SELECT s.* FROM " + TABLE_NAME + " s WHERE s.rating >= $1 AND s.rating <= $2 ORDER BY s.rating DESC",
            use(useMinRating),
            use(useMaxRating),
            now;
        
        RecordSet rs(select);
        std::vector<std::unique_ptr<models::Supplier>> suppliers;
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            Row row = rs.row(i);
            suppliers.push_back(std::make_unique<models::Supplier>(mapRowToSupplier(row)));
        }
        
        return suppliers;
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

std::vector<std::unique_ptr<models::Supplier>> SupplierRepository::findSuppliersWithProducts()
{
    auto connection = acquireConnection();
    
    try
    {
        Statement select(connection->getSession());
        select << "SELECT DISTINCT s.* FROM " + TABLE_NAME + " s "
               << "JOIN products p ON p.supplier_id = s.id "
               << "WHERE p.is_active = true ORDER BY s.name",
            now;
        
        RecordSet rs(select);
        std::vector<std::unique_ptr<models::Supplier>> suppliers;
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            Row row = rs.row(i);
            suppliers.push_back(std::make_unique<models::Supplier>(mapRowToSupplier(row)));
        }
        
        return suppliers;
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

bool SupplierRepository::updateRating(long long id, double newRating)
{
    auto connection = acquireConnection();
    
    try
    {
        Poco::Int64 pocoId = static_cast<Poco::Int64>(id);
        double useNewRating = newRating;
        
        connection->beginTransaction();
        
        Statement updateStmt(connection->getSession());
        updateStmt << "UPDATE " + TABLE_NAME + " SET rating = $1 WHERE id = $2",
            use(useNewRating),
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

bool SupplierRepository::updateStatus(long long id, bool isActive)
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

bool SupplierRepository::updateContactInfo(long long id, const std::string& contactPerson, 
                                         const std::string& email, const std::string& phone)
{
    auto connection = acquireConnection();
    
    try
    {
        Poco::Int64 pocoId = static_cast<Poco::Int64>(id);
        std::string contactPersonCopy = contactPerson;
        std::string emailCopy = email;
        std::string phoneCopy = phone;
        
        connection->beginTransaction();
        
        Statement updateStmt(connection->getSession());
        updateStmt << "UPDATE " + TABLE_NAME + " SET contact_person = $1, email = $2, phone = $3 WHERE id = $4",
            use(contactPersonCopy),
            use(emailCopy),
            use(phoneCopy),
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

int SupplierRepository::countActiveSuppliers()
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

double SupplierRepository::getAverageRating()
{
    auto connection = acquireConnection();
    
    try
    {
        Statement select(connection->getSession());
        select << "SELECT AVG(rating) FROM " + TABLE_NAME + " WHERE rating > 0",
            now;
        
        RecordSet rs(select);
        if (rs.rowCount() > 0 && !rs.row(0)[0].isEmpty())
        {
            return rs.row(0)[0].convert<double>();
        }
        return 0.0;
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

Poco::JSON::Array SupplierRepository::getSupplierStatistics()
{
    auto connection = acquireConnection();
    
    try
    {
        Statement select(connection->getSession());
        select << "SELECT "
               << "COUNT(*) as total_suppliers, "
               << "SUM(CASE WHEN is_active THEN 1 ELSE 0 END) as active_suppliers, "
               << "AVG(rating) as avg_rating, "
               << "MIN(rating) as min_rating, "
               << "MAX(rating) as max_rating "
               << "FROM " + TABLE_NAME,
            now;
        
        RecordSet rs(select);
        Poco::JSON::Array jsonArray;
        
        if (rs.rowCount() > 0)
        {
            Row row = rs.row(0);
            Poco::JSON::Object obj;
            
            obj.set("total_suppliers", row["total_suppliers"].convert<int>());
            obj.set("active_suppliers", row["active_suppliers"].convert<int>());
            obj.set("avg_rating", row["avg_rating"].convert<double>());
            obj.set("min_rating", row["min_rating"].convert<double>());
            obj.set("max_rating", row["max_rating"].convert<double>());
            
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

Poco::JSON::Array SupplierRepository::getSupplierPerformanceReport()
{
    auto connection = acquireConnection();
    
    try
    {
        Statement select(connection->getSession());
        select << "SELECT s.id, s.name, COUNT(p.id) as product_count, "
               << "AVG(p.unit_price) as avg_product_price, "
               << "SUM(CASE WHEN p.is_active THEN 1 ELSE 0 END) as active_products "
               << "FROM " + TABLE_NAME + " s "
               << "LEFT JOIN products p ON p.supplier_id = s.id "
               << "GROUP BY s.id, s.name "
               << "ORDER BY product_count DESC",
            now;
        
        RecordSet rs(select);
        Poco::JSON::Array jsonArray;
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            Row row = rs.row(i);
            Poco::JSON::Object obj;
            
            obj.set("id", row["id"].convert<long long>());
            obj.set("name", row["name"].convert<std::string>());
            obj.set("product_count", row["product_count"].convert<int>());
            obj.set("avg_product_price", row["avg_product_price"].convert<double>());
            obj.set("active_products", row["active_products"].convert<int>());
            
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

std::vector<std::pair<long long, std::string>> SupplierRepository::getSupplierNames()
{
    auto connection = acquireConnection();
    std::vector<std::pair<long long, std::string>> supplierNames;
    
    try
    {
        Statement select(connection->getSession());
        select << "SELECT id, name FROM " + TABLE_NAME + " WHERE is_active = true ORDER BY name",
            now;
        
        RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            Row row = rs.row(i);
            supplierNames.emplace_back(
                row["id"].convert<long long>(),
                row["name"].convert<std::string>()
            );
        }
        
        return supplierNames;
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

models::Supplier SupplierRepository::mapRowToSupplier(Poco::Data::Row& row) const
{
    models::Supplier supplier;
    
    supplier.id = row["id"].convert<long long>();
    supplier.name = row["name"].convert<std::string>();
    
    if (!row["contact_person"].isEmpty())
    {
        supplier.contactPerson = row["contact_person"].convert<std::string>();
    }
    
    if (!row["email"].isEmpty())
    {
        supplier.email = row["email"].convert<std::string>();
    }
    
    if (!row["phone"].isEmpty())
    {
        supplier.phone = row["phone"].convert<std::string>();
    }
    
    if (!row["address"].isEmpty())
    {
        supplier.address = row["address"].convert<std::string>();
    }
    
    if (!row["tax_id"].isEmpty())
    {
        supplier.taxId = row["tax_id"].convert<std::string>();
    }
    
    if (!row["payment_terms"].isEmpty())
    {
        supplier.paymentTerms = row["payment_terms"].convert<std::string>();
    }
    
    if (!row["rating"].isEmpty())
    {
        supplier.rating = row["rating"].convert<double>();
    }
    
    if (!row["created_at"].isEmpty())
    {
        DateTime dt = row["created_at"].extract<DateTime>();
        supplier.createdAt = DateTimeFormatter::format(dt, DateTimeFormat::ISO8601_FORMAT);
    }
    
    supplier.isActive = row["is_active"].convert<bool>();
    
    return supplier;
}

} // namespace database::repositories
