#pragma once

#include "../database/repositories/CustomerOrderRepository.hpp"
#include "../database/repositories/ProductRepository.hpp"
#include "../database/repositories/InventoryMovementRepository.hpp"
#include "../database/repositories/ShipmentRepository.hpp"
#include "../config/ConfigManager.hpp"
#include "../utils/DateUtils.hpp"
#include "../utils/JsonUtils.hpp"
#include <memory>
#include <string>
#include <Poco/JSON/Object.h>
#include <Poco/JSON/Array.h>

namespace warehouse_backend::services
{

struct ReportResult
{
    bool success;
    std::string message;
    Poco::JSON::Object reportData;
    
    ReportResult();
    Poco::JSON::Object toJson() const;
};

class ReportService
{
public:
    ReportService();
    ~ReportService() = default;
    
    ReportResult getWarehouseOccupancyReport();
    ReportResult getInventoryValueReport();
    ReportResult getDailyStatisticsReport(const std::string& date);
    ReportResult getSalesReport(const std::string& startDate, const std::string& endDate);
    ReportResult getStockMovementReport(const std::string& startDate, const std::string& endDate);
    ReportResult getExpiringProductsReport(int daysThreshold);
    ReportResult getSupplierPerformanceReport();
    ReportResult getCustomerOrderReport(const std::string& customerEmail);
    
private:
    std::unique_ptr<database::repositories::CustomerOrderRepository> orderRepository;
    std::unique_ptr<database::repositories::ProductRepository> productRepository;
    std::unique_ptr<database::repositories::InventoryMovementRepository> movementRepository;
    std::unique_ptr<database::repositories::ShipmentRepository> shipmentRepository;
};

} // namespace services