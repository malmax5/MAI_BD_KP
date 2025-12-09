#include "OrderService.hpp"
#include "../database/ConnectionPool.hpp"
#include <Poco/JSON/Parser.h>
#include <Poco/JSON/Stringifier.h>
#include <Poco/DateTimeFormatter.h>
#include <Poco/DateTimeFormat.h>
#include <Poco/Random.h>
#include <Poco/NumberFormatter.h>
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace warehouse_backend::services
{

OrderCreationResult::OrderCreationResult()
    : success(false), orderId(0), orderNumber(""), order(nullptr)
{
    
}

Poco::JSON::Object OrderCreationResult::toJson() const
{
    Poco::JSON::Object result;
    result.set("success", success);
    result.set("message", message);
    result.set("orderId", static_cast<Poco::Int64>(orderId));
    result.set("orderNumber", orderNumber);
    
    if (order)
    {
        result.set("order", order->toJson());
    }
    
    if (!orderDetails.size())
    {
        result.set("orderDetails", orderDetails);
    }
    
    return result;
}

Poco::JSON::Object OrderUpdateResult::toJson() const
{
    Poco::JSON::Object result;
    result.set("success", success);
    result.set("message", message);
    
    if (updatedOrder)
    {
        result.set("updatedOrder", updatedOrder->toJson());
    }
    
    Poco::JSON::Array changesArray;
    for (const auto& change : changes)
    {
        changesArray.add(change);
    }
    result.set("changes", changesArray);
    
    return result;
}

OrderService::OrderService()
    : orderRepository(std::make_unique<database::repositories::CustomerOrderRepository>()),
      itemRepository(std::make_unique<database::repositories::OrderItemRepository>()),
      paymentRepository(std::make_unique<database::repositories::OrderPaymentRepository>()),
      productRepository(std::make_unique<database::repositories::ProductRepository>()),
      batchRepository(std::make_unique<database::repositories::ProductBatchRepository>()),
      auditRepository(std::make_unique<database::repositories::AuditLogRepository>())
{
}

OrderCreationResult OrderService::createOrder(const database::models::CustomerOrder& order,
                                             const std::vector<database::models::OrderItem>& items,
                                             long long createdBy)
{
    OrderCreationResult result;
    
    std::string itemsError;
    if (!validateOrderItems(items, itemsError))
    {
        result.success = false;
        result.message = "Invalid order items: " + itemsError;
        return result;
    }
    
    for (const auto& item : items)
    {
        auto batch = batchRepository->findById(item.batchId);
        if (!batch)
        {
            result.success = false;
            result.message = "Batch not found with ID: " + std::to_string(item.batchId);
            return result;
        }
        
        if (batch->quantityAvailable < item.quantityOrdered)
        {
            result.success = false;
            result.message = "Insufficient quantity in batch " + batch->batchNumber + 
                            ". Available: " + std::to_string(batch->quantityAvailable) + 
                            ", Ordered: " + std::to_string(item.quantityOrdered);
            return result;
        }
        
        if (batch->productId != item.productId)
        {
            result.success = false;
            result.message = "Batch " + batch->batchNumber + " doesn't match product ID";
            return result;
        }
    }
    
    auto connection = database::ConnectionPool::getInstance().acquireConnection();
    if (!connection)
    {
        result.success = false;
        result.message = "Failed to acquire database connection";
        return result;
    }
    
    try
    {
        connection->beginTransaction();
        
        database::models::CustomerOrder orderToCreate = order;
        orderToCreate.orderNumber = generateOrderNumber();
        orderToCreate.totalAmount = calculateOrderTotal(items);
        orderToCreate.createdBy = createdBy;
        
        if (!orderToCreate.validate())
        {
            connection->rollbackTransaction();
            result.success = false;
            result.message = "Invalid order data";
            return result;
        }
        
        long long orderId = orderRepository->create(orderToCreate);
        
        if (orderId <= 0)
        {
            connection->rollbackTransaction();
            result.success = false;
            result.message = "Failed to create order";
            return result;
        }
        
        for (const auto& item : items)
        {
            database::models::OrderItem itemToCreate = item;
            itemToCreate.orderId = orderId;
            itemToCreate.calculateLineTotal();
            
            long long itemId = itemRepository->create(itemToCreate);
            
            if (itemId <= 0)
            {
                connection->rollbackTransaction();
                result.success = false;
                result.message = "Failed to create order item";
                return result;
            }
            
            auto batch = batchRepository->findById(item.batchId);
            if (batch)
            {
                int newQuantity = batch->quantityAvailable - item.quantityOrdered;
                if (newQuantity < 0)
                {
                    connection->rollbackTransaction();
                    result.success = false;
                    result.message = "Insufficient quantity after reservation";
                    return result;
                }
                
                database::models::ProductBatch updatedBatch = *batch;
                updatedBatch.quantityAvailable = newQuantity;
                
                if (!batchRepository->update(item.batchId, updatedBatch))
                {
                    connection->rollbackTransaction();
                    result.success = false;
                    result.message = "Failed to update batch quantity";
                    return result;
                }
            }
        }
        
        updateOrderTotal(orderId);
        
        auto createdOrder = orderRepository->getOrderWithItems(orderId);
        
        if (!createdOrder)
        {
            connection->rollbackTransaction();
            result.success = false;
            result.message = "Failed to retrieve created order";
            return result;
        }
        
        logOrderAudit("CREATE", orderId, createdBy, 
                     "Order created with " + std::to_string(items.size()) + " items");
        
        connection->commitTransaction();
        
        result.success = true;
        result.orderId = orderId;
        result.orderNumber = orderToCreate.orderNumber;
        result.message = "Order created successfully";
        result.order = std::move(createdOrder);
        
        result.orderDetails.set("orderId", static_cast<Poco::Int64>(orderId));
        result.orderDetails.set("orderNumber", orderToCreate.orderNumber);
        result.orderDetails.set("totalAmount", orderToCreate.totalAmount);
        result.orderDetails.set("itemCount", static_cast<int>(items.size()));
        result.orderDetails.set("createdAt", utils::DateUtils::formatDateTime(utils::DateUtils::now()));
        
    }
    catch (const std::exception& e)
    {
        connection->rollbackTransaction();
        result.success = false;
        result.message = "Error creating order: " + std::string(e.what());
    }
    
    return result;
}

OrderUpdateResult OrderService::updateOrderStatus(long long orderId,
                                                 const std::string& newStatus,
                                                 long long updatedBy,
                                                 const std::string& notes)
{
    OrderUpdateResult result;
    
    try
    {
        auto order = orderRepository->findById(orderId);
        if (!order)
        {
            result.success = false;
            result.message = "Order not found with ID: " + std::to_string(orderId);
            return result;
        }
        
        std::string oldStatus = database::models::CustomerOrder::statusToString(order->status);
        
        if (oldStatus == newStatus)
        {
            result.success = false;
            result.message = "Order already has status: " + newStatus;
            return result;
        }
        
        if (order->status == database::models::OrderStatus::CANCELLED)
        {
            result.success = false;
            result.message = "Cannot update status of cancelled order";
            return result;
        }
        
        if (order->status == database::models::OrderStatus::DELIVERED && 
            newStatus != "delivered")
        {
            result.success = false;
            result.message = "Cannot change status from delivered";
            return result;
        }
        
        database::models::OrderStatus statusEnum = 
            database::models::CustomerOrder::stringToStatus(newStatus);
        
        bool updateSuccess = orderRepository->updateStatus(orderId, statusEnum);
        
        if (updateSuccess)
        {
            result.success = true;
            result.message = "Order status updated successfully";
            result.updatedOrder = orderRepository->findById(orderId);
            result.changes.push_back("Status changed from " + oldStatus + " to " + newStatus);
            
            if (!notes.empty())
            {
                database::models::CustomerOrder updatedOrder = *result.updatedOrder;
                if (updatedOrder.notes.isNull())
                {
                    updatedOrder.notes = notes;
                }
                else
                {
                    updatedOrder.notes = updatedOrder.notes.value() + "\n" + notes;
                }
                orderRepository->update(orderId, updatedOrder);
                result.updatedOrder = orderRepository->findById(orderId);
                result.changes.push_back("Notes updated");
            }
            
            logOrderAudit("UPDATE_STATUS", orderId, updatedBy, 
                         "Status changed from " + oldStatus + " to " + newStatus + 
                         (notes.empty() ? "" : " (" + notes + ")"));
        }
        else
        {
            result.success = false;
            result.message = "Failed to update order status";
        }
    }
    catch (const std::exception& e)
    {
        result.success = false;
        result.message = "Error updating order status: " + std::string(e.what());
    }
    
    return result;
}

OrderUpdateResult OrderService::updateOrderPriority(long long orderId,
                                                   const std::string& newPriority,
                                                   long long updatedBy)
{
    OrderUpdateResult result;
    
    try
    {
        auto order = orderRepository->findById(orderId);
        if (!order)
        {
            result.success = false;
            result.message = "Order not found with ID: " + std::to_string(orderId);
            return result;
        }
        
        std::string oldPriority = database::models::CustomerOrder::priorityToString(order->priority);
        
        if (oldPriority == newPriority)
        {
            result.success = false;
            result.message = "Order already has priority: " + newPriority;
            return result;
        }
        
        database::models::OrderPriority priorityEnum = 
            database::models::CustomerOrder::stringToPriority(newPriority);
        
        bool updateSuccess = orderRepository->updatePriority(orderId, priorityEnum);
        
        if (updateSuccess)
        {
            result.success = true;
            result.message = "Order priority updated successfully";
            result.updatedOrder = orderRepository->findById(orderId);
            result.changes.push_back("Priority changed from " + oldPriority + " to " + newPriority);
            
            logOrderAudit("UPDATE_PRIORITY", orderId, updatedBy, 
                         "Priority changed from " + oldPriority + " to " + newPriority);
        }
        else
        {
            result.success = false;
            result.message = "Failed to update order priority";
        }
    }
    catch (const std::exception& e)
    {
        result.success = false;
        result.message = "Error updating order priority: " + std::string(e.what());
    }
    
    return result;
}

OrderUpdateResult OrderService::addOrderItem(long long orderId,
                                            const database::models::OrderItem& item,
                                            long long addedBy)
{
    OrderUpdateResult result;
    
    auto connection = database::ConnectionPool::getInstance().acquireConnection();
    if (!connection)
    {
        result.success = false;
        result.message = "Failed to acquire database connection";
        return result;
    }
    
    try
    {
        connection->beginTransaction();
        
        auto order = orderRepository->findById(orderId);
        if (!order)
        {
            connection->rollbackTransaction();
            result.success = false;
            result.message = "Order not found with ID: " + std::to_string(orderId);
            return result;
        }
        
        if (order->status == database::models::OrderStatus::CANCELLED ||
            order->status == database::models::OrderStatus::DELIVERED ||
            order->status == database::models::OrderStatus::SHIPPED)
        {
            connection->rollbackTransaction();
            result.success = false;
            result.message = "Cannot add items to order with status: " + 
                            database::models::CustomerOrder::statusToString(order->status);
            return result;
        }
        
        auto batch = batchRepository->findById(item.batchId);
        if (!batch)
        {
            connection->rollbackTransaction();
            result.success = false;
            result.message = "Batch not found with ID: " + std::to_string(item.batchId);
            return result;
        }
        
        if (batch->quantityAvailable < item.quantityOrdered)
        {
            connection->rollbackTransaction();
            result.success = false;
            result.message = "Insufficient quantity in batch " + batch->batchNumber;
            return result;
        }
        
        auto product = productRepository->findById(item.productId);
        if (!product)
        {
            connection->rollbackTransaction();
            result.success = false;
            result.message = "Product not found with ID: " + std::to_string(item.productId);
            return result;
        }
        
        database::models::OrderItem itemToCreate = item;
        itemToCreate.orderId = orderId;
        itemToCreate.calculateLineTotal();
        
        long long itemId = itemRepository->create(itemToCreate);
        
        if (itemId <= 0)
        {
            connection->rollbackTransaction();
            result.success = false;
            result.message = "Failed to create order item";
            return result;
        }
        
        database::models::ProductBatch updatedBatch = *batch;
        updatedBatch.quantityAvailable -= item.quantityOrdered;
        
        if (!batchRepository->update(item.batchId, updatedBatch))
        {
            connection->rollbackTransaction();
            result.success = false;
            result.message = "Failed to update batch quantity";
            return result;
        }
        
        updateOrderTotal(orderId);
        
        connection->commitTransaction();
        
        result.success = true;
        result.message = "Order item added successfully";
        result.updatedOrder = orderRepository->findById(orderId);
        result.changes.push_back("Added item: " + product->name + " x" + 
                                std::to_string(item.quantityOrdered));
        
        logOrderAudit("ADD_ITEM", orderId, addedBy, 
                     "Added product " + product->name + " (ID: " + 
                     std::to_string(product->id) + ") x" + 
                     std::to_string(item.quantityOrdered));
        
    }
    catch (const std::exception& e)
    {
        connection->rollbackTransaction();
        result.success = false;
        result.message = "Error adding order item: " + std::string(e.what());
    }
    
    return result;
}

OrderUpdateResult OrderService::updateOrderItem(long long orderId,
                                               long long itemId,
                                               const database::models::OrderItem& updatedItem,
                                               long long updatedBy)
{
    OrderUpdateResult result;
    
    auto connection = database::ConnectionPool::getInstance().acquireConnection();
    if (!connection)
    {
        result.success = false;
        result.message = "Failed to acquire database connection";
        return result;
    }
    
    try
    {
        connection->beginTransaction();
        
        auto order = orderRepository->findById(orderId);
        if (!order)
        {
            connection->rollbackTransaction();
            result.success = false;
            result.message = "Order not found with ID: " + std::to_string(orderId);
            return result;
        }
        
        auto existingItem = itemRepository->findById(itemId);
        if (!existingItem)
        {
            connection->rollbackTransaction();
            result.success = false;
            result.message = "Order item not found with ID: " + std::to_string(itemId);
            return result;
        }
        
        if (existingItem->orderId != orderId)
        {
            connection->rollbackTransaction();
            result.success = false;
            result.message = "Item does not belong to order " + std::to_string(orderId);
            return result;
        }
        
        if (order->status == database::models::OrderStatus::CANCELLED ||
            order->status == database::models::OrderStatus::DELIVERED ||
            order->status == database::models::OrderStatus::SHIPPED)
        {
            connection->rollbackTransaction();
            result.success = false;
            result.message = "Cannot update items in order with status: " + 
                            database::models::CustomerOrder::statusToString(order->status);
            return result;
        }
        
        if (existingItem->quantityOrdered != updatedItem.quantityOrdered ||
            existingItem->batchId != updatedItem.batchId)
        {
            auto oldBatch = batchRepository->findById(existingItem->batchId);
            if (oldBatch)
            {
                database::models::ProductBatch updatedOldBatch = *oldBatch;
                updatedOldBatch.quantityAvailable += existingItem->quantityOrdered;
                
                if (!batchRepository->update(existingItem->batchId, updatedOldBatch))
                {
                    connection->rollbackTransaction();
                    result.success = false;
                    result.message = "Failed to return old batch quantity";
                    return result;
                }
            }
            
            auto newBatch = batchRepository->findById(updatedItem.batchId);
            if (!newBatch)
            {
                connection->rollbackTransaction();
                result.success = false;
                result.message = "New batch not found with ID: " + std::to_string(updatedItem.batchId);
                return result;
            }
            
            if (newBatch->quantityAvailable < updatedItem.quantityOrdered)
            {
                connection->rollbackTransaction();
                result.success = false;
                result.message = "Insufficient quantity in batch " + newBatch->batchNumber;
                return result;
            }
            
            database::models::ProductBatch updatedNewBatch = *newBatch;
            updatedNewBatch.quantityAvailable -= updatedItem.quantityOrdered;
            
            if (!batchRepository->update(updatedItem.batchId, updatedNewBatch))
            {
                connection->rollbackTransaction();
                result.success = false;
                result.message = "Failed to reserve new batch quantity";
                return result;
            }
            
            result.changes.push_back("Quantity changed from " + 
                                    std::to_string(existingItem->quantityOrdered) + 
                                    " to " + std::to_string(updatedItem.quantityOrdered));
            
            if (existingItem->batchId != updatedItem.batchId)
            {
                result.changes.push_back("Batch changed from " + 
                                        std::to_string(existingItem->batchId) + 
                                        " to " + std::to_string(updatedItem.batchId));
            }
        }
        
        database::models::OrderItem itemToUpdate = updatedItem;
        itemToUpdate.id = itemId;
        itemToUpdate.orderId = orderId;
        itemToUpdate.calculateLineTotal();
        
        bool updateSuccess = itemRepository->update(itemId, itemToUpdate);
        
        if (!updateSuccess)
        {
            connection->rollbackTransaction();
            result.success = false;
            result.message = "Failed to update order item";
            return result;
        }
        
        updateOrderTotal(orderId);
        
        connection->commitTransaction();
        
        result.success = true;
        result.message = "Order item updated successfully";
        result.updatedOrder = orderRepository->findById(orderId);
        
        logOrderAudit("UPDATE_ITEM", orderId, updatedBy, 
                     "Updated item ID: " + std::to_string(itemId));
        
    }
    catch (const std::exception& e)
    {
        connection->rollbackTransaction();
        result.success = false;
        result.message = "Error updating order item: " + std::string(e.what());
    }
    
    return result;
}

OrderUpdateResult OrderService::removeOrderItem(long long orderId,
                                               long long itemId,
                                               long long removedBy)
{
    OrderUpdateResult result;
    
    auto connection = database::ConnectionPool::getInstance().acquireConnection();
    if (!connection)
    {
        result.success = false;
        result.message = "Failed to acquire database connection";
        return result;
    }
    
    try
    {
        connection->beginTransaction();
        
        auto order = orderRepository->findById(orderId);
        if (!order)
        {
            connection->rollbackTransaction();
            result.success = false;
            result.message = "Order not found with ID: " + std::to_string(orderId);
            return result;
        }
        
        auto item = itemRepository->findById(itemId);
        if (!item)
        {
            connection->rollbackTransaction();
            result.success = false;
            result.message = "Order item not found with ID: " + std::to_string(itemId);
            return result;
        }
        
        if (item->orderId != orderId)
        {
            connection->rollbackTransaction();
            result.success = false;
            result.message = "Item does not belong to order " + std::to_string(orderId);
            return result;
        }
        
        if (order->status == database::models::OrderStatus::CANCELLED ||
            order->status == database::models::OrderStatus::DELIVERED ||
            order->status == database::models::OrderStatus::SHIPPED)
        {
            connection->rollbackTransaction();
            result.success = false;
            result.message = "Cannot remove items from order with status: " + 
                            database::models::CustomerOrder::statusToString(order->status);
            return result;
        }
        
        auto batch = batchRepository->findById(item->batchId);
        if (batch)
        {
            database::models::ProductBatch updatedBatch = *batch;
            updatedBatch.quantityAvailable += item->quantityOrdered;
            
            if (!batchRepository->update(item->batchId, updatedBatch))
            {
                connection->rollbackTransaction();
                result.success = false;
                result.message = "Failed to return batch quantity";
                return result;
            }
        }
        
        bool deleteSuccess = itemRepository->remove(itemId);
        
        if (!deleteSuccess)
        {
            connection->rollbackTransaction();
            result.success = false;
            result.message = "Failed to remove order item";
            return result;
        }
        
        updateOrderTotal(orderId);
        
        connection->commitTransaction();
        
        result.success = true;
        result.message = "Order item removed successfully";
        result.updatedOrder = orderRepository->findById(orderId);
        result.changes.push_back("Removed item ID: " + std::to_string(itemId));
        
        logOrderAudit("REMOVE_ITEM", orderId, removedBy, 
                     "Removed item ID: " + std::to_string(itemId));
        
    }
    catch (const std::exception& e)
    {
        connection->rollbackTransaction();
        result.success = false;
        result.message = "Error removing order item: " + std::string(e.what());
    }
    
    return result;
}

Poco::JSON::Object OrderService::getOrderDetails(long long orderId)
{
    Poco::JSON::Object result;
    
    try
    {
        auto order = orderRepository->findById(orderId);
        if (!order)
        {
            result.set("error", "Order not found with ID: " + std::to_string(orderId));
            return result;
        }
        
        result = order->toJson();
        
        if (order->createdBy > 0)
        {
            result.set("createdByUserId", static_cast<Poco::Int64>(order->createdBy));
        }
        
        result.set("retrievedAt", utils::DateUtils::formatDateTime(utils::DateUtils::now()));
        
    }
    catch (const std::exception& e)
    {
        result.set("error", "Error retrieving order details: " + std::string(e.what()));
    }
    
    return result;
}

Poco::JSON::Object OrderService::getOrderWithItems(long long orderId)
{
    Poco::JSON::Object result;
    
    try
    {
        auto order = orderRepository->getOrderWithItems(orderId);
        if (!order)
        {
            result.set("error", "Order not found with ID: " + std::to_string(orderId));
            return result;
        }
        
        result = order->toJsonWithItems();
        result.set("retrievedAt", utils::DateUtils::formatDateTime(utils::DateUtils::now()));
        
    }
    catch (const std::exception& e)
    {
        result.set("error", "Error retrieving order with items: " + std::string(e.what()));
    }
    
    return result;
}

Poco::JSON::Object OrderService::getOrderWithItemsAndShipments(long long orderId)
{
    Poco::JSON::Object result;
    
    try
    {
        auto order = orderRepository->getOrderWithItemsAndShipments(orderId);
        if (!order)
        {
            result.set("error", "Order not found with ID: " + std::to_string(orderId));
            return result;
        }
        
        result = order->toJsonWithItems();
        
        result.set("retrievedAt", utils::DateUtils::formatDateTime(utils::DateUtils::now()));
        
    }
    catch (const std::exception& e)
    {
        result.set("error", "Error retrieving order with items and shipments: " + std::string(e.what()));
    }
    
    return result;
}

Poco::JSON::Array OrderService::findOrdersByCustomer(const std::string& customerEmail,
                                                    int page,
                                                    int pageSize)
{
    Poco::JSON::Array result;
    
    try
    {
        auto orders = orderRepository->findByCustomerEmail(customerEmail);
        
        int startIndex = (page - 1) * pageSize;
        int endIndex = std::min(static_cast<int>(orders.size()), startIndex + pageSize);
        
        for (int i = startIndex; i < endIndex && i < static_cast<int>(orders.size()); ++i)
        {
            result.add(orders[i]->toJson());
        }
        
        if (orders.size() > 0)
        {
            Poco::JSON::Object metadata;
            metadata.set("totalOrders", static_cast<int>(orders.size()));
            metadata.set("page", page);
            metadata.set("pageSize", pageSize);
            metadata.set("totalPages", (orders.size() + pageSize - 1) / pageSize);
            result.add(metadata);
        }
        
    }
    catch (const std::exception& e)
    {
        Poco::JSON::Object error;
        error.set("error", "Error finding orders by customer: " + std::string(e.what()));
        result.add(error);
    }
    
    return result;
}

Poco::JSON::Array OrderService::findOrdersByStatus(const std::string& status,
                                                  int page,
                                                  int pageSize)
{
    Poco::JSON::Array result;
    
    try
    {
        database::models::OrderStatus statusEnum = 
            database::models::CustomerOrder::stringToStatus(status);
        
        auto orders = orderRepository->findByStatus(statusEnum);
        
        int startIndex = (page - 1) * pageSize;
        int endIndex = std::min(static_cast<int>(orders.size()), startIndex + pageSize);
        
        for (int i = startIndex; i < endIndex && i < static_cast<int>(orders.size()); ++i)
        {
            result.add(orders[i]->toJson());
        }
        
        if (orders.size() > 0)
        {
            Poco::JSON::Object metadata;
            metadata.set("totalOrders", static_cast<int>(orders.size()));
            metadata.set("page", page);
            metadata.set("pageSize", pageSize);
            metadata.set("totalPages", (orders.size() + pageSize - 1) / pageSize);
            result.add(metadata);
        }
        
    }
    catch (const std::exception& e)
    {
        Poco::JSON::Object error;
        error.set("error", "Error finding orders by status: " + std::string(e.what()));
        result.add(error);
    }
    
    return result;
}

Poco::JSON::Array OrderService::findOrdersByDateRange(const std::string& startDate,
                                                     const std::string& endDate,
                                                     int page,
                                                     int pageSize)
{
    Poco::JSON::Array result;
    
    try
    {
        auto orders = orderRepository->findByDateRange(startDate, endDate);
        
        int startIndex = (page - 1) * pageSize;
        int endIndex = std::min(static_cast<int>(orders.size()), startIndex + pageSize);
        
        for (int i = startIndex; i < endIndex && i < static_cast<int>(orders.size()); ++i)
        {
            result.add(orders[i]->toJson());
        }
        
        if (orders.size() > 0)
        {
            Poco::JSON::Object metadata;
            metadata.set("totalOrders", static_cast<int>(orders.size()));
            metadata.set("page", page);
            metadata.set("pageSize", pageSize);
            metadata.set("totalPages", (orders.size() + pageSize - 1) / pageSize);
            metadata.set("startDate", startDate);
            metadata.set("endDate", endDate);
            result.add(metadata);
        }
        
    }
    catch (const std::exception& e)
    {
        Poco::JSON::Object error;
        error.set("error", "Error finding orders by date range: " + std::string(e.what()));
        result.add(error);
    }
    
    return result;
}

Poco::JSON::Array OrderService::getPendingOrders(int page, int pageSize)
{
    Poco::JSON::Array result;
    
    try
    {
        auto orders = orderRepository->findPendingOrders();
        
        int startIndex = (page - 1) * pageSize;
        int endIndex = std::min(static_cast<int>(orders.size()), startIndex + pageSize);
        
        for (int i = startIndex; i < endIndex && i < static_cast<int>(orders.size()); ++i)
        {
            result.add(orders[i]->toJson());
        }
        
        if (orders.size() > 0)
        {
            Poco::JSON::Object metadata;
            metadata.set("totalOrders", static_cast<int>(orders.size()));
            metadata.set("page", page);
            metadata.set("pageSize", pageSize);
            metadata.set("totalPages", (orders.size() + pageSize - 1) / pageSize);
            result.add(metadata);
        }
        
    }
    catch (const std::exception& e)
    {
        Poco::JSON::Object error;
        error.set("error", "Error retrieving pending orders: " + std::string(e.what()));
        result.add(error);
    }
    
    return result;
}

Poco::JSON::Array OrderService::getUrgentOrders(int page, int pageSize)
{
    Poco::JSON::Array result;
    
    try
    {
        auto orders = orderRepository->findUrgentOrders();
        
        int startIndex = (page - 1) * pageSize;
        int endIndex = std::min(static_cast<int>(orders.size()), startIndex + pageSize);
        
        for (int i = startIndex; i < endIndex && i < static_cast<int>(orders.size()); ++i)
        {
            result.add(orders[i]->toJson());
        }
        
        if (orders.size() > 0)
        {
            Poco::JSON::Object metadata;
            metadata.set("totalOrders", static_cast<int>(orders.size()));
            metadata.set("page", page);
            metadata.set("pageSize", pageSize);
            metadata.set("totalPages", (orders.size() + pageSize - 1) / pageSize);
            result.add(metadata);
        }
        
    }
    catch (const std::exception& e)
    {
        Poco::JSON::Object error;
        error.set("error", "Error retrieving urgent orders: " + std::string(e.what()));
        result.add(error);
    }
    
    return result;
}

bool OrderService::cancelOrder(long long orderId, long long cancelledBy, const std::string& reason)
{
    try
    {
        auto order = orderRepository->findById(orderId);
        if (!order)
        {
            return false;
        }
        
        if (!order->canBeCancelled())
        {
            return false;
        }
        
        auto connection = database::ConnectionPool::getInstance().acquireConnection();
        if (!connection)
        {
            return false;
        }
        
        connection->beginTransaction();
        
        auto items = itemRepository->findByOrderId(orderId);
        for (const auto& item : items)
        {
            auto batch = batchRepository->findById(item->batchId);
            if (batch)
            {
                database::models::ProductBatch updatedBatch = *batch;
                updatedBatch.quantityAvailable += item->quantityOrdered;
                
                if (!batchRepository->update(item->batchId, updatedBatch))
                {
                    connection->rollbackTransaction();
                    return false;
                }
            }
        }
        
        bool success = orderRepository->cancelOrder(orderId);
        
        if (!success)
        {
            connection->rollbackTransaction();
            return false;
        }
        
        if (!reason.empty())
        {
            database::models::CustomerOrder updatedOrder = *order;
            if (updatedOrder.notes.isNull())
            {
                updatedOrder.notes = "Cancelled: " + reason;
            }
            else
            {
                updatedOrder.notes = updatedOrder.notes.value() + "\nCancelled: " + reason;
            }
            orderRepository->update(orderId, updatedOrder);
        }
        
        logOrderAudit("CANCEL", orderId, cancelledBy, 
                     "Order cancelled" + (reason.empty() ? "" : ": " + reason));
        
        connection->commitTransaction();
        return true;
        
    }
    catch (const std::exception&)
    {
        return false;
    }
}

bool OrderService::markOrderAsShipped(long long orderId, long long shippedBy)
{
    try
    {
        auto order = orderRepository->findById(orderId);
        if (!order)
        {
            return false;
        }
        
        if (order->status != database::models::OrderStatus::PACKED &&
            order->status != database::models::OrderStatus::PROCESSING)
        {
            return false;
        }
        
        bool success = orderRepository->markAsShipped(orderId);
        
        if (success)
        {
            logOrderAudit("SHIP", orderId, shippedBy, "Order marked as shipped");
        }
        
        return success;
        
    }
    catch (const std::exception&)
    {
        return false;
    }
}

bool OrderService::markOrderAsDelivered(long long orderId, const std::string& actualDeliveryDate)
{
    try
    {
        auto order = orderRepository->findById(orderId);
        if (!order)
        {
            return false;
        }
        
        if (order->status != database::models::OrderStatus::SHIPPED)
        {
            return false;
        }
        
        bool success = orderRepository->markAsDelivered(orderId);
        
        if (success && !actualDeliveryDate.empty())
        {
            database::models::CustomerOrder updatedOrder = *order;
            updatedOrder.actualDeliveryDate = actualDeliveryDate;
            orderRepository->update(orderId, updatedOrder);
        }
        
        if (success)
        {
            logOrderAudit("DELIVER", orderId, 0, "Order marked as delivered");
        }
        
        return success;
        
    }
    catch (const std::exception&)
    {
        return false;
    }
}

Poco::JSON::Object OrderService::getOrderStatistics()
{
    Poco::JSON::Object stats;
    
    try
    {
        auto statistics = orderRepository->getOrderStatistics();
        if (statistics.size() > 0)
        {
            stats = *statistics.getObject(0);
        
            int totalOrders = orderRepository->count();
            double totalRevenue = orderRepository->getTotalRevenue();
            double avgOrderValue = orderRepository->getAverageOrderValue();

            stats.set("totalOrders", totalOrders);
            stats.set("totalRevenue", totalRevenue);
            stats.set("averageOrderValue", avgOrderValue);
            stats.set("calculatedAt", utils::DateUtils::formatDateTime(utils::DateUtils::now()));
        }
        
    }
    catch (const std::exception& e)
    {
        stats.set("error", "Error calculating order statistics: " + std::string(e.what()));
    }
    
    return stats;
}

Poco::JSON::Array OrderService::getRevenueReport(const std::string& startDate,
                                                const std::string& endDate)
{
    Poco::JSON::Array result;
    
    try
    {
        result = orderRepository->getRevenueReport(startDate, endDate);
        
        if (result.size() > 0)
        {
            double totalRevenue = 0.0;
            int totalOrders = 0;
            
            for (size_t i = 0; i < result.size(); ++i)
            {
                auto dayData = result.getObject(i);
                totalRevenue += dayData->get("dailyRevenue");
                totalOrders += dayData->get("totalOrders");
            }
            
            Poco::JSON::Object summary;
            summary.set("periodStart", startDate);
            summary.set("periodEnd", endDate);
            summary.set("totalRevenue", totalRevenue);
            summary.set("totalOrders", totalOrders);
            summary.set("averageDailyRevenue", totalRevenue / result.size());
            summary.set("reportGenerated", utils::DateUtils::formatDateTime(utils::DateUtils::now()));
            
            result.add(summary);
        }
        
    }
    catch (const std::exception& e)
    {
        Poco::JSON::Object error;
        error.set("error", "Error generating revenue report: " + std::string(e.what()));
        result.add(error);
    }
    
    return result;
}

Poco::JSON::Array OrderService::getCustomerOrderHistory(const std::string& customerEmail)
{
    Poco::JSON::Array result;
    
    try
    {
        result = orderRepository->getCustomerOrderHistory(customerEmail);
        
        if (result.size() > 0)
        {
            double totalSpent = 0.0;
            int totalOrders = result.size();
            std::string lastOrderDate = "";
            
            for (size_t i = 0; i < result.size(); ++i)
            {
                auto orderData = result.getObject(i);
                totalSpent += orderData->get("totalAmount");
                
                if (i == 0)
                {
                    lastOrderDate = orderData->get("orderDate").toString();
                }
            }
            
            Poco::JSON::Object customerSummary;
            customerSummary.set("customerEmail", customerEmail);
            customerSummary.set("totalOrders", totalOrders);
            customerSummary.set("totalSpent", totalSpent);
            customerSummary.set("averageOrderValue", totalSpent / totalOrders);
            customerSummary.set("lastOrderDate", lastOrderDate);
            customerSummary.set("reportGenerated", utils::DateUtils::formatDateTime(utils::DateUtils::now()));
            
            result.add(customerSummary);
        }
        
    }
    catch (const std::exception& e)
    {
        Poco::JSON::Object error;
        error.set("error", "Error generating customer order history: " + std::string(e.what()));
        result.add(error);
    }
    
    return result;
}

double OrderService::calculateOrderTotal(const std::vector<database::models::OrderItem>& items)
{
    double total = 0.0;
    
    for (const auto& item : items)
    {
        total += item.calculateLineTotal();
    }
    
    return total;
}

bool OrderService::validateOrderItems(const std::vector<database::models::OrderItem>& items,
                                     std::string& errorMessage)
{
    if (items.empty())
    {
        errorMessage = "Order must have at least one item";
        return false;
    }
    
    for (const auto& item : items)
    {
        if (!item.validate())
        {
            errorMessage = "Invalid order item data";
            return false;
        }
        
        if (item.quantityOrdered <= 0)
        {
            errorMessage = "Quantity must be greater than 0";
            return false;
        }
        
        if (item.unitPrice < 0)
        {
            errorMessage = "Unit price cannot be negative";
            return false;
        }
        
        if (item.discountPercent < 0 || item.discountPercent > 100)
        {
            errorMessage = "Discount must be between 0 and 100 percent";
            return false;
        }
    }
    
    return true;
}

std::string OrderService::generateOrderNumber()
{
    Poco::Random rng;
    rng.seed();
    
    auto now = utils::DateUtils::now();
    std::string datePart = Poco::DateTimeFormatter::format(now, "%Y%m%d");
    
    int randomPart = rng.next(99999);
    
    return "ORD-" + datePart + "-" + std::to_string(randomPart);
}

void OrderService::updateOrderTotal(long long orderId)
{
    try
    {
        auto items = itemRepository->findByOrderId(orderId);
        double total = 0.0;
        
        for (const auto& item : items)
        {
            total += item->lineTotal;
        }
        
        orderRepository->updateTotalAmount(orderId, total);
        
    }
    catch (const std::exception&)
    {
    }
}

void OrderService::logOrderAudit(const std::string& action,
                                long long orderId,
                                long long changedBy,
                                const std::string& details)
{
    try
    {
        database::models::AuditLog auditLog;
        auditLog.tableName = "customer_orders";
        auditLog.recordId = orderId;
        auditLog.action = database::models::AuditAction::UPDATE;
        auditLog.changedBy = changedBy;
        auditLog.description = "Order " + action + ": " + details;
        auditLog.ipAddress = "127.0.0.1";
        auditLog.userAgent = "OrderService";
        
        auditRepository->create(auditLog);
    }
    catch (const std::exception&)
    {
    }
}

} // namespace services
