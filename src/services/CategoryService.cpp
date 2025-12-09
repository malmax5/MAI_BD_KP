#include "CategoryService.hpp"
#include "../database/ConnectionPool.hpp"
#include <Poco/JSON/Parser.h>
#include <Poco/JSON/Stringifier.h>
#include <Poco/DateTimeFormatter.h>
#include <Poco/DateTimeFormat.h>
#include <algorithm>
#include <sstream>

namespace warehouse_backend::services
{

CategoryServiceResult::CategoryServiceResult()
    : success(false), categoryId(0), category(nullptr)
{
    
}

Poco::JSON::Object CategoryServiceResult::toJson() const
{
    Poco::JSON::Object result;
    result.set("success", success);
    result.set("message", message);
    result.set("categoryId", static_cast<Poco::Int64>(categoryId));
    
    if (category)
    {
        result.set("category", category->toJson());
    }
    
    if (!data.size())
    {
        result.set("data", data);
    }
    
    return result;
}

CategoryService::CategoryService()
    : categoryRepository(std::make_unique<database::repositories::CategoryRepository>()),
      productRepository(std::make_unique<database::repositories::ProductRepository>()),
      auditRepository(std::make_unique<database::repositories::AuditLogRepository>())
{
}

CategoryServiceResult CategoryService::createCategory(const database::models::Category& category,
                                                     long long createdBy)
{
    CategoryServiceResult result;
    
    std::string validationError;
    if (!validateCategoryData(category, validationError))
    {
        result.success = false;
        result.message = "Validation failed: " + validationError;
        return result;
    }
    
    if (!category.parentId.isNull() && category.parentId.value() > 0)
    {
        std::string parentError;
        if (!validateParentCategory(category.parentId.value(), parentError))
        {
            result.success = false;
            result.message = parentError;
            return result;
        }
    }
    
    try
    {
        long long categoryId = categoryRepository->create(category);
        
        if (categoryId > 0)
        {
            result.success = true;
            result.categoryId = categoryId;
            result.message = "Category created successfully";
            
            result.category = categoryRepository->findById(categoryId);
            
            logCategoryAudit("CREATE",
                            categoryId,
                            createdBy,
                            "",
                            categoryToJsonString(*result.category),
                            "Category created");
        }
        else
        {
            result.success = false;
            result.message = "Failed to create category";
        }
    }
    catch (const std::exception& e)
    {
        result.success = false;
        result.message = "Error creating category: " + std::string(e.what());
    }
    
    return result;
}

CategoryServiceResult CategoryService::getCategoryById(long long categoryId)
{
    CategoryServiceResult result;
    
    try
    {
        auto category = categoryRepository->findById(categoryId);
        if (category)
        {
            result.success = true;
            result.categoryId = categoryId;
            result.message = "Category found";
            result.category = enrichCategoryWithDetails(std::move(category));
        }
        else
        {
            result.success = false;
            result.message = "Category not found with ID: " + std::to_string(categoryId);
        }
    }
    catch (const std::exception& e)
    {
        result.success = false;
        result.message = "Error retrieving category: " + std::string(e.what());
    }
    
    return result;
}

CategoryServiceResult CategoryService::getCategoryWithChildren(long long categoryId)
{
    CategoryServiceResult result;
    
    try
    {
        auto category = categoryRepository->findCategoryWithChildren(categoryId);
        if (category)
        {
            result.success = true;
            result.categoryId = categoryId;
            result.message = "Category with children found";
            result.category = enrichCategoryWithDetails(std::move(category));
            
            result.data.set("tree", buildCategoryTreeJson(*result.category));
        }
        else
        {
            result.success = false;
            result.message = "Category not found with ID: " + std::to_string(categoryId);
        }
    }
    catch (const std::exception& e)
    {
        result.success = false;
        result.message = "Error retrieving category with children: " + std::string(e.what());
    }
    
    return result;
}

CategoryServiceResult CategoryService::updateCategory(long long categoryId,
                                                     const database::models::Category& updatedCategory,
                                                     long long updatedBy)
{
    CategoryServiceResult result;
    
    try
    {
        auto currentCategory = categoryRepository->findById(categoryId);
        if (!currentCategory)
        {
            result.success = false;
            result.message = "Category not found with ID: " + std::to_string(categoryId);
            return result;
        }
        
        std::string validationError;
        if (!validateCategoryData(updatedCategory, validationError))
        {
            result.success = false;
            result.message = "Validation failed: " + validationError;
            return result;
        }
        
        bool parentChanged = false;
        long long oldParentId = currentCategory->parentId.isNull() ? 0 : currentCategory->parentId.value();
        long long newParentId = updatedCategory.parentId.isNull() ? 0 : updatedCategory.parentId.value();
        
        if (oldParentId != newParentId && newParentId > 0)
        {
            std::string parentError;
            if (!validateParentCategory(newParentId, parentError))
            {
                result.success = false;
                result.message = parentError;
                return result;
            }
            
            if (newParentId == categoryId)
            {
                result.success = false;
                result.message = "Cannot set category as its own parent";
                return result;
            }
            
            auto childIds = categoryRepository->getCategoryAndSubcategoryIds(categoryId);
            if (std::find(childIds.begin(), childIds.end(), newParentId) != childIds.end())
            {
                result.success = false;
                result.message = "Cannot set a child category as parent";
                return result;
            }
            
            parentChanged = true;
        }
        
        std::string oldValues = categoryToJsonString(*currentCategory);
        
        bool updateSuccess = categoryRepository->update(categoryId, updatedCategory);
        
        if (updateSuccess)
        {
            result.success = true;
            result.categoryId = categoryId;
            result.message = "Category updated successfully";
            
            result.category = categoryRepository->findById(categoryId);
            
            std::string newValues = categoryToJsonString(*result.category);
            std::string description = "Category updated";
            if (parentChanged)
            {
                description += " (parent changed)";
            }
            
            logCategoryAudit("UPDATE",
                            categoryId,
                            updatedBy,
                            oldValues,
                            newValues,
                            description);
        }
        else
        {
            result.success = false;
            result.message = "Failed to update category";
        }
    }
    catch (const std::exception& e)
    {
        result.success = false;
        result.message = "Error updating category: " + std::string(e.what());
    }
    
    return result;
}

CategoryServiceResult CategoryService::deleteCategory(long long categoryId,
                                                     long long deletedBy,
                                                     bool forceDelete)
{
    CategoryServiceResult result;
    
    try
    {
        auto category = categoryRepository->findById(categoryId);
        if (!category)
        {
            result.success = false;
            result.message = "Category not found with ID: " + std::to_string(categoryId);
            return result;
        }
        
        std::string deleteError;
        if (!forceDelete && !canDeleteCategory(categoryId, deleteError))
        {
            result.success = false;
            result.message = deleteError;
            return result;
        }
        
        std::string oldValues = categoryToJsonString(*category);
        
        bool deleteSuccess = false;
        if (forceDelete)
        {
            deleteSuccess = categoryRepository->remove(categoryId);
        }
        else
        {
            deleteSuccess = categoryRepository->softDelete(categoryId);
        }
        
        if (deleteSuccess)
        {
            result.success = true;
            result.categoryId = categoryId;
            result.message = forceDelete ? "Category permanently deleted" : "Category soft deleted successfully";
            
            std::string description = forceDelete ? "Category permanently deleted" : "Category soft deleted";
            logCategoryAudit(forceDelete ? "DELETE" : "SOFT_DELETE",
                            categoryId,
                            deletedBy,
                            oldValues,
                            "",
                            description);
        }
        else
        {
            result.success = false;
            result.message = "Failed to delete category";
        }
    }
    catch (const std::exception& e)
    {
        result.success = false;
        result.message = "Error deleting category: " + std::string(e.what());
    }
    
    return result;
}

Poco::JSON::Array CategoryService::getRootCategories()
{
    Poco::JSON::Array result;
    
    try
    {
        auto categories = categoryRepository->findRootCategories();
        
        for (auto& category : categories)
        {
            auto enrichedCategory = enrichCategoryWithDetails(std::move(category));
            result.add(enrichedCategory->toJson());
        }
    }
    catch (const std::exception& e)
    {
        Poco::JSON::Object error;
        error.set("error", "Error retrieving root categories: " + std::string(e.what()));
        result.add(error);
    }
    
    return result;
}

Poco::JSON::Array CategoryService::getChildCategories(long long parentId)
{
    Poco::JSON::Array result;
    
    try
    {
        auto categories = categoryRepository->findChildCategories(parentId);
        
        for (auto& category : categories)
        {
            auto enrichedCategory = enrichCategoryWithDetails(std::move(category));
            result.add(enrichedCategory->toJson());
        }
    }
    catch (const std::exception& e)
    {
        Poco::JSON::Object error;
        error.set("error", "Error retrieving child categories: " + std::string(e.what()));
        result.add(error);
    }
    
    return result;
}

Poco::JSON::Array CategoryService::getCategoriesTree()
{
    Poco::JSON::Array result;
    
    try
    {
        Poco::JSON::Object treeJson = categoryRepository->findCategoryTree();
        if (treeJson.size() > 0)
        {
            result.add(treeJson);
        }
    }
    catch (const std::exception& e)
    {
        Poco::JSON::Object error;
        error.set("error", "Error retrieving categories tree: " + std::string(e.what()));
        result.add(error);
    }
    
    return result;
}

Poco::JSON::Array CategoryService::getAllCategories(int page, int pageSize)
{
    Poco::JSON::Array result;
    
    try
    {
        auto categories = categoryRepository->findPaginated(page, pageSize);
        
        for (auto& category : categories)
        {
            auto enrichedCategory = enrichCategoryWithDetails(std::move(category));
            result.add(enrichedCategory->toJson());
        }
    }
    catch (const std::exception& e)
    {
        Poco::JSON::Object error;
        error.set("error", "Error retrieving categories: " + std::string(e.what()));
        result.add(error);
    }
    
    return result;
}

Poco::JSON::Array CategoryService::searchCategories(const std::string& query,
                                                   int page,
                                                   int pageSize)
{
    Poco::JSON::Array result;
    
    try
    {
        std::vector<std::string> searchFields = {"name", "description"};
        auto categories = categoryRepository->search(query, searchFields);
        
        int startIndex = (page - 1) * pageSize;
        int endIndex = std::min(static_cast<int>(categories.size()), startIndex + pageSize);
        
        for (int i = startIndex; i < endIndex && i < static_cast<int>(categories.size()); ++i)
        {
            auto enrichedCategory = enrichCategoryWithDetails(std::move(categories[i]));
            result.add(enrichedCategory->toJson());
        }
    }
    catch (const std::exception& e)
    {
        Poco::JSON::Object error;
        error.set("error", "Error searching categories: " + std::string(e.what()));
        result.add(error);
    }
    
    return result;
}

CategoryServiceResult CategoryService::moveCategory(long long categoryId,
                                                   long long newParentId,
                                                   long long movedBy)
{
    CategoryServiceResult result;
    
    try
    {
        auto currentCategory = categoryRepository->findById(categoryId);
        if (!currentCategory)
        {
            result.success = false;
            result.message = "Category not found with ID: " + std::to_string(categoryId);
            return result;
        }
        
        if (newParentId > 0)
        {
            auto parentCategory = categoryRepository->findById(newParentId);
            if (!parentCategory)
            {
                result.success = false;
                result.message = "Parent category not found with ID: " + std::to_string(newParentId);
                return result;
            }
            
            if (newParentId == categoryId)
            {
                result.success = false;
                result.message = "Cannot move category to itself";
                return result;
            }
            
            auto childIds = categoryRepository->getCategoryAndSubcategoryIds(categoryId);
            if (std::find(childIds.begin(), childIds.end(), newParentId) != childIds.end())
            {
                result.success = false;
                result.message = "Cannot move category to its child";
                return result;
            }
        }
        
        std::string oldValues = categoryToJsonString(*currentCategory);
        
        bool updateSuccess = categoryRepository->updateParent(categoryId, newParentId);
        
        if (updateSuccess)
        {
            result.success = true;
            result.categoryId = categoryId;
            result.message = "Category moved successfully";
            
            result.category = categoryRepository->findById(categoryId);
            
            std::string newValues = categoryToJsonString(*result.category);
            logCategoryAudit("MOVE",
                            categoryId,
                            movedBy,
                            oldValues,
                            newValues,
                            "Category moved to parent ID: " + std::to_string(newParentId));
        }
        else
        {
            result.success = false;
            result.message = "Failed to move category";
        }
    }
    catch (const std::exception& e)
    {
        result.success = false;
        result.message = "Error moving category: " + std::string(e.what());
    }
    
    return result;
}

CategoryServiceResult CategoryService::updateCategorySortOrder(long long categoryId,
                                                             int newSortOrder,
                                                             long long updatedBy)
{
    CategoryServiceResult result;
    
    try
    {
        auto currentCategory = categoryRepository->findById(categoryId);
        if (!currentCategory)
        {
            result.success = false;
            result.message = "Category not found with ID: " + std::to_string(categoryId);
            return result;
        }
        
        std::string oldValues = categoryToJsonString(*currentCategory);
        
        bool updateSuccess = categoryRepository->updateSortOrder(categoryId, newSortOrder);
        
        if (updateSuccess)
        {
            result.success = true;
            result.categoryId = categoryId;
            result.message = "Category sort order updated successfully";
            
            result.category = categoryRepository->findById(categoryId);
            
            std::string newValues = categoryToJsonString(*result.category);
            logCategoryAudit("UPDATE",
                            categoryId,
                            updatedBy,
                            oldValues,
                            newValues,
                            "Category sort order updated to: " + std::to_string(newSortOrder));
        }
        else
        {
            result.success = false;
            result.message = "Failed to update category sort order";
        }
    }
    catch (const std::exception& e)
    {
        result.success = false;
        result.message = "Error updating category sort order: " + std::string(e.what());
    }
    
    return result;
}

Poco::JSON::Object CategoryService::getCategoryStatistics(long long categoryId)
{
    Poco::JSON::Object stats;
    
    try
    {
        auto category = categoryRepository->findById(categoryId);
        if (!category)
        {
            stats.set("error", "Category not found");
            return stats;
        }
        
        int productCount = categoryRepository->countProductsInCategory(categoryId);
        int childCount = categoryRepository->hasChildren(categoryId) ? 1 : 0;
        bool hasProductsFlag = categoryRepository->hasProducts(categoryId);
        bool hasChildrenFlag = categoryRepository->hasChildren(categoryId);
        
        auto allChildIds = categoryRepository->getCategoryAndSubcategoryIds(categoryId);
        int totalProductsInHierarchy = 0;
        
        for (long long childId : allChildIds)
        {
            totalProductsInHierarchy += categoryRepository->countProductsInCategory(childId);
        }
        
        stats.set("categoryId", static_cast<Poco::Int64>(categoryId));
        stats.set("categoryName", category->name);
        stats.set("productCount", productCount);
        stats.set("childCategoryCount", childCount);
        stats.set("hasProducts", hasProductsFlag);
        stats.set("hasChildren", hasChildrenFlag);
        stats.set("totalProductsInHierarchy", totalProductsInHierarchy);
        stats.set("subcategoryCount", static_cast<int>(allChildIds.size() - 1));
        stats.set("lastUpdated", utils::DateUtils::formatDateTime(utils::DateUtils::now()));
    }
    catch (const std::exception& e)
    {
        stats.set("error", "Error calculating statistics: " + std::string(e.what()));
    }
    
    return stats;
}

Poco::JSON::Array CategoryService::getCategoryProducts(long long categoryId,
                                                      int page,
                                                      int pageSize)
{
    Poco::JSON::Array result;
    
    try
    {
        auto allCategoryIds = categoryRepository->getCategoryAndSubcategoryIds(categoryId);
        
        if (allCategoryIds.empty())
        {
            return result;
        }
        
        std::vector<std::unique_ptr<database::models::Product>> allProducts;
        
        for (long long catId : allCategoryIds)
        {
            auto products = productRepository->findByCategory(catId);
            allProducts.insert(allProducts.end(),
                              std::make_move_iterator(products.begin()),
                              std::make_move_iterator(products.end()));
        }
        
        int startIndex = (page - 1) * pageSize;
        int endIndex = std::min(static_cast<int>(allProducts.size()), startIndex + pageSize);
        
        for (int i = startIndex; i < endIndex && i < static_cast<int>(allProducts.size()); ++i)
        {
            result.add(allProducts[i]->toJson());
        }
        
        Poco::JSON::Object pagination;
        pagination.set("totalProducts", static_cast<int>(allProducts.size()));
        pagination.set("page", page);
        pagination.set("pageSize", pageSize);
        pagination.set("totalPages", (allProducts.size() + pageSize - 1) / pageSize);
        
        result.add(pagination);
    }
    catch (const std::exception& e)
    {
        Poco::JSON::Object error;
        error.set("error", "Error retrieving category products: " + std::string(e.what()));
        result.add(error);
    }
    
    return result;
}

Poco::JSON::Object CategoryService::getFullCategoryHierarchy()
{
    Poco::JSON::Object result;
    
    try
    {
        auto categories = categoryRepository->findAll();
        
        Poco::JSON::Array rootCategories;
        
        for (auto& category : categories)
        {
            if (category->parentId.isNull() || category->parentId.value() == 0)
            {
                buildRecursiveTree(*category, categories);
                rootCategories.add(buildCategoryTreeJson(*category));
            }
        }
        
        result.set("categories", rootCategories);
        result.set("totalCategories", static_cast<int>(categories.size()));
        result.set("lastUpdated", utils::DateUtils::formatDateTime(utils::DateUtils::now()));
    }
    catch (const std::exception& e)
    {
        result.set("error", "Error building category hierarchy: " + std::string(e.what()));
    }
    
    return result;
}

bool CategoryService::validateCategoryData(const database::models::Category& category,
                                          std::string& errorMessage)
{
    if (category.name.empty())
    {
        errorMessage = "Category name is required";
        return false;
    }
    
    if (category.name.length() > database::models::Category::MAX_NAME_LENGTH)
    {
        errorMessage = "Category name cannot exceed " + 
                      std::to_string(database::models::Category::MAX_NAME_LENGTH) + 
                      " characters";
        return false;
    }
    
    if (!category.description.isNull() && 
        category.description.value().length() > database::models::Category::MAX_DESCRIPTION_LENGTH)
    {
        errorMessage = "Category description cannot exceed " + 
                      std::to_string(database::models::Category::MAX_DESCRIPTION_LENGTH) + 
                      " characters";
        return false;
    }
    
    return true;
}

bool CategoryService::canDeleteCategory(long long categoryId, std::string& errorMessage)
{
    try
    {
        if (hasChildren(categoryId))
        {
            errorMessage = "Cannot delete category because it has child categories. "
                          "Delete child categories first or use force delete.";
            return false;
        }
        
        if (hasProducts(categoryId))
        {
            errorMessage = "Cannot delete category because it contains products. "
                          "Move or delete products first or use force delete.";
            return false;
        }
        
        return true;
    }
    catch (const std::exception& e)
    {
        errorMessage = "Error checking if category can be deleted: " + std::string(e.what());
        return false;
    }
}

bool CategoryService::hasChildren(long long categoryId)
{
    try
    {
        return categoryRepository->hasChildren(categoryId);
    }
    catch (const std::exception&)
    {
        return false;
    }
}

bool CategoryService::hasProducts(long long categoryId)
{
    try
    {
        return categoryRepository->hasProducts(categoryId);
    }
    catch (const std::exception&)
    {
        return false;
    }
}

Poco::JSON::Array CategoryService::importCategories(const Poco::JSON::Array& categoriesArray,
                                                   long long importedBy)
{
    Poco::JSON::Array result;
    
    try
    {
        int successCount = 0;
        int failureCount = 0;
        
        for (size_t i = 0; i < categoriesArray.size(); ++i)
        {
            Poco::JSON::Object itemResult;
            
            try
            {
                auto categoryJson = categoriesArray.getObject(i);
                database::models::Category category = database::models::Category::fromJson(*categoryJson);
                
                auto createResult = createCategory(category, importedBy);
                
                itemResult.set("index", static_cast<int>(i));
                itemResult.set("success", createResult.success);
                itemResult.set("message", createResult.message);
                
                if (createResult.success)
                {
                    successCount++;
                    itemResult.set("categoryId", static_cast<Poco::Int64>(createResult.categoryId));
                }
                else
                {
                    failureCount++;
                }
            }
            catch (const std::exception& e)
            {
                itemResult.set("index", static_cast<int>(i));
                itemResult.set("success", false);
                itemResult.set("message", "Error importing category: " + std::string(e.what()));
                failureCount++;
            }
            
            result.add(itemResult);
        }
        
        Poco::JSON::Object summary;
        summary.set("total", static_cast<int>(categoriesArray.size()));
        summary.set("success", successCount);
        summary.set("failure", failureCount);
        summary.set("importedBy", static_cast<Poco::Int64>(importedBy));
        summary.set("timestamp", utils::DateUtils::formatDateTime(utils::DateUtils::now()));
        
        result.add(summary);
    }
    catch (const std::exception& e)
    {
        Poco::JSON::Object error;
        error.set("error", "Error during import: " + std::string(e.what()));
        result.add(error);
    }
    
    return result;
}

Poco::JSON::Array CategoryService::exportCategories(const std::vector<long long>& categoryIds)
{
    Poco::JSON::Array result;
    
    try
    {
        for (long long categoryId : categoryIds)
        {
            auto category = categoryRepository->findById(categoryId);
            if (category)
            {
                auto enrichedCategory = enrichCategoryWithDetails(std::move(category));
                result.add(enrichedCategory->toJson());
            }
        }
        
        Poco::JSON::Object summary;
        summary.set("exportedCount", static_cast<int>(result.size()));
        summary.set("timestamp", utils::DateUtils::formatDateTime(utils::DateUtils::now()));
        
        result.add(summary);
    }
    catch (const std::exception& e)
    {
        Poco::JSON::Object error;
        error.set("error", "Error exporting categories: " + std::string(e.what()));
        result.add(error);
    }
    
    return result;
}

void CategoryService::logCategoryAudit(const std::string& action,
                                      long long categoryId,
                                      long long changedBy,
                                      const std::string& oldValues,
                                      const std::string& newValues,
                                      const std::string& description)
{
    try
    {
        database::models::AuditLog auditLog;
        auditLog.tableName = "categories";
        auditLog.recordId = categoryId;
        
        if (action == "CREATE" || action == "INSERT")
            auditLog.action = database::models::AuditAction::INSERT;
        else if (action == "UPDATE" || action == "MOVE")
            auditLog.action = database::models::AuditAction::UPDATE;
        else if (action == "DELETE")
            auditLog.action = database::models::AuditAction::DELETE;
        else if (action == "SOFT_DELETE")
            auditLog.action = database::models::AuditAction::SOFT_DELETE;
        else
            auditLog.action = database::models::AuditAction::UPDATE;
        
        auditLog.oldValues = oldValues;
        auditLog.newValues = newValues;
        auditLog.changedBy = changedBy;
        auditLog.description = description;
        auditLog.changedAt = utils::DateUtils::formatDateTime(utils::DateUtils::now());
        
        auditRepository->create(auditLog);
    }
    catch (const std::exception&)
    {
    }
}

std::string CategoryService::categoryToJsonString(const database::models::Category& category) const
{
    try
    {
        auto json = category.toJson();
        return utils::JsonUtils::objectToString(json, false);
    }
    catch (const std::exception&)
    {
        return "{}";
    }
}

std::unique_ptr<database::models::Category> CategoryService::enrichCategoryWithDetails(
    std::unique_ptr<database::models::Category> category)
{
    if (!category)
    {
        return nullptr;
    }
    
    try
    {
        if (!category->parentId.isNull() && category->parentId.value() > 0)
        {
            auto parent = categoryRepository->findById(category->parentId.value());
            if (parent)
            {
                category->parentName = parent->name;
            }
        }
        
        int productCount = categoryRepository->countProductsInCategory(category->id);
        bool hasChildrenFlag = categoryRepository->hasChildren(category->id);
        
        return category;
    }
    catch (const std::exception&)
    {
        return category;
    }
}

bool CategoryService::validateParentCategory(long long parentId, std::string& errorMessage)
{
    try
    {
        auto parentCategory = categoryRepository->findById(parentId);
        if (!parentCategory)
        {
            errorMessage = "Parent category not found with ID: " + std::to_string(parentId);
            return false;
        }
        
        return true;
    }
    catch (const std::exception& e)
    {
        errorMessage = "Error validating parent category: " + std::string(e.what());
        return false;
    }
}

Poco::JSON::Object CategoryService::buildCategoryTreeJson(const database::models::Category& category)
{
    Poco::JSON::Object treeJson = category.toJson();
    
    if (!category.children.empty())
    {
        Poco::JSON::Array childrenArray;
        
        for (const auto& child : category.children)
        {
            childrenArray.add(buildCategoryTreeJson(*child));
        }
        
        treeJson.set("children", childrenArray);
    }
    
    treeJson.set("productCount", categoryRepository->countProductsInCategory(category.id));
    treeJson.set("hasChildren", !category.children.empty());
    
    return treeJson;
}

void CategoryService::buildRecursiveTree(database::models::Category& category,
                                        const std::vector<std::unique_ptr<database::models::Category>>& allCategories)
{
    try
    {
        auto children = categoryRepository->findChildCategories(category.id);
        
        for (auto& child : children)
        {
            auto enrichedChild = enrichCategoryWithDetails(std::move(child));
            if (enrichedChild)
            {
                category.children.push_back(std::move(enrichedChild));
            }
        }
        
        for (auto& child : category.children)
        {
            buildRecursiveTree(*child, allCategories);
        }
    }
    catch (const std::exception&)
    {
    }
}

} // namespace services
