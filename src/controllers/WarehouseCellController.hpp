#pragma once

#include "BaseController.hpp"
#include "../services/WarehouseCellService.hpp"
#include "../services/AuthService.hpp"
#include "../services/InventoryService.hpp"
#include <memory>

namespace warehouse_backend::controllers
{

class WarehouseCellController : public BaseController
{
public:
    WarehouseCellController();
    virtual ~WarehouseCellController() = default;
    
    void handleRequest(Poco::Net::HTTPServerRequest& request, 
                      Poco::Net::HTTPServerResponse& response) override;
    
    void handleGetCells(Poco::Net::HTTPServerRequest& request, 
                       Poco::Net::HTTPServerResponse& response);
    
    void handleGetCellById(Poco::Net::HTTPServerRequest& request, 
                          Poco::Net::HTTPServerResponse& response);
    
    void handleGetCellByCode(Poco::Net::HTTPServerRequest& request, 
                            Poco::Net::HTTPServerResponse& response);
    
    void handleCreateCell(Poco::Net::HTTPServerRequest& request, 
                         Poco::Net::HTTPServerResponse& response);
    
    void handleUpdateCell(Poco::Net::HTTPServerRequest& request, 
                         Poco::Net::HTTPServerResponse& response);
    
    void handleDeleteCell(Poco::Net::HTTPServerRequest& request, 
                         Poco::Net::HTTPServerResponse& response);
    
    void handleClearCell(Poco::Net::HTTPServerRequest& request, 
                        Poco::Net::HTTPServerResponse& response);
    
    void handleBlockCell(Poco::Net::HTTPServerRequest& request, 
                        Poco::Net::HTTPServerResponse& response);
    
    void handleUnblockCell(Poco::Net::HTTPServerRequest& request, 
                          Poco::Net::HTTPServerResponse& response);
    
    void handleGetAvailableCells(Poco::Net::HTTPServerRequest& request, 
                                Poco::Net::HTTPServerResponse& response);
    
    void handleGetCellsByZone(Poco::Net::HTTPServerRequest& request, 
                             Poco::Net::HTTPServerResponse& response);
    
    void handleGetCellsByStatus(Poco::Net::HTTPServerRequest& request, 
                               Poco::Net::HTTPServerResponse& response);
    
    void handleGetCellBatches(Poco::Net::HTTPServerRequest& request, 
                             Poco::Net::HTTPServerResponse& response);
    
    void handleFindBestCellForStorage(Poco::Net::HTTPServerRequest& request, 
                                     Poco::Net::HTTPServerResponse& response);
    
    void handleGetCellStatistics(Poco::Net::HTTPServerRequest& request, 
                                Poco::Net::HTTPServerResponse& response);
    
    void handleGetOccupancyReport(Poco::Net::HTTPServerRequest& request, 
                                 Poco::Net::HTTPServerResponse& response);
    
    bool validateRequest(Poco::Net::HTTPServerRequest& request, 
                        Poco::Net::HTTPServerResponse& response,
                        std::string& errorMessage) override;
    
    bool authorizeRequest(Poco::Net::HTTPServerRequest& request, 
                         Poco::Net::HTTPServerResponse& response,
                         std::string& errorMessage) override;
    
    bool validateCellAccess(Poco::Net::HTTPServerRequest& request, 
                           long long cellId,
                           std::string& errorMessage);
    
    long long getCurrentUserId(Poco::Net::HTTPServerRequest& request);
    database::models::UserRole getCurrentUserRole(Poco::Net::HTTPServerRequest& request);
    
private:
    std::unique_ptr<services::WarehouseCellService> cellService;
    std::unique_ptr<services::AuthService> authService;
    std::unique_ptr<services::InventoryService> inventoryService;
    
    bool validateCreateCellData(const Poco::JSON::Object::Ptr& json, 
                               std::vector<std::string>& errors);
    
    bool validateUpdateCellData(const Poco::JSON::Object::Ptr& json, 
                               std::vector<std::string>& errors);
    
    bool validateStorageParameters(const Poco::JSON::Object::Ptr& json, 
                                  std::vector<std::string>& errors);
    
    bool validateSearchParameters(const std::map<std::string, std::string>& filters,
                                 std::vector<std::string>& errors);
    
    void logCellEvent(long long userId, 
                     const std::string& action,
                     long long cellId,
                     const std::string& ipAddress,
                     const std::string& userAgent,
                     bool success,
                     const std::string& details = "");
    
    std::unique_ptr<database::models::WarehouseCell> extractCellFromJson(const Poco::JSON::Object::Ptr& json);
    
    bool canCreateCell(Poco::Net::HTTPServerRequest& request, 
                      const database::models::WarehouseCell& cellData,
                      std::string& errorMessage);
    
    bool canUpdateCell(Poco::Net::HTTPServerRequest& request, 
                      long long cellId,
                      const database::models::WarehouseCell& cellData,
                      std::string& errorMessage);
    
    bool canDeleteCell(Poco::Net::HTTPServerRequest& request, 
                      long long cellId,
                      std::string& errorMessage);
    
    bool canBlockCell(Poco::Net::HTTPServerRequest& request, 
                     long long cellId,
                     std::string& errorMessage);
    
    Poco::JSON::Object buildPaginationResponse(int page, int pageSize, int totalItems, 
                                              const Poco::JSON::Array& data);
};

} // namespace controllers