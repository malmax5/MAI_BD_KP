#include "OrderItemRepository.hpp"
#include "../models/OrderItem.hpp"
#include "../models/ProductBatch.hpp"
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

const std::string OrderItemRepository::TABLE_NAME = "order_items";
const std::vector<std::string> OrderItemRepository::SEARCH_FIELDS = {};

OrderItemRepository::OrderItemRepository() : BaseRepository<models::OrderItem>()
{
}

std::unique_ptr<models::OrderItem> OrderItemRepository::findById(long long id)
{
    auto connection = acquireConnection();
    
    try
    {
        Poco::Int64 pocoId = static_cast<Poco::Int64>(id);
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT oi.id, oi.order_id, oi.product_id, oi.batch_id, "
                  "oi.quantity_ordered, oi.quantity_shipped, oi.unit_price, "
                  "oi.discount_percent, oi.line_total, oi.picking_status, "
                  "oi.picked_by, oi.picked_at, "
                  "p.name as product_name, p.sku as product_sku, "
                  "pb.batch_number, u.full_name as picked_by_name "
                  "FROM " << TABLE_NAME << " oi "
                  "JOIN products p ON p.id = oi.product_id "
                  "JOIN product_batches pb ON pb.id = oi.batch_id "
                  "LEFT JOIN users u ON u.id = oi.picked_by "
                  "WHERE oi.id = $1",
            Poco::Data::Keywords::use(pocoId),
            now;
        
        Poco::Data::RecordSet rs(select);
        
        if (rs.rowCount() > 0)
        {
            auto item = std::make_unique<models::OrderItem>();
            item->id = rs.value("id", 0).convert<long long>();
            item->orderId = rs.value("order_id", 0).convert<long long>();
            item->productId = rs.value("product_id", 0).convert<long long>();
            item->batchId = rs.value("batch_id", 0).convert<long long>();
            item->quantityOrdered = rs.value("quantity_ordered", 0).convert<int>();
            item->quantityShipped = rs.value("quantity_shipped", 0).convert<int>();
            item->unitPrice = rs.value("unit_price", 0.0).convert<double>();
            item->discountPercent = rs.value("discount_percent", 0.0).convert<double>();
            item->lineTotal = rs.value("line_total", 0.0).convert<double>();
            
            std::string pickingStatusStr = rs.value("picking_status").convert<std::string>();
            item->pickingStatus = models::OrderItem::stringToPickingStatus(pickingStatusStr);
            
            item->pickedBy = rs.value("picked_by").isEmpty() ? 0LL : rs.value("picked_by").convert<long long>();
            item->pickedAt = rs.value("picked_at").isEmpty() ? "" : rs.value("picked_at").convert<std::string>();
            item->productName = rs.value("product_name").convert<std::string>();
            item->productSku = rs.value("product_sku").convert<std::string>();
            item->batchNumber = rs.value("batch_number").convert<std::string>();
            item->pickedByName = rs.value("picked_by_name").isEmpty() ? "" : rs.value("picked_by_name").convert<std::string>();
            
            return item;
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findById: " + e.displayText());
    }
    
    return nullptr;
}

std::vector<std::unique_ptr<models::OrderItem>> OrderItemRepository::findAll()
{
    std::vector<std::unique_ptr<models::OrderItem>> items;
    auto connection = acquireConnection();
    
    try
    {
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT oi.id, oi.order_id, oi.product_id, oi.batch_id, "
                  "oi.quantity_ordered, oi.quantity_shipped, oi.unit_price, "
                  "oi.discount_percent, oi.line_total, oi.picking_status, "
                  "oi.picked_by, oi.picked_at, "
                  "p.name as product_name, p.sku as product_sku, "
                  "pb.batch_number, u.full_name as picked_by_name "
                  "FROM " << TABLE_NAME << " oi "
                  "JOIN products p ON p.id = oi.product_id "
                  "JOIN product_batches pb ON pb.id = oi.batch_id "
                  "LEFT JOIN users u ON u.id = oi.picked_by "
                  "ORDER BY oi.id",
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            auto item = std::make_unique<models::OrderItem>();
            item->id = rs.value("id", 0).convert<long long>();
            item->orderId = rs.value("order_id", 0).convert<long long>();
            item->productId = rs.value("product_id", 0).convert<long long>();
            item->batchId = rs.value("batch_id", 0).convert<long long>();
            item->quantityOrdered = rs.value("quantity_ordered", 0).convert<int>();
            item->quantityShipped = rs.value("quantity_shipped", 0).convert<int>();
            item->unitPrice = rs.value("unit_price", 0.0).convert<double>();
            item->discountPercent = rs.value("discount_percent", 0.0).convert<double>();
            item->lineTotal = rs.value("line_total", 0.0).convert<double>();
            
            std::string pickingStatusStr = rs.value("picking_status").convert<std::string>();
            item->pickingStatus = models::OrderItem::stringToPickingStatus(pickingStatusStr);
            
            item->pickedBy = rs.value("picked_by").isEmpty() ? 0LL : rs.value("picked_by").convert<long long>();
            item->pickedAt = rs.value("picked_at").isEmpty() ? "" : rs.value("picked_at").convert<std::string>();
            item->productName = rs.value("product_name").convert<std::string>();
            item->productSku = rs.value("product_sku").convert<std::string>();
            item->batchNumber = rs.value("batch_number").convert<std::string>();
            item->pickedByName = rs.value("picked_by_name").isEmpty() ? "" : rs.value("picked_by_name").convert<std::string>();
            
            items.push_back(std::move(item));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findAll: " + e.displayText());
    }
    
    return items;
}

std::vector<std::unique_ptr<models::OrderItem>> OrderItemRepository::findPaginated(int page, int pageSize)
{
    std::vector<std::unique_ptr<models::OrderItem>> items;
    auto connection = acquireConnection();
    
    if (page < 1) page = 1;
    int offset = (page - 1) * pageSize;
    
    try
    {
        int usePageSize = pageSize;
        int useOffset = offset;
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT oi.id, oi.order_id, oi.product_id, oi.batch_id, "
                  "oi.quantity_ordered, oi.quantity_shipped, oi.unit_price, "
                  "oi.discount_percent, oi.line_total, oi.picking_status, "
                  "oi.picked_by, oi.picked_at, "
                  "p.name as product_name, p.sku as product_sku, "
                  "pb.batch_number, u.full_name as picked_by_name "
                  "FROM " << TABLE_NAME << " oi "
                  "JOIN products p ON p.id = oi.product_id "
                  "JOIN product_batches pb ON pb.id = oi.batch_id "
                  "LEFT JOIN users u ON u.id = oi.picked_by "
                  "ORDER BY oi.id LIMIT $1 OFFSET $2",
            Poco::Data::Keywords::use(usePageSize),
            Poco::Data::Keywords::use(useOffset),
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            auto item = std::make_unique<models::OrderItem>();
            item->id = rs.value("id", 0).convert<long long>();
            item->orderId = rs.value("order_id", 0).convert<long long>();
            item->productId = rs.value("product_id", 0).convert<long long>();
            item->batchId = rs.value("batch_id", 0).convert<long long>();
            item->quantityOrdered = rs.value("quantity_ordered", 0).convert<int>();
            item->quantityShipped = rs.value("quantity_shipped", 0).convert<int>();
            item->unitPrice = rs.value("unit_price", 0.0).convert<double>();
            item->discountPercent = rs.value("discount_percent", 0.0).convert<double>();
            item->lineTotal = rs.value("line_total", 0.0).convert<double>();
            
            std::string pickingStatusStr = rs.value("picking_status").convert<std::string>();
            item->pickingStatus = models::OrderItem::stringToPickingStatus(pickingStatusStr);
            
            item->pickedBy = rs.value("picked_by").isEmpty() ? 0LL : rs.value("picked_by").convert<long long>();
            item->pickedAt = rs.value("picked_at").isEmpty() ? "" : rs.value("picked_at").convert<std::string>();
            item->productName = rs.value("product_name").convert<std::string>();
            item->productSku = rs.value("product_sku").convert<std::string>();
            item->batchNumber = rs.value("batch_number").convert<std::string>();
            item->pickedByName = rs.value("picked_by_name").isEmpty() ? "" : rs.value("picked_by_name").convert<std::string>();
            
            items.push_back(std::move(item));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findPaginated: " + e.displayText());
    }
    
    return items;
}

long long OrderItemRepository::create(const models::OrderItem& item)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        std::string pickingStatusStr = models::OrderItem::pickingStatusToString(item.pickingStatus);
        
        Poco::Int64 orderIdCopy = item.orderId;
        Poco::Int64 productIdCopy = item.productId;
        Poco::Int64 batchIdCopy = item.batchId;
        int quantityOrderedCopy = item.quantityOrdered;
        int quantityShippedCopy = item.quantityShipped;
        double unitPriceCopy = item.unitPrice;
        double discountPercentCopy = item.discountPercent;
        std::string pickingStatusStrCopy = pickingStatusStr;
        Poco::Nullable<Poco::Int64> pickedByCopy;
        if (item.pickedBy > 0)
            pickedByCopy = item.pickedBy;

        Poco::Nullable<std::string> pickedAtCopy;
        if (!item.pickedAt.empty())
            pickedAtCopy = item.pickedAt;
        
        Poco::Data::Statement insert(connection->getSession());
        Poco::Int64 newId = 0;

        
        insert << "INSERT INTO " << TABLE_NAME << " "
                  "(order_id, product_id, batch_id, quantity_ordered, quantity_shipped, "
                  "unit_price, discount_percent, picking_status, "
                  "picked_by, picked_at) "
                  "VALUES ($1, $2, $3, $4, $5, $6, $7, $8::picking_status, $9, $10) "
                  "RETURNING id",
            Poco::Data::Keywords::use(orderIdCopy),
            Poco::Data::Keywords::use(productIdCopy),
            Poco::Data::Keywords::use(batchIdCopy),
            Poco::Data::Keywords::use(quantityOrderedCopy),
            Poco::Data::Keywords::use(quantityShippedCopy),
            Poco::Data::Keywords::use(unitPriceCopy),
            Poco::Data::Keywords::use(discountPercentCopy),
            Poco::Data::Keywords::use(pickingStatusStrCopy),
            Poco::Data::Keywords::use(pickedByCopy),
            Poco::Data::Keywords::use(pickedAtCopy),
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

bool OrderItemRepository::update(long long id, const models::OrderItem& item)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        std::string pickingStatusStr = models::OrderItem::pickingStatusToString(item.pickingStatus);
        models::OrderItem itemCopy = item;
        itemCopy.calculateLineTotal();
        
        Poco::Int64 productIdCopy = item.productId;
        Poco::Int64 batchIdCopy = item.batchId;
        int quantityOrderedCopy = item.quantityOrdered;
        int quantityShippedCopy = item.quantityShipped;
        double unitPriceCopy = item.unitPrice;
        double discountPercentCopy = item.discountPercent;
        double lineTotalCopy = itemCopy.lineTotal;
        std::string pickingStatusStrCopy = pickingStatusStr;
        Poco::Nullable<Poco::Int64> pickedByCopy;
        if (item.pickedBy > 0)
            pickedByCopy = item.pickedBy;

        Poco::Nullable<std::string> pickedAtCopy;
        if (!item.pickedAt.empty())
            pickedAtCopy = item.pickedAt;

        Poco::Int64 idCopy = id;
        
        Poco::Data::Statement update(connection->getSession());
        update << "UPDATE " << TABLE_NAME << " SET "
                  "product_id = $1, batch_id = $2, quantity_ordered = $3, "
                  "quantity_shipped = $4, unit_price = $5, discount_percent = $6, "
                  "picking_status = $7, picked_by = $8, "
                  "picked_at = $9 WHERE id = $10",
            Poco::Data::Keywords::use(productIdCopy),
            Poco::Data::Keywords::use(batchIdCopy),
            Poco::Data::Keywords::use(quantityOrderedCopy),
            Poco::Data::Keywords::use(quantityShippedCopy),
            Poco::Data::Keywords::use(unitPriceCopy),
            Poco::Data::Keywords::use(discountPercentCopy),
            Poco::Data::Keywords::use(pickingStatusStrCopy),
            Poco::Data::Keywords::use(pickedByCopy),
            Poco::Data::Keywords::use(pickedAtCopy),
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

bool OrderItemRepository::remove(long long id)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        long long idCopy = id;
        
        Poco::Data::Statement check(connection->getSession());
        check << "SELECT quantity_shipped FROM " << TABLE_NAME << " WHERE id = $1",
            Poco::Data::Keywords::use(idCopy),
            now;
        
        Poco::Data::RecordSet rs(check);
        if (rs.rowCount() > 0)
        {
            int quantityShipped = rs.value("quantity_shipped", 0).convert<int>();
            if (quantityShipped > 0)
            {
                throw std::runtime_error("Cannot delete order item that has already been shipped");
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

bool OrderItemRepository::softDelete(long long id)
{
    throw std::runtime_error("Soft delete not supported for order items");
}

int OrderItemRepository::count()
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

Poco::JSON::Array OrderItemRepository::findAllAsJson()
{
    auto items = findAll();
    Poco::JSON::Array jsonArray;
    
    for (const auto& item : items)
    {
        if (item)
        {
            jsonArray.add(item->toJson());
        }
    }
    
    return jsonArray;
}

Poco::JSON::Object OrderItemRepository::findByIdAsJson(long long id)
{
    auto item = findById(id);
    if (item)
    {
        return item->toJson();
    }
    
    return Poco::JSON::Object();
}

std::vector<std::unique_ptr<models::OrderItem>> OrderItemRepository::findByField(
    const std::string& fieldName, const std::string& fieldValue)
{
    std::vector<std::unique_ptr<models::OrderItem>> items;
    auto connection = acquireConnection();
    
    try
    {
        std::string sql = "SELECT oi.id, oi.order_id, oi.product_id, oi.batch_id, "
                          "oi.quantity_ordered, oi.quantity_shipped, oi.unit_price, "
                          "oi.discount_percent, oi.line_total, oi.picking_status, "
                          "oi.picked_by, oi.picked_at, "
                          "p.name as product_name, p.sku as product_sku, "
                          "pb.batch_number, u.full_name as picked_by_name "
                          "FROM " + TABLE_NAME + " oi "
                          "JOIN products p ON p.id = oi.product_id "
                          "JOIN product_batches pb ON pb.id = oi.batch_id "
                          "LEFT JOIN users u ON u.id = oi.picked_by "
                          "WHERE oi." + fieldName + " = $1 "
                          "ORDER BY oi.id";
        
        std::string fieldValueCopy = fieldValue;
        Poco::Data::Statement select(connection->getSession());
        select << sql,
            Poco::Data::Keywords::use(fieldValueCopy),
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            auto item = std::make_unique<models::OrderItem>();
            item->id = rs.value("id", 0).convert<long long>();
            item->orderId = rs.value("order_id", 0).convert<long long>();
            item->productId = rs.value("product_id", 0).convert<long long>();
            item->batchId = rs.value("batch_id", 0).convert<long long>();
            item->quantityOrdered = rs.value("quantity_ordered", 0).convert<int>();
            item->quantityShipped = rs.value("quantity_shipped", 0).convert<int>();
            item->unitPrice = rs.value("unit_price", 0.0).convert<double>();
            item->discountPercent = rs.value("discount_percent", 0.0).convert<double>();
            item->lineTotal = rs.value("line_total", 0.0).convert<double>();
            
            std::string pickingStatusStr = rs.value("picking_status").convert<std::string>();
            item->pickingStatus = models::OrderItem::stringToPickingStatus(pickingStatusStr);
            
            item->pickedBy = rs.value("picked_by").isEmpty() ? 0LL : rs.value("picked_by").convert<long long>();
            item->pickedAt = rs.value("picked_at").isEmpty() ? "" : rs.value("picked_at").convert<std::string>();
            item->productName = rs.value("product_name").convert<std::string>();
            item->productSku = rs.value("product_sku").convert<std::string>();
            item->batchNumber = rs.value("batch_number").convert<std::string>();
            item->pickedByName = rs.value("picked_by_name").isEmpty() ? "" : rs.value("picked_by_name").convert<std::string>();
            
            items.push_back(std::move(item));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findByField: " + e.displayText());
    }
    
    return items;
}

std::vector<std::unique_ptr<models::OrderItem>> OrderItemRepository::search(
    const std::string& query, const std::vector<std::string>& fields)
{
    return std::vector<std::unique_ptr<models::OrderItem>>();
}

std::vector<std::unique_ptr<models::OrderItem>> OrderItemRepository::findByOrderId(long long orderId)
{
    auto connection = acquireConnection();
    std::vector<std::unique_ptr<models::OrderItem>> items;
    
    try
    {
        long long orderIdCopy = orderId;
        
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT oi.id, oi.order_id, oi.product_id, oi.batch_id, "
                  "oi.quantity_ordered, oi.quantity_shipped, oi.unit_price, "
                  "oi.discount_percent, oi.line_total, oi.picking_status, "
                  "oi.picked_by, oi.picked_at, "
                  "p.name as product_name, p.sku as product_sku, "
                  "pb.batch_number, u.full_name as picked_by_name "
                  "FROM " << TABLE_NAME << " oi "
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
            auto item = std::make_unique<models::OrderItem>();
            item->id = rs.value("id", 0).convert<long long>();
            item->orderId = rs.value("order_id", 0).convert<long long>();
            item->productId = rs.value("product_id", 0).convert<long long>();
            item->batchId = rs.value("batch_id", 0).convert<long long>();
            item->quantityOrdered = rs.value("quantity_ordered", 0).convert<int>();
            item->quantityShipped = rs.value("quantity_shipped", 0).convert<int>();
            item->unitPrice = rs.value("unit_price", 0.0).convert<double>();
            item->discountPercent = rs.value("discount_percent", 0.0).convert<double>();
            item->lineTotal = rs.value("line_total", 0.0).convert<double>();
            
            std::string pickingStatusStr = rs.value("picking_status").convert<std::string>();
            item->pickingStatus = models::OrderItem::stringToPickingStatus(pickingStatusStr);
            
            item->pickedBy = rs.value("picked_by").isEmpty() ? 0LL : rs.value("picked_by").convert<long long>();
            item->pickedAt = rs.value("picked_at").isEmpty() ? "" : rs.value("picked_at").convert<std::string>();
            item->productName = rs.value("product_name").convert<std::string>();
            item->productSku = rs.value("product_sku").convert<std::string>();
            item->batchNumber = rs.value("batch_number").convert<std::string>();
            item->pickedByName = rs.value("picked_by_name").isEmpty() ? "" : rs.value("picked_by_name").convert<std::string>();
            
            items.push_back(std::move(item));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findByOrderId: " + e.displayText());
    }
    
    return items;
}

std::vector<std::unique_ptr<models::OrderItem>> OrderItemRepository::findByProductId(long long productId)
{
    return findByField("product_id", std::to_string(productId));
}

std::vector<std::unique_ptr<models::OrderItem>> OrderItemRepository::findByBatchId(long long batchId)
{
    return findByField("batch_id", std::to_string(batchId));
}

std::vector<std::unique_ptr<models::OrderItem>> OrderItemRepository::findByPickedBy(long long userId)
{
    return findByField("picked_by", std::to_string(userId));
}

std::vector<std::unique_ptr<models::OrderItem>> OrderItemRepository::findByPickingStatus(models::PickingStatus status)
{
    auto connection = acquireConnection();
    std::vector<std::unique_ptr<models::OrderItem>> items;
    
    try
    {
        std::string statusStr = models::OrderItem::pickingStatusToString(status);
        std::string statusStrCopy = statusStr;
        
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT oi.id, oi.order_id, oi.product_id, oi.batch_id, "
                  "oi.quantity_ordered, oi.quantity_shipped, oi.unit_price, "
                  "oi.discount_percent, oi.line_total, oi.picking_status, "
                  "oi.picked_by, oi.picked_at, "
                  "p.name as product_name, p.sku as product_sku, "
                  "pb.batch_number, u.full_name as picked_by_name "
                  "FROM " << TABLE_NAME << " oi "
                  "JOIN products p ON p.id = oi.product_id "
                  "JOIN product_batches pb ON pb.id = oi.batch_id "
                  "LEFT JOIN users u ON u.id = oi.picked_by "
                  "WHERE oi.picking_status = $1 "
                  "ORDER BY oi.id",
            Poco::Data::Keywords::use(statusStrCopy),
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            auto item = std::make_unique<models::OrderItem>();
            item->id = rs.value("id", 0).convert<long long>();
            item->orderId = rs.value("order_id", 0).convert<long long>();
            item->productId = rs.value("product_id", 0).convert<long long>();
            item->batchId = rs.value("batch_id", 0).convert<long long>();
            item->quantityOrdered = rs.value("quantity_ordered", 0).convert<int>();
            item->quantityShipped = rs.value("quantity_shipped", 0).convert<int>();
            item->unitPrice = rs.value("unit_price", 0.0).convert<double>();
            item->discountPercent = rs.value("discount_percent", 0.0).convert<double>();
            item->lineTotal = rs.value("line_total", 0.0).convert<double>();
            
            item->pickingStatus = status;
            item->pickedBy = rs.value("picked_by").isEmpty() ? 0LL : rs.value("picked_by").convert<long long>();
            item->pickedAt = rs.value("picked_at").isEmpty() ? "" : rs.value("picked_at").convert<std::string>();
            item->productName = rs.value("product_name").convert<std::string>();
            item->productSku = rs.value("product_sku").convert<std::string>();
            item->batchNumber = rs.value("batch_number").convert<std::string>();
            item->pickedByName = rs.value("picked_by_name").isEmpty() ? "" : rs.value("picked_by_name").convert<std::string>();
            
            items.push_back(std::move(item));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findByPickingStatus: " + e.displayText());
    }
    
    return items;
}

std::vector<std::unique_ptr<models::OrderItem>> OrderItemRepository::findItemsToPick()
{
    auto connection = acquireConnection();
    std::vector<std::unique_ptr<models::OrderItem>> items;
    
    try
    {
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT oi.id, oi.order_id, oi.product_id, oi.batch_id, "
                  "oi.quantity_ordered, oi.quantity_shipped, oi.unit_price, "
                  "oi.discount_percent, oi.line_total, oi.picking_status, "
                  "oi.picked_by, oi.picked_at, "
                  "p.name as product_name, p.sku as product_sku, "
                  "pb.batch_number, u.full_name as picked_by_name, "
                  "co.order_number, co.customer_name, co.priority "
                  "FROM " << TABLE_NAME << " oi "
                  "JOIN products p ON p.id = oi.product_id "
                  "JOIN product_batches pb ON pb.id = oi.batch_id "
                  "JOIN customer_orders co ON co.id = oi.order_id "
                  "LEFT JOIN users u ON u.id = oi.picked_by "
                  "WHERE oi.picking_status = 'not_started' "
                  "AND co.status NOT IN ('cancelled', 'delivered') "
                  "ORDER BY co.priority DESC, co.order_date ASC, oi.id",
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            auto item = std::make_unique<models::OrderItem>();
            item->id = rs.value("id", 0).convert<long long>();
            item->orderId = rs.value("order_id", 0).convert<long long>();
            item->productId = rs.value("product_id", 0).convert<long long>();
            item->batchId = rs.value("batch_id", 0).convert<long long>();
            item->quantityOrdered = rs.value("quantity_ordered", 0).convert<int>();
            item->quantityShipped = rs.value("quantity_shipped", 0).convert<int>();
            item->unitPrice = rs.value("unit_price", 0.0).convert<double>();
            item->discountPercent = rs.value("discount_percent", 0.0).convert<double>();
            item->lineTotal = rs.value("line_total", 0.0).convert<double>();
            
            item->pickingStatus = models::PickingStatus::NOT_STARTED;
            item->pickedBy = rs.value("picked_by").isEmpty() ? 0LL : rs.value("picked_by").convert<long long>();
            item->pickedAt = rs.value("picked_at").isEmpty() ? "" : rs.value("picked_at").convert<std::string>();
            item->productName = rs.value("product_name").convert<std::string>();
            item->productSku = rs.value("product_sku").convert<std::string>();
            item->batchNumber = rs.value("batch_number").convert<std::string>();
            item->pickedByName = rs.value("picked_by_name").isEmpty() ? "" : rs.value("picked_by_name").convert<std::string>();
            
            items.push_back(std::move(item));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findItemsToPick: " + e.displayText());
    }
    
    return items;
}

std::vector<std::unique_ptr<models::OrderItem>> OrderItemRepository::findItemsInProgress()
{
    return findByPickingStatus(models::PickingStatus::IN_PROGRESS);
}

std::vector<std::unique_ptr<models::OrderItem>> OrderItemRepository::findPickedItems()
{
    auto connection = acquireConnection();
    std::vector<std::unique_ptr<models::OrderItem>> items;
    
    try
    {
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT oi.id, oi.order_id, oi.product_id, oi.batch_id, "
                  "oi.quantity_ordered, oi.quantity_shipped, oi.unit_price, "
                  "oi.discount_percent, oi.line_total, oi.picking_status, "
                  "oi.picked_by, oi.picked_at, "
                  "p.name as product_name, p.sku as product_sku, "
                  "pb.batch_number, u.full_name as picked_by_name "
                  "FROM " << TABLE_NAME << " oi "
                  "JOIN products p ON p.id = oi.product_id "
                  "JOIN product_batches pb ON pb.id = oi.batch_id "
                  "LEFT JOIN users u ON u.id = oi.picked_by "
                  "WHERE oi.picking_status IN ('picked', 'packed') "
                  "ORDER BY oi.picked_at DESC",
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            auto item = std::make_unique<models::OrderItem>();
            item->id = rs.value("id", 0).convert<long long>();
            item->orderId = rs.value("order_id", 0).convert<long long>();
            item->productId = rs.value("product_id", 0).convert<long long>();
            item->batchId = rs.value("batch_id", 0).convert<long long>();
            item->quantityOrdered = rs.value("quantity_ordered", 0).convert<int>();
            item->quantityShipped = rs.value("quantity_shipped", 0).convert<int>();
            item->unitPrice = rs.value("unit_price", 0.0).convert<double>();
            item->discountPercent = rs.value("discount_percent", 0.0).convert<double>();
            item->lineTotal = rs.value("line_total", 0.0).convert<double>();
            
            std::string pickingStatusStr = rs.value("picking_status").convert<std::string>();
            item->pickingStatus = models::OrderItem::stringToPickingStatus(pickingStatusStr);
            
            item->pickedBy = rs.value("picked_by").isEmpty() ? 0LL : rs.value("picked_by").convert<long long>();
            item->pickedAt = rs.value("picked_at").isEmpty() ? "" : rs.value("picked_at").convert<std::string>();
            item->productName = rs.value("product_name").convert<std::string>();
            item->productSku = rs.value("product_sku").convert<std::string>();
            item->batchNumber = rs.value("batch_number").convert<std::string>();
            item->pickedByName = rs.value("picked_by_name").isEmpty() ? "" : rs.value("picked_by_name").convert<std::string>();
            
            items.push_back(std::move(item));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findPickedItems: " + e.displayText());
    }
    
    return items;
}

std::vector<std::unique_ptr<models::OrderItem>> OrderItemRepository::findItemsWithLowStock()
{
    auto connection = acquireConnection();
    std::vector<std::unique_ptr<models::OrderItem>> items;
    
    try
    {
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT oi.id, oi.order_id, oi.product_id, oi.batch_id, "
                  "oi.quantity_ordered, oi.quantity_shipped, oi.unit_price, "
                  "oi.discount_percent, oi.line_total, oi.picking_status, "
                  "oi.picked_by, oi.picked_at, "
                  "p.name as product_name, p.sku as product_sku, "
                  "pb.batch_number, u.full_name as picked_by_name, "
                  "pb.quantity_available as batch_available "
                  "FROM " << TABLE_NAME << " oi "
                  "JOIN products p ON p.id = oi.product_id "
                  "JOIN product_batches pb ON pb.id = oi.batch_id "
                  "LEFT JOIN users u ON u.id = oi.picked_by "
                  "WHERE oi.picking_status = 'not_started' "
                  "AND pb.quantity_available < (oi.quantity_ordered - oi.quantity_shipped) "
                  "ORDER BY oi.id",
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            auto item = std::make_unique<models::OrderItem>();
            item->id = rs.value("id", 0).convert<long long>();
            item->orderId = rs.value("order_id", 0).convert<long long>();
            item->productId = rs.value("product_id", 0).convert<long long>();
            item->batchId = rs.value("batch_id", 0).convert<long long>();
            item->quantityOrdered = rs.value("quantity_ordered", 0).convert<int>();
            item->quantityShipped = rs.value("quantity_shipped", 0).convert<int>();
            item->unitPrice = rs.value("unit_price", 0.0).convert<double>();
            item->discountPercent = rs.value("discount_percent", 0.0).convert<double>();
            item->lineTotal = rs.value("line_total", 0.0).convert<double>();
            
            std::string pickingStatusStr = rs.value("picking_status").convert<std::string>();
            item->pickingStatus = models::OrderItem::stringToPickingStatus(pickingStatusStr);
            
            item->pickedBy = rs.value("picked_by").isEmpty() ? 0LL : rs.value("picked_by").convert<long long>();
            item->pickedAt = rs.value("picked_at").isEmpty() ? "" : rs.value("picked_at").convert<std::string>();
            item->productName = rs.value("product_name").convert<std::string>();
            item->productSku = rs.value("product_sku").convert<std::string>();
            item->batchNumber = rs.value("batch_number").convert<std::string>();
            item->pickedByName = rs.value("picked_by_name").isEmpty() ? "" : rs.value("picked_by_name").convert<std::string>();
            
            items.push_back(std::move(item));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findItemsWithLowStock: " + e.displayText());
    }
    
    return items;
}

bool OrderItemRepository::updateQuantityShipped(long long id, int quantityShipped)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        long long idCopy = id;
        int quantityShippedCopy = quantityShipped;
        
        Poco::Data::Statement update(connection->getSession());
        update << "UPDATE " << TABLE_NAME << " SET quantity_shipped = $1 WHERE id = $2",
            Poco::Data::Keywords::use(quantityShippedCopy),
            Poco::Data::Keywords::use(idCopy);
        
        int rowsAffected = update.execute();
        
        commitTransaction(*connection);
        return rowsAffected > 0;
    }
    catch (const Poco::Exception& e)
    {
        rollbackTransaction(*connection);
        throw std::runtime_error("Database error in updateQuantityShipped: " + e.displayText());
    }
}

bool OrderItemRepository::updatePickingStatus(long long id, models::PickingStatus status)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        std::string statusStr = models::OrderItem::pickingStatusToString(status);
        long long idCopy = id;
        std::string statusStrCopy = statusStr;
        
        Poco::Data::Statement update(connection->getSession());
        update << "UPDATE " << TABLE_NAME << " SET picking_status = $1 WHERE id = $2",
            Poco::Data::Keywords::use(statusStrCopy),
            Poco::Data::Keywords::use(idCopy);
        
        int rowsAffected = update.execute();
        
        commitTransaction(*connection);
        return rowsAffected > 0;
    }
    catch (const Poco::Exception& e)
    {
        rollbackTransaction(*connection);
        throw std::runtime_error("Database error in updatePickingStatus: " + e.displayText());
    }
}

bool OrderItemRepository::markAsPicked(long long id, long long pickedBy)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        std::string statusStr = models::OrderItem::pickingStatusToString(models::PickingStatus::PICKED);
        std::string pickedAt = DateUtils::formatDateTime(DateUtils::now());
        
        Poco::Int64 idCopy = id;
        std::string statusStrCopy = statusStr;
        Poco::Int64 pickedByCopy = pickedBy;
        std::string pickedAtCopy = pickedAt;
        
        Poco::Data::Statement update(connection->getSession());
        update << "UPDATE " << TABLE_NAME << " SET "
                  "picking_status = $1, picked_by = $2, picked_at = $3 "
                  "WHERE id = $4",
            Poco::Data::Keywords::use(statusStrCopy),
            Poco::Data::Keywords::use(pickedByCopy),
            Poco::Data::Keywords::use(pickedAtCopy),
            Poco::Data::Keywords::use(idCopy);
        
        int rowsAffected = update.execute();
        
        commitTransaction(*connection);
        return rowsAffected > 0;
    }
    catch (const Poco::Exception& e)
    {
        rollbackTransaction(*connection);
        throw std::runtime_error("Database error in markAsPicked: " + e.displayText());
    }
}

bool OrderItemRepository::updateUnitPrice(long long id, double newUnitPrice)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        long long idCopy = id;
        double unitPriceCopy = newUnitPrice;
        
        Poco::Data::Statement update(connection->getSession());
        update << "UPDATE " << TABLE_NAME << " SET unit_price = $1 WHERE id = $2",
            Poco::Data::Keywords::use(unitPriceCopy),
            Poco::Data::Keywords::use(idCopy);
        
        int rowsAffected = update.execute();
        
        commitTransaction(*connection);
        return rowsAffected > 0;
    }
    catch (const Poco::Exception& e)
    {
        rollbackTransaction(*connection);
        throw std::runtime_error("Database error in updateUnitPrice: " + e.displayText());
    }
}

bool OrderItemRepository::updateDiscount(long long id, double newDiscountPercent)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        long long idCopy = id;
        double discountCopy = newDiscountPercent;
        
        Poco::Data::Statement update(connection->getSession());
        update << "UPDATE " << TABLE_NAME << " SET discount_percent = $1 WHERE id = $2",
            Poco::Data::Keywords::use(discountCopy),
            Poco::Data::Keywords::use(idCopy);
        
        int rowsAffected = update.execute();
        
        commitTransaction(*connection);
        return rowsAffected > 0;
    }
    catch (const Poco::Exception& e)
    {
        rollbackTransaction(*connection);
        throw std::runtime_error("Database error in updateDiscount: " + e.displayText());
    }
}

bool OrderItemRepository::updateBatch(long long id, long long newBatchId)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        long long idCopy = id;
        long long batchIdCopy = newBatchId;
        
        Poco::Data::Statement update(connection->getSession());
        update << "UPDATE " << TABLE_NAME << " SET batch_id = $1 WHERE id = $2",
            Poco::Data::Keywords::use(batchIdCopy),
            Poco::Data::Keywords::use(idCopy);
        
        int rowsAffected = update.execute();
        
        commitTransaction(*connection);
        return rowsAffected > 0;
    }
    catch (const Poco::Exception& e)
    {
        rollbackTransaction(*connection);
        throw std::runtime_error("Database error in updateBatch: " + e.displayText());
    }
}

bool OrderItemRepository::shipItem(long long id, int quantity, long long pickedBy)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        auto item = findById(id);
        if (!item)
        {
            throw std::runtime_error("Order item not found");
        }
        
        if (!canShipItem(id, quantity))
        {
            throw std::runtime_error("Cannot ship the requested quantity");
        }
        
        int newQuantityShipped = item->quantityShipped + quantity;
        
        bool success = updateQuantityShipped(id, newQuantityShipped);
        if (!success)
        {
            throw std::runtime_error("Failed to update quantity shipped");
        }
        
        if (newQuantityShipped == item->quantityOrdered)
        {
            success = markAsPicked(id, pickedBy);
        }
        else if (newQuantityShipped > 0)
        {
            success = updatePickingStatus(id, models::PickingStatus::IN_PROGRESS);
        }
        
        commitTransaction(*connection);
        return success;
    }
    catch (const Poco::Exception& e)
    {
        rollbackTransaction(*connection);
        throw std::runtime_error("Database error in shipItem: " + e.displayText());
    }
    catch (const std::exception& e)
    {
        rollbackTransaction(*connection);
        throw std::runtime_error(e.what());
    }
}

bool OrderItemRepository::cancelShipment(long long id)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        long long idCopy = id;
        
        Poco::Data::Statement update(connection->getSession());
        update << "UPDATE " << TABLE_NAME << " SET "
                  "quantity_shipped = 0, picking_status = 'not_started', "
                  "picked_by = NULL, picked_at = NULL "
                  "WHERE id = $1",
            Poco::Data::Keywords::use(idCopy);
        
        int rowsAffected = update.execute();
        
        commitTransaction(*connection);
        return rowsAffected > 0;
    }
    catch (const Poco::Exception& e)
    {
        rollbackTransaction(*connection);
        throw std::runtime_error("Database error in cancelShipment: " + e.displayText());
    }
}

int OrderItemRepository::countByOrder(long long orderId)
{
    auto connection = acquireConnection();
    
    try
    {
        long long orderIdCopy = orderId;
        
        Poco::Data::Statement countStmt(connection->getSession());
        countStmt << "SELECT COUNT(*) FROM " << TABLE_NAME << " WHERE order_id = $1",
            Poco::Data::Keywords::use(orderIdCopy),
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
        throw std::runtime_error("Database error in countByOrder: " + e.displayText());
    }
}

int OrderItemRepository::countByProduct(long long productId)
{
    auto connection = acquireConnection();
    
    try
    {
        long long productIdCopy = productId;
        
        Poco::Data::Statement countStmt(connection->getSession());
        countStmt << "SELECT COUNT(*) FROM " << TABLE_NAME << " WHERE product_id = $1",
            Poco::Data::Keywords::use(productIdCopy),
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
        throw std::runtime_error("Database error in countByProduct: " + e.displayText());
    }
}

int OrderItemRepository::countByPickingStatus(models::PickingStatus status)
{
    auto connection = acquireConnection();
    
    try
    {
        std::string statusStr = models::OrderItem::pickingStatusToString(status);
        std::string statusStrCopy = statusStr;
        
        Poco::Data::Statement countStmt(connection->getSession());
        countStmt << "SELECT COUNT(*) FROM " << TABLE_NAME << " WHERE picking_status = $1",
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
        throw std::runtime_error("Database error in countByPickingStatus: " + e.displayText());
    }
}

double OrderItemRepository::getTotalOrderValue(long long orderId)
{
    auto connection = acquireConnection();
    
    try
    {
        long long orderIdCopy = orderId;
        
        Poco::Data::Statement sumStmt(connection->getSession());
        sumStmt << "SELECT COALESCE(SUM(line_total), 0) FROM " << TABLE_NAME 
                << " WHERE order_id = $1",
            Poco::Data::Keywords::use(orderIdCopy),
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
        throw std::runtime_error("Database error in getTotalOrderValue: " + e.displayText());
    }
}

int OrderItemRepository::getTotalQuantityOrdered(long long orderId)
{
    auto connection = acquireConnection();
    
    try
    {
        long long orderIdCopy = orderId;
        
        Poco::Data::Statement sumStmt(connection->getSession());
        sumStmt << "SELECT COALESCE(SUM(quantity_ordered), 0) FROM " << TABLE_NAME 
                << " WHERE order_id = $1",
            Poco::Data::Keywords::use(orderIdCopy),
            now;
        
        Poco::Data::RecordSet rs(sumStmt);
        if (rs.rowCount() > 0)
        {
            return rs.value(0, 0).convert<int>();
        }
        
        return 0;
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in getTotalQuantityOrdered: " + e.displayText());
    }
}

int OrderItemRepository::getTotalQuantityShipped(long long orderId)
{
    auto connection = acquireConnection();
    
    try
    {
        long long orderIdCopy = orderId;
        
        Poco::Data::Statement sumStmt(connection->getSession());
        sumStmt << "SELECT COALESCE(SUM(quantity_shipped), 0) FROM " << TABLE_NAME 
                << " WHERE order_id = $1",
            Poco::Data::Keywords::use(orderIdCopy),
            now;
        
        Poco::Data::RecordSet rs(sumStmt);
        if (rs.rowCount() > 0)
        {
            return rs.value(0, 0).convert<int>();
        }
        
        return 0;
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in getTotalQuantityShipped: " + e.displayText());
    }
}

std::vector<std::pair<long long, std::string>> OrderItemRepository::getItemsForPicking(long long orderId)
{
    auto items = findByOrderId(orderId);
    std::vector<std::pair<long long, std::string>> result;
    
    for (const auto& item : items)
    {
        if (item && item->pickingStatus == models::PickingStatus::NOT_STARTED)
        {
            result.emplace_back(item->id, item->productName + " (" + item->productSku + ")");
        }
    }
    
    return result;
}

Poco::JSON::Array OrderItemRepository::getPickingReport(long long orderId)
{
    auto items = findByOrderId(orderId);
    Poco::JSON::Array jsonArray;
    
    for (const auto& item : items)
    {
        if (item)
        {
            Poco::JSON::Object itemJson = item->toJson();
            
            itemJson.set("remaining_to_pick", item->getRemainingToShip());
            itemJson.set("is_fully_shipped", item->isFullyShipped());
            itemJson.set("can_be_picked", item->canBePicked());
            
            jsonArray.add(itemJson);
        }
    }
    
    return jsonArray;
}

Poco::JSON::Array OrderItemRepository::getOrderItemSummary(long long orderId)
{
    auto connection = acquireConnection();
    Poco::JSON::Array jsonArray;
    
    try
    {
        long long orderIdCopy = orderId;
        
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT p.sku, p.name, "
                  "SUM(oi.quantity_ordered) as total_ordered, "
                  "SUM(oi.quantity_shipped) as total_shipped, "
                  "AVG(oi.unit_price) as avg_price, "
                  "SUM(oi.line_total) as total_value "
                  "FROM " << TABLE_NAME << " oi "
                  "JOIN products p ON p.id = oi.product_id "
                  "WHERE oi.order_id = $1 "
                  "GROUP BY p.sku, p.name "
                  "ORDER BY p.sku",
            Poco::Data::Keywords::use(orderIdCopy),
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            Poco::JSON::Object summary;
            summary.set("sku", rs.value("sku").convert<std::string>());
            summary.set("product_name", rs.value("name").convert<std::string>());
            summary.set("total_ordered", rs.value("total_ordered", 0).convert<int>());
            summary.set("total_shipped", rs.value("total_shipped", 0).convert<int>());
            summary.set("avg_price", rs.value("avg_price", 0.0).convert<double>());
            summary.set("total_value", rs.value("total_value", 0.0).convert<double>());
            
            jsonArray.add(summary);
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in getOrderItemSummary: " + e.displayText());
    }
    
    return jsonArray;
}

bool OrderItemRepository::canShipItem(long long id, int quantity)
{
    auto item = findById(id);
    if (!item)
    {
        return false;
    }
    
    int remaining = item->getRemainingToShip();
    return quantity <= remaining;
}

bool OrderItemRepository::isItemFullyShipped(long long id)
{
    auto item = findById(id);
    if (!item)
    {
        return false;
    }
    
    return item->isFullyShipped();
}

models::OrderItem OrderItemRepository::mapRowToItem(Poco::Data::Row& row) const
{
    models::OrderItem item;
    item.id = row.get(0).convert<long long>();
    item.orderId = row.get(1).convert<long long>();
    item.productId = row.get(2).convert<long long>();
    item.batchId = row.get(3).convert<long long>();
    item.quantityOrdered = row.get(4).convert<int>();
    item.quantityShipped = row.get(5).convert<int>();
    item.unitPrice = row.get(6).convert<double>();
    item.discountPercent = row.get(7).convert<double>();
    item.lineTotal = row.get(8).convert<double>();
    
    std::string pickingStatusStr = row.get(9).convert<std::string>();
    item.pickingStatus = models::OrderItem::stringToPickingStatus(pickingStatusStr);
    
    item.pickedBy = row.get(10).convert<long long>();
    item.pickedAt = row.get(11).convert<std::string>();
    
    return item;
}

} // namespace database::repositories
