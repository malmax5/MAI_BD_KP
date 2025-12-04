#pragma once

#include "BaseRepository.hpp"
#include "../models/Category.hpp"
#include <memory>
#include <string>
#include <vector>
#include <Poco/JSON/Object.h>
#include <Poco/JSON/Array.h>

namespace warehouse_backend::database::repositories
{

class CategoryRepository : public BaseRepository<models::Category>
{
public:
    CategoryRepository();
    ~CategoryRepository() override = default;
    
    std::unique_ptr<models::Category> findById(long long id) override;
    std::vector<std::unique_ptr<models::Category>> findAll() override;
    std::vector<std::unique_ptr<models::Category>> findPaginated(int page, int pageSize) override;
    long long create(const models::Category& category) override;
    bool update(long long id, const models::Category& category) override;
    bool remove(long long id) override;
    bool softDelete(long long id) override;
    int count() override;
    
    Poco::JSON::Array findAllAsJson() override;
    Poco::JSON::Object findByIdAsJson(long long id) override;
    
    std::vector<std::unique_ptr<models::Category>> findByField(const std::string& fieldName, 
                                                               const std::string& fieldValue) override;
    std::vector<std::unique_ptr<models::Category>> search(const std::string& query, 
                                                          const std::vector<std::string>& fields) override;
    
    std::vector<std::unique_ptr<models::Category>> findRootCategories();
    std::vector<std::unique_ptr<models::Category>> findChildCategories(long long parentId);
    std::vector<std::unique_ptr<models::Category>> findCategoriesWithProducts();
    std::vector<std::unique_ptr<models::Category>> findLeafCategories();
    
    bool updateParent(long long id, long long newParentId);
    bool updateSortOrder(long long id, int sortOrder);
    
    std::unique_ptr<models::Category> findCategoryWithChildren(long long id);
    Poco::JSON::Object findCategoryTree();
    
    int countRootCategories();
    int countProductsInCategory(long long categoryId);
    
    bool hasProducts(long long categoryId);
    bool hasChildren(long long categoryId);
    
    std::vector<long long> getCategoryAndSubcategoryIds(long long categoryId);
    
private:
    long long getParentIdFromVar(const Poco::Dynamic::Var& var) const;

    models::Category mapRowToCategory(Poco::Data::Row& row) const;
    
    void buildCategoryTree(models::Category& parent, 
                          const std::vector<models::Category>& allCategories);
    Poco::JSON::Object buildJsonCategoryTree(const models::Category& category);
    
    static const std::string TABLE_NAME;
    static const std::vector<std::string> SEARCH_FIELDS;
};

} // namespace database::repositories
