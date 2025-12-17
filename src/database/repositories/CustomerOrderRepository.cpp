#include "CustomerOrderRepository.hpp"
#include "../models/CustomerOrder.hpp"
#include "../models/OrderItem.hpp"
#include "OrderItemRepository.hpp"
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

const std::string CustomerOrderRepository::TABLE_NAME = "customer_orders";
const std::vector<std::string> CustomerOrderRepository::SEARCH_FIELDS = {
    "order_number", "customer_name", "customer_email", "customer_phone"
};

CustomerOrderRepository::CustomerOrderRepository() : BaseRepository<models::CustomerOrder>()
{
}

std::unique_ptr<models::CustomerOrder> CustomerOrderRepository::findById(long long id)
{
    auto connection = acquireConnection();
    
    try
    {
        Poco::Int64 pocoId = static_cast<Poco::Int64>(id);
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT id, order_number, customer_name, customer_email, customer_phone, "
                  "shipping_address, order_date, status, total_amount, priority, notes, "
                  "estimated_delivery_date, actual_delivery_date, created_by "
                  "FROM " << TABLE_NAME << " WHERE id = $1",
            Poco::Data::Keywords::use(pocoId),
            now;
        
        Poco::Data::RecordSet rs(select);
        
        if (rs.rowCount() > 0)
        {
            Row row = rs.row(0);
            auto order = std::make_unique<models::CustomerOrder>(mapRowToOrder(row));
            
            Poco::Data::Statement userSelect(connection->getSession());
            Poco::Int64 userIdCopy = order->createdBy;
            userSelect << "SELECT full_name FROM users WHERE id = $1",
                Poco::Data::Keywords::use(userIdCopy),
                now;
            
            Poco::Data::RecordSet userRs(userSelect);
            if (userRs.rowCount() > 0)
            {
                order->createdByName = userRs.value("full_name").convert<std::string>();
            }
            
            return order;
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findById: " + e.displayText());
    }
    
    return nullptr;
}

std::vector<std::unique_ptr<models::CustomerOrder>> CustomerOrderRepository::findAll()
{
    std::vector<std::unique_ptr<models::CustomerOrder>> orders;
    auto connection = acquireConnection();
    
    try
    {
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT id, order_number, customer_name, customer_email, customer_phone, "
                  "shipping_address, order_date, status, total_amount, priority, notes, "
                  "estimated_delivery_date, actual_delivery_date, created_by "
                  "FROM " << TABLE_NAME << " ORDER BY order_date DESC",
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            Poco::Data::Row row = rs.row(i);
            auto order = std::make_unique<models::CustomerOrder>(mapRowToOrder(row));
            
            orders.push_back(std::move(order));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findAll: " + e.displayText());
    }
    
    return orders;
}

std::vector<std::unique_ptr<models::CustomerOrder>> CustomerOrderRepository::findPaginated(int page, int pageSize)
{
    std::vector<std::unique_ptr<models::CustomerOrder>> orders;
    auto connection = acquireConnection();
    
    if (page < 1) page = 1;
    int offset = (page - 1) * pageSize;
    
    try
    {
        int usePageSize = pageSize;
        int useOffset = offset;
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT id, order_number, customer_name, customer_email, customer_phone, "
                  "shipping_address, order_date, status, total_amount, priority, notes, "
                  "estimated_delivery_date, actual_delivery_date, created_by "
                  "FROM " << TABLE_NAME << " ORDER BY order_date DESC LIMIT $1 OFFSET $2",
            Poco::Data::Keywords::use(usePageSize),
            Poco::Data::Keywords::use(useOffset),
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            Poco::Data::Row row = rs.row(i);
            auto order = std::make_unique<models::CustomerOrder>(mapRowToOrder(row));
            
            orders.push_back(std::move(order));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findPaginated: " + e.displayText());
    }
    
    return orders;
}

long long CustomerOrderRepository::create(const models::CustomerOrder& order)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        std::string statusStr = models::CustomerOrder::statusToString(order.status);
        std::string priorityStr = models::CustomerOrder::priorityToString(order.priority);
        
        models::CustomerOrder orderCopy = order;
        
        Poco::Data::Statement insert(connection->getSession());
        Poco::Int64 newId = 0;
        
        insert << "INSERT INTO " << TABLE_NAME << " "
                  "(order_number, customer_name, customer_email, customer_phone, "
                  "shipping_address, order_date, status, total_amount, priority, notes, "
                  "estimated_delivery_date, actual_delivery_date, created_by) "
                  "VALUES ($1, $2, $3, $4, $5, $6, $7::order_status, $8, $9::order_priority, $10, $11, $12, $13) "
                  "RETURNING id",
            Poco::Data::Keywords::use(orderCopy.orderNumber),
            Poco::Data::Keywords::use(orderCopy.customerName),
            Poco::Data::Keywords::use(orderCopy.customerEmail),
            Poco::Data::Keywords::use(orderCopy.customerPhone),
            Poco::Data::Keywords::use(orderCopy.shippingAddress),
            Poco::Data::Keywords::use(orderCopy.orderDate),
            Poco::Data::Keywords::use(statusStr),
            Poco::Data::Keywords::use(orderCopy.totalAmount),
            Poco::Data::Keywords::use(priorityStr),
            Poco::Data::Keywords::use(orderCopy.notes),
            Poco::Data::Keywords::use(orderCopy.estimatedDeliveryDate),
            Poco::Data::Keywords::use(orderCopy.actualDeliveryDate),
            Poco::Data::Keywords::use(orderCopy.createdBy),
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

bool CustomerOrderRepository::update(long long id, const models::CustomerOrder& order)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);

        models::CustomerOrder orderCopy = order;

        std::string statusStr = models::CustomerOrder::statusToString(orderCopy.status);
        std::string priorityStr = models::CustomerOrder::priorityToString(orderCopy.priority);
        
        long long idCopy = id;
        
        Poco::Data::Statement update(connection->getSession());
        update << "UPDATE " << TABLE_NAME << " SET "
                  "customer_name = $1, customer_email = $2, customer_phone = $3, "
                  "shipping_address = $4, status = $5, total_amount = $6, "
                  "priority = $7, notes = $8, estimated_delivery_date = $9, "
                  "actual_delivery_date = $10 WHERE id = $11",
            Poco::Data::Keywords::use(orderCopy.customerName),
            Poco::Data::Keywords::use(orderCopy.customerEmail),
            Poco::Data::Keywords::use(orderCopy.customerPhone),
            Poco::Data::Keywords::use(orderCopy.shippingAddress),
            Poco::Data::Keywords::use(statusStr),
            Poco::Data::Keywords::use(orderCopy.totalAmount),
            Poco::Data::Keywords::use(priorityStr),
            Poco::Data::Keywords::use(orderCopy.notes),
            Poco::Data::Keywords::use(orderCopy.estimatedDeliveryDate),
            Poco::Data::Keywords::use(orderCopy.actualDeliveryDate),
            Poco::Data::Keywords::use(idCopy);
        
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

bool CustomerOrderRepository::remove(long long id)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        long long idCopy = id;
        
        Poco::Data::Statement check(connection->getSession());
        check << "SELECT status FROM " << TABLE_NAME << " WHERE id = $1",
            Poco::Data::Keywords::use(idCopy),
            now;
        
        Poco::Data::RecordSet rs(check);
        if (rs.rowCount() > 0)
        {
            std::string status = rs.value("status").convert<std::string>();
            models::OrderStatus orderStatus = models::CustomerOrder::stringToStatus(status);
            
            if (orderStatus != models::OrderStatus::CANCELLED && 
                orderStatus != models::OrderStatus::NEW)
            {
                throw std::runtime_error("Cannot delete order with status other than NEW or CANCELLED");
            }
        }
        
        Poco::Data::Statement del(connection->getSession());
        del << "DELETE FROM " << TABLE_NAME << " WHERE id = $1",
            Poco::Data::Keywords::use(idCopy);
        
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

bool CustomerOrderRepository::softDelete(long long id)
{
    throw std::runtime_error("Soft delete not supported for customer orders. Use cancelOrder instead.");
}

int CustomerOrderRepository::count()
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

Poco::JSON::Array CustomerOrderRepository::findAllAsJson()
{
    auto orders = findAll();
    Poco::JSON::Array jsonArray;
    
    for (const auto& order : orders)
    {
        if (order)
        {
            jsonArray.add(order->toJson());
        }
    }
    
    return jsonArray;
}

Poco::JSON::Object CustomerOrderRepository::findByIdAsJson(long long id)
{
    auto order = findById(id);
    if (order)
    {
        return order->toJson();
    }
    
    return Poco::JSON::Object();
}

std::vector<std::unique_ptr<models::CustomerOrder>> CustomerOrderRepository::findByField(
    const std::string& fieldName, const std::string& fieldValue)
{
    std::vector<std::unique_ptr<models::CustomerOrder>> orders;
    auto connection = acquireConnection();
    
    try
    {
        std::string sql = "SELECT id, order_number, customer_name, customer_email, customer_phone, "
                          "shipping_address, order_date, status, total_amount, priority, notes, "
                          "estimated_delivery_date, actual_delivery_date, created_by "
                          "FROM " + TABLE_NAME + " WHERE " + fieldName + " = $1 "
                          "ORDER BY order_date DESC";
        
        std::string fieldValueCopy = fieldValue;
        Poco::Data::Statement select(connection->getSession());
        select << sql,
            Poco::Data::Keywords::use(fieldValueCopy),
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            Poco::Data::Row row = rs.row(i);
            auto order = std::make_unique<models::CustomerOrder>(mapRowToOrder(row));

            orders.push_back(std::move(order));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findByField: " + e.displayText());
    }
    
    return orders;
}

std::vector<std::unique_ptr<models::CustomerOrder>> CustomerOrderRepository::search(
    const std::string& query, const std::vector<std::string>& fields)
{
    std::vector<std::unique_ptr<models::CustomerOrder>> orders;
    auto connection = acquireConnection();
    
    try
    {
        std::string searchClause = buildSearchQuery(query, fields);
        std::string sql = "SELECT id, order_number, customer_name, customer_email, customer_phone, "
                          "shipping_address, order_date, status, total_amount, priority, notes, "
                          "estimated_delivery_date, actual_delivery_date, created_by "
                          "FROM " + TABLE_NAME + " WHERE " + searchClause + " "
                          "ORDER BY order_date DESC";
        
        Poco::Data::Statement select(connection->getSession());
        select << sql,
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            Poco::Data::Row row = rs.row(i);
            auto order = std::make_unique<models::CustomerOrder>(mapRowToOrder(row));
            
            orders.push_back(std::move(order));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in search: " + e.displayText());
    }
    
    return orders;
}

std::unique_ptr<models::CustomerOrder> CustomerOrderRepository::findByOrderNumber(const std::string& orderNumber)
{
    auto orders = findByField("order_number", orderNumber);
    if (!orders.empty())
    {
        return std::move(orders[0]);
    }
    return nullptr;
}

std::vector<std::unique_ptr<models::CustomerOrder>> CustomerOrderRepository::findByCustomerEmail(const std::string& email)
{
    return findByField("customer_email", email);
}

std::vector<std::unique_ptr<models::CustomerOrder>> CustomerOrderRepository::findByCustomerPhone(const std::string& phone)
{
    return findByField("customer_phone", phone);
}

std::vector<std::unique_ptr<models::CustomerOrder>> CustomerOrderRepository::findByStatus(models::OrderStatus status)
{
    auto connection = acquireConnection();
    std::vector<std::unique_ptr<models::CustomerOrder>> orders;
    
    try
    {
        std::string statusStr = models::CustomerOrder::statusToString(status);
        std::string statusStrCopy = statusStr;
        
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT id, order_number, customer_name, customer_email, customer_phone, "
                  "shipping_address, order_date, status, total_amount, priority, notes, "
                  "estimated_delivery_date, actual_delivery_date, created_by "
                  "FROM " << TABLE_NAME << " WHERE status = $1 "
                  "ORDER BY order_date DESC",
            Poco::Data::Keywords::use(statusStrCopy),
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            Row row = rs.row(i);
            auto order = std::make_unique<models::CustomerOrder>(mapRowToOrder(row));
            
            orders.push_back(std::move(order));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findByStatus: " + e.displayText());
    }
    
    return orders;
}

std::vector<std::unique_ptr<models::CustomerOrder>> CustomerOrderRepository::findByPriority(models::OrderPriority priority)
{
    auto connection = acquireConnection();
    std::vector<std::unique_ptr<models::CustomerOrder>> orders;
    
    try
    {
        std::string priorityStr = models::CustomerOrder::priorityToString(priority);
        std::string priorityStrCopy = priorityStr;
        
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT id, order_number, customer_name, customer_email, customer_phone, "
                  "shipping_address, order_date, status, total_amount, priority, notes, "
                  "estimated_delivery_date, actual_delivery_date, created_by "
                  "FROM " << TABLE_NAME << " WHERE priority = $1 "
                  "ORDER BY order_date DESC",
            Poco::Data::Keywords::use(priorityStrCopy),
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            Poco::Data::Row row = rs.row(i);
            auto order = std::make_unique<models::CustomerOrder>(mapRowToOrder(row));
            
            orders.push_back(std::move(order));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findByPriority: " + e.displayText());
    }
    
    return orders;
}

std::vector<std::unique_ptr<models::CustomerOrder>> CustomerOrderRepository::findByCreatedBy(long long userId)
{
    auto connection = acquireConnection();
    std::vector<std::unique_ptr<models::CustomerOrder>> orders;
    
    try
    {
        long long userIdCopy = userId;
        
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT id, order_number, customer_name, customer_email, customer_phone, "
                  "shipping_address, order_date, status, total_amount, priority, notes, "
                  "estimated_delivery_date, actual_delivery_date, created_by "
                  "FROM " << TABLE_NAME << " WHERE created_by = $1 "
                  "ORDER BY order_date DESC",
            Poco::Data::Keywords::use(userIdCopy),
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            Poco::Data::Row row = rs.row(i);
            auto order = std::make_unique<models::CustomerOrder>(mapRowToOrder(row));
            
            orders.push_back(std::move(order));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findByCreatedBy: " + e.displayText());
    }
    
    return orders;
}

std::vector<std::unique_ptr<models::CustomerOrder>> CustomerOrderRepository::findByDateRange(
    const std::string& startDate, const std::string& endDate)
{
    auto connection = acquireConnection();
    std::vector<std::unique_ptr<models::CustomerOrder>> orders;
    
    try
    {
        std::string startDateCopy = startDate;
        std::string endDateCopy = endDate;
        
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT id, order_number, customer_name, customer_email, customer_phone, "
                  "shipping_address, order_date, status, total_amount, priority, notes, "
                  "estimated_delivery_date, actual_delivery_date, created_by "
                  "FROM " << TABLE_NAME << " WHERE order_date >= $1 AND order_date <= $2 "
                  "ORDER BY order_date DESC",
            Poco::Data::Keywords::use(startDateCopy),
            Poco::Data::Keywords::use(endDateCopy),
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            Poco::Data::Row row = rs.row(i);
            auto order = std::make_unique<models::CustomerOrder>(mapRowToOrder(row));
            
            orders.push_back(std::move(order));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findByDateRange: " + e.displayText());
    }
    
    return orders;
}

std::vector<std::unique_ptr<models::CustomerOrder>> CustomerOrderRepository::findPendingOrders()
{
    return findByStatus(models::OrderStatus::NEW);
}

std::vector<std::unique_ptr<models::CustomerOrder>> CustomerOrderRepository::findActiveOrders()
{
    auto connection = acquireConnection();
    std::vector<std::unique_ptr<models::CustomerOrder>> orders;
    
    try
    {
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT id, order_number, customer_name, customer_email, customer_phone, "
                  "shipping_address, order_date, status, total_amount, priority, notes, "
                  "estimated_delivery_date, actual_delivery_date, created_by "
                  "FROM " << TABLE_NAME << " WHERE status IN ('new', 'processing', 'picked', 'packed', 'shipped') "
                  "ORDER BY priority DESC, order_date ASC",
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            Poco::Data::Row row = rs.row(i);
            auto order = std::make_unique<models::CustomerOrder>(mapRowToOrder(row));
            
            orders.push_back(std::move(order));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findActiveOrders: " + e.displayText());
    }
    
    return orders;
}

std::vector<std::unique_ptr<models::CustomerOrder>> CustomerOrderRepository::findCompletedOrders()
{
    return findByStatus(models::OrderStatus::DELIVERED);
}

std::vector<std::unique_ptr<models::CustomerOrder>> CustomerOrderRepository::findUrgentOrders()
{
    auto connection = acquireConnection();
    std::vector<std::unique_ptr<models::CustomerOrder>> orders;
    
    try
    {
        std::string priorityHigh = models::CustomerOrder::priorityToString(models::OrderPriority::HIGH);
        std::string priorityUrgent = models::CustomerOrder::priorityToString(models::OrderPriority::URGENT);
        
        std::string priorityHighCopy = priorityHigh;
        std::string priorityUrgentCopy = priorityUrgent;
        
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT id, order_number, customer_name, customer_email, customer_phone, "
                  "shipping_address, order_date, status, total_amount, priority, notes, "
                  "estimated_delivery_date, actual_delivery_date, created_by "
                  "FROM " << TABLE_NAME << " WHERE priority IN ($1, $2) "
                  "AND status NOT IN ('delivered', 'cancelled') "
                  "ORDER BY order_date ASC",
            Poco::Data::Keywords::use(priorityHighCopy),
            Poco::Data::Keywords::use(priorityUrgentCopy),
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            Poco::Data::Row row = rs.row(i);
            auto order = std::make_unique<models::CustomerOrder>(mapRowToOrder(row));
            
            orders.push_back(std::move(order));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findUrgentOrders: " + e.displayText());
    }
    
    return orders;
}

bool CustomerOrderRepository::updateStatus(long long id, models::OrderStatus newStatus)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        std::string statusStr = models::CustomerOrder::statusToString(newStatus);
        long long idCopy = id;
        std::string statusStrCopy = statusStr;
        
        Poco::Data::Statement update(connection->getSession());
        update << "UPDATE " << TABLE_NAME << " SET status = $1 WHERE id = $2",
            Poco::Data::Keywords::use(statusStrCopy),
            Poco::Data::Keywords::use(idCopy);
        
        int rowsAffected = update.execute();
        
        commitTransaction(*connection);
        return rowsAffected > 0;
    }
    catch (const Poco::Exception& e)
    {
        rollbackTransaction(*connection);
        throw std::runtime_error("Database error in updateStatus: " + e.displayText());
    }
}

bool CustomerOrderRepository::updatePriority(long long id, models::OrderPriority newPriority)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        std::string priorityStr = models::CustomerOrder::priorityToString(newPriority);
        long long idCopy = id;
        std::string priorityStrCopy = priorityStr;
        
        Poco::Data::Statement update(connection->getSession());
        update << "UPDATE " << TABLE_NAME << " SET priority = $1 WHERE id = $2",
            Poco::Data::Keywords::use(priorityStrCopy),
            Poco::Data::Keywords::use(idCopy),
            now;
        
        int rowsAffected = update.execute();
        
        commitTransaction(*connection);
        return rowsAffected > 0;
    }
    catch (const Poco::Exception& e)
    {
        rollbackTransaction(*connection);
        throw std::runtime_error("Database error in updatePriority: " + e.displayText());
    }
}

bool CustomerOrderRepository::updateDeliveryDate(long long id, const std::string& estimatedDelivery, const std::string& actualDelivery)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        long long idCopy = id;
        std::string estimatedDeliveryCopy = estimatedDelivery;
        
        Poco::Data::Statement update(connection->getSession());
        
        if (!actualDelivery.empty())
        {
            std::string actualDeliveryCopy = actualDelivery;
            update << "UPDATE " << TABLE_NAME << " SET estimated_delivery_date = $1, actual_delivery_date = $2 WHERE id = $3",
                Poco::Data::Keywords::use(estimatedDeliveryCopy),
                Poco::Data::Keywords::use(actualDeliveryCopy),
                Poco::Data::Keywords::use(idCopy),
                now;
        }
        else
        {
            update << "UPDATE " << TABLE_NAME << " SET estimated_delivery_date = $1 WHERE id = $2",
                Poco::Data::Keywords::use(estimatedDeliveryCopy),
                Poco::Data::Keywords::use(idCopy),
                now;
        }
        
        int rowsAffected = update.execute();
        
        commitTransaction(*connection);
        return rowsAffected > 0;
    }
    catch (const Poco::Exception& e)
    {
        rollbackTransaction(*connection);
        throw std::runtime_error("Database error in updateDeliveryDate: " + e.displayText());
    }
}

bool CustomerOrderRepository::updateTotalAmount(long long id, double newTotalAmount)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        long long idCopy = id;
        double totalAmountCopy = newTotalAmount;
        
        Poco::Data::Statement update(connection->getSession());
        update << "UPDATE " << TABLE_NAME << " SET total_amount = $1 WHERE id = $2",
            Poco::Data::Keywords::use(totalAmountCopy),
            Poco::Data::Keywords::use(idCopy),
            now;
        
        int rowsAffected = update.execute();
        
        commitTransaction(*connection);
        return rowsAffected > 0;
    }
    catch (const Poco::Exception& e)
    {
        rollbackTransaction(*connection);
        throw std::runtime_error("Database error in updateTotalAmount: " + e.displayText());
    }
}

bool CustomerOrderRepository::updateCustomerInfo(long long id, const std::string& name, const std::string& email, const std::string& phone)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        long long idCopy = id;
        std::string nameCopy = name;
        std::string emailCopy = email;
        std::string phoneCopy = phone;
        
        Poco::Data::Statement update(connection->getSession());
        update << "UPDATE " << TABLE_NAME << " SET customer_name = $1, customer_email = $2, customer_phone = $3 WHERE id = $4",
            Poco::Data::Keywords::use(nameCopy),
            Poco::Data::Keywords::use(emailCopy),
            Poco::Data::Keywords::use(phoneCopy),
            Poco::Data::Keywords::use(idCopy),
            now;
        
        int rowsAffected = update.execute();
        
        commitTransaction(*connection);
        return rowsAffected > 0;
    }
    catch (const Poco::Exception& e)
    {
        rollbackTransaction(*connection);
        throw std::runtime_error("Database error in updateCustomerInfo: " + e.displayText());
    }
}

bool CustomerOrderRepository::cancelOrder(long long id)
{
    return updateStatus(id, models::OrderStatus::CANCELLED);
}

bool CustomerOrderRepository::markAsShipped(long long id)
{
    return updateStatus(id, models::OrderStatus::SHIPPED);
}

bool CustomerOrderRepository::markAsDelivered(long long id)
{
    return updateStatus(id, models::OrderStatus::DELIVERED);
}

bool CustomerOrderRepository::markAsProcessing(long long id)
{
    return updateStatus(id, models::OrderStatus::PROCESSING);
}

int CustomerOrderRepository::countByStatus(models::OrderStatus status)
{
    auto connection = acquireConnection();
    
    try
    {
        std::string statusStr = models::CustomerOrder::statusToString(status);
        std::string statusStrCopy = statusStr;
        
        Poco::Data::Statement countStmt(connection->getSession());
        countStmt << "SELECT COUNT(*) FROM " << TABLE_NAME << " WHERE status = $1",
            Poco::Data::Keywords::use(statusStrCopy),
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
        throw std::runtime_error("Database error in countByStatus: " + e.displayText());
    }
}

int CustomerOrderRepository::countByPriority(models::OrderPriority priority)
{
    auto connection = acquireConnection();
    
    try
    {
        std::string priorityStr = models::CustomerOrder::priorityToString(priority);
        std::string priorityStrCopy = priorityStr;
        
        Poco::Data::Statement countStmt(connection->getSession());
        countStmt << "SELECT COUNT(*) FROM " << TABLE_NAME << " WHERE priority = $1",
            Poco::Data::Keywords::use(priorityStrCopy),
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
        throw std::runtime_error("Database error in countByPriority: " + e.displayText());
    }
}

int CustomerOrderRepository::countByCustomer(const std::string& customerEmail)
{
    auto connection = acquireConnection();
    
    try
    {
        std::string emailCopy = customerEmail;
        
        Poco::Data::Statement countStmt(connection->getSession());
        countStmt << "SELECT COUNT(*) FROM " << TABLE_NAME << " WHERE customer_email = $1",
            Poco::Data::Keywords::use(emailCopy),
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
        throw std::runtime_error("Database error in countByCustomer: " + e.displayText());
    }
}

int CustomerOrderRepository::countByUser(long long userId)
{
    auto connection = acquireConnection();
    
    try
    {
        long long userIdCopy = userId;
        
        Poco::Data::Statement countStmt(connection->getSession());
        countStmt << "SELECT COUNT(*) FROM " << TABLE_NAME << " WHERE created_by = $1",
            Poco::Data::Keywords::use(userIdCopy),
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
        throw std::runtime_error("Database error in countByUser: " + e.displayText());
    }
}

double CustomerOrderRepository::getTotalRevenue()
{
    auto connection = acquireConnection();
    
    try
    {
        Poco::Data::Statement sumStmt(connection->getSession());
        sumStmt << "SELECT COALESCE(SUM(total_amount), 0) FROM " << TABLE_NAME 
                << " WHERE status = 'delivered'",
            now;
        
        Poco::Data::RecordSet rs(sumStmt);
        if (rs.rowCount() > 0)
        {
            return rs.value(0, 0.0).convert<double>();
        }
        
        return 0.0;
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in getTotalRevenue: " + e.displayText());
    }
}

double CustomerOrderRepository::getAverageOrderValue()
{
    auto connection = acquireConnection();
    
    try
    {
        Poco::Data::Statement avgStmt(connection->getSession());
        avgStmt << "SELECT COALESCE(AVG(total_amount), 0) FROM " << TABLE_NAME 
                << " WHERE status = 'delivered'",
            now;
        
        Poco::Data::RecordSet rs(avgStmt);
        if (rs.rowCount() > 0)
        {
            return rs.value(0, 0.0).convert<double>();
        }
        
        return 0.0;
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in getAverageOrderValue: " + e.displayText());
    }
}

double CustomerOrderRepository::getMonthlyRevenue(int year, int month)
{
    auto connection = acquireConnection();
    
    try
    {
        std::string startDate = std::to_string(year) + "-" + 
                               (month < 10 ? "0" : "") + std::to_string(month) + "-01";
        std::string endDate;
        
        if (month == 12)
        {
            endDate = std::to_string(year + 1) + "-01-01";
        }
        else
        {
            endDate = std::to_string(year) + "-" + 
                     ((month + 1) < 10 ? "0" : "") + std::to_string(month + 1) + "-01";
        }
        
        std::string startDateCopy = startDate;
        std::string endDateCopy = endDate;
        
        Poco::Data::Statement sumStmt(connection->getSession());
        sumStmt << "SELECT COALESCE(SUM(total_amount), 0) FROM " << TABLE_NAME 
                << " WHERE status = 'delivered' "
                << "AND order_date >= $1 AND order_date < $2",
            Poco::Data::Keywords::use(startDateCopy),
            Poco::Data::Keywords::use(endDateCopy),
            now;
        
        Poco::Data::RecordSet rs(sumStmt);
        if (rs.rowCount() > 0)
        {
            return rs.value(0, 0.0).convert<double>();
        }
        
        return 0.0;
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in getMonthlyRevenue: " + e.displayText());
    }
}

std::unique_ptr<models::CustomerOrder> CustomerOrderRepository::getOrderWithItems(long long id)
{
    auto order = findById(id);
    if (!order)
    {
        return nullptr;
    }
    
    auto connection = acquireConnection();
    
    try
    {
        Poco::Int64 orderIdCopy = id;
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT oi.id, oi.order_id, oi.product_id, oi.batch_id, "
                  "oi.quantity_ordered, oi.quantity_shipped, oi.unit_price, "
                  "oi.discount_percent, oi.line_total, oi.picking_status, "
                  "oi.picked_by, oi.picked_at, "
                  "p.name as product_name, p.sku as product_sku, "
                  "pb.batch_number, u.full_name as picked_by_name "
                  "FROM order_items oi "
                  "JOIN products p ON p.id = oi.product_id "
                  "JOIN product_batches pb ON pb.id = oi.batch_id "
                  "LEFT JOIN users u ON u.id = oi.picked_by "
                  "WHERE oi.order_id = $1 "
                  "ORDER BY oi.id",
            Poco::Data::Keywords::use(orderIdCopy),
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            Row row = rs.row(i);
            auto item = std::make_shared<models::OrderItem>();

            item->id = row["id"].convert<long long>();
            item->orderId = row["order_id"].convert<long long>();
            item->productId = row["product_id"].convert<long long>();
            item->batchId = row["batch_id"].convert<long long>();
            item->quantityOrdered = row["quantity_ordered"].convert<int>();
            item->quantityShipped = row["quantity_shipped"].convert<int>();
            item->unitPrice = row["unit_price"].convert<double>();
            item->discountPercent = row["discount_percent"].convert<double>();
            item->lineTotal = row["line_total"].convert<double>();

            std::string pickingStatusStr = row["picking_status"].convert<std::string>();
            item->pickingStatus = models::OrderItem::stringToPickingStatus(pickingStatusStr);

            if (!row["picked_by"].isEmpty())
            {
                item->pickedBy = row["picked_by"].convert<long long>();
            }

            if (!row["picked_at"].isEmpty())
            {
                item->pickedAt = row["picked_at"].convert<std::string>();
            }

            item->productName = row["product_name"].convert<std::string>();
            item->productSku = row["product_sku"].convert<std::string>();
            item->batchNumber = row["batch_number"].convert<std::string>();

            if (!row["picked_by_name"].isEmpty())
            {
                item->pickedByName = row["picked_by_name"].convert<std::string>();
            }

            order->addOrderItem(item);
        }
        
        return order;
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in getOrderWithItems: " + e.displayText());
    }
}

std::unique_ptr<models::CustomerOrder> CustomerOrderRepository::getOrderWithItemsAndShipments(long long id)
{
    auto order = getOrderWithItems(id);
    if (!order)
    {
        return nullptr;
    }
    
    return order;
}

Poco::JSON::Array CustomerOrderRepository::getOrderStatistics()
{
    auto connection = acquireConnection();
    Poco::JSON::Array jsonArray;
    
    try
    {
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT status, COUNT(*) as count, COALESCE(SUM(total_amount), 0) as total "
                  "FROM " << TABLE_NAME << " GROUP BY status ORDER BY status",
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            Poco::Data::Row row = rs.row(i);
            std::string status = row["status"].convert<std::string>();
            int count = row["count"].convert<int>();
            double total = row["total"].convert<double>();
            
            std::cout << "DEBUG: Row " << i << ": status=" << status 
                      << ", count=" << count << ", total=" << total << std::endl;
            
            Poco::JSON::Object stat;
            stat.set("status", status);
            stat.set("count", count);
            stat.set("total", total);
            jsonArray.add(stat);
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in getOrderStatistics: " + e.displayText());
    }
    
    return jsonArray;
}

Poco::JSON::Array CustomerOrderRepository::getRevenueReport(const std::string& startDate, const std::string& endDate)
{
    auto connection = acquireConnection();
    Poco::JSON::Array jsonArray;
    
    try
    {
        std::string startDateCopy = startDate;
        std::string endDateCopy = endDate;
        
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT DATE(order_date) as order_day, COUNT(*) as order_count, "
                  "COALESCE(SUM(total_amount), 0) as daily_revenue, "
                  "AVG(total_amount) as avg_order_value "
                  "FROM " << TABLE_NAME << " WHERE order_date >= $1 AND order_date <= $2 "
                  "AND status = 'delivered' "
                  "GROUP BY DATE(order_date) ORDER BY order_day DESC",
            Poco::Data::Keywords::use(startDateCopy),
            Poco::Data::Keywords::use(endDateCopy),
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            Poco::Data::Row row = rs.row(i);
            
            std::string date = row["order_day"].convert<std::string>();
            int orderCount = row["order_count"].convert<int>();
            double dailyRevenue = row["daily_revenue"].convert<double>();
            double avgOrderValue = row["avg_order_value"].convert<double>();
            
            std::cout << "DEBUG: Row " << i << ": date=" << date 
                      << ", order_count=" << orderCount 
                      << ", daily_revenue=" << dailyRevenue 
                      << ", avg_order_value=" << avgOrderValue << std::endl;
            
            Poco::JSON::Object dayStat;
            dayStat.set("date", date);
            dayStat.set("order_count", orderCount);
            dayStat.set("daily_revenue", dailyRevenue);
            dayStat.set("avg_order_value", avgOrderValue);
            
            jsonArray.add(dayStat);
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in getRevenueReport: " + e.displayText());
    }
    
    return jsonArray;
}

Poco::JSON::Array CustomerOrderRepository::getCustomerOrderHistory(const std::string& customerEmail)
{
    auto orders = findByCustomerEmail(customerEmail);
    Poco::JSON::Array jsonArray;
    
    for (const auto& order : orders)
    {
        if (order)
        {
            jsonArray.add(order->toJson());
        }
    }
    
    return jsonArray;
}

bool CustomerOrderRepository::orderNumberExists(const std::string& orderNumber)
{
    auto connection = acquireConnection();
    
    try
    {
        std::string orderNumberCopy = orderNumber;
        
        Poco::Data::Statement check(connection->getSession());
        check << "SELECT COUNT(*) FROM " << TABLE_NAME << " WHERE order_number = $1",
            Poco::Data::Keywords::use(orderNumberCopy),
            now;
        
        Poco::Data::RecordSet rs(check);
        if (rs.rowCount() > 0)
        {
            int count = rs.value(0, 0).convert<int>();
            return count > 0;
        }
        
        return false;
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in orderNumberExists: " + e.displayText());
    }
}

std::vector<std::pair<long long, std::string>> CustomerOrderRepository::getActiveOrderNumbers()
{
    auto connection = acquireConnection();
    std::vector<std::pair<long long, std::string>> result;
    
    try
    {
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT id, order_number FROM " << TABLE_NAME 
                << " WHERE status NOT IN ('delivered', 'cancelled') "
                << "ORDER BY order_number",
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            long long id = rs.value("id", 0).convert<long long>();
            std::string orderNumber = rs.value("order_number").convert<std::string>();
            result.emplace_back(id, orderNumber);
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in getActiveOrderNumbers: " + e.displayText());
    }
    
    return result;
}

std::vector<std::string> CustomerOrderRepository::getUniqueCustomers()
{
    auto connection = acquireConnection();
    std::vector<std::string> customers;
    
    try
    {
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT DISTINCT customer_email FROM " << TABLE_NAME 
                << " ORDER BY customer_email",
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            std::string email = rs.value("customer_email").convert<std::string>();
            customers.push_back(email);
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in getUniqueCustomers: " + e.displayText());
    }
    
    return customers;
}

models::CustomerOrder CustomerOrderRepository::mapRowToOrder(Poco::Data::Row& row) const
{
    models::CustomerOrder order;
    
    order.id = row["id"].convert<long long>();
    order.orderNumber = row["order_number"].convert<std::string>();
    order.customerName = row["customer_name"].convert<std::string>();
    
    if (!row["customer_email"].isEmpty())
    {
        order.customerEmail = row["customer_email"].convert<std::string>();
    }
    
    if (!row["customer_phone"].isEmpty())
    {
        order.customerPhone = row["customer_phone"].convert<std::string>();
    }
    
    order.shippingAddress = row["shipping_address"].convert<std::string>();
    order.orderDate = row["order_date"].convert<std::string>();
    
    std::string statusStr = row["status"].convert<std::string>();
    order.status = models::CustomerOrder::stringToStatus(statusStr);
    
    order.totalAmount = row["total_amount"].convert<double>();
    
    std::string priorityStr = row["priority"].convert<std::string>();
    order.priority = models::CustomerOrder::stringToPriority(priorityStr);
    
    if (!row["notes"].isEmpty())
    {
        order.notes = row["notes"].convert<std::string>();
    }
    
    if (!row["estimated_delivery_date"].isEmpty())
    {
        order.estimatedDeliveryDate = row["estimated_delivery_date"].convert<std::string>();
    }
    
    if (!row["actual_delivery_date"].isEmpty())
    {
        order.actualDeliveryDate = row["actual_delivery_date"].convert<std::string>();
    }
    
    order.createdBy = row["created_by"].convert<long long>();
    
    return order;
}

std::string CustomerOrderRepository::generateOrderNumber()
{
    auto now = DateUtils::now();
    std::string dateStr = DateUtils::formatDateTime(now, "YYYYMMDD");
    
    auto connection = acquireConnection();
    
    try
    {
        Poco::Data::Statement countStmt(connection->getSession());
        countStmt << "SELECT COUNT(*) FROM " << TABLE_NAME 
                  << " WHERE order_number LIKE 'ORD-" + dateStr + "%'",
            now;
        
        Poco::Data::RecordSet rs(countStmt);
        int count = 0;
        if (rs.rowCount() > 0)
        {
            count = rs.value(0, 0).convert<int>();
        }
        
        std::string sequence = std::to_string(count + 1);
        while (sequence.length() < 4)
        {
            sequence = "0" + sequence;
        }
        
        return "ORD-" + dateStr + "-" + sequence;
    }
    catch (const Poco::Exception& e)
    {
        return "ORD-TEMP-" + std::to_string(std::time(nullptr));
    }
}

} // namespace database::repositories
