#include "ProductRepository.hpp"
#include <Poco/Data/Session.h>
#include <Poco/Data/RecordSet.h>
#include <Poco/Data/Statement.h>
#include <Poco/Data/Row.h>
#include <Poco/DateTimeFormat.h>
#include <Poco/DateTimeFormatter.h>
#include <Poco/DateTimeParser.h>
#include "../../utils/DateUtils.hpp"
#include "../../utils/JsonUtils.hpp"

using namespace Poco::Data;
using namespace Poco::Data::Keywords;
using namespace Poco;

using namespace warehouse_backend::utils;

namespace warehouse_backend::database::repositories
{

const std::string ProductRepository::TABLE_NAME = "products";
const std::vector<std::string> ProductRepository::SEARCH_FIELDS = {
    "sku", "name", "description"
};

ProductRepository::ProductRepository() : BaseRepository<models::Product>()
{

}

std::unique_ptr<models::Product> ProductRepository::findById(long long id)
{
    auto connection = acquireConnection();
    
    try
    {
        Poco::Int64 pocoId = static_cast<Poco::Int64>(id);
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT p.id, p.sku, p.name, p.description, p.category_id, "
                  "p.supplier_id, p.unit_price, p.weight, p.dimensions, "
                  "p.min_stock_level, p.max_stock_level, p.is_active, p.created_at, "
                  "c.name as category_name, s.name as supplier_name "
                  "FROM " << TABLE_NAME << " p "
                  "LEFT JOIN categories c ON c.id = p.category_id "
                  "LEFT JOIN suppliers s ON s.id = p.supplier_id "
                  "WHERE p.id = $1",
            Poco::Data::Keywords::use(pocoId),
            now;
        
        Poco::Data::RecordSet rs(select);
        
        if (rs.rowCount() > 0)
        {
            auto product = std::make_unique<models::Product>();
            product->id = rs.value("id", 0).convert<long long>();
            product->sku = rs.value("sku").convert<std::string>();
            product->name = rs.value("name").convert<std::string>();
            product->description = rs.value("description").convert<std::string>();
            product->categoryId = rs.value("category_id", 0).convert<long long>();
            product->supplierId = rs.value("supplier_id", 0).convert<long long>();
            product->unitPrice = rs.value("unit_price", 0.0).convert<double>();
            product->weight = rs.value("weight", 0.0).convert<double>();
            product->dimensions = rs.value("dimensions").convert<std::string>();
            product->minStockLevel = rs.value("min_stock_level", 0).convert<int>();
            product->maxStockLevel = rs.value("max_stock_level", 0).convert<int>();
            product->isActive = rs.value("is_active", false).convert<bool>();
            product->createdAt = rs.value("created_at").convert<std::string>();
            product->categoryName = rs.value("category_name").convert<std::string>();
            product->supplierName = rs.value("supplier_name").convert<std::string>();
            
            // Рассчитываем текущий остаток
            calculateCurrentStock(*product);
            
            return product;
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findById: " + e.displayText());
    }
    
    return nullptr;
}

std::vector<std::unique_ptr<models::Product>> ProductRepository::findAll()
{
    std::vector<std::unique_ptr<models::Product>> products;
    auto connection = acquireConnection();
    
    try
    {
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT p.id, p.sku, p.name, p.description, p.category_id, "
                  "p.supplier_id, p.unit_price, p.weight, p.dimensions, "
                  "p.min_stock_level, p.max_stock_level, p.is_active, p.created_at, "
                  "c.name as category_name, s.name as supplier_name "
                  "FROM " << TABLE_NAME << " p "
                  "LEFT JOIN categories c ON c.id = p.category_id "
                  "LEFT JOIN suppliers s ON s.id = p.supplier_id "
                  "ORDER BY p.name",
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            auto product = std::make_unique<models::Product>();
            product->id = rs.value("id", 0).convert<long long>();
            product->sku = rs.value("sku").convert<std::string>();
            product->name = rs.value("name").convert<std::string>();
            product->description = rs.value("description").convert<std::string>();
            product->categoryId = rs.value("category_id", 0).convert<long long>();
            product->supplierId = rs.value("supplier_id", 0).convert<long long>();
            product->unitPrice = rs.value("unit_price", 0.0).convert<double>();
            product->weight = rs.value("weight", 0.0).convert<double>();
            product->dimensions = rs.value("dimensions").convert<std::string>();
            product->minStockLevel = rs.value("min_stock_level", 0).convert<int>();
            product->maxStockLevel = rs.value("max_stock_level", 0).convert<int>();
            product->isActive = rs.value("is_active", false).convert<bool>();
            product->createdAt = rs.value("created_at").convert<std::string>();
            product->categoryName = rs.value("category_name").convert<std::string>();
            product->supplierName = rs.value("supplier_name").convert<std::string>();
            
            // Рассчитываем текущий остаток
            calculateCurrentStock(*product);
            
            products.push_back(std::move(product));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findAll: " + e.displayText());
    }
    
    return products;
}

std::vector<std::unique_ptr<models::Product>> ProductRepository::findPaginated(int page, int pageSize)
{
    std::vector<std::unique_ptr<models::Product>> products;
    auto connection = acquireConnection();
    
    if (page < 1) page = 1;
    int offset = (page - 1) * pageSize;
    
    try
    {
        int usePageSize = pageSize;
        int useOffset = offset;
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT p.id, p.sku, p.name, p.description, p.category_id, "
                  "p.supplier_id, p.unit_price, p.weight, p.dimensions, "
                  "p.min_stock_level, p.max_stock_level, p.is_active, p.created_at, "
                  "c.name as category_name, s.name as supplier_name "
                  "FROM " << TABLE_NAME << " p "
                  "LEFT JOIN categories c ON c.id = p.category_id "
                  "LEFT JOIN suppliers s ON s.id = p.supplier_id "
                  "ORDER BY p.name LIMIT $1 OFFSET $2",
            Poco::Data::Keywords::use(usePageSize),
            Poco::Data::Keywords::use(useOffset),
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            auto product = std::make_unique<models::Product>();
            product->id = rs.value("id", 0).convert<long long>();
            product->sku = rs.value("sku").convert<std::string>();
            product->name = rs.value("name").convert<std::string>();
            product->description = rs.value("description").convert<std::string>();
            product->categoryId = rs.value("category_id", 0).convert<long long>();
            product->supplierId = rs.value("supplier_id", 0).convert<long long>();
            product->unitPrice = rs.value("unit_price", 0.0).convert<double>();
            product->weight = rs.value("weight", 0.0).convert<double>();
            product->dimensions = rs.value("dimensions").convert<std::string>();
            product->minStockLevel = rs.value("min_stock_level", 0).convert<int>();
            product->maxStockLevel = rs.value("max_stock_level", 0).convert<int>();
            product->isActive = rs.value("is_active", false).convert<bool>();
            product->createdAt = rs.value("created_at").convert<std::string>();
            product->categoryName = rs.value("category_name").convert<std::string>();
            product->supplierName = rs.value("supplier_name").convert<std::string>();
            
            // Рассчитываем текущий остаток
            calculateCurrentStock(*product);
            
            products.push_back(std::move(product));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findPaginated: " + e.displayText());
    }
    
    return products;
}

long long ProductRepository::create(const models::Product& product)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        // Создаем локальные переменные
        std::string sku = product.sku;
        std::string name = product.name;
        std::string description = product.description;
        Poco::Int64 categoryId = static_cast<Poco::Int64>(product.categoryId);
        Poco::Int64 supplierId = static_cast<Poco::Int64>(product.supplierId);
        double unitPrice = product.unitPrice;
        double weight = product.weight;
        std::string dimensions = product.dimensions;
        int minStockLevel = product.minStockLevel;
        int maxStockLevel = product.maxStockLevel;
        bool isActive = product.isActive;
        
        std::string createdAt = product.createdAt.empty() ? 
            DateUtils::formatDateTime(DateUtils::now()) : product.createdAt;
        
        Poco::Data::Statement insert(connection->getSession());
        Poco::Int64 newId = 0;
        
        insert << "INSERT INTO " << TABLE_NAME << " "
                  "(sku, name, description, category_id, supplier_id, "
                  "unit_price, weight, dimensions, min_stock_level, "
                  "max_stock_level, is_active, created_at) "
                  "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12) "
                  "RETURNING id",
            Poco::Data::Keywords::use(sku),
            Poco::Data::Keywords::use(name),
            Poco::Data::Keywords::use(description),
            Poco::Data::Keywords::use(categoryId),
            Poco::Data::Keywords::use(supplierId),
            Poco::Data::Keywords::use(unitPrice),
            Poco::Data::Keywords::use(weight),
            Poco::Data::Keywords::use(dimensions),
            Poco::Data::Keywords::use(minStockLevel),
            Poco::Data::Keywords::use(maxStockLevel),
            Poco::Data::Keywords::use(isActive),
            Poco::Data::Keywords::use(createdAt),
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

bool ProductRepository::update(long long id, const models::Product& product)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        // Создаем локальные переменные
        std::string sku = product.sku;
        std::string name = product.name;
        std::string description = product.description;
        Poco::Int64 categoryId = static_cast<Poco::Int64>(product.categoryId);
        Poco::Int64 supplierId = static_cast<Poco::Int64>(product.supplierId);
        double unitPrice = product.unitPrice;
        double weight = product.weight;
        std::string dimensions = product.dimensions;
        int minStockLevel = product.minStockLevel;
        int maxStockLevel = product.maxStockLevel;
        bool isActive = product.isActive;
        Poco::Int64 pocoId = static_cast<Poco::Int64>(id);
        
        Poco::Data::Statement update(connection->getSession());
        update << "UPDATE " << TABLE_NAME << " SET "
                  "sku = $1, name = $2, description = $3, category_id = $4, "
                  "supplier_id = $5, unit_price = $6, weight = $7, dimensions = $8, "
                  "min_stock_level = $9, max_stock_level = $10, is_active = $11 "
                  "WHERE id = $12",
            Poco::Data::Keywords::use(sku),
            Poco::Data::Keywords::use(name),
            Poco::Data::Keywords::use(description),
            Poco::Data::Keywords::use(categoryId),
            Poco::Data::Keywords::use(supplierId),
            Poco::Data::Keywords::use(unitPrice),
            Poco::Data::Keywords::use(weight),
            Poco::Data::Keywords::use(dimensions),
            Poco::Data::Keywords::use(minStockLevel),
            Poco::Data::Keywords::use(maxStockLevel),
            Poco::Data::Keywords::use(isActive),
            Poco::Data::Keywords::use(pocoId),
            now;
        
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

bool ProductRepository::remove(long long id)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        Poco::Int64 pocoId = static_cast<Poco::Int64>(id);
        
        // Проверяем, есть ли связанные партии товаров
        Poco::Data::Statement checkBatches(connection->getSession());
        checkBatches << "SELECT COUNT(*) FROM product_batches WHERE product_id = $1",
            Poco::Data::Keywords::use(pocoId),
            now;
        
        int batchCount = 0;
        Poco::Data::RecordSet rsBatches(checkBatches);
        if (rsBatches.rowCount() > 0)
        {
            batchCount = rsBatches.value(0, 0).convert<int>();
        }
        
        if (batchCount > 0)
        {
            throw std::runtime_error("Cannot delete product with associated batches");
        }
        
        // Проверяем, есть ли связанные позиции заказов
        Poco::Data::Statement checkOrders(connection->getSession());
        checkOrders << "SELECT COUNT(*) FROM order_items WHERE product_id = $1",
            Poco::Data::Keywords::use(pocoId),
            now;
        
        int orderCount = 0;
        Poco::Data::RecordSet rsOrders(checkOrders);
        if (rsOrders.rowCount() > 0)
        {
            orderCount = rsOrders.value(0, 0).convert<int>();
        }
        
        if (orderCount > 0)
        {
            throw std::runtime_error("Cannot delete product with associated order items");
        }
        
        Poco::Data::Statement del(connection->getSession());
        del << "DELETE FROM " << TABLE_NAME << " WHERE id = $1",
            Poco::Data::Keywords::use(pocoId);
        
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

bool ProductRepository::softDelete(long long id)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        Poco::Int64 pocoId = static_cast<Poco::Int64>(id);
        
        Poco::Data::Statement update(connection->getSession());
        update << "UPDATE " << TABLE_NAME << " SET is_active = false WHERE id = $1",
            Poco::Data::Keywords::use(pocoId),
            now;
        
        int rowsAffected = update.execute();
        
        commitTransaction(*connection);
        return rowsAffected > 0;
    }
    catch (const Poco::Exception& e)
    {
        rollbackTransaction(*connection);
        throw std::runtime_error("Database error in softDelete: " + e.displayText());
    }
}

int ProductRepository::count()
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

Poco::JSON::Array ProductRepository::findAllAsJson()
{
    auto products = findAll();
    Poco::JSON::Array jsonArray;
    
    for (const auto& product : products)
    {
        if (product)
        {
            jsonArray.add(product->toJson());
        }
    }
    
    return jsonArray;
}

Poco::JSON::Object ProductRepository::findByIdAsJson(long long id)
{
    auto product = findById(id);
    if (product)
    {
        return product->toJson();
    }
    
    return Poco::JSON::Object();
}

std::vector<std::unique_ptr<models::Product>> ProductRepository::findByField(
    const std::string& fieldName, const std::string& fieldValue)
{
    std::vector<std::unique_ptr<models::Product>> products;
    auto connection = acquireConnection();
    
    try
    {
        std::string sql = "SELECT p.id, p.sku, p.name, p.description, p.category_id, "
                          "p.supplier_id, p.unit_price, p.weight, p.dimensions, "
                          "p.min_stock_level, p.max_stock_level, p.is_active, p.created_at, "
                          "c.name as category_name, s.name as supplier_name "
                          "FROM " + TABLE_NAME + " p "
                          "LEFT JOIN categories c ON c.id = p.category_id "
                          "LEFT JOIN suppliers s ON s.id = p.supplier_id "
                          "WHERE p." + fieldName + " = $1 ORDER BY p.name";
        
        std::string fieldValueCopy = fieldValue;
        Poco::Data::Statement select(connection->getSession());
        select << sql,
            Poco::Data::Keywords::use(fieldValueCopy),
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            auto product = std::make_unique<models::Product>();
            product->id = rs.value("id", 0).convert<long long>();
            product->sku = rs.value("sku").convert<std::string>();
            product->name = rs.value("name").convert<std::string>();
            product->description = rs.value("description").convert<std::string>();
            product->categoryId = rs.value("category_id", 0).convert<long long>();
            product->supplierId = rs.value("supplier_id", 0).convert<long long>();
            product->unitPrice = rs.value("unit_price", 0.0).convert<double>();
            product->weight = rs.value("weight", 0.0).convert<double>();
            product->dimensions = rs.value("dimensions").convert<std::string>();
            product->minStockLevel = rs.value("min_stock_level", 0).convert<int>();
            product->maxStockLevel = rs.value("max_stock_level", 0).convert<int>();
            product->isActive = rs.value("is_active", false).convert<bool>();
            product->createdAt = rs.value("created_at").convert<std::string>();
            product->categoryName = rs.value("category_name").convert<std::string>();
            product->supplierName = rs.value("supplier_name").convert<std::string>();
            
            // Рассчитываем текущий остаток
            calculateCurrentStock(*product);
            
            products.push_back(std::move(product));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findByField: " + e.displayText());
    }
    
    return products;
}

std::vector<std::unique_ptr<models::Product>> ProductRepository::search(
    const std::string& query, const std::vector<std::string>& fields)
{
    std::vector<std::unique_ptr<models::Product>> products;
    auto connection = acquireConnection();
    
    try
    {
        std::string searchClause = buildSearchQuery(query, fields);
        std::string sql = "SELECT p.id, p.sku, p.name, p.description, p.category_id, "
                          "p.supplier_id, p.unit_price, p.weight, p.dimensions, "
                          "p.min_stock_level, p.max_stock_level, p.is_active, p.created_at, "
                          "c.name as category_name, s.name as supplier_name "
                          "FROM " + TABLE_NAME + " p "
                          "LEFT JOIN categories c ON c.id = p.category_id "
                          "LEFT JOIN suppliers s ON s.id = p.supplier_id "
                          "WHERE " + searchClause + " ORDER BY p.name";
        
        Poco::Data::Statement select(connection->getSession());
        select << sql,
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            auto product = std::make_unique<models::Product>();
            product->id = rs.value("id", 0).convert<long long>();
            product->sku = rs.value("sku").convert<std::string>();
            product->name = rs.value("name").convert<std::string>();
            product->description = rs.value("description").convert<std::string>();
            product->categoryId = rs.value("category_id", 0).convert<long long>();
            product->supplierId = rs.value("supplier_id", 0).convert<long long>();
            product->unitPrice = rs.value("unit_price", 0.0).convert<double>();
            product->weight = rs.value("weight", 0.0).convert<double>();
            product->dimensions = rs.value("dimensions").convert<std::string>();
            product->minStockLevel = rs.value("min_stock_level", 0).convert<int>();
            product->maxStockLevel = rs.value("max_stock_level", 0).convert<int>();
            product->isActive = rs.value("is_active", false).convert<bool>();
            product->createdAt = rs.value("created_at").convert<std::string>();
            product->categoryName = rs.value("category_name").convert<std::string>();
            product->supplierName = rs.value("supplier_name").convert<std::string>();
            
            // Рассчитываем текущий остаток
            calculateCurrentStock(*product);
            
            products.push_back(std::move(product));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in search: " + e.displayText());
    }
    
    return products;
}

std::unique_ptr<models::Product> ProductRepository::findBySku(const std::string& sku)
{
    auto products = findByField("sku", sku);
    if (!products.empty())
    {
        return std::move(products[0]);
    }
    return nullptr;
}

std::vector<std::unique_ptr<models::Product>> ProductRepository::findByCategory(long long categoryId)
{
    std::string categoryIdStr = std::to_string(categoryId);
    return findByField("category_id", categoryIdStr);
}

std::vector<std::unique_ptr<models::Product>> ProductRepository::findBySupplier(long long supplierId)
{
    std::string supplierIdStr = std::to_string(supplierId);
    return findByField("supplier_id", supplierIdStr);
}

std::vector<std::unique_ptr<models::Product>> ProductRepository::findActiveProducts()
{
    auto connection = acquireConnection();
    std::vector<std::unique_ptr<models::Product>> products;
    
    try
    {
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT p.id, p.sku, p.name, p.description, p.category_id, "
                  "p.supplier_id, p.unit_price, p.weight, p.dimensions, "
                  "p.min_stock_level, p.max_stock_level, p.is_active, p.created_at, "
                  "c.name as category_name, s.name as supplier_name "
                  "FROM " << TABLE_NAME << " p "
                  "LEFT JOIN categories c ON c.id = p.category_id "
                  "LEFT JOIN suppliers s ON s.id = p.supplier_id "
                  "WHERE p.is_active = true ORDER BY p.name",
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            auto product = std::make_unique<models::Product>();
            product->id = rs.value("id", 0).convert<long long>();
            product->sku = rs.value("sku").convert<std::string>();
            product->name = rs.value("name").convert<std::string>();
            product->description = rs.value("description").convert<std::string>();
            product->categoryId = rs.value("category_id", 0).convert<long long>();
            product->supplierId = rs.value("supplier_id", 0).convert<long long>();
            product->unitPrice = rs.value("unit_price", 0.0).convert<double>();
            product->weight = rs.value("weight", 0.0).convert<double>();
            product->dimensions = rs.value("dimensions").convert<std::string>();
            product->minStockLevel = rs.value("min_stock_level", 0).convert<int>();
            product->maxStockLevel = rs.value("max_stock_level", 0).convert<int>();
            product->isActive = rs.value("is_active", false).convert<bool>();
            product->createdAt = rs.value("created_at").convert<std::string>();
            product->categoryName = rs.value("category_name").convert<std::string>();
            product->supplierName = rs.value("supplier_name").convert<std::string>();
            
            // Рассчитываем текущий остаток
            calculateCurrentStock(*product);
            
            products.push_back(std::move(product));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findActiveProducts: " + e.displayText());
    }
    
    return products;
}

std::vector<std::unique_ptr<models::Product>> ProductRepository::findInactiveProducts()
{
    auto connection = acquireConnection();
    std::vector<std::unique_ptr<models::Product>> products;
    
    try
    {
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT p.id, p.sku, p.name, p.description, p.category_id, "
                  "p.supplier_id, p.unit_price, p.weight, p.dimensions, "
                  "p.min_stock_level, p.max_stock_level, p.is_active, p.created_at, "
                  "c.name as category_name, s.name as supplier_name "
                  "FROM " << TABLE_NAME << " p "
                  "LEFT JOIN categories c ON c.id = p.category_id "
                  "LEFT JOIN suppliers s ON s.id = p.supplier_id "
                  "WHERE p.is_active = false ORDER BY p.name",
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            auto product = std::make_unique<models::Product>();
            product->id = rs.value("id", 0).convert<long long>();
            product->sku = rs.value("sku").convert<std::string>();
            product->name = rs.value("name").convert<std::string>();
            product->description = rs.value("description").convert<std::string>();
            product->categoryId = rs.value("category_id", 0).convert<long long>();
            product->supplierId = rs.value("supplier_id", 0).convert<long long>();
            product->unitPrice = rs.value("unit_price", 0.0).convert<double>();
            product->weight = rs.value("weight", 0.0).convert<double>();
            product->dimensions = rs.value("dimensions").convert<std::string>();
            product->minStockLevel = rs.value("min_stock_level", 0).convert<int>();
            product->maxStockLevel = rs.value("max_stock_level", 0).convert<int>();
            product->isActive = rs.value("is_active", false).convert<bool>();
            product->createdAt = rs.value("created_at").convert<std::string>();
            product->categoryName = rs.value("category_name").convert<std::string>();
            product->supplierName = rs.value("supplier_name").convert<std::string>();
            
            // Рассчитываем текущий остаток
            calculateCurrentStock(*product);
            
            products.push_back(std::move(product));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findInactiveProducts: " + e.displayText());
    }
    
    return products;
}

std::vector<std::unique_ptr<models::Product>> ProductRepository::findProductsWithLowStock()
{
    auto activeProducts = findActiveProducts();
    std::vector<std::unique_ptr<models::Product>> lowStockProducts;
    
    for (auto& product : activeProducts)
    {
        if (product && product->currentStock <= product->minStockLevel)
        {
            lowStockProducts.push_back(std::move(product));
        }
    }
    
    return lowStockProducts;
}

std::vector<std::unique_ptr<models::Product>> ProductRepository::findProductsWithHighStock()
{
    auto activeProducts = findActiveProducts();
    std::vector<std::unique_ptr<models::Product>> highStockProducts;
    
    for (auto& product : activeProducts)
    {
        if (product && product->currentStock >= product->maxStockLevel * 0.9)
        {
            highStockProducts.push_back(std::move(product));
        }
    }
    
    return highStockProducts;
}

std::vector<std::unique_ptr<models::Product>> ProductRepository::findProductsWithoutStock()
{
    auto activeProducts = findActiveProducts();
    std::vector<std::unique_ptr<models::Product>> noStockProducts;
    
    for (auto& product : activeProducts)
    {
        if (product && product->currentStock == 0)
        {
            noStockProducts.push_back(std::move(product));
        }
    }
    
    return noStockProducts;
}

bool ProductRepository::updateStockLevels(long long id, int minStockLevel, int maxStockLevel)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        Poco::Int64 pocoId = static_cast<Poco::Int64>(id);
        int useMinStockLevel = minStockLevel;
        int useMaxStockLevel = maxStockLevel;
        
        Poco::Data::Statement update(connection->getSession());
        update << "UPDATE " << TABLE_NAME << " SET "
                  "min_stock_level = $1, max_stock_level = $2 WHERE id = $3",
            Poco::Data::Keywords::use(useMinStockLevel),
            Poco::Data::Keywords::use(useMaxStockLevel),
            Poco::Data::Keywords::use(pocoId),
            now;
        
        int rowsAffected = update.execute();
        
        commitTransaction(*connection);
        return rowsAffected > 0;
    }
    catch (const Poco::Exception& e)
    {
        rollbackTransaction(*connection);
        throw std::runtime_error("Database error in updateStockLevels: " + e.displayText());
    }
}

bool ProductRepository::updatePrice(long long id, double newPrice)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        Poco::Int64 pocoId = static_cast<Poco::Int64>(id);
        double useNewPrice = newPrice;
        
        Poco::Data::Statement update(connection->getSession());
        update << "UPDATE " << TABLE_NAME << " SET unit_price = $1 WHERE id = $2",
            Poco::Data::Keywords::use(useNewPrice),
            Poco::Data::Keywords::use(pocoId),
            now;
        
        int rowsAffected = update.execute();
        
        commitTransaction(*connection);
        return rowsAffected > 0;
    }
    catch (const Poco::Exception& e)
    {
        rollbackTransaction(*connection);
        throw std::runtime_error("Database error in updatePrice: " + e.displayText());
    }
}

bool ProductRepository::updateStatus(long long id, bool isActive)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        Poco::Int64 pocoId = static_cast<Poco::Int64>(id);
        bool useIsActive = isActive;
        
        Poco::Data::Statement update(connection->getSession());
        update << "UPDATE " << TABLE_NAME << " SET is_active = $1 WHERE id = $2",
            Poco::Data::Keywords::use(useIsActive),
            Poco::Data::Keywords::use(pocoId),
            now;
        
        int rowsAffected = update.execute();
        
        commitTransaction(*connection);
        return rowsAffected > 0;
    }
    catch (const Poco::Exception& e)
    {
        rollbackTransaction(*connection);
        throw std::runtime_error("Database error in updateStatus: " + e.displayText());
    }
}

bool ProductRepository::updateCategory(long long id, long long newCategoryId)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        Poco::Int64 pocoId = static_cast<Poco::Int64>(id);
        Poco::Int64 pocoNewCategoryId = static_cast<Poco::Int64>(newCategoryId);
        
        Poco::Data::Statement update(connection->getSession());
        update << "UPDATE " << TABLE_NAME << " SET category_id = $1 WHERE id = $2",
            Poco::Data::Keywords::use(pocoNewCategoryId),
            Poco::Data::Keywords::use(pocoId),
            now;
        
        int rowsAffected = update.execute();
        
        commitTransaction(*connection);
        return rowsAffected > 0;
    }
    catch (const Poco::Exception& e)
    {
        rollbackTransaction(*connection);
        throw std::runtime_error("Database error in updateCategory: " + e.displayText());
    }
}

bool ProductRepository::updateSupplier(long long id, long long newSupplierId)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        Poco::Int64 pocoId = static_cast<Poco::Int64>(id);
        Poco::Int64 pocoNewSupplierId = static_cast<Poco::Int64>(newSupplierId);
        
        Poco::Data::Statement update(connection->getSession());
        update << "UPDATE " << TABLE_NAME << " SET supplier_id = $1 WHERE id = $2",
            Poco::Data::Keywords::use(pocoNewSupplierId),
            Poco::Data::Keywords::use(pocoId),
            now;
        
        int rowsAffected = update.execute();
        
        commitTransaction(*connection);
        return rowsAffected > 0;
    }
    catch (const Poco::Exception& e)
    {
        rollbackTransaction(*connection);
        throw std::runtime_error("Database error in updateSupplier: " + e.displayText());
    }
}

int ProductRepository::countActiveProducts()
{
    auto connection = acquireConnection();
    
    try
    {
        Poco::Data::Statement countStmt(connection->getSession());
        countStmt << "SELECT COUNT(*) FROM " << TABLE_NAME << " WHERE is_active = true",
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
        throw std::runtime_error("Database error in countActiveProducts: " + e.displayText());
    }
}

int ProductRepository::countProductsInCategory(long long categoryId)
{
    auto connection = acquireConnection();
    
    try
    {
        Poco::Int64 pocoCategoryId = static_cast<Poco::Int64>(categoryId);
        
        Poco::Data::Statement countStmt(connection->getSession());
        countStmt << "SELECT COUNT(*) FROM " << TABLE_NAME 
                  << " WHERE category_id = $1 AND is_active = true",
            Poco::Data::Keywords::use(pocoCategoryId),
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
        throw std::runtime_error("Database error in countProductsInCategory: " + e.displayText());
    }
}

int ProductRepository::countProductsBySupplier(long long supplierId)
{
    auto connection = acquireConnection();
    
    try
    {
        Poco::Int64 pocoSupplierId = static_cast<Poco::Int64>(supplierId);
        
        Poco::Data::Statement countStmt(connection->getSession());
        countStmt << "SELECT COUNT(*) FROM " << TABLE_NAME 
                  << " WHERE supplier_id = $1 AND is_active = true",
            Poco::Data::Keywords::use(pocoSupplierId),
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
        throw std::runtime_error("Database error in countProductsBySupplier: " + e.displayText());
    }
}

double ProductRepository::getTotalStockValue()
{
    auto connection = acquireConnection();
    
    try
    {
        Poco::Data::Statement sumStmt(connection->getSession());
        sumStmt << "SELECT COALESCE(SUM(pb.quantity_available * pb.unit_cost), 0) as total_value "
                << "FROM product_batches pb "
                << "JOIN products p ON p.id = pb.product_id "
                << "WHERE pb.quality_status = 'approved' AND p.is_active = true",
            now;
        
        Poco::Data::RecordSet rs(sumStmt);
        if (rs.rowCount() > 0)
        {
            return rs.value(0, 0.0).convert<double>();
        }
        
        return 0.0;
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in getTotalStockValue: " + e.displayText());
    }
}

double ProductRepository::getAveragePrice()
{
    auto connection = acquireConnection();
    
    try
    {
        Poco::Data::Statement avgStmt(connection->getSession());
        avgStmt << "SELECT AVG(unit_price) FROM " << TABLE_NAME 
                << " WHERE is_active = true",
            now;
        
        Poco::Data::RecordSet rs(avgStmt);
        if (rs.rowCount() > 0)
        {
            return rs.value(0, 0.0).convert<double>();
        }
        
        return 0.0;
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in getAveragePrice: " + e.displayText());
    }
}

bool ProductRepository::skuExists(const std::string& sku)
{
    auto connection = acquireConnection();
    
    try
    {
        std::string skuCopy = sku;
        Poco::Data::Statement countStmt(connection->getSession());
        countStmt << "SELECT COUNT(*) FROM " << TABLE_NAME << " WHERE sku = $1",
            Poco::Data::Keywords::use(skuCopy),
            now;
        
        Poco::Data::RecordSet rs(countStmt);
        int count = 0;
        if (rs.rowCount() > 0)
        {
            count = rs.value(0, 0).convert<int>();
        }
        
        return count > 0;
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in skuExists: " + e.displayText());
    }
}

Poco::JSON::Array ProductRepository::getProductStatistics()
{
    auto connection = acquireConnection();
    Poco::JSON::Array statsArray;
    
    try
    {
        // Статистика по активности
        Poco::Data::Statement activityStats(connection->getSession());
        activityStats << "SELECT "
                         "COUNT(*) as total_products, "
                         "SUM(CASE WHEN is_active THEN 1 ELSE 0 END) as active_products, "
                         "AVG(unit_price) as avg_price, "
                         "MIN(unit_price) as min_price, "
                         "MAX(unit_price) as max_price "
                         "FROM " << TABLE_NAME,
            now;
        
        Poco::Data::RecordSet rs(activityStats);
        
        if (rs.rowCount() > 0)
        {
            Poco::JSON::Object activityStat;
            activityStat.set("total_products", rs.value("total_products", 0).convert<int>());
            activityStat.set("active_products", rs.value("active_products", 0).convert<int>());
            activityStat.set("avg_price", rs.value("avg_price", 0.0).convert<double>());
            activityStat.set("min_price", rs.value("min_price", 0.0).convert<double>());
            activityStat.set("max_price", rs.value("max_price", 0.0).convert<double>());
            statsArray.add(activityStat);
        }
        
        // Статистика по категориям
        Poco::Data::Statement categoryStats(connection->getSession());
        categoryStats << "SELECT c.name as category_name, COUNT(p.id) as product_count, "
                         "AVG(p.unit_price) as avg_price, "
                         "SUM(CASE WHEN p.is_active THEN 1 ELSE 0 END) as active_products "
                         "FROM categories c "
                         "LEFT JOIN products p ON p.category_id = c.id "
                         "GROUP BY c.id, c.name "
                         "ORDER BY product_count DESC",
            now;
        
        Poco::Data::RecordSet rsCategories(categoryStats);
        
        for (size_t i = 0; i < rsCategories.rowCount(); ++i)
        {
            Poco::JSON::Object categoryStat;
            categoryStat.set("category_name", rsCategories.value("category_name").convert<std::string>());
            categoryStat.set("product_count", rsCategories.value("product_count", 0).convert<int>());
            categoryStat.set("avg_price", rsCategories.value("avg_price", 0.0).convert<double>());
            categoryStat.set("active_products", rsCategories.value("active_products", 0).convert<int>());
            statsArray.add(categoryStat);
        }
        
        // Статистика по поставщикам
        Poco::Data::Statement supplierStats(connection->getSession());
        supplierStats << "SELECT s.name as supplier_name, COUNT(p.id) as product_count, "
                         "AVG(p.unit_price) as avg_price, "
                         "SUM(CASE WHEN p.is_active THEN 1 ELSE 0 END) as active_products "
                         "FROM suppliers s "
                         "LEFT JOIN products p ON p.supplier_id = s.id "
                         "WHERE s.is_active = true "
                         "GROUP BY s.id, s.name "
                         "ORDER BY product_count DESC",
            now;
        
        Poco::Data::RecordSet rsSuppliers(supplierStats);
        
        for (size_t i = 0; i < rsSuppliers.rowCount(); ++i)
        {
            Poco::JSON::Object supplierStat;
            supplierStat.set("supplier_name", rsSuppliers.value("supplier_name").convert<std::string>());
            supplierStat.set("product_count", rsSuppliers.value("product_count", 0).convert<int>());
            supplierStat.set("avg_price", rsSuppliers.value("avg_price", 0.0).convert<double>());
            supplierStat.set("active_products", rsSuppliers.value("active_products", 0).convert<int>());
            statsArray.add(supplierStat);
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in getProductStatistics: " + e.displayText());
    }
    
    return statsArray;
}

Poco::JSON::Array ProductRepository::getStockReport()
{
    auto connection = acquireConnection();
    Poco::JSON::Array reportArray;
    
    try
    {
        Poco::Data::Statement stockReport(connection->getSession());
        stockReport << "SELECT "
                       "p.id, p.sku, p.name, c.name as category_name, "
                       "COALESCE(SUM(pb.quantity_available), 0) as current_stock, "
                       "p.min_stock_level, p.max_stock_level, p.unit_price, "
                       "COALESCE(SUM(pb.quantity_available * pb.unit_cost), 0) as stock_value, "
                       "CASE "
                       "  WHEN COALESCE(SUM(pb.quantity_available), 0) <= p.min_stock_level THEN 'LOW' "
                       "  WHEN COALESCE(SUM(pb.quantity_available), 0) >= p.max_stock_level THEN 'HIGH' "
                       "  ELSE 'NORMAL' "
                       "END as stock_status "
                       "FROM products p "
                       "LEFT JOIN categories c ON c.id = p.category_id "
                       "LEFT JOIN product_batches pb ON pb.product_id = p.id "
                       "WHERE p.is_active = true AND (pb.quality_status = 'approved' OR pb.id IS NULL) "
                       "GROUP BY p.id, p.sku, p.name, c.name, p.min_stock_level, p.max_stock_level, p.unit_price "
                       "ORDER BY stock_status, current_stock ASC",
            now;
        
        Poco::Data::RecordSet rs(stockReport);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            Poco::JSON::Object report;
            report.set("product_id", rs.value("id", 0).convert<long long>());
            report.set("sku", rs.value("sku").convert<std::string>());
            report.set("name", rs.value("name").convert<std::string>());
            report.set("category_name", rs.value("category_name").convert<std::string>());
            report.set("current_stock", rs.value("current_stock", 0).convert<int>());
            report.set("min_stock_level", rs.value("min_stock_level", 0).convert<int>());
            report.set("max_stock_level", rs.value("max_stock_level", 0).convert<int>());
            report.set("unit_price", rs.value("unit_price", 0.0).convert<double>());
            report.set("stock_value", rs.value("stock_value", 0.0).convert<double>());
            report.set("stock_status", rs.value("stock_status").convert<std::string>());
            reportArray.add(report);
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in getStockReport: " + e.displayText());
    }
    
    return reportArray;
}

Poco::JSON::Array ProductRepository::getPriceAnalysisReport()
{
    auto connection = acquireConnection();
    Poco::JSON::Array reportArray;
    
    try
    {
        Poco::Data::Statement priceReport(connection->getSession());
        priceReport << "SELECT "
                       "p.id, p.sku, p.name, c.name as category_name, "
                       "s.name as supplier_name, p.unit_price, "
                       "COALESCE(AVG(pb.unit_cost), 0) as avg_cost, "
                       "p.unit_price - COALESCE(AVG(pb.unit_cost), 0) as margin, "
                       "CASE WHEN COALESCE(AVG(pb.unit_cost), 0) > 0 THEN "
                       "  ((p.unit_price - COALESCE(AVG(pb.unit_cost), 0)) / COALESCE(AVG(pb.unit_cost), 0)) * 100 "
                       "  ELSE 0 "
                       "END as margin_percent "
                       "FROM products p "
                       "LEFT JOIN categories c ON c.id = p.category_id "
                       "LEFT JOIN suppliers s ON s.id = p.supplier_id "
                       "LEFT JOIN product_batches pb ON pb.product_id = p.id "
                       "WHERE p.is_active = true AND (pb.quality_status = 'approved' OR pb.id IS NULL) "
                       "GROUP BY p.id, p.sku, p.name, c.name, s.name, p.unit_price "
                       "ORDER BY margin_percent DESC",
            now;
        
        Poco::Data::RecordSet rs(priceReport);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            Poco::JSON::Object report;
            report.set("product_id", rs.value("id", 0).convert<long long>());
            report.set("sku", rs.value("sku").convert<std::string>());
            report.set("name", rs.value("name").convert<std::string>());
            report.set("category_name", rs.value("category_name").convert<std::string>());
            report.set("supplier_name", rs.value("supplier_name").convert<std::string>());
            report.set("unit_price", rs.value("unit_price", 0.0).convert<double>());
            report.set("avg_cost", rs.value("avg_cost", 0.0).convert<double>());
            report.set("margin", rs.value("margin", 0.0).convert<double>());
            report.set("margin_percent", rs.value("margin_percent", 0.0).convert<double>());
            reportArray.add(report);
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in getPriceAnalysisReport: " + e.displayText());
    }
    
    return reportArray;
}

std::vector<std::pair<long long, std::string>> ProductRepository::getProductNames()
{
    auto connection = acquireConnection();
    std::vector<std::pair<long long, std::string>> productNames;
    
    try
    {
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT id, name FROM " << TABLE_NAME 
               << " WHERE is_active = true ORDER BY name",
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            long long id = rs.value("id", 0).convert<long long>();
            std::string name = rs.value("name").convert<std::string>();
            productNames.emplace_back(id, name);
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in getProductNames: " + e.displayText());
    }
    
    return productNames;
}

models::Product ProductRepository::mapRowToProduct(Poco::Data::Row& row) const
{
    models::Product product;
    product.id = row.get(0).convert<long long>();
    product.sku = row.get(1).convert<std::string>();
    product.name = row.get(2).convert<std::string>();
    product.description = row.get(3).convert<std::string>();
    product.categoryId = row.get(4).convert<long long>();
    product.supplierId = row.get(5).convert<long long>();
    product.unitPrice = row.get(6).convert<double>();
    product.weight = row.get(7).convert<double>();
    product.dimensions = row.get(8).convert<std::string>();
    product.minStockLevel = row.get(9).convert<int>();
    product.maxStockLevel = row.get(10).convert<int>();
    product.isActive = row.get(11).convert<bool>();
    product.createdAt = row.get(12).convert<std::string>();
    product.categoryName = row.get(13).convert<std::string>();
    product.supplierName = row.get(14).convert<std::string>();
    return product;
}

void ProductRepository::calculateCurrentStock(models::Product& product)
{
    auto connection = acquireConnection();
    
    try
    {
        Poco::Int64 pocoId = static_cast<Poco::Int64>(product.id);
        Poco::Data::Statement stockStmt(connection->getSession());
        stockStmt << "SELECT COALESCE(SUM(quantity_available), 0) as total_stock "
                  << "FROM product_batches "
                  << "WHERE product_id = $1 AND quality_status = 'approved'",
            Poco::Data::Keywords::use(pocoId),
            now;
        
        int stock = 0;
        Poco::Data::RecordSet rs(stockStmt);
        if (rs.rowCount() > 0)
        {
            stock = rs.value(0, 0).convert<int>();
        }
        
        product.currentStock = stock;
    }
    catch (const Poco::Exception& e)
    {
        // Если произошла ошибка, устанавливаем stock в 0
        product.currentStock = 0;
    }
}

} // namespace database::repositories
