#pragma once

#include "BaseRepository.hpp"
#include "../models/CustomerOrder.hpp"
#include <memory>
#include <string>
#include <vector>
#include <Poco/JSON/Object.h>
#include <Poco/JSON/Array.h>

namespace warehouse_backend::database::repositories
{

class CustomerOrderRepository : public BaseRepository<models::CustomerOrder>
{
public:
    CustomerOrderRepository();
    ~CustomerOrderRepository() override = default;
    
    std::unique_ptr<models::CustomerOrder> findById(long long id) override;
    std::vector<std::unique_ptr<models::CustomerOrder>> findAll() override;
    std::vector<std::unique_ptr<models::CustomerOrder>> findPaginated(int page, int pageSize) override;
    long long create(const models::CustomerOrder& order) override;
    bool update(long long id, const models::CustomerOrder& order) override;
    bool remove(long long id) override;
    bool softDelete(long long id) override;
    int count() override;
    
    Poco::JSON::Array findAllAsJson() override;
    Poco::JSON::Object findByIdAsJson(long long id) override;
    
    std::vector<std::unique_ptr<models::CustomerOrder>> findByField(const std::string& fieldName, 
                                                                    const std::string& fieldValue) override;
    std::vector<std::unique_ptr<models::CustomerOrder>> search(const std::string& query, 
                                                               const std::vector<std::string>& fields) override;
    
    std::unique_ptr<models::CustomerOrder> findByOrderNumber(const std::string& orderNumber);
    std::vector<std::unique_ptr<models::CustomerOrder>> findByCustomerEmail(const std::string& email);
    std::vector<std::unique_ptr<models::CustomerOrder>> findByCustomerPhone(const std::string& phone);
    std::vector<std::unique_ptr<models::CustomerOrder>> findByStatus(models::OrderStatus status);
    std::vector<std::unique_ptr<models::CustomerOrder>> findByPriority(models::OrderPriority priority);
    std::vector<std::unique_ptr<models::CustomerOrder>> findByCreatedBy(long long userId);
    std::vector<std::unique_ptr<models::CustomerOrder>> findByDateRange(const std::string& startDate, const std::string& endDate);
    
    std::vector<std::unique_ptr<models::CustomerOrder>> findPendingOrders();
    std::vector<std::unique_ptr<models::CustomerOrder>> findActiveOrders();
    std::vector<std::unique_ptr<models::CustomerOrder>> findCompletedOrders();
    std::vector<std::unique_ptr<models::CustomerOrder>> findUrgentOrders();
    
    bool updateStatus(long long id, models::OrderStatus newStatus);
    bool updatePriority(long long id, models::OrderPriority newPriority);
    bool updateDeliveryDate(long long id, const std::string& estimatedDelivery, const std::string& actualDelivery = "");
    bool updateTotalAmount(long long id, double newTotalAmount);
    bool updateCustomerInfo(long long id, const std::string& name, const std::string& email, const std::string& phone);
    
    bool cancelOrder(long long id);
    bool markAsShipped(long long id);
    bool markAsDelivered(long long id);
    bool markAsProcessing(long long id);
    
    int countByStatus(models::OrderStatus status);
    int countByPriority(models::OrderPriority priority);
    int countByCustomer(const std::string& customerEmail);
    int countByUser(long long userId);
    
    double getTotalRevenue();
    double getAverageOrderValue();
    double getMonthlyRevenue(int year, int month);
    
    std::unique_ptr<models::CustomerOrder> getOrderWithItems(long long id);
    std::unique_ptr<models::CustomerOrder> getOrderWithItemsAndShipments(long long id);
    
    Poco::JSON::Array getOrderStatistics();
    Poco::JSON::Array getRevenueReport(const std::string& startDate, const std::string& endDate);
    Poco::JSON::Array getCustomerOrderHistory(const std::string& customerEmail);
    
    bool orderNumberExists(const std::string& orderNumber);
    
    std::vector<std::pair<long long, std::string>> getActiveOrderNumbers();
    std::vector<std::string> getUniqueCustomers();
    
private:
    models::CustomerOrder mapRowToOrder(Poco::Data::Row& row) const;
    std::string generateOrderNumber();
    
    static const std::string TABLE_NAME;
    static const std::vector<std::string> SEARCH_FIELDS;
};

} // namespace database::repositories
