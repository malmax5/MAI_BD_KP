#pragma once

#include "BaseController.hpp"
#include "../services/CategoryService.hpp"
#include "../services/AuthService.hpp"
#include <memory>

namespace warehouse_backend::controllers
{

class CategoryController : public BaseController
{
public:
    CategoryController();
    virtual ~CategoryController() = default;
    
    void handleRequest(Poco::Net::HTTPServerRequest& request, 
                      Poco::Net::HTTPServerResponse& response) override;
    
    void handleGetCategories(Poco::Net::HTTPServerRequest& request, 
                           Poco::Net::HTTPServerResponse& response);
    
    void handleGetCategoryById(Poco::Net::HTTPServerRequest& request, 
                              Poco::Net::HTTPServerResponse& response);
    
    void handleCreateCategory(Poco::Net::HTTPServerRequest& request, 
                             Poco::Net::HTTPServerResponse& response);
    
    void handleUpdateCategory(Poco::Net::HTTPServerRequest& request, 
                             Poco::Net::HTTPServerResponse& response);
    
    void handleDeleteCategory(Poco::Net::HTTPServerRequest& request, 
                             Poco::Net::HTTPServerResponse& response);
    
    void handleGetCategoryTree(Poco::Net::HTTPServerRequest& request, 
                              Poco::Net::HTTPServerResponse& response);
    
    void handleGetCategorySubtree(Poco::Net::HTTPServerRequest& request, 
                                  Poco::Net::HTTPServerResponse& response);
    
    void handleGetRootCategories(Poco::Net::HTTPServerRequest& request, 
                                Poco::Net::HTTPServerResponse& response);
    
    void handleGetChildCategories(Poco::Net::HTTPServerRequest& request, 
                                 Poco::Net::HTTPServerResponse& response);
    
    void handleGetCategoryProducts(Poco::Net::HTTPServerRequest& request, 
                                  Poco::Net::HTTPServerResponse& response);
    
    void handleSearchCategories(Poco::Net::HTTPServerRequest& request, 
                               Poco::Net::HTTPServerResponse& response);
    
    void handleMoveCategory(Poco::Net::HTTPServerRequest& request, 
                           Poco::Net::HTTPServerResponse& response);
    
    void handleGetCategoryStatistics(Poco::Net::HTTPServerRequest& request, 
                                    Poco::Net::HTTPServerResponse& response);
    
    void handleImportCategories(Poco::Net::HTTPServerRequest& request, 
                               Poco::Net::HTTPServerResponse& response);
    
    void handleExportCategories(Poco::Net::HTTPServerRequest& request, 
                               Poco::Net::HTTPServerResponse& response);
    
    bool validateRequest(Poco::Net::HTTPServerRequest& request, 
                        Poco::Net::HTTPServerResponse& response,
                        std::string& errorMessage) override;
    
    bool authorizeRequest(Poco::Net::HTTPServerRequest& request, 
                         Poco::Net::HTTPServerResponse& response,
                         std::string& errorMessage) override;
    
    bool validateCategoryAccess(Poco::Net::HTTPServerRequest& request, 
                               long long categoryId,
                               std::string& errorMessage);
    
    long long getCurrentUserId(Poco::Net::HTTPServerRequest& request);
    database::models::UserRole getCurrentUserRole(Poco::Net::HTTPServerRequest& request);
    
private:
    std::unique_ptr<services::CategoryService> categoryService;
    std::unique_ptr<services::AuthService> authService;
    
    bool validateCreateCategoryData(const Poco::JSON::Object::Ptr& json, 
                                   std::vector<std::string>& errors);
    
    bool validateUpdateCategoryData(const Poco::JSON::Object::Ptr& json, 
                                   std::vector<std::string>& errors);
    
    bool validateMoveCategoryData(const Poco::JSON::Object::Ptr& json, 
                                 std::vector<std::string>& errors);
    
    bool validateSearchParameters(const std::map<std::string, std::string>& filters,
                                 std::vector<std::string>& errors);
    
    void logCategoryEvent(long long userId, 
                         const std::string& action,
                         long long categoryId,
                         const std::string& ipAddress,
                         const std::string& userAgent,
                         bool success,
                         const std::string& details = "");
    
    std::unique_ptr<database::models::Category> extractCategoryFromJson(const Poco::JSON::Object::Ptr& json);
    
    bool canCreateCategory(Poco::Net::HTTPServerRequest& request, 
                          const database::models::Category& categoryData,
                          std::string& errorMessage);
    
    bool canUpdateCategory(Poco::Net::HTTPServerRequest& request, 
                          long long categoryId,
                          const database::models::Category& categoryData,
                          std::string& errorMessage);
    
    bool canDeleteCategory(Poco::Net::HTTPServerRequest& request, 
                          long long categoryId,
                          std::string& errorMessage);
    
    bool canMoveCategory(Poco::Net::HTTPServerRequest& request, 
                        long long categoryId,
                        long long newParentId,
                        std::string& errorMessage);
    
    Poco::JSON::Object buildPaginationResponse(int page, int pageSize, int totalItems, 
                                              const Poco::JSON::Array& data);
};

} // namespace controllers
