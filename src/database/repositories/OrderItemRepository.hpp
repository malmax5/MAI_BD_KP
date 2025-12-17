#pragma once

#include "BaseRepository.hpp"
#include "../models/OrderItem.hpp"
#include <memory>
#include <string>
#include <vector>
#include <Poco/JSON/Object.h>
#include <Poco/JSON/Array.h>

namespace warehouse_backend::database::repositories
{

class OrderItemRepository : public BaseRepository<models::OrderItem>
{
public:
    OrderItemRepository();
    ~OrderItemRepository() override = default;
    
    std::unique_ptr<models::OrderItem> findById(long long id) override;
    std::vector<std::unique_ptr<models::OrderItem>> findAll() override;
    std::vector<std::unique_ptr<models::OrderItem>> findPaginated(int page, int pageSize) override;
    long long create(const models::OrderItem& item) override;
    bool update(long long id, const models::OrderItem& item) override;
    bool remove(long long id) override;
    bool softDelete(long long id) override;
    int count() override;
    
    Poco::JSON::Array findAllAsJson() override;
    Poco::JSON::Object findByIdAsJson(long long id) override;
    
    std::vector<std::unique_ptr<models::OrderItem>> findByField(const std::string& fieldName, 
                                                               const std::string& fieldValue) override;
    std::vector<std::unique_ptr<models::OrderItem>> search(const std::string& query, 
                                                          const std::vector<std::string>& fields) override;
    
    std::vector<std::unique_ptr<models::OrderItem>> findByOrderId(long long orderId);
    std::vector<std::unique_ptr<models::OrderItem>> findByProductId(long long productId);
    std::vector<std::unique_ptr<models::OrderItem>> findByBatchId(long long batchId);
    std::vector<std::unique_ptr<models::OrderItem>> findByPickedBy(long long userId);
    std::vector<std::unique_ptr<models::OrderItem>> findByPickingStatus(models::PickingStatus status);
    
    std::vector<std::unique_ptr<models::OrderItem>> findItemsToPick();
    std::vector<std::unique_ptr<models::OrderItem>> findItemsInProgress();
    std::vector<std::unique_ptr<models::OrderItem>> findPickedItems();
    std::vector<std::unique_ptr<models::OrderItem>> findItemsWithLowStock();
    
    bool updateQuantityShipped(long long id, int quantityShipped);
    bool updatePickingStatus(long long id, models::PickingStatus status);
    bool markAsPicked(long long id, long long pickedBy);
    bool updateUnitPrice(long long id, double newUnitPrice);
    bool updateDiscount(long long id, double newDiscountPercent);
    bool updateBatch(long long id, long long newBatchId);
    
    bool shipItem(long long id, int quantity, long long pickedBy);
    bool cancelShipment(long long id);
    
    int countByOrder(long long orderId);
    int countByProduct(long long productId);
    int countByPickingStatus(models::PickingStatus status);
    
    double getTotalOrderValue(long long orderId);
    int getTotalQuantityOrdered(long long orderId);
    int getTotalQuantityShipped(long long orderId);
    
    std::vector<std::pair<long long, std::string>> getItemsForPicking(long long orderId);
    Poco::JSON::Array getPickingReport(long long orderId);
    Poco::JSON::Array getOrderItemSummary(long long orderId);
    
    bool canShipItem(long long id, int quantity);
    bool isItemFullyShipped(long long id);
    
private:
    models::OrderItem mapRowToItem(Poco::Data::Row& row) const;
    
    static const std::string TABLE_NAME;
    static const std::vector<std::string> SEARCH_FIELDS;
};

} // namespace database::repositories
