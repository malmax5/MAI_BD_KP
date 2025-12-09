#pragma once

#include "../database/repositories/CategoryRepository.hpp"
#include "../database/repositories/ProductRepository.hpp"
#include "../database/repositories/AuditLogRepository.hpp"
#include "../config/ConfigManager.hpp"
#include "../utils/Validator.hpp"
#include "../utils/JsonUtils.hpp"
#include "../utils/DateUtils.hpp"
#include <memory>
#include <string>
#include <vector>
#include <Poco/JSON/Object.h>
#include <Poco/JSON/Array.h>

namespace warehouse_backend::services
{

struct CategoryServiceResult
{
    bool success;
    std::string message;
    long long categoryId;
    std::unique_ptr<database::models::Category> category;
    Poco::JSON::Object data;
    
    CategoryServiceResult();
    
    Poco::JSON::Object toJson() const;
};

class CategoryService
{
public:
    CategoryService();
    ~CategoryService() = default;
    
    CategoryServiceResult createCategory(const database::models::Category& category,
                                        long long createdBy = 0);
    
    CategoryServiceResult getCategoryById(long long categoryId);
    CategoryServiceResult getCategoryWithChildren(long long categoryId);
    
    CategoryServiceResult updateCategory(long long categoryId,
                                        const database::models::Category& updatedCategory,
                                        long long updatedBy = 0);
    
    CategoryServiceResult deleteCategory(long long categoryId,
                                        long long deletedBy = 0,
                                        bool forceDelete = false);
    
    Poco::JSON::Array getRootCategories();
    Poco::JSON::Array getChildCategories(long long parentId);
    Poco::JSON::Array getCategoriesTree();
    Poco::JSON::Array getAllCategories(int page = 1, int pageSize = 20);
    
    Poco::JSON::Array searchCategories(const std::string& query,
                                      int page = 1,
                                      int pageSize = 20);
    
    CategoryServiceResult moveCategory(long long categoryId,
                                      long long newParentId,
                                      long long movedBy = 0);
    
    CategoryServiceResult updateCategorySortOrder(long long categoryId,
                                                int newSortOrder,
                                                long long updatedBy = 0);
    
    Poco::JSON::Object getCategoryStatistics(long long categoryId);
    Poco::JSON::Array getCategoryProducts(long long categoryId,
                                         int page = 1,
                                         int pageSize = 20);
    Poco::JSON::Object getFullCategoryHierarchy();
    
    bool validateCategoryData(const database::models::Category& category,
                             std::string& errorMessage);
    
    bool canDeleteCategory(long long categoryId, std::string& errorMessage);
    bool hasChildren(long long categoryId);
    bool hasProducts(long long categoryId);
    
    Poco::JSON::Array importCategories(const Poco::JSON::Array& categoriesArray,
                                      long long importedBy = 0);
    Poco::JSON::Array exportCategories(const std::vector<long long>& categoryIds);
    
private:
    std::unique_ptr<database::repositories::CategoryRepository> categoryRepository;
    std::unique_ptr<database::repositories::ProductRepository> productRepository;
    std::unique_ptr<database::repositories::AuditLogRepository> auditRepository;
    
    void logCategoryAudit(const std::string& action,
                         long long categoryId,
                         long long changedBy,
                         const std::string& oldValues = "",
                         const std::string& newValues = "",
                         const std::string& description = "");
    
    std::string categoryToJsonString(const database::models::Category& category) const;
    
    std::unique_ptr<database::models::Category> enrichCategoryWithDetails(
        std::unique_ptr<database::models::Category> category);
    
    bool validateParentCategory(long long parentId, std::string& errorMessage);
    
    Poco::JSON::Object buildCategoryTreeJson(const database::models::Category& category);
    void buildRecursiveTree(database::models::Category& category,
                           const std::vector<std::unique_ptr<database::models::Category>>& allCategories);
};

} // namespace services
