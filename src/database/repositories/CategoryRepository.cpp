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
                  "FROM " << TABLE_NAME << " WHERE id = ?",
            Poco::Data::Keywords::use(pocoId),
            now;
        
        Poco::Data::RecordSet rs(select);
        
        if (rs.rowCount() > 0)
        {
            auto category = std::make_unique<models::Category>();
            category->id = rs.value("id", 0).convert<long long>();
            category->name = rs.value("name").convert<std::string>();
            category->description = rs.value("description").convert<std::string>();
            category->parentId = rs.value("parent_id", 0).convert<long long>();
            category->path = rs.value("path").convert<std::string>();
            category->sortOrder = rs.value("sort_order", 0).convert<int>();
            category->createdAt = rs.value("created_at").convert<std::string>();
            
            if (category->parentId > 0)
            {
                Poco::Data::Statement parentSelect(connection->getSession());
                Poco::Int64 parentIdCopy = category->parentId;  // Неконстантная копия
                parentSelect << "SELECT name FROM " << TABLE_NAME << " WHERE id = ?",
                    Poco::Data::Keywords::use(parentIdCopy),
                    now;
                
                Poco::Data::RecordSet parentRs(parentSelect);
                if (parentRs.rowCount() > 0)
                {
                    category->parentName = parentRs.value("name").convert<std::string>();
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
            auto category = std::make_unique<models::Category>();
            category->id = rs.value("id", 0).convert<long long>();
            category->name = rs.value("name").convert<std::string>();
            category->description = rs.value("description").convert<std::string>();
            category->parentId = rs.value("parent_id", 0).convert<long long>();
            category->path = rs.value("path").convert<std::string>();
            category->sortOrder = rs.value("sort_order", 0).convert<int>();
            category->createdAt = rs.value("created_at").convert<std::string>();
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
                  "FROM " << TABLE_NAME << " ORDER BY path, sort_order LIMIT ? OFFSET ?",
            Poco::Data::Keywords::use(usePageSize),
            Poco::Data::Keywords::use(useOffset),
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            auto category = std::make_unique<models::Category>();
            category->id = rs.value("id", 0).convert<long long>();
            category->name = rs.value("name").convert<std::string>();
            category->description = rs.value("description").convert<std::string>();
            category->parentId = rs.value("parent_id", 0).convert<long long>();
            category->path = rs.value("path").convert<std::string>();
            category->sortOrder = rs.value("sort_order", 0).convert<int>();
            category->createdAt = rs.value("created_at").convert<std::string>();
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
        
        std::string createdAt = category.createdAt.empty() ? 
            DateUtils::formatDateTime(DateUtils::now()) : category.createdAt;
        
        // Создаем неконстантные копии всех параметров
        std::string nameCopy = category.name;
        std::string descriptionCopy = category.description;
        long long parentIdCopy = category.parentId;
        int sortOrderCopy = category.sortOrder;
        std::string createdAtCopy = createdAt;
        
        Poco::Data::Statement insert(connection->getSession());
        Poco::Int64 newId = 0;
        
        insert << "INSERT INTO " << TABLE_NAME << " "
                  "(name, description, parent_id, sort_order, created_at) "
                  "VALUES (?, ?, ?, ?, ?) "
                  "RETURNING id",
            Poco::Data::Keywords::use(nameCopy),
            Poco::Data::Keywords::use(descriptionCopy),
            Poco::Data::Keywords::use(parentIdCopy),
            Poco::Data::Keywords::use(sortOrderCopy),
            Poco::Data::Keywords::use(createdAtCopy),
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

bool CategoryRepository::update(long long id, const models::Category& category)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        // Создаем неконстантные копии всех параметров
        std::string nameCopy = category.name;
        std::string descriptionCopy = category.description;
        long long parentIdCopy = category.parentId;
        int sortOrderCopy = category.sortOrder;
        long long idCopy = id;
        
        Poco::Data::Statement update(connection->getSession());
        update << "UPDATE " << TABLE_NAME << " SET "
                  "name = ?, description = ?, parent_id = ?, sort_order = ? WHERE id = ?",
            Poco::Data::Keywords::use(nameCopy),
            Poco::Data::Keywords::use(descriptionCopy),
            Poco::Data::Keywords::use(parentIdCopy),
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
        
        Poco::Data::Statement checkProducts(connection->getSession());
        checkProducts << "SELECT COUNT(*) FROM products WHERE category_id = ?",
            Poco::Data::Keywords::use(idCopy),
            now;
        
        int productCount = 0;
        Poco::Data::RecordSet rsProducts(checkProducts);
        if (rsProducts.rowCount() > 0)
        {
            productCount = rsProducts.value(0, 0).convert<int>();
        }
        
        if (productCount > 0)
        {
            throw std::runtime_error("Cannot delete category with associated products");
        }
        
        Poco::Data::Statement checkChildren(connection->getSession());
        checkChildren << "SELECT COUNT(*) FROM " << TABLE_NAME << " WHERE parent_id = ?",
            Poco::Data::Keywords::use(idCopy),
            now;
        
        int childCount = 0;
        Poco::Data::RecordSet rsChildren(checkChildren);
        if (rsChildren.rowCount() > 0)
        {
            childCount = rsChildren.value(0, 0).convert<int>();
        }
        
        if (childCount > 0)
        {
            throw std::runtime_error("Cannot delete category with child categories");
        }
        
        Poco::Data::Statement del(connection->getSession());
        del << "DELETE FROM " << TABLE_NAME << " WHERE id = ?",
            Poco::Data::Keywords::use(idCopy),
            now;
        
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
            return rs.value(0, 0).convert<int>();
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
                          "FROM " + TABLE_NAME + " WHERE " + fieldName + " = ? "
                          "ORDER BY path, sort_order";
        
        std::string fieldValueCopy = fieldValue;
        Poco::Data::Statement select(connection->getSession());
        select << sql,
            Poco::Data::Keywords::use(fieldValueCopy),
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            auto category = std::make_unique<models::Category>();
            category->id = rs.value("id", 0).convert<long long>();
            category->name = rs.value("name").convert<std::string>();
            category->description = rs.value("description").convert<std::string>();
            category->parentId = rs.value("parent_id", 0).convert<long long>();
            category->path = rs.value("path").convert<std::string>();
            category->sortOrder = rs.value("sort_order", 0).convert<int>();
            category->createdAt = rs.value("created_at").convert<std::string>();
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
            auto category = std::make_unique<models::Category>();
            category->id = rs.value("id", 0).convert<long long>();
            category->name = rs.value("name").convert<std::string>();
            category->description = rs.value("description").convert<std::string>();
            category->parentId = rs.value("parent_id", 0).convert<long long>();
            category->path = rs.value("path").convert<std::string>();
            category->sortOrder = rs.value("sort_order", 0).convert<int>();
            category->createdAt = rs.value("created_at").convert<std::string>();
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
            auto category = std::make_unique<models::Category>();
            category->id = rs.value("id", 0).convert<long long>();
            category->name = rs.value("name").convert<std::string>();
            category->description = rs.value("description").convert<std::string>();
            category->parentId = rs.value("parent_id", 0).convert<long long>();
            category->path = rs.value("path").convert<std::string>();
            category->sortOrder = rs.value("sort_order", 0).convert<int>();
            category->createdAt = rs.value("created_at").convert<std::string>();
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
                  "FROM " << TABLE_NAME << " WHERE parent_id = ? "
                  "ORDER BY sort_order, name",
            Poco::Data::Keywords::use(parentIdCopy),
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            auto category = std::make_unique<models::Category>();
            category->id = rs.value("id", 0).convert<long long>();
            category->name = rs.value("name").convert<std::string>();
            category->description = rs.value("description").convert<std::string>();
            category->parentId = rs.value("parent_id", 0).convert<long long>();
            category->path = rs.value("path").convert<std::string>();
            category->sortOrder = rs.value("sort_order", 0).convert<int>();
            category->createdAt = rs.value("created_at").convert<std::string>();
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
            auto category = std::make_unique<models::Category>();
            category->id = rs.value("id", 0).convert<long long>();
            category->name = rs.value("name").convert<std::string>();
            category->description = rs.value("description").convert<std::string>();
            category->parentId = rs.value("parent_id", 0).convert<long long>();
            category->path = rs.value("path").convert<std::string>();
            category->sortOrder = rs.value("sort_order", 0).convert<int>();
            category->createdAt = rs.value("created_at").convert<std::string>();
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
            auto category = std::make_unique<models::Category>();
            category->id = rs.value("id", 0).convert<long long>();
            category->name = rs.value("name").convert<std::string>();
            category->description = rs.value("description").convert<std::string>();
            category->parentId = rs.value("parent_id", 0).convert<long long>();
            category->path = rs.value("path").convert<std::string>();
            category->sortOrder = rs.value("sort_order", 0).convert<int>();
            category->createdAt = rs.value("created_at").convert<std::string>();
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
            check << "SELECT COUNT(*) FROM " << TABLE_NAME << " WHERE id = ?",
                Poco::Data::Keywords::use(newParentIdCopy),
                now;
            
            Poco::Data::RecordSet rs(check);
            int count = 0;
            if (rs.rowCount() > 0)
            {
                count = rs.value(0, 0).convert<int>();
            }
            
            if (count == 0)
            {
                throw std::runtime_error("Parent category does not exist");
            }
        }
        
        Poco::Data::Statement update(connection->getSession());
        update << "UPDATE " << TABLE_NAME << " SET parent_id = ? WHERE id = ?",
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
        update << "UPDATE " << TABLE_NAME << " SET sort_order = ? WHERE id = ?",
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
            return rs.value(0, 0).convert<int>();
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
            return rs.value(0, 0).convert<int>();
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
    categoryIds.push_back(categoryId);
    
    auto children = findChildCategories(categoryId);
    for (const auto& child : children)
    {
        if (child)
        {
            auto childIds = getCategoryAndSubcategoryIds(child->id);
            categoryIds.insert(categoryIds.end(), childIds.begin(), childIds.end());
        }
    }
    
    return categoryIds;
}

models::Category CategoryRepository::mapRowToCategory(Poco::Data::Row& row) const
{
    models::Category category;
    category.id = row.get(0).convert<long long>();
    category.name = row.get(1).convert<std::string>();
    category.description = row.get(2).convert<std::string>();
    category.parentId = row.get(3).convert<long long>();
    category.path = row.get(4).convert<std::string>();
    category.sortOrder = row.get(5).convert<int>();
    category.createdAt = row.get(6).convert<std::string>();
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
