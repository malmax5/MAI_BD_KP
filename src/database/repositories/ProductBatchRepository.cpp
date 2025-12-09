#include "ProductBatchRepository.hpp"
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

const std::string ProductBatchRepository::TABLE_NAME = "product_batches";
const std::vector<std::string> ProductBatchRepository::SEARCH_FIELDS = {
    "batch_number", "invoice_number"
};

ProductBatchRepository::ProductBatchRepository() : BaseRepository<models::ProductBatch>()
{

}

std::unique_ptr<models::ProductBatch> ProductBatchRepository::findById(long long id)
{
    auto connection = acquireConnection();
    
    try
    {
        Poco::Int64 pocoId = static_cast<Poco::Int64>(id);
        Statement select(connection->getSession());
        select << "SELECT pb.id, pb.batch_number, pb.product_id, pb.supplier_id, "
                  "pb.quantity_received, pb.quantity_available, pb.unit_cost, "
                  "pb.manufacture_date, pb.expiration_date, pb.arrival_date, "
                  "pb.storage_cell_id, pb.quality_status, pb.invoice_number, "
                  "p.name as product_name, p.sku as product_sku, "
                  "s.name as supplier_name, wc.cell_code as storage_cell_code "
                  "FROM " << TABLE_NAME << " pb "
                  "LEFT JOIN products p ON p.id = pb.product_id "
                  "LEFT JOIN suppliers s ON s.id = pb.supplier_id "
                  "LEFT JOIN warehouse_cells wc ON wc.id = pb.storage_cell_id "
                  "WHERE pb.id = $1",
            use(pocoId),
            now;
        
        RecordSet rs(select);
        
        if (rs.rowCount() > 0)
        {
            return std::make_unique<models::ProductBatch>(mapRowToProductBatch(rs.row(0)));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findById: " + e.displayText());
    }
    
    return nullptr;
}

std::vector<std::unique_ptr<models::ProductBatch>> ProductBatchRepository::findAll()
{
    std::vector<std::unique_ptr<models::ProductBatch>> productBatches;
    auto connection = acquireConnection();
    
    try
    {
        Statement select(connection->getSession());
        select << "SELECT pb.id, pb.batch_number, pb.product_id, pb.supplier_id, "
                  "pb.quantity_received, pb.quantity_available, pb.unit_cost, "
                  "pb.manufacture_date, pb.expiration_date, pb.arrival_date, "
                  "pb.storage_cell_id, pb.quality_status, pb.invoice_number, "
                  "p.name as product_name, p.sku as product_sku, "
                  "s.name as supplier_name, wc.cell_code as storage_cell_code "
                  "FROM " << TABLE_NAME << " pb "
                  "LEFT JOIN products p ON p.id = pb.product_id "
                  "LEFT JOIN suppliers s ON s.id = pb.supplier_id "
                  "LEFT JOIN warehouse_cells wc ON wc.id = pb.storage_cell_id "
                  "ORDER BY pb.arrival_date DESC",
            now;
        
        RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            productBatches.push_back(std::make_unique<models::ProductBatch>(mapRowToProductBatch(rs.row(i))));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findAll: " + e.displayText());
    }
    
    return productBatches;
}

std::vector<std::unique_ptr<models::ProductBatch>> ProductBatchRepository::findPaginated(int page, int pageSize)
{
    std::vector<std::unique_ptr<models::ProductBatch>> productBatches;
    auto connection = acquireConnection();
    
    if (page < 1) page = 1;
    int offset = (page - 1) * pageSize;
    
    try
    {
        int usePageSize = pageSize;
        int useOffset = offset;
        Statement select(connection->getSession());
        select << "SELECT pb.id, pb.batch_number, pb.product_id, pb.supplier_id, "
                  "pb.quantity_received, pb.quantity_available, pb.unit_cost, "
                  "pb.manufacture_date, pb.expiration_date, pb.arrival_date, "
                  "pb.storage_cell_id, pb.quality_status, pb.invoice_number, "
                  "p.name as product_name, p.sku as product_sku, "
                  "s.name as supplier_name, wc.cell_code as storage_cell_code "
                  "FROM " << TABLE_NAME << " pb "
                  "LEFT JOIN products p ON p.id = pb.product_id "
                  "LEFT JOIN suppliers s ON s.id = pb.supplier_id "
                  "LEFT JOIN warehouse_cells wc ON wc.id = pb.storage_cell_id "
                  "ORDER BY pb.arrival_date DESC LIMIT $1 OFFSET $2",
            use(usePageSize),
            use(useOffset),
            now;
        
        RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            productBatches.push_back(std::make_unique<models::ProductBatch>(mapRowToProductBatch(rs.row(i))));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findPaginated: " + e.displayText());
    }
    
    return productBatches;
}

long long ProductBatchRepository::create(const models::ProductBatch& productBatch)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        models::ProductBatch productBatchCopy = productBatch;

        std::string qualityStatusStr = models::ProductBatch::qualityStatusToString(productBatch.qualityStatus);
        
        Poco::Int64 newId = 0;
        Statement insert(connection->getSession());
        
        insert << "INSERT INTO " << TABLE_NAME << " "
                  "(batch_number, product_id, supplier_id, quantity_received, "
                  "quantity_available, unit_cost, arrival_date, storage_cell_id, "
                  "quality_status, invoice_number) "
                  "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10) "
                  "RETURNING id",
            use(productBatchCopy.batchNumber),
            use(productBatchCopy.productId),
            use(productBatchCopy.supplierId),
            use(productBatchCopy.quantityReceived),
            use(productBatchCopy.quantityAvailable),
            use(productBatchCopy.unitCost),
            use(productBatchCopy.arrivalDate),
            use(productBatchCopy.storageCellId),
            use(qualityStatusStr),
            use(productBatchCopy.invoiceNumber),
            into(newId),
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

bool ProductBatchRepository::update(long long id, const models::ProductBatch& productBatch)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);

        models::ProductBatch productBatchCopy = productBatch;
        
        std::string qualityStatusStr = models::ProductBatch::qualityStatusToString(productBatch.qualityStatus);
        
        Statement update(connection->getSession());
        
        update << "UPDATE " << TABLE_NAME << " SET "
                  "batch_number = $1, product_id = $2, supplier_id = $3, "
                  "quantity_received = $4, quantity_available = $5, unit_cost = $6, "
                  "arrival_date = $7, storage_cell_id = $8, quality_status = $9::quality_status, "
                  "invoice_number = $10 WHERE id = $11",
            use(productBatchCopy.batchNumber),
            use(productBatchCopy.productId),
            use(productBatchCopy.supplierId),
            use(productBatchCopy.quantityReceived),
            use(productBatchCopy.quantityAvailable),
            use(productBatchCopy.unitCost),
            use(productBatchCopy.arrivalDate),
            use(productBatchCopy.storageCellId),
            use(qualityStatusStr),
            use(productBatchCopy.invoiceNumber),
            use(productBatchCopy.id);
        
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

bool ProductBatchRepository::remove(long long id)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        long long idCopy = id;
        
        Statement checkOrderItems(connection->getSession());
        checkOrderItems << "SELECT COUNT(*) FROM order_items WHERE batch_id = $1",
            use(idCopy),
            now;
        
        RecordSet rs(checkOrderItems);
        int orderItemCount = 0;
        if (rs.rowCount() > 0)
        {
            orderItemCount = rs.value(0, 0).convert<int>();
        }
        
        if (orderItemCount > 0)
        {
            throw std::runtime_error("Cannot delete product batch with associated order items");
        }
        
        Statement del(connection->getSession());
        del << "DELETE FROM " << TABLE_NAME << " WHERE id = $1",
            use(idCopy);
        
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

bool ProductBatchRepository::softDelete(long long id)
{
    return updateQualityStatus(id, "rejected");
}

int ProductBatchRepository::count()
{
    auto connection = acquireConnection();
    
    try
    {
        Statement countStmt(connection->getSession());
        countStmt << "SELECT COUNT(*) FROM " << TABLE_NAME,
            now;
        
        RecordSet rs(countStmt);
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

Poco::JSON::Array ProductBatchRepository::findAllAsJson()
{
    auto productBatches = findAll();
    Poco::JSON::Array jsonArray;
    
    for (const auto& batch : productBatches)
    {
        if (batch)
        {
            jsonArray.add(batch->toJson());
        }
    }
    
    return jsonArray;
}

Poco::JSON::Object ProductBatchRepository::findByIdAsJson(long long id)
{
    auto batch = findById(id);
    if (batch)
    {
        return batch->toJson();
    }
    
    return Poco::JSON::Object();
}

std::vector<std::unique_ptr<models::ProductBatch>> ProductBatchRepository::findByField(
    const std::string& fieldName, const std::string& fieldValue)
{
    std::vector<std::unique_ptr<models::ProductBatch>> productBatches;
    auto connection = acquireConnection();
    
    try
    {
        std::string sql = "SELECT pb.id, pb.batch_number, pb.product_id, pb.supplier_id, "
                          "pb.quantity_received, pb.quantity_available, pb.unit_cost, "
                          "pb.manufacture_date, pb.expiration_date, pb.arrival_date, "
                          "pb.storage_cell_id, pb.quality_status, pb.invoice_number, "
                          "p.name as product_name, p.sku as product_sku, "
                          "s.name as supplier_name, wc.cell_code as storage_cell_code "
                          "FROM " + TABLE_NAME + " pb "
                          "LEFT JOIN products p ON p.id = pb.product_id "
                          "LEFT JOIN suppliers s ON s.id = pb.supplier_id "
                          "LEFT JOIN warehouse_cells wc ON wc.id = pb.storage_cell_id "
                          "WHERE pb." + fieldName + " = $1 "
                          "ORDER BY pb.arrival_date DESC";
        
        std::string fieldValueCopy = fieldValue;
        Statement select(connection->getSession());
        select << sql,
            use(fieldValueCopy),
            now;
        
        RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            productBatches.push_back(std::make_unique<models::ProductBatch>(mapRowToProductBatch(rs.row(i))));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findByField: " + e.displayText());
    }
    
    return productBatches;
}

std::vector<std::unique_ptr<models::ProductBatch>> ProductBatchRepository::search(
    const std::string& query, const std::vector<std::string>& fields)
{
    std::vector<std::unique_ptr<models::ProductBatch>> productBatches;
    auto connection = acquireConnection();
    
    try
    {
        std::string searchClause = buildSearchQuery(query, fields);
        std::string sql = "SELECT pb.id, pb.batch_number, pb.product_id, pb.supplier_id, "
                          "pb.quantity_received, pb.quantity_available, pb.unit_cost, "
                          "pb.manufacture_date, pb.expiration_date, pb.arrival_date, "
                          "pb.storage_cell_id, pb.quality_status, pb.invoice_number, "
                          "p.name as product_name, p.sku as product_sku, "
                          "s.name as supplier_name, wc.cell_code as storage_cell_code "
                          "FROM " + TABLE_NAME + " pb "
                          "LEFT JOIN products p ON p.id = pb.product_id "
                          "LEFT JOIN suppliers s ON s.id = pb.supplier_id "
                          "LEFT JOIN warehouse_cells wc ON wc.id = pb.storage_cell_id "
                          "WHERE " + searchClause + " "
                          "ORDER BY pb.arrival_date DESC";
        
        Statement select(connection->getSession());
        select << sql,
            now;
        
        RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            productBatches.push_back(std::make_unique<models::ProductBatch>(mapRowToProductBatch(rs.row(i))));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in search: " + e.displayText());
    }
    
    return productBatches;
}

std::unique_ptr<models::ProductBatch> ProductBatchRepository::findByBatchNumber(const std::string& batchNumber)
{
    auto connection = acquireConnection();
    
    try
    {
        std::string batchNumberCopy = batchNumber;
        Statement select(connection->getSession());
        select << "SELECT pb.id, pb.batch_number, pb.product_id, pb.supplier_id, "
                  "pb.quantity_received, pb.quantity_available, pb.unit_cost, "
                  "pb.manufacture_date, pb.expiration_date, pb.arrival_date, "
                  "pb.storage_cell_id, pb.quality_status, pb.invoice_number, "
                  "p.name as product_name, p.sku as product_sku, "
                  "s.name as supplier_name, wc.cell_code as storage_cell_code "
                  "FROM " << TABLE_NAME << " pb "
                  "LEFT JOIN products p ON p.id = pb.product_id "
                  "LEFT JOIN suppliers s ON s.id = pb.supplier_id "
                  "LEFT JOIN warehouse_cells wc ON wc.id = pb.storage_cell_id "
                  "WHERE pb.batch_number = $1",
            use(batchNumberCopy),
            now;
        
        RecordSet rs(select);
        
        if (rs.rowCount() > 0)
        {
            return std::make_unique<models::ProductBatch>(mapRowToProductBatch(rs.row(0)));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findByBatchNumber: " + e.displayText());
    }
    
    return nullptr;
}

std::vector<std::unique_ptr<models::ProductBatch>> ProductBatchRepository::findByProduct(long long productId)
{
    auto connection = acquireConnection();
    std::vector<std::unique_ptr<models::ProductBatch>> productBatches;
    
    try
    {
        long long productIdCopy = productId;
        Statement select(connection->getSession());
        select << "SELECT pb.id, pb.batch_number, pb.product_id, pb.supplier_id, "
                  "pb.quantity_received, pb.quantity_available, pb.unit_cost, "
                  "pb.manufacture_date, pb.expiration_date, pb.arrival_date, "
                  "pb.storage_cell_id, pb.quality_status, pb.invoice_number, "
                  "p.name as product_name, p.sku as product_sku, "
                  "s.name as supplier_name, wc.cell_code as storage_cell_code "
                  "FROM " << TABLE_NAME << " pb "
                  "LEFT JOIN products p ON p.id = pb.product_id "
                  "LEFT JOIN suppliers s ON s.id = pb.supplier_id "
                  "LEFT JOIN warehouse_cells wc ON wc.id = pb.storage_cell_id "
                  "WHERE pb.product_id = $1 "
                  "ORDER BY pb.expiration_date ASC, pb.arrival_date DESC",
            use(productIdCopy),
            now;
        
        RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            productBatches.push_back(std::make_unique<models::ProductBatch>(mapRowToProductBatch(rs.row(i))));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findByProduct: " + e.displayText());
    }
    
    return productBatches;
}

std::vector<std::unique_ptr<models::ProductBatch>> ProductBatchRepository::findBySupplier(long long supplierId)
{
    auto connection = acquireConnection();
    std::vector<std::unique_ptr<models::ProductBatch>> productBatches;
    
    try
    {
        long long supplierIdCopy = supplierId;
        Statement select(connection->getSession());
        select << "SELECT pb.id, pb.batch_number, pb.product_id, pb.supplier_id, "
                  "pb.quantity_received, pb.quantity_available, pb.unit_cost, "
                  "pb.manufacture_date, pb.expiration_date, pb.arrival_date, "
                  "pb.storage_cell_id, pb.quality_status, pb.invoice_number, "
                  "p.name as product_name, p.sku as product_sku, "
                  "s.name as supplier_name, wc.cell_code as storage_cell_code "
                  "FROM " << TABLE_NAME << " pb "
                  "LEFT JOIN products p ON p.id = pb.product_id "
                  "LEFT JOIN suppliers s ON s.id = pb.supplier_id "
                  "LEFT JOIN warehouse_cells wc ON wc.id = pb.storage_cell_id "
                  "WHERE pb.supplier_id = $1 "
                  "ORDER BY pb.arrival_date DESC",
            use(supplierIdCopy),
            now;
        
        RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            productBatches.push_back(std::make_unique<models::ProductBatch>(mapRowToProductBatch(rs.row(i))));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findBySupplier: " + e.displayText());
    }
    
    return productBatches;
}

std::vector<std::unique_ptr<models::ProductBatch>> ProductBatchRepository::findByStorageCell(long long storageCellId)
{
    auto connection = acquireConnection();
    std::vector<std::unique_ptr<models::ProductBatch>> productBatches;
    
    try
    {
        long long storageCellIdCopy = storageCellId;
        Statement select(connection->getSession());
        select << "SELECT pb.id, pb.batch_number, pb.product_id, pb.supplier_id, "
                  "pb.quantity_received, pb.quantity_available, pb.unit_cost, "
                  "pb.manufacture_date, pb.expiration_date, pb.arrival_date, "
                  "pb.storage_cell_id, pb.quality_status, pb.invoice_number, "
                  "p.name as product_name, p.sku as product_sku, "
                  "s.name as supplier_name, wc.cell_code as storage_cell_code "
                  "FROM " << TABLE_NAME << " pb "
                  "LEFT JOIN products p ON p.id = pb.product_id "
                  "LEFT JOIN suppliers s ON s.id = pb.supplier_id "
                  "LEFT JOIN warehouse_cells wc ON wc.id = pb.storage_cell_id "
                  "WHERE pb.storage_cell_id = $1 "
                  "ORDER BY pb.expiration_date ASC",
            use(storageCellIdCopy),
            now;
        
        RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            productBatches.push_back(std::make_unique<models::ProductBatch>(mapRowToProductBatch(rs.row(i))));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findByStorageCell: " + e.displayText());
    }
    
    return productBatches;
}

std::vector<std::unique_ptr<models::ProductBatch>> ProductBatchRepository::findByQualityStatus(const std::string& qualityStatus)
{
    auto connection = acquireConnection();
    std::vector<std::unique_ptr<models::ProductBatch>> productBatches;
    
    try
    {
        std::string qualityStatusCopy = qualityStatus;
        Statement select(connection->getSession());
        select << "SELECT pb.id, pb.batch_number, pb.product_id, pb.supplier_id, "
                  "pb.quantity_received, pb.quantity_available, pb.unit_cost, "
                  "pb.manufacture_date, pb.expiration_date, pb.arrival_date, "
                  "pb.storage_cell_id, pb.quality_status, pb.invoice_number, "
                  "p.name as product_name, p.sku as product_sku, "
                  "s.name as supplier_name, wc.cell_code as storage_cell_code "
                  "FROM " << TABLE_NAME << " pb "
                  "LEFT JOIN products p ON p.id = pb.product_id "
                  "LEFT JOIN suppliers s ON s.id = pb.supplier_id "
                  "LEFT JOIN warehouse_cells wc ON wc.id = pb.storage_cell_id "
                  "WHERE pb.quality_status = $1::quality_status "
                  "ORDER BY pb.arrival_date DESC",
            use(qualityStatusCopy),
            now;
        
        RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            productBatches.push_back(std::make_unique<models::ProductBatch>(mapRowToProductBatch(rs.row(i))));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findByQualityStatus: " + e.displayText());
    }
    
    return productBatches;
}

std::vector<std::unique_ptr<models::ProductBatch>> ProductBatchRepository::findExpiringBatches(int daysThreshold)
{
    auto connection = acquireConnection();
    std::vector<std::unique_ptr<models::ProductBatch>> productBatches;
    
    try
    {
        Statement select(connection->getSession());
        select << "SELECT pb.id, pb.batch_number, pb.product_id, pb.supplier_id, "
                  "pb.quantity_received, pb.quantity_available, pb.unit_cost, "
                  "pb.manufacture_date, pb.expiration_date, pb.arrival_date, "
                  "pb.storage_cell_id, pb.quality_status, pb.invoice_number, "
                  "p.name as product_name, p.sku as product_sku, "
                  "s.name as supplier_name, wc.cell_code as storage_cell_code "
                  "FROM " << TABLE_NAME << " pb "
                  "LEFT JOIN products p ON p.id = pb.product_id "
                  "LEFT JOIN suppliers s ON s.id = pb.supplier_id "
                  "LEFT JOIN warehouse_cells wc ON wc.id = pb.storage_cell_id "
                  "WHERE pb.expiration_date IS NOT NULL "
                  "AND pb.expiration_date >= CURRENT_DATE "
                  "AND pb.expiration_date <= CURRENT_DATE + $1 "
                  "AND pb.quantity_available > 0 "
                  "AND pb.quality_status = 'approved'::quality_status "
                  "ORDER BY pb.expiration_date ASC",
            use(daysThreshold),
            now;
        
        RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            productBatches.push_back(std::make_unique<models::ProductBatch>(mapRowToProductBatch(rs.row(i))));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findExpiringBatches: " + e.displayText());
    }
    
    return productBatches;
}

std::vector<std::unique_ptr<models::ProductBatch>> ProductBatchRepository::findExpiredBatches()
{
    auto connection = acquireConnection();
    std::vector<std::unique_ptr<models::ProductBatch>> productBatches;
    
    try
    {
        Statement select(connection->getSession());
        select << "SELECT pb.id, pb.batch_number, pb.product_id, pb.supplier_id, "
                  "pb.quantity_received, pb.quantity_available, pb.unit_cost, "
                  "pb.manufacture_date, pb.expiration_date, pb.arrival_date, "
                  "pb.storage_cell_id, pb.quality_status, pb.invoice_number, "
                  "p.name as product_name, p.sku as product_sku, "
                  "s.name as supplier_name, wc.cell_code as storage_cell_code "
                  "FROM " << TABLE_NAME << " pb "
                  "LEFT JOIN products p ON p.id = pb.product_id "
                  "LEFT JOIN suppliers s ON s.id = pb.supplier_id "
                  "LEFT JOIN warehouse_cells wc ON wc.id = pb.storage_cell_id "
                  "WHERE pb.expiration_date IS NOT NULL "
                  "AND pb.expiration_date < CURRENT_DATE "
                  "AND pb.quantity_available > 0 "
                  "ORDER BY pb.expiration_date ASC",
            now;
        
        RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            productBatches.push_back(std::make_unique<models::ProductBatch>(mapRowToProductBatch(rs.row(i))));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findExpiredBatches: " + e.displayText());
    }
    
    return productBatches;
}

std::vector<std::unique_ptr<models::ProductBatch>> ProductBatchRepository::findBatchesNeedingInspection()
{
    return findByQualityStatus("pending");
}

std::vector<std::unique_ptr<models::ProductBatch>> ProductBatchRepository::findAvailableBatches(long long productId, int quantity)
{
    auto connection = acquireConnection();
    std::vector<std::unique_ptr<models::ProductBatch>> productBatches;
    
    try
    {
        long long productIdCopy = productId;
        int quantityCopy = quantity;

        std::cout << "Connection is connected: " << connection->isConnected() << std::endl;
        std::cout << "AutoCommit: " << connection->getAutoCommit() << std::endl;
        std::cout << "Transaction active: " << connection->isTransactionActive() << std::endl;
        
        Statement select(connection->getSession());
        select << "SELECT pb.id, pb.batch_number, pb.product_id, pb.supplier_id, "
                  "pb.quantity_received, pb.quantity_available, pb.unit_cost, "
                  "pb.manufacture_date, pb.expiration_date, pb.arrival_date, "
                  "pb.storage_cell_id, pb.quality_status, pb.invoice_number, "
                  "p.name as product_name, p.sku as product_sku, "
                  "s.name as supplier_name, wc.cell_code as storage_cell_code "
                  "FROM " << TABLE_NAME << " pb "
                  "LEFT JOIN products p ON p.id = pb.product_id "
                  "LEFT JOIN suppliers s ON s.id = pb.supplier_id "
                  "LEFT JOIN warehouse_cells wc ON wc.id = pb.storage_cell_id "
                  "WHERE pb.product_id = $1 "
                  "AND pb.quantity_available >= $2 "
                  "AND pb.quality_status = 'approved'::quality_status "
                  "AND (pb.expiration_date IS NULL OR pb.expiration_date >= CURRENT_DATE) "
                  "ORDER BY pb.expiration_date ASC, pb.arrival_date ASC",
            use(productIdCopy),
            use(quantityCopy),
            now;
        
        RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            productBatches.push_back(std::make_unique<models::ProductBatch>(mapRowToProductBatch(rs.row(i))));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findAvailableBatches: " + e.displayText());
    }
    
    return productBatches;
}

std::vector<std::unique_ptr<models::ProductBatch>> ProductBatchRepository::findBatchesWithLowQuantity(int threshold)
{
    auto connection = acquireConnection();
    std::vector<std::unique_ptr<models::ProductBatch>> productBatches;
    
    try
    {
        int thresholdCopy = threshold;
        
        Statement select(connection->getSession());
        select << "SELECT pb.id, pb.batch_number, pb.product_id, pb.supplier_id, "
                  "pb.quantity_received, pb.quantity_available, pb.unit_cost, "
                  "pb.manufacture_date, pb.expiration_date, pb.arrival_date, "
                  "pb.storage_cell_id, pb.quality_status, pb.invoice_number, "
                  "p.name as product_name, p.sku as product_sku, "
                  "s.name as supplier_name, wc.cell_code as storage_cell_code "
                  "FROM " << TABLE_NAME << " pb "
                  "LEFT JOIN products p ON p.id = pb.product_id "
                  "LEFT JOIN suppliers s ON s.id = pb.supplier_id "
                  "LEFT JOIN warehouse_cells wc ON wc.id = pb.storage_cell_id "
                  "WHERE pb.quantity_available <= $1 "
                  "AND pb.quantity_available > 0 "
                  "AND pb.quality_status = 'approved'::quality_status "
                  "ORDER BY pb.quantity_available ASC",
            use(thresholdCopy),
            now;
        
        RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            productBatches.push_back(std::make_unique<models::ProductBatch>(mapRowToProductBatch(rs.row(i))));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findBatchesWithLowQuantity: " + e.displayText());
    }
    
    return productBatches;
}

bool ProductBatchRepository::updateQualityStatus(long long id, const std::string& qualityStatus)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        std::string qualityStatusCopy = qualityStatus;
        long long idCopy = id;
        
        Statement update(connection->getSession());
        update << "UPDATE " << TABLE_NAME << " SET quality_status = $1::quality_status WHERE id = $2",
            use(qualityStatusCopy),
            use(idCopy),
            now;
        
        int rowsAffected = update.execute();
        
        commitTransaction(*connection);
        return rowsAffected > 0;
    }
    catch (const Poco::Exception& e)
    {
        rollbackTransaction(*connection);
        throw std::runtime_error("Database error in updateQualityStatus: " + e.displayText());
    }
}

bool ProductBatchRepository::updateQuantity(long long id, int quantityReceived, int quantityAvailable)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        int quantityReceivedCopy = quantityReceived;
        int quantityAvailableCopy = quantityAvailable;
        long long idCopy = id;
        
        if (quantityAvailableCopy > quantityReceivedCopy)
        {
            throw std::runtime_error("Available quantity cannot exceed received quantity");
        }
        
        Statement update(connection->getSession());
        update << "UPDATE " << TABLE_NAME << " SET quantity_received = $1, quantity_available = $2 WHERE id = $3",
            use(quantityReceivedCopy),
            use(quantityAvailableCopy),
            use(idCopy),
            now;
        
        int rowsAffected = update.execute();
        
        commitTransaction(*connection);
        return rowsAffected > 0;
    }
    catch (const Poco::Exception& e)
    {
        rollbackTransaction(*connection);
        throw std::runtime_error("Database error in updateQuantity: " + e.displayText());
    }
}

bool ProductBatchRepository::updateStorageCell(long long id, long long storageCellId)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        long long storageCellIdCopy = storageCellId;
        long long idCopy = id;
        
        Statement update(connection->getSession());
        update << "UPDATE " << TABLE_NAME << " SET storage_cell_id = $1 WHERE id = $2",
            use(storageCellIdCopy),
            use(idCopy),
            now;
        
        int rowsAffected = update.execute();
        
        commitTransaction(*connection);
        return rowsAffected > 0;
    }
    catch (const Poco::Exception& e)
    {
        rollbackTransaction(*connection);
        throw std::runtime_error("Database error in updateStorageCell: " + e.displayText());
    }
}

bool ProductBatchRepository::updateExpirationDate(long long id, const std::string& expirationDate)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        std::string expirationDateCopy = expirationDate;
        long long idCopy = id;
        
        Statement update(connection->getSession());
        update << "UPDATE " << TABLE_NAME << " SET expiration_date = $1 WHERE id = $2",
            use(expirationDateCopy),
            use(idCopy),
            now;
        
        int rowsAffected = update.execute();
        
        commitTransaction(*connection);
        return rowsAffected > 0;
    }
    catch (const Poco::Exception& e)
    {
        rollbackTransaction(*connection);
        throw std::runtime_error("Database error in updateExpirationDate: " + e.displayText());
    }
}

bool ProductBatchRepository::updateInvoiceNumber(long long id, const std::string& invoiceNumber)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        std::string invoiceNumberCopy = invoiceNumber;
        long long idCopy = id;
        
        Statement update(connection->getSession());
        update << "UPDATE " << TABLE_NAME << " SET invoice_number = $1 WHERE id = $2",
            use(invoiceNumberCopy),
            use(idCopy),
            now;
        
        int rowsAffected = update.execute();
        
        commitTransaction(*connection);
        return rowsAffected > 0;
    }
    catch (const Poco::Exception& e)
    {
        rollbackTransaction(*connection);
        throw std::runtime_error("Database error in updateInvoiceNumber: " + e.displayText());
    }
}

bool ProductBatchRepository::useQuantity(long long id, int quantity)
{
    auto batch = findById(id);
    if (!batch)
    {
        return false;
    }
    
    if (batch->quantityAvailable < quantity)
    {
        throw std::runtime_error("Insufficient quantity available in batch");
    }
    
    int newQuantity = batch->quantityAvailable - quantity;
    return updateQuantity(id, batch->quantityReceived, newQuantity);
}

bool ProductBatchRepository::returnQuantity(long long id, int quantity)
{
    auto batch = findById(id);
    if (!batch)
    {
        return false;
    }
    
    int newQuantity = batch->quantityAvailable + quantity;
    if (newQuantity > batch->quantityReceived)
    {
        throw std::runtime_error("Returned quantity cannot exceed received quantity");
    }
    
    return updateQuantity(id, batch->quantityReceived, newQuantity);
}

int ProductBatchRepository::countByProduct(long long productId)
{
    auto connection = acquireConnection();
    
    try
    {
        long long productIdCopy = productId;
        Statement countStmt(connection->getSession());
        countStmt << "SELECT COUNT(*) FROM " << TABLE_NAME << " WHERE product_id = $1",
            use(productIdCopy),
            now;
        
        RecordSet rs(countStmt);
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

int ProductBatchRepository::countBySupplier(long long supplierId)
{
    auto connection = acquireConnection();
    
    try
    {
        long long supplierIdCopy = supplierId;
        Statement countStmt(connection->getSession());
        countStmt << "SELECT COUNT(*) FROM " << TABLE_NAME << " WHERE supplier_id = $1",
            use(supplierIdCopy),
            now;
        
        RecordSet rs(countStmt);
        if (rs.rowCount() > 0)
        {
            return rs.value(0, 0).convert<int>();
        }
        
        return 0;
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in countBySupplier: " + e.displayText());
    }
}

int ProductBatchRepository::countByQualityStatus(const std::string& qualityStatus)
{
    auto connection = acquireConnection();
    
    try
    {
        std::string qualityStatusCopy = qualityStatus;
        Statement countStmt(connection->getSession());
        countStmt << "SELECT COUNT(*) FROM " << TABLE_NAME << " WHERE quality_status = $1::quality_status",
            use(qualityStatusCopy),
            now;
        
        RecordSet rs(countStmt);
        if (rs.rowCount() > 0)
        {
            return rs.value(0, 0).convert<int>();
        }
        
        return 0;
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in countByQualityStatus: " + e.displayText());
    }
}

int ProductBatchRepository::countExpiringBatches(int daysThreshold)
{
    auto connection = acquireConnection();
    
    try
    {
        int daysThresholdCopy = daysThreshold;
        Statement countStmt(connection->getSession());
        countStmt << "SELECT COUNT(*) FROM " << TABLE_NAME << " "
                  "WHERE expiration_date IS NOT NULL "
                  "AND expiration_date >= CURRENT_DATE "
                  "AND expiration_date <= CURRENT_DATE + $1 "
                  "AND quantity_available > 0 "
                  "AND quality_status = 'approved'::quality_status",
            use(daysThresholdCopy),
            now;
        
        RecordSet rs(countStmt);
        if (rs.rowCount() > 0)
        {
            return rs.value(0, 0).convert<int>();
        }
        
        return 0;
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in countExpiringBatches: " + e.displayText());
    }
}

int ProductBatchRepository::countExpiredBatches()
{
    auto connection = acquireConnection();
    
    try
    {
        Statement countStmt(connection->getSession());
        countStmt << "SELECT COUNT(*) FROM " << TABLE_NAME << " "
                  "WHERE expiration_date IS NOT NULL "
                  "AND expiration_date < CURRENT_DATE "
                  "AND quantity_available > 0",
            now;
        
        RecordSet rs(countStmt);
        if (rs.rowCount() > 0)
        {
            return rs.value(0, 0).convert<int>();
        }
        
        return 0;
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in countExpiredBatches: " + e.displayText());
    }
}

double ProductBatchRepository::getTotalBatchValue()
{
    auto connection = acquireConnection();
    
    try
    {
        Statement select(connection->getSession());
        select << "SELECT COALESCE(SUM(quantity_available * unit_cost), 0) FROM " << TABLE_NAME 
                  << " WHERE quality_status = 'approved'::quality_status",
            now;
        
        RecordSet rs(select);
        if (rs.rowCount() > 0)
        {
            return rs.value(0, 0).convert<double>();
        }
        
        return 0.0;
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in getTotalBatchValue: " + e.displayText());
    }
}

double ProductBatchRepository::getTotalBatchValueByProduct(long long productId)
{
    auto connection = acquireConnection();
    
    try
    {
        long long productIdCopy = productId;
        Statement select(connection->getSession());
        select << "SELECT COALESCE(SUM(quantity_available * unit_cost), 0) FROM " << TABLE_NAME 
                  << " WHERE product_id = $1 AND quality_status = 'approved'::quality_status",
            use(productIdCopy),
            now;
        
        RecordSet rs(select);
        if (rs.rowCount() > 0)
        {
            return rs.value(0, 0).convert<double>();
        }
        
        return 0.0;
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in getTotalBatchValueByProduct: " + e.displayText());
    }
}

double ProductBatchRepository::getTotalBatchValueBySupplier(long long supplierId)
{
    auto connection = acquireConnection();
    
    try
    {
        long long supplierIdCopy = supplierId;
        Statement select(connection->getSession());
        select << "SELECT COALESCE(SUM(quantity_available * unit_cost), 0) FROM " << TABLE_NAME 
                  << " WHERE supplier_id = $1 AND quality_status = 'approved'::quality_status",
            use(supplierIdCopy),
            now;
        
        RecordSet rs(select);
        if (rs.rowCount() > 0)
        {
            return rs.value(0, 0).convert<double>();
        }
        
        return 0.0;
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in getTotalBatchValueBySupplier: " + e.displayText());
    }
}

int ProductBatchRepository::getTotalAvailableQuantity(long long productId)
{
    auto connection = acquireConnection();
    
    try
    {
        long long productIdCopy = productId;
        Statement select(connection->getSession());
        select << "SELECT COALESCE(SUM(quantity_available), 0) FROM " << TABLE_NAME 
                  << " WHERE product_id = $1 AND quality_status = 'approved'::quality_status",
            use(productIdCopy),
            now;
        
        RecordSet rs(select);
        if (rs.rowCount() > 0)
        {
            return rs.value(0, 0).convert<int>();
        }
        
        return 0;
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in getTotalAvailableQuantity: " + e.displayText());
    }
}

double ProductBatchRepository::getAverageUnitCost(long long productId)
{
    auto connection = acquireConnection();
    
    try
    {
        long long productIdCopy = productId;
        Statement select(connection->getSession());
        select << "SELECT COALESCE(AVG(unit_cost), 0) FROM " << TABLE_NAME 
                  << " WHERE product_id = $1 AND quality_status = 'approved'::quality_status",
            use(productIdCopy),
            now;
        
        RecordSet rs(select);
        if (rs.rowCount() > 0)
        {
            return rs.value(0, 0).convert<double>();
        }
        
        return 0.0;
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in getAverageUnitCost: " + e.displayText());
    }
}

bool ProductBatchRepository::batchNumberExists(const std::string& batchNumber)
{
    auto connection = acquireConnection();
    
    try
    {
        std::string batchNumberCopy = batchNumber;
        Statement countStmt(connection->getSession());
        countStmt << "SELECT COUNT(*) FROM " << TABLE_NAME << " WHERE batch_number = $1",
            use(batchNumberCopy),
            now;
        
        RecordSet rs(countStmt);
        if (rs.rowCount() > 0)
        {
            int count = rs.value(0, 0).convert<int>();
            return count > 0;
        }
        
        return false;
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in batchNumberExists: " + e.displayText());
    }
}

Poco::JSON::Array ProductBatchRepository::getBatchStatistics()
{
    auto connection = acquireConnection();
    Poco::JSON::Array result;
    
    try
    {
        Statement select(connection->getSession());
        select << "SELECT "
                  "COUNT(*) as total_batches, "
                  "SUM(quantity_received) as total_received, "
                  "SUM(quantity_available) as total_available, "
                  "SUM(quantity_available * unit_cost) as total_value, "
                  "AVG(unit_cost)::DECIMAL(15,2) as avg_unit_cost, "
                  "COUNT(CASE WHEN quality_status = 'approved' THEN 1 END) as approved_batches, "
                  "COUNT(CASE WHEN quality_status = 'pending' THEN 1 END) as pending_batches, "
                  "COUNT(CASE WHEN quality_status = 'quarantine' THEN 1 END) as quarantine_batches, "
                  "COUNT(CASE WHEN quality_status = 'rejected' THEN 1 END) as rejected_batches, "
                  "COUNT(CASE WHEN expiration_date < CURRENT_DATE THEN 1 END) as expired_batches "
                  "FROM " << TABLE_NAME,
            now;
        
        RecordSet rs(select);
        
        if (rs.rowCount() > 0)
        {
            Poco::JSON::Object stat;
            stat.set("total_batches", rs.value("total_batches").convert<int>());
            stat.set("total_received", rs.value("total_received").convert<int>());
            stat.set("total_available", rs.value("total_available").convert<int>());
            stat.set("total_value", rs.value("total_value").convert<double>());
            stat.set("avg_unit_cost", rs.value("avg_unit_cost").convert<double>());
            stat.set("approved_batches", rs.value("approved_batches").convert<int>());
            stat.set("pending_batches", rs.value("pending_batches").convert<int>());
            stat.set("quarantine_batches", rs.value("quarantine_batches").convert<int>());
            stat.set("rejected_batches", rs.value("rejected_batches").convert<int>());
            stat.set("expired_batches", rs.value("expired_batches").convert<int>());
            
            result.add(stat);
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in getBatchStatistics: " + e.displayText());
    }
    
    return result;
}

Poco::JSON::Array ProductBatchRepository::getExpirationReport()
{
    auto connection = acquireConnection();
    Poco::JSON::Array result;
    
    try
    {
        Statement select(connection->getSession());
        select << "SELECT "
                  "CASE "
                  "  WHEN expiration_date IS NULL THEN 'No Expiration' "
                  "  WHEN expiration_date < CURRENT_DATE THEN 'Expired' "
                  "  WHEN expiration_date <= CURRENT_DATE + 7 THEN 'Expiring in 7 days' "
                  "  WHEN expiration_date <= CURRENT_DATE + 30 THEN 'Expiring in 30 days' "
                  "  WHEN expiration_date <= CURRENT_DATE + 90 THEN 'Expiring in 90 days' "
                  "  ELSE 'More than 90 days' "
                  "END as expiration_category, "
                  "COUNT(*) as batch_count, "
                  "SUM(quantity_available) as total_quantity, "
                  "SUM(quantity_available * unit_cost) as total_value "
                  "FROM " << TABLE_NAME << " "
                  "WHERE quality_status = 'approved'::quality_status "
                  "GROUP BY expiration_category "
                  "ORDER BY expiration_category",
            now;
        
        RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            Poco::JSON::Object stat;
            stat.set("expiration_category", rs.value("expiration_category").convert<std::string>());
            stat.set("batch_count", rs.value("batch_count").convert<int>());
            stat.set("total_quantity", rs.value("total_quantity").convert<int>());
            stat.set("total_value", rs.value("total_value").convert<double>());
            
            result.add(stat);
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in getExpirationReport: " + e.displayText());
    }
    
    return result;
}

Poco::JSON::Array ProductBatchRepository::getQualityStatusReport()
{
    auto connection = acquireConnection();
    Poco::JSON::Array result;
    
    try
    {
        Statement select(connection->getSession());
        select << "SELECT quality_status, "
                  "COUNT(*) as batch_count, "
                  "SUM(quantity_received) as total_received, "
                  "SUM(quantity_available) as total_available, "
                  "SUM(quantity_available * unit_cost) as total_value, "
                  "AVG(unit_cost)::DECIMAL(15,2) as avg_unit_cost "
                  "FROM " << TABLE_NAME << " "
                  "GROUP BY quality_status "
                  "ORDER BY quality_status",
            now;
        
        RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            Poco::JSON::Object stat;
            stat.set("quality_status", rs.value("quality_status").convert<std::string>());
            stat.set("batch_count", rs.value("batch_count").convert<int>());
            stat.set("total_received", rs.value("total_received").convert<int>());
            stat.set("total_available", rs.value("total_available").convert<int>());
            stat.set("total_value", rs.value("total_value").convert<double>());
            stat.set("avg_unit_cost", rs.value("avg_unit_cost").convert<double>());
            
            result.add(stat);
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in getQualityStatusReport: " + e.displayText());
    }
    
    return result;
}

Poco::JSON::Array ProductBatchRepository::getSupplierBatchReport(long long supplierId)
{
    auto connection = acquireConnection();
    Poco::JSON::Array result;
    
    try
    {
        long long supplierIdCopy = supplierId;
        Statement select(connection->getSession());
        select << "SELECT p.name as product_name, p.sku, "
                  "COUNT(pb.id) as batch_count, "
                  "SUM(pb.quantity_received) as total_received, "
                  "SUM(pb.quantity_available) as total_available, "
                  "SUM(pb.quantity_available * pb.unit_cost) as total_value, "
                  "AVG(pb.unit_cost)::DECIMAL(15,2) as avg_unit_cost, "
                  "MIN(pb.expiration_date) as earliest_expiration "
                  "FROM " << TABLE_NAME << " pb "
                  "JOIN products p ON p.id = pb.product_id "
                  "WHERE pb.supplier_id = $1 "
                  "AND pb.quality_status = 'approved'::quality_status "
                  "GROUP BY p.id, p.name, p.sku "
                  "ORDER BY p.name",
            use(supplierIdCopy),
            now;
        
        RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            Poco::JSON::Object stat;
            stat.set("product_name", rs.value("product_name").convert<std::string>());
            stat.set("sku", rs.value("sku").convert<std::string>());
            stat.set("batch_count", rs.value("batch_count").convert<int>());
            stat.set("total_received", rs.value("total_received").convert<int>());
            stat.set("total_available", rs.value("total_available").convert<int>());
            stat.set("total_value", rs.value("total_value").convert<double>());
            stat.set("avg_unit_cost", rs.value("avg_unit_cost").convert<double>());
            stat.set("earliest_expiration", rs.value("earliest_expiration").convert<std::string>());
            
            result.add(stat);
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in getSupplierBatchReport: " + e.displayText());
    }
    
    return result;
}

std::vector<std::pair<long long, std::string>> ProductBatchRepository::getBatchNumbers()
{
    auto connection = acquireConnection();
    std::vector<std::pair<long long, std::string>> result;
    
    try
    {
        Statement select(connection->getSession());
        select << "SELECT id, batch_number FROM " << TABLE_NAME << " ORDER BY batch_number",
            now;
        
        RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            result.emplace_back(
                rs.value("id", 0).convert<long long>(),
                rs.value("batch_number").convert<std::string>()
            );
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in getBatchNumbers: " + e.displayText());
    }
    
    return result;
}

std::vector<models::ProductBatch> ProductBatchRepository::findBatchesForOrderItem(long long productId, int quantity)
{
    auto connection = acquireConnection();
    std::vector<models::ProductBatch> result;
    
    try
    {
        long long productIdCopy = productId;
        int quantityCopy = quantity;
        
        Statement select(connection->getSession());
        select << "SELECT pb.id, pb.batch_number, pb.product_id, pb.supplier_id, "
                  "pb.quantity_received, pb.quantity_available, pb.unit_cost, "
                  "pb.manufacture_date, pb.expiration_date, pb.arrival_date, "
                  "pb.storage_cell_id, pb.quality_status, pb.invoice_number, "
                  "p.name as product_name, p.sku as product_sku, "
                  "s.name as supplier_name, wc.cell_code as storage_cell_code "
                  "FROM " << TABLE_NAME << " pb "
                  "LEFT JOIN products p ON p.id = pb.product_id "
                  "LEFT JOIN suppliers s ON s.id = pb.supplier_id "
                  "LEFT JOIN warehouse_cells wc ON wc.id = pb.storage_cell_id "
                  "WHERE pb.product_id = $1 "
                  "AND pb.quantity_available > 0 "
                  "AND pb.quality_status = 'approved'::quality_status "
                  "AND (pb.expiration_date IS NULL OR pb.expiration_date >= CURRENT_DATE) "
                  "ORDER BY pb.expiration_date ASC, pb.arrival_date ASC",
            use(productIdCopy),
            now;
        
        RecordSet rs(select);
        
        int remainingQuantity = quantityCopy;
        
        for (size_t i = 0; i < rs.rowCount() && remainingQuantity > 0; ++i)
        {
            models::ProductBatch batch = mapRowToProductBatch(rs.row(i));
            
            int allocatedQuantity = std::min(batch.quantityAvailable, remainingQuantity);
            
            result.push_back(batch);
            remainingQuantity -= allocatedQuantity;
        }
        
        if (remainingQuantity > 0)
        {
            throw std::runtime_error("Insufficient stock for product ID: " + std::to_string(productId));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findBatchesForOrderItem: " + e.displayText());
    }
    
    return result;
}

std::map<long long, int> ProductBatchRepository::getProductStockSummary()
{
    auto connection = acquireConnection();
    std::map<long long, int> result;
    
    try
    {
        Statement select(connection->getSession());
        select << "SELECT product_id, SUM(quantity_available) as total_stock "
                  "FROM " << TABLE_NAME << " "
                  "WHERE quality_status = 'approved'::quality_status "
                  "GROUP BY product_id",
            now;
        
        RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            long long productId = rs.value("product_id", 0).convert<long long>();
            int totalStock = rs.value("total_stock", 0).convert<int>();
            result[productId] = totalStock;
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in getProductStockSummary: " + e.displayText());
    }
    
    return result;
}

models::ProductBatch ProductBatchRepository::mapRowToProductBatch(Poco::Data::Row& row) const
{
    models::ProductBatch batch;
    batch.id = row.get(0).convert<long long>();
    batch.batchNumber = row.get(1).convert<std::string>();
    batch.productId = row.get(2).convert<long long>();
    batch.supplierId = row.get(3).convert<long long>();
    batch.quantityReceived = row.get(4).convert<int>();
    batch.quantityAvailable = row.get(5).convert<int>();
    batch.unitCost = row.get(6).convert<double>();
    
    if (!row.get(7).isEmpty())
    {
        batch.manufactureDate = row.get(7).convert<std::string>();
    }
    
    if (!row.get(8).isEmpty())
    {
        batch.expirationDate = row.get(8).convert<std::string>();
    }
    
    batch.arrivalDate = row.get(9).convert<std::string>();
    batch.storageCellId = row.get(10).convert<long long>();
    
    std::string qualityStatusStr = row.get(11).convert<std::string>();
    batch.qualityStatus = models::ProductBatch::stringToQualityStatus(qualityStatusStr);
    
    if (!row.get(12).isEmpty())
    {
        batch.invoiceNumber = row.get(12).convert<std::string>();
    }
    
    if (!row.get(13).isEmpty())
    {
        batch.productName = row.get(13).convert<std::string>();
    }
    
    if (!row.get(14).isEmpty())
    {
        batch.productSku = row.get(14).convert<std::string>();
    }
    
    if (!row.get(15).isEmpty())
    {
        batch.supplierName = row.get(15).convert<std::string>();
    }
    
    if (!row.get(16).isEmpty())
    {
        batch.storageCellCode = row.get(16).convert<std::string>();
    }
    
    return batch;
}

void ProductBatchRepository::enrichProductBatchWithDetails(models::ProductBatch& batch)
{
    auto connection = acquireConnection();
    
    try
    {
        if (batch.productId > 0 && batch.productName.empty())
        {
            Poco::Int64 productIdCopy = batch.productId;
            Statement select(connection->getSession());
            select << "SELECT name, sku FROM products WHERE id = $1",
                use(productIdCopy),
                now;
            
            RecordSet rs(select);
            if (rs.rowCount() > 0)
            {
                batch.productName = rs.value("name").convert<std::string>();
                batch.productSku = rs.value("sku").convert<std::string>();
            }
        }
        
        if (batch.supplierId > 0 && batch.supplierName.empty())
        {
            Poco::Int64 supplierIdCopy = batch.supplierId;
            Statement select(connection->getSession());
            select << "SELECT name FROM suppliers WHERE id = $1",
                use(supplierIdCopy),
                now;
            
            RecordSet rs(select);
            if (rs.rowCount() > 0)
            {
                batch.supplierName = rs.value("name").convert<std::string>();
            }
        }
        
        if (batch.storageCellId > 0 && batch.storageCellCode.empty())
        {
            Poco::Int64 storageCellIdCopy = batch.storageCellId;
            Statement select(connection->getSession());
            select << "SELECT cell_code FROM warehouse_cells WHERE id = $1",
                use(storageCellIdCopy),
                now;
            
            RecordSet rs(select);
            if (rs.rowCount() > 0)
            {
                batch.storageCellCode = rs.value("cell_code").convert<std::string>();
            }
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in enrichProductBatchWithDetails: " + e.displayText());
    }
}

} // namespace database::repositories
