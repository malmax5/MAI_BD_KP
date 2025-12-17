#include "CategoryRepository.hpp"
#include "../models/Category.hpp"
#include <Poco/Data/Session.h>
#include <Poco/Data/RecordSet.h>
#include <Poco/Data/Statement.h>
#include <Poco/Data/Row.h>
#include <Poco/DateTime.h>
#include <Poco/DateTimeFormat.h>
#include <Poco/DateTimeFormatter.h>
#include <Poco/DateTimeParser.h>
#include <Poco/Dynamic/Var.h>
#include "../../utils/DateUtils.hpp"
#include "../../utils/JsonUtils.hpp"

#include <stack>

using namespace Poco::Data;
using namespace Poco::Data::Keywords;
using namespace Poco;

using namespace warehouse_backend::utils;

namespace warehouse_backend::database::repositories
{

const std::string CategoryRepository::TABLE_NAME = "categories";
const std::vector<std::string> CategoryRepository::SEARCH_FIELDS = {
    "name", "description"
};

CategoryRepository::CategoryRepository() : BaseRepository<models::Category>()
{

}

std::unique_ptr<models::Category> CategoryRepository::findById(long long id)
{
    auto connection = acquireConnection();
    
    try
    {
        Poco::Int64 pocoId = static_cast<Poco::Int64>(id);
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT id, name, description, parent_id, path, sort_order, created_at "
                  "FROM " << TABLE_NAME << " WHERE id = $1",
            Poco::Data::Keywords::use(pocoId),
            now;
        
        Poco::Data::RecordSet rs(select);
        
        if (rs.rowCount() > 0)
        {
            auto category = std::make_unique<models::Category>();
            category->id = rs.value("id").isEmpty() ? 0 : rs.value("id").convert<long long>();
            category->name = rs.value("name").isEmpty() ? "" : rs.value("name").convert<std::string>();
            category->description = rs.value("description").isEmpty() ? "" : rs.value("description").convert<std::string>();
            category->parentId = getParentIdFromVar(rs.value("parent_id"));
            category->path = rs.value("path").isEmpty() ? "" : rs.value("path").convert<std::string>();
            category->sortOrder = rs.value("sort_order").isEmpty() ? 0 : rs.value("sort_order").convert<int>();
            category->createdAt = rs.value("created_at").isEmpty() ? "" : rs.value("created_at").convert<std::string>();
            
            if (category->parentId.value() > 0)
            {
                Poco::Data::Statement parentSelect(connection->getSession());
                Poco::Int64 parentIdCopy = category->parentId.value();
                parentSelect << "SELECT name FROM " << TABLE_NAME << " WHERE id = $1",
                    Poco::Data::Keywords::use(parentIdCopy),
                    now;
                
                Poco::Data::RecordSet parentRs(parentSelect);
                if (parentRs.rowCount() > 0)
                {
                    category->parentName = parentRs.value("name").isEmpty() ? "" : parentRs.value("name").convert<std::string>();
                }
            }
            
            return category;
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findById: " + e.displayText());
    }
    
    return nullptr;
}

std::vector<std::unique_ptr<models::Category>> CategoryRepository::findAll()
{
    std::vector<std::unique_ptr<models::Category>> categories;
    auto connection = acquireConnection();
    
    try
    {
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT id, name, description, parent_id, path, sort_order, created_at "
                  "FROM " << TABLE_NAME << " ORDER BY path, sort_order",
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            Poco::Data::Row row = rs.row(i);
        
            auto category = std::make_unique<models::Category>();
        
            category->id = row["id"].isEmpty() ? 0 : row["id"].convert<Poco::Int64>();
            category->name = row["name"].isEmpty() ? "" : row["name"].convert<std::string>();
            category->sortOrder = row["sort_order"].isEmpty() ? 0 : row["sort_order"].convert<int>();
        
            if (!row["description"].isEmpty())
                category->description = row["description"].convert<std::string>();
            else
                category->description.clear();
        
            if (!row["parent_id"].isEmpty())
                category->parentId = row["parent_id"].convert<Poco::Int64>();
            else
                category->parentId.clear();
        
            if (!row["path"].isEmpty())
                category->path = row["path"].convert<std::string>();
            else
                category->path.clear();
        
            if (!row["created_at"].isEmpty())
                category->createdAt = row["created_at"].convert<std::string>();
            else
                category->createdAt.clear();
        
            categories.push_back(std::move(category));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findAll: " + e.displayText());
    }
    
    return categories;
}

std::vector<std::unique_ptr<models::Category>> CategoryRepository::findPaginated(int page, int pageSize)
{
    std::vector<std::unique_ptr<models::Category>> categories;
    auto connection = acquireConnection();
    
    if (page < 1) page = 1;
    int offset = (page - 1) * pageSize;
    
    try
    {
        int usePageSize = pageSize;
        int useOffset = offset;
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT id, name, description, parent_id, path, sort_order, created_at "
                  "FROM " << TABLE_NAME << " ORDER BY path, sort_order LIMIT $1 OFFSET $2",
            Poco::Data::Keywords::use(usePageSize),
            Poco::Data::Keywords::use(useOffset),
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            Poco::Data::Row row = rs.row(i);

            auto category = std::make_unique<models::Category>();

            category->id = row["id"].isEmpty() ? 0 : row["id"].convert<Poco::Int64>();
            category->name = row["name"].isEmpty() ? "" : row["name"].convert<std::string>();
            category->sortOrder = row["sort_order"].isEmpty() ? 0 : row["sort_order"].convert<int>();

            if (!row["description"].isEmpty())
                category->description = row["description"].convert<std::string>();
            else
                category->description.clear();

            if (!row["parent_id"].isEmpty())
                category->parentId = row["parent_id"].convert<Poco::Int64>();
            else
                category->parentId.clear();

            if (!row["path"].isEmpty())
                category->path = row["path"].convert<std::string>();
            else
                category->path.clear();

            if (!row["created_at"].isEmpty())
                category->createdAt = row["created_at"].convert<std::string>();
            else
                category->createdAt.clear();

            categories.push_back(std::move(category));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findPaginated: " + e.displayText());
    }
    
    return categories;
}

long long CategoryRepository::create(const models::Category& category)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        models::Category categoryCopy = category;
        
        Poco::Data::Statement insert(connection->getSession());
        Poco::Int64 newId = 0;
        
        if (!categoryCopy.parentId.isNull())
        {
            insert << "INSERT INTO " << TABLE_NAME << " "
                      "(name, description, parent_id, path, sort_order, created_at) "
                      "VALUES ($1, $2, $3, $4, $5, $6) "
                      "RETURNING id",
                Poco::Data::Keywords::use(categoryCopy.name),
                Poco::Data::Keywords::use(categoryCopy.description),
                Poco::Data::Keywords::use(categoryCopy.parentId),
                Poco::Data::Keywords::use(categoryCopy.path),
                Poco::Data::Keywords::use(categoryCopy.sortOrder),
                Poco::Data::Keywords::use(categoryCopy.createdAt),
                Poco::Data::Keywords::into(newId),
                now;
        }
        else
        {
            insert << "INSERT INTO " << TABLE_NAME << " "
                      "(name, description, parent_id, path, sort_order, created_at) "
                      "VALUES ($1, $2, NULL, $3, $4, $5) "
                      "RETURNING id",
                Poco::Data::Keywords::use(categoryCopy.name),
                Poco::Data::Keywords::use(categoryCopy.description),
                Poco::Data::Keywords::use(categoryCopy.path),
                Poco::Data::Keywords::use(categoryCopy.sortOrder),
                Poco::Data::Keywords::use(categoryCopy.createdAt),
                Poco::Data::Keywords::into(newId),
                now;
        }
        
        commitTransaction(*connection);
        return static_cast<long long>(newId);
    }
    catch (const Poco::Exception& e)
    {
        rollbackTransaction(*connection);
        throw std::runtime_error("Database error in create: " + e.displayText());
    }
}

bool CategoryRepository::update(long long id, const models::Category& category)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);

        models::Category categoryCopy = category;
        
        long long idCopy = id;
        
        Poco::Data::Statement update(connection->getSession());
        if (!categoryCopy.parentId.isNull() && categoryCopy.parentId.value() > 0)
        {
            update << "UPDATE " << TABLE_NAME << " SET "
                      "name = $1, description = $2, parent_id = $3, sort_order = $4 WHERE id = $5",
                Poco::Data::Keywords::use(categoryCopy.name),
                Poco::Data::Keywords::use(categoryCopy.description),
                Poco::Data::Keywords::use(categoryCopy.parentId),
                Poco::Data::Keywords::use(categoryCopy.sortOrder),
                Poco::Data::Keywords::use(id);
        }
        else
        {
            update << "UPDATE " << TABLE_NAME << " SET "
                      "name = $1, description = $2, parent_id = NULL, sort_order = $3 WHERE id = $4",
                Poco::Data::Keywords::use(categoryCopy.name),
                Poco::Data::Keywords::use(categoryCopy.description),
                Poco::Data::Keywords::use(categoryCopy.sortOrder),
                Poco::Data::Keywords::use(id);
        }
        
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

bool CategoryRepository::remove(long long id)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        long long idCopy = id;
        
        std::vector<long long> allIdsToDelete;
        std::stack<long long> stack;
        stack.push(id);
        
        while (!stack.empty())
        {
            long long currentId = stack.top();
            stack.pop();
            
            if (std::find(allIdsToDelete.begin(), allIdsToDelete.end(), currentId) != allIdsToDelete.end())
            {
                continue;
            }
            
            allIdsToDelete.push_back(currentId);
            
            Poco::Data::Statement getChildren(connection->getSession());
            getChildren << "SELECT id FROM " << TABLE_NAME << " WHERE parent_id = $1",
                Poco::Data::Keywords::use(currentId),
                now;
            
            Poco::Data::RecordSet rsChildren(getChildren);
            for (size_t i = 0; i < rsChildren.rowCount(); ++i)
            {
                long long childId = rsChildren.value("id").isEmpty() ? 0 : rsChildren.value("id").convert<long long>();
                stack.push(childId);
            }
        }
        
        for (long long categoryId : allIdsToDelete)
        {
            Poco::Data::Statement checkProducts(connection->getSession());
            checkProducts << "SELECT COUNT(*) FROM products WHERE category_id = $1",
                Poco::Data::Keywords::use(categoryId),
                now;
            
            Poco::Data::RecordSet rsProducts(checkProducts);
            if (rsProducts.rowCount() > 0) {
                int productCount = rsProducts.value(0).isEmpty() ? 0 : rsProducts.value(0).convert<int>();
                if (productCount > 0) {
                    throw std::runtime_error("Cannot delete category with associated products");
                }
            }
        }
        
        if (!allIdsToDelete.empty())
        {
            std::ostringstream oss;
            oss << "DELETE FROM " << TABLE_NAME << " WHERE id IN (";
            for (size_t i = 0; i < allIdsToDelete.size(); ++i)
            {
                if (i > 0) oss << ", ";
                oss << allIdsToDelete[i];
            }
            oss << ")";
            
            Poco::Data::Statement del(connection->getSession());
            del << oss.str();
            
            int rowsAffected = del.execute();
        }
        
        commitTransaction(*connection);
        return true;
    }
    catch (const Poco::Exception& e)
    {
        rollbackTransaction(*connection);
        throw std::runtime_error("Database error in remove: " + e.displayText());
    }
    catch (const std::exception& e)
    {
        rollbackTransaction(*connection);
        throw std::runtime_error(e.what());
    }
}

bool CategoryRepository::softDelete(long long id)
{
    throw std::runtime_error("Soft delete not supported for categories");
}

int CategoryRepository::count()
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
            return rs.value(0).isEmpty() ? 0 : rs.value(0).convert<int>();
        }
        
        return 0;
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in count: " + e.displayText());
    }
}

Poco::JSON::Array CategoryRepository::findAllAsJson()
{
    auto categories = findAll();
    Poco::JSON::Array jsonArray;
    
    for (const auto& category : categories)
    {
        if (category)
        {
            jsonArray.add(category->toJson());
        }
    }
    
    return jsonArray;
}

Poco::JSON::Object CategoryRepository::findByIdAsJson(long long id)
{
    auto category = findById(id);
    if (category)
    {
        return category->toJson();
    }
    
    return Poco::JSON::Object();
}

std::vector<std::unique_ptr<models::Category>> CategoryRepository::findByField(
    const std::string& fieldName, const std::string& fieldValue)
{
    std::vector<std::unique_ptr<models::Category>> categories;
    auto connection = acquireConnection();
    
    try
    {
        std::string sql = "SELECT id, name, description, parent_id, path, sort_order, created_at "
                          "FROM " + TABLE_NAME + " WHERE " + fieldName + " = $1 "
                          "ORDER BY path, sort_order";
        
        std::string fieldValueCopy = fieldValue;
        Poco::Data::Statement select(connection->getSession());
        select << sql,
            Poco::Data::Keywords::use(fieldValueCopy),
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            Poco::Data::Row row = rs.row(i);
        
            auto category = std::make_unique<models::Category>();
        
            category->id = row["id"].isEmpty() ? 0 : row["id"].convert<Poco::Int64>();
            category->name = row["name"].isEmpty() ? "" : row["name"].convert<std::string>();
            category->sortOrder = row["sort_order"].isEmpty() ? 0 : row["sort_order"].convert<int>();
        
            if (!row["description"].isEmpty())
                category->description = row["description"].convert<std::string>();
            else
                category->description.clear();
        
            if (!row["parent_id"].isEmpty())
                category->parentId = row["parent_id"].convert<Poco::Int64>();
            else
                category->parentId.clear();
        
            if (!row["path"].isEmpty())
                category->path = row["path"].convert<std::string>();
            else
                category->path.clear();
        
            if (!row["created_at"].isEmpty())
                category->createdAt = row["created_at"].convert<std::string>();
            else
                category->createdAt.clear();
        
            categories.push_back(std::move(category));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findByField: " + e.displayText());
    }
    
    return categories;
}

std::vector<std::unique_ptr<models::Category>> CategoryRepository::search(
    const std::string& query, const std::vector<std::string>& fields)
{
    std::vector<std::unique_ptr<models::Category>> categories;
    auto connection = acquireConnection();
    
    try
    {
        std::string searchClause = buildSearchQuery(query, fields);
        std::string sql = "SELECT id, name, description, parent_id, path, sort_order, created_at "
                          "FROM " + TABLE_NAME + " WHERE " + searchClause + " "
                          "ORDER BY path, sort_order";
        
        Poco::Data::Statement select(connection->getSession());
        select << sql,
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            Poco::Data::Row row = rs.row(i);
        
            auto category = std::make_unique<models::Category>();
        
            category->id = row["id"].isEmpty() ? 0 : row["id"].convert<Poco::Int64>();
            category->name = row["name"].isEmpty() ? "" : row["name"].convert<std::string>();
            category->sortOrder = row["sort_order"].isEmpty() ? 0 : row["sort_order"].convert<int>();
        
            if (!row["description"].isEmpty())
                category->description = row["description"].convert<std::string>();
            else
                category->description.clear();
        
            if (!row["parent_id"].isEmpty())
                category->parentId = row["parent_id"].convert<Poco::Int64>();
            else
                category->parentId.clear();
        
            if (!row["path"].isEmpty())
                category->path = row["path"].convert<std::string>();
            else
                category->path.clear();
        
            if (!row["created_at"].isEmpty())
                category->createdAt = row["created_at"].convert<std::string>();
            else
                category->createdAt.clear();
        
            categories.push_back(std::move(category));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in search: " + e.displayText());
    }
    
    return categories;
}

std::vector<std::unique_ptr<models::Category>> CategoryRepository::findRootCategories()
{
    auto connection = acquireConnection();
    std::vector<std::unique_ptr<models::Category>> categories;
    
    try
    {
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT id, name, description, parent_id, path, sort_order, created_at "
                  "FROM " << TABLE_NAME << " WHERE parent_id IS NULL OR parent_id = 0 "
                  "ORDER BY sort_order, name",
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            Poco::Data::Row row = rs.row(i);
        
            auto category = std::make_unique<models::Category>();
        
            category->id = row["id"].isEmpty() ? 0 : row["id"].convert<Poco::Int64>();
            category->name = row["name"].isEmpty() ? "" : row["name"].convert<std::string>();
            category->sortOrder = row["sort_order"].isEmpty() ? 0 : row["sort_order"].convert<int>();
        
            if (!row["description"].isEmpty())
                category->description = row["description"].convert<std::string>();
            else
                category->description.clear();
        
            if (!row["parent_id"].isEmpty())
                category->parentId = row["parent_id"].convert<Poco::Int64>();
            else
                category->parentId.clear();
        
            if (!row["path"].isEmpty())
                category->path = row["path"].convert<std::string>();
            else
                category->path.clear();
        
            if (!row["created_at"].isEmpty())
                category->createdAt = row["created_at"].convert<std::string>();
            else
                category->createdAt.clear();
        
            categories.push_back(std::move(category));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findRootCategories: " + e.displayText());
    }
    
    return categories;
}

std::vector<std::unique_ptr<models::Category>> CategoryRepository::findChildCategories(long long parentId)
{
    auto connection = acquireConnection();
    std::vector<std::unique_ptr<models::Category>> categories;
    
    try
    {
        long long parentIdCopy = parentId;
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT id, name, description, parent_id, path, sort_order, created_at "
                  "FROM " << TABLE_NAME << " WHERE parent_id = $1 "
                  "ORDER BY sort_order, name",
            Poco::Data::Keywords::use(parentIdCopy),
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            Poco::Data::Row row = rs.row(i);
        
            auto category = std::make_unique<models::Category>();
        
            category->id = row["id"].isEmpty() ? 0 : row["id"].convert<Poco::Int64>();
            category->name = row["name"].isEmpty() ? "" : row["name"].convert<std::string>();
            category->sortOrder = row["sort_order"].isEmpty() ? 0 : row["sort_order"].convert<int>();
        
            if (!row["description"].isEmpty())
                category->description = row["description"].convert<std::string>();
            else
                category->description.clear();
        
            if (!row["parent_id"].isEmpty())
                category->parentId = row["parent_id"].convert<Poco::Int64>();
            else
                category->parentId.clear();
        
            if (!row["path"].isEmpty())
                category->path = row["path"].convert<std::string>();
            else
                category->path.clear();
        
            if (!row["created_at"].isEmpty())
                category->createdAt = row["created_at"].convert<std::string>();
            else
                category->createdAt.clear();
        
            categories.push_back(std::move(category));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findChildCategories: " + e.displayText());
    }
    
    return categories;
}

std::vector<std::unique_ptr<models::Category>> CategoryRepository::findCategoriesWithProducts()
{
    auto connection = acquireConnection();
    std::vector<std::unique_ptr<models::Category>> categories;
    
    try
    {
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT DISTINCT c.id, c.name, c.description, c.parent_id, "
                  "c.path, c.sort_order, c.created_at "
                  "FROM " << TABLE_NAME << " c "
                  "JOIN products p ON p.category_id = c.id "
                  "WHERE p.is_active = true "
                  "ORDER BY c.path, c.sort_order",
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            Poco::Data::Row row = rs.row(i);
        
            auto category = std::make_unique<models::Category>();
        
            category->id = row["id"].isEmpty() ? 0 : row["id"].convert<Poco::Int64>();
            category->name = row["name"].isEmpty() ? "" : row["name"].convert<std::string>();
            category->sortOrder = row["sort_order"].isEmpty() ? 0 : row["sort_order"].convert<int>();
        
            if (!row["description"].isEmpty())
                category->description = row["description"].convert<std::string>();
            else
                category->description.clear();
        
            if (!row["parent_id"].isEmpty())
                category->parentId = row["parent_id"].convert<Poco::Int64>();
            else
                category->parentId.clear();
        
            if (!row["path"].isEmpty())
                category->path = row["path"].convert<std::string>();
            else
                category->path.clear();
        
            if (!row["created_at"].isEmpty())
                category->createdAt = row["created_at"].convert<std::string>();
            else
                category->createdAt.clear();
        
            categories.push_back(std::move(category));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findCategoriesWithProducts: " + e.displayText());
    }
    
    return categories;
}

std::vector<std::unique_ptr<models::Category>> CategoryRepository::findLeafCategories()
{
    auto connection = acquireConnection();
    std::vector<std::unique_ptr<models::Category>> categories;
    
    try
    {
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT c.id, c.name, c.description, c.parent_id, "
                  "c.path, c.sort_order, c.created_at "
                  "FROM " << TABLE_NAME << " c "
                  "LEFT JOIN " << TABLE_NAME << " children ON children.parent_id = c.id "
                  "WHERE children.id IS NULL "
                  "ORDER BY c.path, c.sort_order",
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            Poco::Data::Row row = rs.row(i);
        
            auto category = std::make_unique<models::Category>();
        
            category->id = row["id"].isEmpty() ? 0 : row["id"].convert<Poco::Int64>();
            category->name = row["name"].isEmpty() ? "" : row["name"].convert<std::string>();
            category->sortOrder = row["sort_order"].isEmpty() ? 0 : row["sort_order"].convert<int>();
        
            if (!row["description"].isEmpty())
                category->description = row["description"].convert<std::string>();
            else
                category->description.clear();
        
            if (!row["parent_id"].isEmpty())
                category->parentId = row["parent_id"].convert<Poco::Int64>();
            else
                category->parentId.clear();
        
            if (!row["path"].isEmpty())
                category->path = row["path"].convert<std::string>();
            else
                category->path.clear();
        
            if (!row["created_at"].isEmpty())
                category->createdAt = row["created_at"].convert<std::string>();
            else
                category->createdAt.clear();
        
            categories.push_back(std::move(category));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findLeafCategories: " + e.displayText());
    }
    
    return categories;
}

bool CategoryRepository::updateParent(long long id, long long newParentId)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        long long newParentIdCopy = newParentId;
        long long idCopy = id;
        
        if (newParentIdCopy > 0)
        {
            Poco::Data::Statement check(connection->getSession());
            check << "SELECT COUNT(*) FROM " << TABLE_NAME << " WHERE id = $1",
                Poco::Data::Keywords::use(newParentIdCopy),
                now;
            
            Poco::Data::RecordSet rs(check);
            int count = 0;
            if (rs.rowCount() > 0)
            {
                count = rs.value(0).isEmpty() ? 0 : rs.value(0).convert<int>();
            }
            
            if (count == 0)
            {
                throw std::runtime_error("Parent category does not exist");
            }
        }
        
        Poco::Data::Statement update(connection->getSession());
        update << "UPDATE " << TABLE_NAME << " SET parent_id = $1 WHERE id = $2",
            Poco::Data::Keywords::use(newParentIdCopy),
            Poco::Data::Keywords::use(idCopy),
            now;
        
        int rowsAffected = update.execute();
        
        commitTransaction(*connection);
        return rowsAffected > 0;
    }
    catch (const Poco::Exception& e)
    {
        rollbackTransaction(*connection);
        throw std::runtime_error("Database error in updateParent: " + e.displayText());
    }
}

bool CategoryRepository::updateSortOrder(long long id, int sortOrder)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        long long idCopy = id;
        int sortOrderCopy = sortOrder;
        
        Poco::Data::Statement update(connection->getSession());
        update << "UPDATE " << TABLE_NAME << " SET sort_order = $1 WHERE id = $2",
            Poco::Data::Keywords::use(sortOrderCopy),
            Poco::Data::Keywords::use(idCopy),
            now;
        
        int rowsAffected = update.execute();
        
        commitTransaction(*connection);
        return rowsAffected > 0;
    }
    catch (const Poco::Exception& e)
    {
        rollbackTransaction(*connection);
        throw std::runtime_error("Database error in updateSortOrder: " + e.displayText());
    }
}

std::unique_ptr<models::Category> CategoryRepository::findCategoryWithChildren(long long id)
{
    auto category = findById(id);
    if (!category)
    {
        return nullptr;
    }
    
    auto children = findChildCategories(id);
    for (auto& child : children)
    {
        category->addChild(std::move(child));
    }
    
    return category;
}

Poco::JSON::Object CategoryRepository::findCategoryTree()
{
    auto rootCategories = findRootCategories();
    auto allCategories = findAll();
    
    std::vector<models::Category> flatCategories;
    for (auto& cat : allCategories)
    {
        if (cat)
        {
            flatCategories.push_back(*cat);
        }
    }
    
    Poco::JSON::Object tree;
    Poco::JSON::Array rootsArray;
    
    for (auto& rootCat : rootCategories)
    {
        if (rootCat)
        {
            buildCategoryTree(*rootCat, flatCategories);
            rootsArray.add(buildJsonCategoryTree(*rootCat));
        }
    }
    
    tree.set("categories", rootsArray);
    tree.set("total_categories", static_cast<int>(flatCategories.size()));
    tree.set("root_categories", static_cast<int>(rootCategories.size()));
    
    return tree;
}

int CategoryRepository::countRootCategories()
{
    auto connection = acquireConnection();
    
    try
    {
        Poco::Data::Statement countStmt(connection->getSession());
        countStmt << "SELECT COUNT(*) FROM " << TABLE_NAME 
                  << " WHERE parent_id IS NULL OR parent_id = 0",
            now;
        
        Poco::Data::RecordSet rs(countStmt);
        if (rs.rowCount() > 0)
        {
            return rs.value(0).isEmpty() ? 0 : rs.value(0).convert<int>();
        }
        
        return 0;
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in countRootCategories: " + e.displayText());
    }
}

int CategoryRepository::countProductsInCategory(long long categoryId)
{
    auto connection = acquireConnection();
    
    try
    {
        auto categoryIds = getCategoryAndSubcategoryIds(categoryId);
        
        if (categoryIds.empty())
        {
            return 0;
        }
        
        std::string inClause = "(";
        for (size_t i = 0; i < categoryIds.size(); ++i)
        {
            if (i > 0) inClause += ", ";
            inClause += std::to_string(categoryIds[i]);
        }
        inClause += ")";
        
        Poco::Data::Statement countStmt(connection->getSession());
        countStmt << "SELECT COUNT(*) FROM products WHERE category_id IN " + inClause,
            now;
        
        Poco::Data::RecordSet rs(countStmt);
        if (rs.rowCount() > 0)
        {
            return rs.value(0).isEmpty() ? 0 : rs.value(0).convert<int>();
        }
        
        return 0;
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in countProductsInCategory: " + e.displayText());
    }
}

bool CategoryRepository::hasProducts(long long categoryId)
{
    return countProductsInCategory(categoryId) > 0;
}

bool CategoryRepository::hasChildren(long long categoryId)
{
    auto children = findChildCategories(categoryId);
    return !children.empty();
}

std::vector<long long> CategoryRepository::getCategoryAndSubcategoryIds(long long categoryId)
{
    std::vector<long long> categoryIds;
    std::set<long long> visited;
    std::stack<long long> stack;
    
    stack.push(categoryId);
    
    while (!stack.empty())
    {
        long long currentId = stack.top();
        stack.pop();
        
        if (visited.find(currentId) != visited.end())
        {
            std::cerr << "Warning: Cycle detected in category hierarchy at category ID: " 
                      << currentId << std::endl;
            continue;
        }
        
        visited.insert(currentId);
        categoryIds.push_back(currentId);
        
        auto children = findChildCategories(currentId);
        for (const auto& child : children)
        {
            if (child && visited.find(child->id) == visited.end())
            {
                stack.push(child->id);
            }
        }
    }
    
    return categoryIds;
}

long long CategoryRepository::getParentIdFromVar(const Poco::Dynamic::Var& var) const
{
    if (var.isEmpty())
    {
        return 0;
    }

    return var.convert<long long>();
}

models::Category CategoryRepository::mapRowToCategory(Poco::Data::Row& row) const
{
    models::Category category;
    category.id = row.get(0).isEmpty() ? 0 : row.get(0).convert<long long>();
    category.name = row.get(1).isEmpty() ? "" : row.get(1).convert<std::string>();
    category.description = row.get(2).isEmpty() ? "" : row.get(2).convert<std::string>();
    category.parentId = row.get(3).isEmpty() ? 0 : row.get(3).convert<long long>();
    category.path = row.get(4).isEmpty() ? "" : row.get(4).convert<std::string>();
    category.sortOrder = row.get(5).isEmpty() ? 0 : row.get(5).convert<int>();
    category.createdAt = row.get(6).isEmpty() ? "" : row.get(6).convert<std::string>();
    return category;
}

void CategoryRepository::buildCategoryTree(models::Category& parent, 
                                          const std::vector<models::Category>& allCategories)
{
    for (const auto& cat : allCategories)
    {
        if (cat.parentId == parent.id)
        {
            auto child = std::make_shared<models::Category>(cat);
            buildCategoryTree(*child, allCategories);
            parent.addChild(child);
        }
    }
}

Poco::JSON::Object CategoryRepository::buildJsonCategoryTree(const models::Category& category)
{
    Poco::JSON::Object jsonCat = category.toJson();
    
    if (!category.children.empty())
    {
        Poco::JSON::Array childrenArray;
        for (const auto& child : category.children)
        {
            childrenArray.add(buildJsonCategoryTree(*child));
        }
        jsonCat.set("children", childrenArray);
    }
    
    return jsonCat;
}

} // namespace database::repositories
