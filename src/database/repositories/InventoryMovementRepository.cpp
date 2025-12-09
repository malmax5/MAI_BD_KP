#include "InventoryMovementRepository.hpp"
#include "../models/InventoryMovement.hpp"
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

const std::string InventoryMovementRepository::TABLE_NAME = "inventory_movements";
const std::vector<std::string> InventoryMovementRepository::SEARCH_FIELDS = {
    "reason", "reference_type"
};

InventoryMovementRepository::InventoryMovementRepository() : BaseRepository<models::InventoryMovement>()
{
}

std::unique_ptr<models::InventoryMovement> InventoryMovementRepository::findById(long long id)
{
    auto connection = acquireConnection();
    
    try
    {
        Poco::Int64 pocoId = static_cast<Poco::Int64>(id);
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT im.id, im.movement_type, im.product_id, im.batch_id, "
                  "im.from_cell_id, im.to_cell_id, im.quantity, im.reference_id, "
                  "im.reference_type, im.movement_date, im.performed_by, "
                  "im.reason, im.status, "
                  "p.name as product_name, p.sku as product_sku, "
                  "pb.batch_number, "
                  "wc_from.cell_code as from_cell_code, "
                  "wc_to.cell_code as to_cell_code, "
                  "u.full_name as performed_by_name "
                  "FROM " << TABLE_NAME << " im "
                  "JOIN products p ON p.id = im.product_id "
                  "JOIN product_batches pb ON pb.id = im.batch_id "
                  "LEFT JOIN warehouse_cells wc_from ON wc_from.id = im.from_cell_id "
                  "LEFT JOIN warehouse_cells wc_to ON wc_to.id = im.to_cell_id "
                  "LEFT JOIN users u ON u.id = im.performed_by "
                  "WHERE im.id = $1",
            use(pocoId),
            now;
        
        Poco::Data::RecordSet rs(select);
        
        if (rs.rowCount() > 0)
        {
            auto movement = std::make_unique<models::InventoryMovement>();
            movement->id = rs.value("id").isEmpty() ? 0 : rs.value("id").convert<long long>();
            
            std::string movementTypeStr = rs.value("movement_type").isEmpty() ? "" : rs.value("movement_type").convert<std::string>();
            movement->movementType = models::InventoryMovement::stringToMovementType(movementTypeStr);
            
            movement->productId = rs.value("product_id").isEmpty() ? 0 : rs.value("product_id").convert<long long>();
            movement->batchId = rs.value("batch_id").isEmpty() ? 0 : rs.value("batch_id").convert<long long>();
            
            if (rs.value("from_cell_id").isEmpty()) {
                movement->fromCellId = Poco::Nullable<Poco::Int64>();
            } else {
                movement->fromCellId = rs.value("from_cell_id").convert<long long>();
            }
            
            if (rs.value("to_cell_id").isEmpty()) {
                movement->toCellId = Poco::Nullable<Poco::Int64>();
            } else {
                movement->toCellId = rs.value("to_cell_id").convert<long long>();
            }
            
            movement->quantity = rs.value("quantity").isEmpty() ? 0 : rs.value("quantity").convert<int>();
            
            if (rs.value("reference_id").isEmpty()) {
                movement->referenceId = Poco::Nullable<Poco::Int64>();
            } else {
                movement->referenceId = rs.value("reference_id").convert<long long>();
            }
            
            if (rs.value("reference_type").isEmpty()) {
                movement->referenceType = Poco::Nullable<std::string>();
            } else {
                movement->referenceType = rs.value("reference_type").convert<std::string>();
            }
            
            movement->movementDate = rs.value("movement_date").isEmpty() ? "" : rs.value("movement_date").convert<std::string>();
            movement->performedBy = rs.value("performed_by").isEmpty() ? 0 : rs.value("performed_by").convert<long long>();
            
            if (rs.value("reason").isEmpty()) {
                movement->reason = Poco::Nullable<std::string>();
            } else {
                movement->reason = rs.value("reason").convert<std::string>();
            }
            
            std::string statusStr = rs.value("status").isEmpty() ? "" : rs.value("status").convert<std::string>();
            movement->status = models::InventoryMovement::stringToMovementStatus(statusStr);
            
            movement->productName = rs.value("product_name").isEmpty() ? "" : rs.value("product_name").convert<std::string>();
            movement->productSku = rs.value("product_sku").isEmpty() ? "" : rs.value("product_sku").convert<std::string>();
            movement->batchNumber = rs.value("batch_number").isEmpty() ? "" : rs.value("batch_number").convert<std::string>();
            
            if (rs.value("from_cell_code").isEmpty()) {
                movement->fromCellCode = Poco::Nullable<std::string>();
            } else {
                movement->fromCellCode = rs.value("from_cell_code").convert<std::string>();
            }
            
            if (rs.value("to_cell_code").isEmpty()) {
                movement->toCellCode = Poco::Nullable<std::string>();
            } else {
                movement->toCellCode = rs.value("to_cell_code").convert<std::string>();
            }
            
            movement->performedByName = rs.value("performed_by_name").isEmpty() ? "" : rs.value("performed_by_name").convert<std::string>();
            
            return movement;
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findById: " + e.displayText());
    }
    
    return nullptr;
}

std::vector<std::unique_ptr<models::InventoryMovement>> InventoryMovementRepository::findAll()
{
    std::vector<std::unique_ptr<models::InventoryMovement>> movements;
    auto connection = acquireConnection();
    
    try
    {
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT im.id, im.movement_type, im.product_id, im.batch_id, "
                  "im.from_cell_id, im.to_cell_id, im.quantity, im.reference_id, "
                  "im.reference_type, im.movement_date, im.performed_by, "
                  "im.reason, im.status, "
                  "p.name as product_name, p.sku as product_sku, "
                  "pb.batch_number, "
                  "wc_from.cell_code as from_cell_code, "
                  "wc_to.cell_code as to_cell_code, "
                  "u.full_name as performed_by_name "
                  "FROM " << TABLE_NAME << " im "
                  "JOIN products p ON p.id = im.product_id "
                  "JOIN product_batches pb ON pb.id = im.batch_id "
                  "LEFT JOIN warehouse_cells wc_from ON wc_from.id = im.from_cell_id "
                  "LEFT JOIN warehouse_cells wc_to ON wc_to.id = im.to_cell_id "
                  "LEFT JOIN users u ON u.id = im.performed_by "
                  "ORDER BY im.movement_date DESC, im.id",
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            auto movement = std::make_unique<models::InventoryMovement>();
            movement->id = rs.value("id").isEmpty() ? 0 : rs.value("id").convert<long long>();
            
            std::string movementTypeStr = rs.value("movement_type").isEmpty() ? "" : rs.value("movement_type").convert<std::string>();
            movement->movementType = models::InventoryMovement::stringToMovementType(movementTypeStr);
            
            movement->productId = rs.value("product_id").isEmpty() ? 0 : rs.value("product_id").convert<long long>();
            movement->batchId = rs.value("batch_id").isEmpty() ? 0 : rs.value("batch_id").convert<long long>();
            
            if (rs.value("from_cell_id").isEmpty()) {
                movement->fromCellId = Poco::Nullable<Poco::Int64>();
            } else {
                movement->fromCellId = rs.value("from_cell_id").convert<long long>();
            }
            
            if (rs.value("to_cell_id").isEmpty()) {
                movement->toCellId = Poco::Nullable<Poco::Int64>();
            } else {
                movement->toCellId = rs.value("to_cell_id").convert<long long>();
            }
            
            movement->quantity = rs.value("quantity").isEmpty() ? 0 : rs.value("quantity").convert<int>();
            
            if (rs.value("reference_id").isEmpty()) {
                movement->referenceId = Poco::Nullable<Poco::Int64>();
            } else {
                movement->referenceId = rs.value("reference_id").convert<long long>();
            }
            
            if (rs.value("reference_type").isEmpty()) {
                movement->referenceType = Poco::Nullable<std::string>();
            } else {
                movement->referenceType = rs.value("reference_type").convert<std::string>();
            }
            
            movement->movementDate = rs.value("movement_date").isEmpty() ? "" : rs.value("movement_date").convert<std::string>();
            movement->performedBy = rs.value("performed_by").isEmpty() ? 0 : rs.value("performed_by").convert<long long>();
            
            if (rs.value("reason").isEmpty()) {
                movement->reason = Poco::Nullable<std::string>();
            } else {
                movement->reason = rs.value("reason").convert<std::string>();
            }
            
            std::string statusStr = rs.value("status").isEmpty() ? "" : rs.value("status").convert<std::string>();
            movement->status = models::InventoryMovement::stringToMovementStatus(statusStr);
            
            movement->productName = rs.value("product_name").isEmpty() ? "" : rs.value("product_name").convert<std::string>();
            movement->productSku = rs.value("product_sku").isEmpty() ? "" : rs.value("product_sku").convert<std::string>();
            movement->batchNumber = rs.value("batch_number").isEmpty() ? "" : rs.value("batch_number").convert<std::string>();
            
            if (rs.value("from_cell_code").isEmpty()) {
                movement->fromCellCode = Poco::Nullable<std::string>();
            } else {
                movement->fromCellCode = rs.value("from_cell_code").convert<std::string>();
            }
            
            if (rs.value("to_cell_code").isEmpty()) {
                movement->toCellCode = Poco::Nullable<std::string>();
            } else {
                movement->toCellCode = rs.value("to_cell_code").convert<std::string>();
            }
            
            movement->performedByName = rs.value("performed_by_name").isEmpty() ? "" : rs.value("performed_by_name").convert<std::string>();
            
            rs.moveNext();
            movements.push_back(std::move(movement));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findAll: " + e.displayText());
    }
    
    return movements;
}

std::vector<std::unique_ptr<models::InventoryMovement>> InventoryMovementRepository::findPaginated(int page, int pageSize)
{
    std::vector<std::unique_ptr<models::InventoryMovement>> movements;
    auto connection = acquireConnection();
    
    if (page < 1) page = 1;
    int offset = (page - 1) * pageSize;
    
    try
    {
        int usePageSize = pageSize;
        int useOffset = offset;
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT im.id, im.movement_type, im.product_id, im.batch_id, "
                  "im.from_cell_id, im.to_cell_id, im.quantity, im.reference_id, "
                  "im.reference_type, im.movement_date, im.performed_by, "
                  "im.reason, im.status, "
                  "p.name as product_name, p.sku as product_sku, "
                  "pb.batch_number, "
                  "wc_from.cell_code as from_cell_code, "
                  "wc_to.cell_code as to_cell_code, "
                  "u.full_name as performed_by_name "
                  "FROM " << TABLE_NAME << " im "
                  "JOIN products p ON p.id = im.product_id "
                  "JOIN product_batches pb ON pb.id = im.batch_id "
                  "LEFT JOIN warehouse_cells wc_from ON wc_from.id = im.from_cell_id "
                  "LEFT JOIN warehouse_cells wc_to ON wc_to.id = im.to_cell_id "
                  "LEFT JOIN users u ON u.id = im.performed_by "
                  "ORDER BY im.movement_date DESC, im.id LIMIT $1 OFFSET $2",
            use(usePageSize),
            use(useOffset),
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            auto movement = std::make_unique<models::InventoryMovement>();
            movement->id = rs.value("id").isEmpty() ? 0 : rs.value("id").convert<long long>();
            
            std::string movementTypeStr = rs.value("movement_type").isEmpty() ? "" : rs.value("movement_type").convert<std::string>();
            movement->movementType = models::InventoryMovement::stringToMovementType(movementTypeStr);
            
            movement->productId = rs.value("product_id").isEmpty() ? 0 : rs.value("product_id").convert<long long>();
            movement->batchId = rs.value("batch_id").isEmpty() ? 0 : rs.value("batch_id").convert<long long>();
            
            if (rs.value("from_cell_id").isEmpty()) {
                movement->fromCellId = Poco::Nullable<Poco::Int64>();
            } else {
                movement->fromCellId = rs.value("from_cell_id").convert<long long>();
            }
            
            if (rs.value("to_cell_id").isEmpty()) {
                movement->toCellId = Poco::Nullable<Poco::Int64>();
            } else {
                movement->toCellId = rs.value("to_cell_id").convert<long long>();
            }
            
            movement->quantity = rs.value("quantity").isEmpty() ? 0 : rs.value("quantity").convert<int>();
            
            if (rs.value("reference_id").isEmpty()) {
                movement->referenceId = Poco::Nullable<Poco::Int64>();
            } else {
                movement->referenceId = rs.value("reference_id").convert<long long>();
            }
            
            if (rs.value("reference_type").isEmpty()) {
                movement->referenceType = Poco::Nullable<std::string>();
            } else {
                movement->referenceType = rs.value("reference_type").convert<std::string>();
            }
            
            movement->movementDate = rs.value("movement_date").isEmpty() ? "" : rs.value("movement_date").convert<std::string>();
            movement->performedBy = rs.value("performed_by").isEmpty() ? 0 : rs.value("performed_by").convert<long long>();
            
            if (rs.value("reason").isEmpty()) {
                movement->reason = Poco::Nullable<std::string>();
            } else {
                movement->reason = rs.value("reason").convert<std::string>();
            }
            
            std::string statusStr = rs.value("status").isEmpty() ? "" : rs.value("status").convert<std::string>();
            movement->status = models::InventoryMovement::stringToMovementStatus(statusStr);
            
            movement->productName = rs.value("product_name").isEmpty() ? "" : rs.value("product_name").convert<std::string>();
            movement->productSku = rs.value("product_sku").isEmpty() ? "" : rs.value("product_sku").convert<std::string>();
            movement->batchNumber = rs.value("batch_number").isEmpty() ? "" : rs.value("batch_number").convert<std::string>();
            
            if (rs.value("from_cell_code").isEmpty()) {
                movement->fromCellCode = Poco::Nullable<std::string>();
            } else {
                movement->fromCellCode = rs.value("from_cell_code").convert<std::string>();
            }
            
            if (rs.value("to_cell_code").isEmpty()) {
                movement->toCellCode = Poco::Nullable<std::string>();
            } else {
                movement->toCellCode = rs.value("to_cell_code").convert<std::string>();
            }
            
            movement->performedByName = rs.value("performed_by_name").isEmpty() ? "" : rs.value("performed_by_name").convert<std::string>();
            
            rs.moveNext();
            movements.push_back(std::move(movement));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findPaginated: " + e.displayText());
    }
    
    return movements;
}

long long InventoryMovementRepository::create(const models::InventoryMovement& movement)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        std::string movementTypeStr = models::InventoryMovement::movementTypeToString(movement.movementType);
        std::string statusStr = models::InventoryMovement::movementStatusToString(movement.status);
        
        models::InventoryMovement movementCopy = movement;
        
        Poco::Data::Statement insert(connection->getSession());
        Poco::Int64 newId = 0;
        
        insert << "INSERT INTO " << TABLE_NAME << " "
                  "(movement_type, product_id, batch_id, from_cell_id, "
                  "to_cell_id, quantity, reference_id, reference_type, "
                  "movement_date, performed_by, reason, status) "
                  "VALUES ($1::movement_type, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12::movement_status) "
                  "RETURNING id",
            use(movementTypeStr),
            use(movementCopy.productId),
            use(movementCopy.batchId),
            use(movementCopy.fromCellId),
            use(movementCopy.toCellId),
            use(movementCopy.quantity),
            use(movementCopy.referenceId),
            use(movementCopy.referenceType),
            use(movementCopy.movementDate),
            use(movementCopy.performedBy),
            use(movementCopy.reason),
            use(statusStr),
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

bool InventoryMovementRepository::update(long long id, const models::InventoryMovement& movement)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        std::string movementTypeStr = models::InventoryMovement::movementTypeToString(movement.movementType);
        std::string statusStr = models::InventoryMovement::movementStatusToString(movement.status);
        
        models::InventoryMovement movementCopy = movement;

        Poco::Int64 idCopy = id;
        
        Poco::Data::Statement update(connection->getSession());
        update << "UPDATE " << TABLE_NAME << " SET "
                  "movement_type = $1, product_id = $2, batch_id = $3, "
                  "from_cell_id = $4, to_cell_id = $5, quantity = $6, "
                  "reference_id = $7, reference_type = $8, movement_date = $9, "
                  "performed_by = $10, reason = $11, status = $12 "
                  "WHERE id = $13",
            use(movementTypeStr),
            use(movementCopy.productId),
            use(movementCopy.batchId),
            use(movementCopy.fromCellId),
            use(movementCopy.toCellId),
            use(movementCopy.quantity),
            use(movementCopy.referenceId),
            use(movementCopy.referenceType),
            use(movementCopy.movementDate),
            use(movementCopy.performedBy),
            use(movementCopy.reason),
            use(statusStr),
            use(idCopy);
        
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

bool InventoryMovementRepository::remove(long long id)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        long long idCopy = id;
        
        Poco::Data::Statement del(connection->getSession());
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

bool InventoryMovementRepository::softDelete(long long id)
{
    return remove(id);
}

int InventoryMovementRepository::count()
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

Poco::JSON::Array InventoryMovementRepository::findAllAsJson()
{
    auto movements = findAll();
    Poco::JSON::Array jsonArray;
    
    for (const auto& movement : movements)
    {
        if (movement)
        {
            jsonArray.add(movement->toJson());
        }
    }
    
    return jsonArray;
}

Poco::JSON::Object InventoryMovementRepository::findByIdAsJson(long long id)
{
    auto movement = findById(id);
    if (movement)
    {
        return movement->toJson();
    }
    
    return Poco::JSON::Object();
}

std::vector<std::unique_ptr<models::InventoryMovement>> InventoryMovementRepository::findByField(
    const std::string& fieldName, const std::string& fieldValue)
{
    std::vector<std::unique_ptr<models::InventoryMovement>> movements;
    auto connection = acquireConnection();
    
    try
    {
        std::string sql = "SELECT im.id, im.movement_type, im.product_id, im.batch_id, "
                          "im.from_cell_id, im.to_cell_id, im.quantity, im.reference_id, "
                          "im.reference_type, im.movement_date, im.performed_by, "
                          "im.reason, im.status, "
                          "p.name as product_name, p.sku as product_sku, "
                          "pb.batch_number, "
                          "wc_from.cell_code as from_cell_code, "
                          "wc_to.cell_code as to_cell_code, "
                          "u.full_name as performed_by_name "
                          "FROM " + TABLE_NAME + " im "
                          "JOIN products p ON p.id = im.product_id "
                          "JOIN product_batches pb ON pb.id = im.batch_id "
                          "LEFT JOIN warehouse_cells wc_from ON wc_from.id = im.from_cell_id "
                          "LEFT JOIN warehouse_cells wc_to ON wc_to.id = im.to_cell_id "
                          "LEFT JOIN users u ON u.id = im.performed_by "
                          "WHERE im." + fieldName + " = $1 "
                          "ORDER BY im.movement_date DESC";
        
        std::string fieldValueCopy = fieldValue;
        Poco::Data::Statement select(connection->getSession());
        select << sql,
            use(fieldValueCopy),
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            auto movement = std::make_unique<models::InventoryMovement>();
            movement->id = rs.value("id").isEmpty() ? 0 : rs.value("id").convert<long long>();
            
            std::string movementTypeStr = rs.value("movement_type").isEmpty() ? "" : rs.value("movement_type").convert<std::string>();
            movement->movementType = models::InventoryMovement::stringToMovementType(movementTypeStr);
            
            movement->productId = rs.value("product_id").isEmpty() ? 0 : rs.value("product_id").convert<long long>();
            movement->batchId = rs.value("batch_id").isEmpty() ? 0 : rs.value("batch_id").convert<long long>();
            
            if (rs.value("from_cell_id").isEmpty()) {
                movement->fromCellId = Poco::Nullable<Poco::Int64>();
            } else {
                movement->fromCellId = rs.value("from_cell_id").convert<long long>();
            }
            
            if (rs.value("to_cell_id").isEmpty()) {
                movement->toCellId = Poco::Nullable<Poco::Int64>();
            } else {
                movement->toCellId = rs.value("to_cell_id").convert<long long>();
            }
            
            movement->quantity = rs.value("quantity").isEmpty() ? 0 : rs.value("quantity").convert<int>();
            
            if (rs.value("reference_id").isEmpty()) {
                movement->referenceId = Poco::Nullable<Poco::Int64>();
            } else {
                movement->referenceId = rs.value("reference_id").convert<long long>();
            }
            
            if (rs.value("reference_type").isEmpty()) {
                movement->referenceType = Poco::Nullable<std::string>();
            } else {
                movement->referenceType = rs.value("reference_type").convert<std::string>();
            }
            
            movement->movementDate = rs.value("movement_date").isEmpty() ? "" : rs.value("movement_date").convert<std::string>();
            movement->performedBy = rs.value("performed_by").isEmpty() ? 0 : rs.value("performed_by").convert<long long>();
            
            if (rs.value("reason").isEmpty()) {
                movement->reason = Poco::Nullable<std::string>();
            } else {
                movement->reason = rs.value("reason").convert<std::string>();
            }
            
            std::string statusStr = rs.value("status").isEmpty() ? "" : rs.value("status").convert<std::string>();
            movement->status = models::InventoryMovement::stringToMovementStatus(statusStr);
            
            movement->productName = rs.value("product_name").isEmpty() ? "" : rs.value("product_name").convert<std::string>();
            movement->productSku = rs.value("product_sku").isEmpty() ? "" : rs.value("product_sku").convert<std::string>();
            movement->batchNumber = rs.value("batch_number").isEmpty() ? "" : rs.value("batch_number").convert<std::string>();
            
            if (rs.value("from_cell_code").isEmpty()) {
                movement->fromCellCode = Poco::Nullable<std::string>();
            } else {
                movement->fromCellCode = rs.value("from_cell_code").convert<std::string>();
            }
            
            if (rs.value("to_cell_code").isEmpty()) {
                movement->toCellCode = Poco::Nullable<std::string>();
            } else {
                movement->toCellCode = rs.value("to_cell_code").convert<std::string>();
            }
            
            movement->performedByName = rs.value("performed_by_name").isEmpty() ? "" : rs.value("performed_by_name").convert<std::string>();
            
            rs.moveNext();
            movements.push_back(std::move(movement));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findByField: " + e.displayText());
    }
    
    return movements;
}

std::vector<std::unique_ptr<models::InventoryMovement>> InventoryMovementRepository::search(
    const std::string& query, const std::vector<std::string>& fields)
{
    std::vector<std::unique_ptr<models::InventoryMovement>> movements;
    auto connection = acquireConnection();
    
    try
    {
        std::string searchQuery = buildSearchQuery(query, SEARCH_FIELDS);
        std::string sql = "SELECT im.id, im.movement_type, im.product_id, im.batch_id, "
                          "im.from_cell_id, im.to_cell_id, im.quantity, im.reference_id, "
                          "im.reference_type, im.movement_date, im.performed_by, "
                          "im.reason, im.status, "
                          "p.name as product_name, p.sku as product_sku, "
                          "pb.batch_number, "
                          "wc_from.cell_code as from_cell_code, "
                          "wc_to.cell_code as to_cell_code, "
                          "u.full_name as performed_by_name "
                          "FROM " + TABLE_NAME + " im "
                          "JOIN products p ON p.id = im.product_id "
                          "JOIN product_batches pb ON pb.id = im.batch_id "
                          "LEFT JOIN warehouse_cells wc_from ON wc_from.id = im.from_cell_id "
                          "LEFT JOIN warehouse_cells wc_to ON wc_to.id = im.to_cell_id "
                          "LEFT JOIN users u ON u.id = im.performed_by "
                          "WHERE " + searchQuery + " "
                          "ORDER BY im.movement_date DESC";
        
        Poco::Data::Statement select(connection->getSession());
        select << sql,
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            auto movement = std::make_unique<models::InventoryMovement>();
            movement->id = rs.value("id").isEmpty() ? 0 : rs.value("id").convert<long long>();
            
            std::string movementTypeStr = rs.value("movement_type").isEmpty() ? "" : rs.value("movement_type").convert<std::string>();
            movement->movementType = models::InventoryMovement::stringToMovementType(movementTypeStr);
            
            movement->productId = rs.value("product_id").isEmpty() ? 0 : rs.value("product_id").convert<long long>();
            movement->batchId = rs.value("batch_id").isEmpty() ? 0 : rs.value("batch_id").convert<long long>();
            
            if (rs.value("from_cell_id").isEmpty()) {
                movement->fromCellId = Poco::Nullable<Poco::Int64>();
            } else {
                movement->fromCellId = rs.value("from_cell_id").convert<long long>();
            }
            
            if (rs.value("to_cell_id").isEmpty()) {
                movement->toCellId = Poco::Nullable<Poco::Int64>();
            } else {
                movement->toCellId = rs.value("to_cell_id").convert<long long>();
            }
            
            movement->quantity = rs.value("quantity").isEmpty() ? 0 : rs.value("quantity").convert<int>();
            
            if (rs.value("reference_id").isEmpty()) {
                movement->referenceId = Poco::Nullable<Poco::Int64>();
            } else {
                movement->referenceId = rs.value("reference_id").convert<long long>();
            }
            
            if (rs.value("reference_type").isEmpty()) {
                movement->referenceType = Poco::Nullable<std::string>();
            } else {
                movement->referenceType = rs.value("reference_type").convert<std::string>();
            }
            
            movement->movementDate = rs.value("movement_date").isEmpty() ? "" : rs.value("movement_date").convert<std::string>();
            movement->performedBy = rs.value("performed_by").isEmpty() ? 0 : rs.value("performed_by").convert<long long>();
            
            if (rs.value("reason").isEmpty()) {
                movement->reason = Poco::Nullable<std::string>();
            } else {
                movement->reason = rs.value("reason").convert<std::string>();
            }
            
            std::string statusStr = rs.value("status").isEmpty() ? "" : rs.value("status").convert<std::string>();
            movement->status = models::InventoryMovement::stringToMovementStatus(statusStr);
            
            movement->productName = rs.value("product_name").isEmpty() ? "" : rs.value("product_name").convert<std::string>();
            movement->productSku = rs.value("product_sku").isEmpty() ? "" : rs.value("product_sku").convert<std::string>();
            movement->batchNumber = rs.value("batch_number").isEmpty() ? "" : rs.value("batch_number").convert<std::string>();
            
            if (rs.value("from_cell_code").isEmpty()) {
                movement->fromCellCode = Poco::Nullable<std::string>();
            } else {
                movement->fromCellCode = rs.value("from_cell_code").convert<std::string>();
            }
            
            if (rs.value("to_cell_code").isEmpty()) {
                movement->toCellCode = Poco::Nullable<std::string>();
            } else {
                movement->toCellCode = rs.value("to_cell_code").convert<std::string>();
            }
            
            movement->performedByName = rs.value("performed_by_name").isEmpty() ? "" : rs.value("performed_by_name").convert<std::string>();
            
            rs.moveNext();
            movements.push_back(std::move(movement));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in search: " + e.displayText());
    }
    
    return movements;
}

std::vector<std::unique_ptr<models::InventoryMovement>> InventoryMovementRepository::findByProductId(long long productId)
{
    return findByField("product_id", std::to_string(productId));
}

std::vector<std::unique_ptr<models::InventoryMovement>> InventoryMovementRepository::findByBatchId(long long batchId)
{
    return findByField("batch_id", std::to_string(batchId));
}

std::vector<std::unique_ptr<models::InventoryMovement>> InventoryMovementRepository::findByMovementType(models::MovementType type)
{
    auto connection = acquireConnection();
    std::vector<std::unique_ptr<models::InventoryMovement>> movements;
    
    try
    {
        std::string typeStr = models::InventoryMovement::movementTypeToString(type);
        std::string typeStrCopy = typeStr;
        
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT im.id, im.movement_type, im.product_id, im.batch_id, "
                  "im.from_cell_id, im.to_cell_id, im.quantity, im.reference_id, "
                  "im.reference_type, im.movement_date, im.performed_by, "
                  "im.reason, im.status, "
                  "p.name as product_name, p.sku as product_sku, "
                  "pb.batch_number, "
                  "wc_from.cell_code as from_cell_code, "
                  "wc_to.cell_code as to_cell_code, "
                  "u.full_name as performed_by_name "
                  "FROM " << TABLE_NAME << " im "
                  "JOIN products p ON p.id = im.product_id "
                  "JOIN product_batches pb ON pb.id = im.batch_id "
                  "LEFT JOIN warehouse_cells wc_from ON wc_from.id = im.from_cell_id "
                  "LEFT JOIN warehouse_cells wc_to ON wc_to.id = im.to_cell_id "
                  "LEFT JOIN users u ON u.id = im.performed_by "
                  "WHERE im.movement_type = $1 "
                  "ORDER BY im.movement_date DESC",
            use(typeStrCopy),
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            auto movement = std::make_unique<models::InventoryMovement>();
            movement->id = rs.value("id").isEmpty() ? 0 : rs.value("id").convert<long long>();
            
            movement->movementType = type;
            movement->productId = rs.value("product_id").isEmpty() ? 0 : rs.value("product_id").convert<long long>();
            movement->batchId = rs.value("batch_id").isEmpty() ? 0 : rs.value("batch_id").convert<long long>();
            
            if (rs.value("from_cell_id").isEmpty()) {
                movement->fromCellId = Poco::Nullable<Poco::Int64>();
            } else {
                movement->fromCellId = rs.value("from_cell_id").convert<long long>();
            }
            
            if (rs.value("to_cell_id").isEmpty()) {
                movement->toCellId = Poco::Nullable<Poco::Int64>();
            } else {
                movement->toCellId = rs.value("to_cell_id").convert<long long>();
            }
            
            movement->quantity = rs.value("quantity").isEmpty() ? 0 : rs.value("quantity").convert<int>();
            
            if (rs.value("reference_id").isEmpty()) {
                movement->referenceId = Poco::Nullable<Poco::Int64>();
            } else {
                movement->referenceId = rs.value("reference_id").convert<long long>();
            }
            
            if (rs.value("reference_type").isEmpty()) {
                movement->referenceType = Poco::Nullable<std::string>();
            } else {
                movement->referenceType = rs.value("reference_type").convert<std::string>();
            }
            
            movement->movementDate = rs.value("movement_date").isEmpty() ? "" : rs.value("movement_date").convert<std::string>();
            movement->performedBy = rs.value("performed_by").isEmpty() ? 0 : rs.value("performed_by").convert<long long>();
            
            if (rs.value("reason").isEmpty()) {
                movement->reason = Poco::Nullable<std::string>();
            } else {
                movement->reason = rs.value("reason").convert<std::string>();
            }
            
            std::string statusStr = rs.value("status").isEmpty() ? "" : rs.value("status").convert<std::string>();
            movement->status = models::InventoryMovement::stringToMovementStatus(statusStr);
            
            movement->productName = rs.value("product_name").isEmpty() ? "" : rs.value("product_name").convert<std::string>();
            movement->productSku = rs.value("product_sku").isEmpty() ? "" : rs.value("product_sku").convert<std::string>();
            movement->batchNumber = rs.value("batch_number").isEmpty() ? "" : rs.value("batch_number").convert<std::string>();
            
            if (rs.value("from_cell_code").isEmpty()) {
                movement->fromCellCode = Poco::Nullable<std::string>();
            } else {
                movement->fromCellCode = rs.value("from_cell_code").convert<std::string>();
            }
            
            if (rs.value("to_cell_code").isEmpty()) {
                movement->toCellCode = Poco::Nullable<std::string>();
            } else {
                movement->toCellCode = rs.value("to_cell_code").convert<std::string>();
            }
            
            movement->performedByName = rs.value("performed_by_name").isEmpty() ? "" : rs.value("performed_by_name").convert<std::string>();
            
            rs.moveNext();
            movements.push_back(std::move(movement));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findByMovementType: " + e.displayText());
    }
    
    return movements;
}

std::vector<std::unique_ptr<models::InventoryMovement>> InventoryMovementRepository::findByStatus(models::MovementStatus status)
{
    auto connection = acquireConnection();
    std::vector<std::unique_ptr<models::InventoryMovement>> movements;
    
    try
    {
        std::string statusStr = models::InventoryMovement::movementStatusToString(status);
        std::string statusStrCopy = statusStr;
        
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT im.id, im.movement_type, im.product_id, im.batch_id, "
                  "im.from_cell_id, im.to_cell_id, im.quantity, im.reference_id, "
                  "im.reference_type, im.movement_date, im.performed_by, "
                  "im.reason, im.status, "
                  "p.name as product_name, p.sku as product_sku, "
                  "pb.batch_number, "
                  "wc_from.cell_code as from_cell_code, "
                  "wc_to.cell_code as to_cell_code, "
                  "u.full_name as performed_by_name "
                  "FROM " << TABLE_NAME << " im "
                  "JOIN products p ON p.id = im.product_id "
                  "JOIN product_batches pb ON pb.id = im.batch_id "
                  "LEFT JOIN warehouse_cells wc_from ON wc_from.id = im.from_cell_id "
                  "LEFT JOIN warehouse_cells wc_to ON wc_to.id = im.to_cell_id "
                  "LEFT JOIN users u ON u.id = im.performed_by "
                  "WHERE im.status = $1 "
                  "ORDER BY im.movement_date DESC",
            use(statusStrCopy),
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            auto movement = std::make_unique<models::InventoryMovement>();
            movement->id = rs.value("id").isEmpty() ? 0 : rs.value("id").convert<long long>();
            
            std::string movementTypeStr = rs.value("movement_type").isEmpty() ? "" : rs.value("movement_type").convert<std::string>();
            movement->movementType = models::InventoryMovement::stringToMovementType(movementTypeStr);
            
            movement->productId = rs.value("product_id").isEmpty() ? 0 : rs.value("product_id").convert<long long>();
            movement->batchId = rs.value("batch_id").isEmpty() ? 0 : rs.value("batch_id").convert<long long>();
            
            if (rs.value("from_cell_id").isEmpty()) {
                movement->fromCellId = Poco::Nullable<Poco::Int64>();
            } else {
                movement->fromCellId = rs.value("from_cell_id").convert<long long>();
            }
            
            if (rs.value("to_cell_id").isEmpty()) {
                movement->toCellId = Poco::Nullable<Poco::Int64>();
            } else {
                movement->toCellId = rs.value("to_cell_id").convert<long long>();
            }
            
            movement->quantity = rs.value("quantity").isEmpty() ? 0 : rs.value("quantity").convert<int>();
            
            if (rs.value("reference_id").isEmpty()) {
                movement->referenceId = Poco::Nullable<Poco::Int64>();
            } else {
                movement->referenceId = rs.value("reference_id").convert<long long>();
            }
            
            if (rs.value("reference_type").isEmpty()) {
                movement->referenceType = Poco::Nullable<std::string>();
            } else {
                movement->referenceType = rs.value("reference_type").convert<std::string>();
            }
            
            movement->movementDate = rs.value("movement_date").isEmpty() ? "" : rs.value("movement_date").convert<std::string>();
            movement->performedBy = rs.value("performed_by").isEmpty() ? 0 : rs.value("performed_by").convert<long long>();
            
            if (rs.value("reason").isEmpty()) {
                movement->reason = Poco::Nullable<std::string>();
            } else {
                movement->reason = rs.value("reason").convert<std::string>();
            }
            
            movement->status = status;
            movement->productName = rs.value("product_name").isEmpty() ? "" : rs.value("product_name").convert<std::string>();
            movement->productSku = rs.value("product_sku").isEmpty() ? "" : rs.value("product_sku").convert<std::string>();
            movement->batchNumber = rs.value("batch_number").isEmpty() ? "" : rs.value("batch_number").convert<std::string>();
            
            if (rs.value("from_cell_code").isEmpty()) {
                movement->fromCellCode = Poco::Nullable<std::string>();
            } else {
                movement->fromCellCode = rs.value("from_cell_code").convert<std::string>();
            }
            
            if (rs.value("to_cell_code").isEmpty()) {
                movement->toCellCode = Poco::Nullable<std::string>();
            } else {
                movement->toCellCode = rs.value("to_cell_code").convert<std::string>();
            }
            
            movement->performedByName = rs.value("performed_by_name").isEmpty() ? "" : rs.value("performed_by_name").convert<std::string>();
            
            rs.moveNext();
            movements.push_back(std::move(movement));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findByStatus: " + e.displayText());
    }
    
    return movements;
}

std::vector<std::unique_ptr<models::InventoryMovement>> InventoryMovementRepository::findByPerformedBy(long long userId)
{
    return findByField("performed_by", std::to_string(userId));
}

std::vector<std::unique_ptr<models::InventoryMovement>> InventoryMovementRepository::findByFromCell(long long cellId)
{
    return findByField("from_cell_id", std::to_string(cellId));
}

std::vector<std::unique_ptr<models::InventoryMovement>> InventoryMovementRepository::findByToCell(long long cellId)
{
    return findByField("to_cell_id", std::to_string(cellId));
}

std::vector<std::unique_ptr<models::InventoryMovement>> InventoryMovementRepository::findByDateRange(const std::string& startDate, const std::string& endDate)
{
    auto connection = acquireConnection();
    std::vector<std::unique_ptr<models::InventoryMovement>> movements;
    
    try
    {
        std::string startDateCopy = startDate;
        std::string endDateCopy = endDate;
        
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT im.id, im.movement_type, im.product_id, im.batch_id, "
                  "im.from_cell_id, im.to_cell_id, im.quantity, im.reference_id, "
                  "im.reference_type, im.movement_date, im.performed_by, "
                  "im.reason, im.status, "
                  "p.name as product_name, p.sku as product_sku, "
                  "pb.batch_number, "
                  "wc_from.cell_code as from_cell_code, "
                  "wc_to.cell_code as to_cell_code, "
                  "u.full_name as performed_by_name "
                  "FROM " << TABLE_NAME << " im "
                  "JOIN products p ON p.id = im.product_id "
                  "JOIN product_batches pb ON pb.id = im.batch_id "
                  "LEFT JOIN warehouse_cells wc_from ON wc_from.id = im.from_cell_id "
                  "LEFT JOIN warehouse_cells wc_to ON wc_to.id = im.to_cell_id "
                  "LEFT JOIN users u ON u.id = im.performed_by "
                  "WHERE im.movement_date >= $1 AND im.movement_date <= $2 "
                  "ORDER BY im.movement_date DESC",
            use(startDateCopy),
            use(endDateCopy),
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            auto movement = std::make_unique<models::InventoryMovement>();
            movement->id = rs.value("id").isEmpty() ? 0 : rs.value("id").convert<long long>();
            
            std::string movementTypeStr = rs.value("movement_type").isEmpty() ? "" : rs.value("movement_type").convert<std::string>();
            movement->movementType = models::InventoryMovement::stringToMovementType(movementTypeStr);
            
            movement->productId = rs.value("product_id").isEmpty() ? 0 : rs.value("product_id").convert<long long>();
            movement->batchId = rs.value("batch_id").isEmpty() ? 0 : rs.value("batch_id").convert<long long>();
            
            if (rs.value("from_cell_id").isEmpty()) {
                movement->fromCellId = Poco::Nullable<Poco::Int64>();
            } else {
                movement->fromCellId = rs.value("from_cell_id").convert<long long>();
            }
            
            if (rs.value("to_cell_id").isEmpty()) {
                movement->toCellId = Poco::Nullable<Poco::Int64>();
            } else {
                movement->toCellId = rs.value("to_cell_id").convert<long long>();
            }
            
            movement->quantity = rs.value("quantity").isEmpty() ? 0 : rs.value("quantity").convert<int>();
            
            if (rs.value("reference_id").isEmpty()) {
                movement->referenceId = Poco::Nullable<Poco::Int64>();
            } else {
                movement->referenceId = rs.value("reference_id").convert<long long>();
            }
            
            if (rs.value("reference_type").isEmpty()) {
                movement->referenceType = Poco::Nullable<std::string>();
            } else {
                movement->referenceType = rs.value("reference_type").convert<std::string>();
            }
            
            movement->movementDate = rs.value("movement_date").isEmpty() ? "" : rs.value("movement_date").convert<std::string>();
            movement->performedBy = rs.value("performed_by").isEmpty() ? 0 : rs.value("performed_by").convert<long long>();
            
            if (rs.value("reason").isEmpty()) {
                movement->reason = Poco::Nullable<std::string>();
            } else {
                movement->reason = rs.value("reason").convert<std::string>();
            }
            
            std::string statusStr = rs.value("status").isEmpty() ? "" : rs.value("status").convert<std::string>();
            movement->status = models::InventoryMovement::stringToMovementStatus(statusStr);
            
            movement->productName = rs.value("product_name").isEmpty() ? "" : rs.value("product_name").convert<std::string>();
            movement->productSku = rs.value("product_sku").isEmpty() ? "" : rs.value("product_sku").convert<std::string>();
            movement->batchNumber = rs.value("batch_number").isEmpty() ? "" : rs.value("batch_number").convert<std::string>();
            
            if (rs.value("from_cell_code").isEmpty()) {
                movement->fromCellCode = Poco::Nullable<std::string>();
            } else {
                movement->fromCellCode = rs.value("from_cell_code").convert<std::string>();
            }
            
            if (rs.value("to_cell_code").isEmpty()) {
                movement->toCellCode = Poco::Nullable<std::string>();
            } else {
                movement->toCellCode = rs.value("to_cell_code").convert<std::string>();
            }
            
            movement->performedByName = rs.value("performed_by_name").isEmpty() ? "" : rs.value("performed_by_name").convert<std::string>();
            
            rs.moveNext();
            movements.push_back(std::move(movement));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findByDateRange: " + e.displayText());
    }
    
    return movements;
}

std::vector<std::unique_ptr<models::InventoryMovement>> InventoryMovementRepository::findByReference(long long referenceId, const std::string& referenceType)
{
    auto connection = acquireConnection();
    std::vector<std::unique_ptr<models::InventoryMovement>> movements;
    
    try
    {
        long long referenceIdCopy = referenceId;
        std::string referenceTypeCopy = referenceType;
        
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT im.id, im.movement_type, im.product_id, im.batch_id, "
                  "im.from_cell_id, im.to_cell_id, im.quantity, im.reference_id, "
                  "im.reference_type, im.movement_date, im.performed_by, "
                  "im.reason, im.status, "
                  "p.name as product_name, p.sku as product_sku, "
                  "pb.batch_number, "
                  "wc_from.cell_code as from_cell_code, "
                  "wc_to.cell_code as to_cell_code, "
                  "u.full_name as performed_by_name "
                  "FROM " << TABLE_NAME << " im "
                  "JOIN products p ON p.id = im.product_id "
                  "JOIN product_batches pb ON pb.id = im.batch_id "
                  "LEFT JOIN warehouse_cells wc_from ON wc_from.id = im.from_cell_id "
                  "LEFT JOIN warehouse_cells wc_to ON wc_to.id = im.to_cell_id "
                  "LEFT JOIN users u ON u.id = im.performed_by "
                  "WHERE im.reference_id = $1 AND im.reference_type = $2 "
                  "ORDER BY im.movement_date DESC",
            use(referenceIdCopy),
            use(referenceTypeCopy),
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            auto movement = std::make_unique<models::InventoryMovement>();
            movement->id = rs.value("id").isEmpty() ? 0 : rs.value("id").convert<long long>();
            
            std::string movementTypeStr = rs.value("movement_type").isEmpty() ? "" : rs.value("movement_type").convert<std::string>();
            movement->movementType = models::InventoryMovement::stringToMovementType(movementTypeStr);
            
            movement->productId = rs.value("product_id").isEmpty() ? 0 : rs.value("product_id").convert<long long>();
            movement->batchId = rs.value("batch_id").isEmpty() ? 0 : rs.value("batch_id").convert<long long>();
            
            if (rs.value("from_cell_id").isEmpty()) {
                movement->fromCellId = Poco::Nullable<Poco::Int64>();
            } else {
                movement->fromCellId = rs.value("from_cell_id").convert<long long>();
            }
            
            if (rs.value("to_cell_id").isEmpty()) {
                movement->toCellId = Poco::Nullable<Poco::Int64>();
            } else {
                movement->toCellId = rs.value("to_cell_id").convert<long long>();
            }
            
            movement->quantity = rs.value("quantity").isEmpty() ? 0 : rs.value("quantity").convert<int>();
            
            if (rs.value("reference_id").isEmpty()) {
                movement->referenceId = Poco::Nullable<Poco::Int64>();
            } else {
                movement->referenceId = rs.value("reference_id").convert<long long>();
            }
            
            if (rs.value("reference_type").isEmpty()) {
                movement->referenceType = Poco::Nullable<std::string>();
            } else {
                movement->referenceType = rs.value("reference_type").convert<std::string>();
            }
            
            movement->movementDate = rs.value("movement_date").isEmpty() ? "" : rs.value("movement_date").convert<std::string>();
            movement->performedBy = rs.value("performed_by").isEmpty() ? 0 : rs.value("performed_by").convert<long long>();
            
            if (rs.value("reason").isEmpty()) {
                movement->reason = Poco::Nullable<std::string>();
            } else {
                movement->reason = rs.value("reason").convert<std::string>();
            }
            
            std::string statusStr = rs.value("status").isEmpty() ? "" : rs.value("status").convert<std::string>();
            movement->status = models::InventoryMovement::stringToMovementStatus(statusStr);
            
            movement->productName = rs.value("product_name").isEmpty() ? "" : rs.value("product_name").convert<std::string>();
            movement->productSku = rs.value("product_sku").isEmpty() ? "" : rs.value("product_sku").convert<std::string>();
            movement->batchNumber = rs.value("batch_number").isEmpty() ? "" : rs.value("batch_number").convert<std::string>();
            
            if (rs.value("from_cell_code").isEmpty()) {
                movement->fromCellCode = Poco::Nullable<std::string>();
            } else {
                movement->fromCellCode = rs.value("from_cell_code").convert<std::string>();
            }
            
            if (rs.value("to_cell_code").isEmpty()) {
                movement->toCellCode = Poco::Nullable<std::string>();
            } else {
                movement->toCellCode = rs.value("to_cell_code").convert<std::string>();
            }
            
            movement->performedByName = rs.value("performed_by_name").isEmpty() ? "" : rs.value("performed_by_name").convert<std::string>();
            
            rs.moveNext();
            movements.push_back(std::move(movement));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findByReference: " + e.displayText());
    }
    
    return movements;
}

std::vector<std::unique_ptr<models::InventoryMovement>> InventoryMovementRepository::findPlannedMovements()
{
    return findByStatus(models::MovementStatus::PLANNED);
}

std::vector<std::unique_ptr<models::InventoryMovement>> InventoryMovementRepository::findInProgressMovements()
{
    return findByStatus(models::MovementStatus::IN_PROGRESS);
}

std::vector<std::unique_ptr<models::InventoryMovement>> InventoryMovementRepository::findCompletedMovements()
{
    return findByStatus(models::MovementStatus::COMPLETED);
}

std::vector<std::unique_ptr<models::InventoryMovement>> InventoryMovementRepository::findReceiptMovements()
{
    return findByMovementType(models::MovementType::RECEIPT);
}

std::vector<std::unique_ptr<models::InventoryMovement>> InventoryMovementRepository::findShipmentMovements()
{
    return findByMovementType(models::MovementType::SHIPMENT);
}

std::vector<std::unique_ptr<models::InventoryMovement>> InventoryMovementRepository::findTransferMovements()
{
    return findByMovementType(models::MovementType::TRANSFER);
}

std::vector<std::unique_ptr<models::InventoryMovement>> InventoryMovementRepository::findAdjustmentMovements()
{
    return findByMovementType(models::MovementType::ADJUSTMENT);
}

bool InventoryMovementRepository::updateStatus(long long id, models::MovementStatus newStatus)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        std::string statusStr = models::InventoryMovement::movementStatusToString(newStatus);
        long long idCopy = id;
        std::string statusStrCopy = statusStr;
        
        Poco::Data::Statement update(connection->getSession());
        update << "UPDATE " << TABLE_NAME << " SET status = $1 WHERE id = $2",
            use(statusStrCopy),
            use(idCopy);
        
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

bool InventoryMovementRepository::updateQuantity(long long id, int newQuantity)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        long long idCopy = id;
        int quantityCopy = newQuantity;
        
        Poco::Data::Statement update(connection->getSession());
        update << "UPDATE " << TABLE_NAME << " SET quantity = $1 WHERE id = $2",
            use(quantityCopy),
            use(idCopy);
        
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

bool InventoryMovementRepository::updateCells(long long id, long long fromCellId, long long toCellId)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        long long idCopy = id;
        Poco::Nullable<Poco::Int64> fromCellIdCopy;
        if (fromCellId > 0)
            fromCellIdCopy = fromCellId;
        Poco::Nullable<Poco::Int64> toCellIdCopy;
        if (toCellId > 0)
            toCellIdCopy = toCellId;
        
        Poco::Data::Statement update(connection->getSession());
        update << "UPDATE " << TABLE_NAME << " SET "
                  "from_cell_id = $1, to_cell_id = $2 WHERE id = $3",
            use(fromCellIdCopy),
            use(toCellIdCopy),
            use(idCopy);
        
        int rowsAffected = update.execute();
        
        commitTransaction(*connection);
        return rowsAffected > 0;
    }
    catch (const Poco::Exception& e)
    {
        rollbackTransaction(*connection);
        throw std::runtime_error("Database error in updateCells: " + e.displayText());
    }
}

bool InventoryMovementRepository::updateReason(long long id, const std::string& reason)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        long long idCopy = id;
        std::string reasonCopy = reason;
        
        Poco::Data::Statement update(connection->getSession());
        update << "UPDATE " << TABLE_NAME << " SET reason = $1 WHERE id = $2",
            use(reasonCopy),
            use(idCopy);
        
        int rowsAffected = update.execute();
        
        commitTransaction(*connection);
        return rowsAffected > 0;
    }
    catch (const Poco::Exception& e)
    {
        rollbackTransaction(*connection);
        throw std::runtime_error("Database error in updateReason: " + e.displayText());
    }
}

bool InventoryMovementRepository::updatePerformedBy(long long id, long long userId)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        long long idCopy = id;
        long long userIdCopy = userId;
        
        Poco::Data::Statement update(connection->getSession());
        update << "UPDATE " << TABLE_NAME << " SET performed_by = $1 WHERE id = $2",
            use(userIdCopy),
            use(idCopy);
        
        int rowsAffected = update.execute();
        
        commitTransaction(*connection);
        return rowsAffected > 0;
    }
    catch (const Poco::Exception& e)
    {
        rollbackTransaction(*connection);
        throw std::runtime_error("Database error in updatePerformedBy: " + e.displayText());
    }
}

bool InventoryMovementRepository::markAsInProgress(long long id)
{
    return updateStatus(id, models::MovementStatus::IN_PROGRESS);
}

bool InventoryMovementRepository::markAsCompleted(long long id)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        std::string statusStr = models::InventoryMovement::movementStatusToString(models::MovementStatus::COMPLETED);
        std::string movementDate = DateUtils::formatDateTime(DateUtils::now());
        
        long long idCopy = id;
        std::string statusStrCopy = statusStr;
        std::string movementDateCopy = movementDate;
        
        Poco::Data::Statement update(connection->getSession());
        update << "UPDATE " << TABLE_NAME << " SET "
                  "status = $1, movement_date = $2 WHERE id = $3",
            use(statusStrCopy),
            use(movementDateCopy),
            use(idCopy);
        
        int rowsAffected = update.execute();
        
        commitTransaction(*connection);
        return rowsAffected > 0;
    }
    catch (const Poco::Exception& e)
    {
        rollbackTransaction(*connection);
        throw std::runtime_error("Database error in markAsCompleted: " + e.displayText());
    }
}

bool InventoryMovementRepository::markAsCancelled(long long id)
{
    return updateStatus(id, models::MovementStatus::CANCELLED);
}

int InventoryMovementRepository::countByMovementType(models::MovementType type)
{
    auto connection = acquireConnection();
    
    try
    {
        std::string typeStr = models::InventoryMovement::movementTypeToString(type);
        std::string typeStrCopy = typeStr;
        
        Poco::Data::Statement countStmt(connection->getSession());
        countStmt << "SELECT COUNT(*) FROM " << TABLE_NAME << " WHERE movement_type = $1",
            use(typeStrCopy),
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
        throw std::runtime_error("Database error in countByMovementType: " + e.displayText());
    }
}

int InventoryMovementRepository::countByStatus(models::MovementStatus status)
{
    auto connection = acquireConnection();
    
    try
    {
        std::string statusStr = models::InventoryMovement::movementStatusToString(status);
        std::string statusStrCopy = statusStr;
        
        Poco::Data::Statement countStmt(connection->getSession());
        countStmt << "SELECT COUNT(*) FROM " << TABLE_NAME << " WHERE status = $1",
            use(statusStrCopy),
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

int InventoryMovementRepository::countByProduct(long long productId)
{
    auto connection = acquireConnection();
    
    try
    {
        long long productIdCopy = productId;
        
        Poco::Data::Statement countStmt(connection->getSession());
        countStmt << "SELECT COUNT(*) FROM " << TABLE_NAME << " WHERE product_id = $1",
            use(productIdCopy),
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

int InventoryMovementRepository::countByUser(long long userId)
{
    auto connection = acquireConnection();
    
    try
    {
        long long userIdCopy = userId;
        
        Poco::Data::Statement countStmt(connection->getSession());
        countStmt << "SELECT COUNT(*) FROM " << TABLE_NAME << " WHERE performed_by = $1",
            use(userIdCopy),
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

int InventoryMovementRepository::getTotalQuantityMoved(models::MovementType type, const std::string& startDate, const std::string& endDate)
{
    auto connection = acquireConnection();
    
    try
    {
        std::string typeStr = models::InventoryMovement::movementTypeToString(type);
        std::string typeStrCopy = typeStr;
        std::string startDateCopy = startDate;
        std::string endDateCopy = endDate;
        
        Poco::Data::Statement sumStmt(connection->getSession());
        sumStmt << "SELECT COALESCE(SUM(quantity), 0) FROM " << TABLE_NAME 
                << " WHERE movement_type = $1 "
                << "AND movement_date >= $2 AND movement_date <= $3",
            use(typeStrCopy),
            use(startDateCopy),
            use(endDateCopy),
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
        throw std::runtime_error("Database error in getTotalQuantityMoved: " + e.displayText());
    }
}

double InventoryMovementRepository::getTotalValueMoved(models::MovementType type, const std::string& startDate, const std::string& endDate)
{
    auto connection = acquireConnection();
    
    try
    {
        std::string typeStr = models::InventoryMovement::movementTypeToString(type);
        std::string typeStrCopy = typeStr;
        std::string startDateCopy = startDate;
        std::string endDateCopy = endDate;
        
        Poco::Data::Statement sumStmt(connection->getSession());
        sumStmt << "SELECT COALESCE(SUM(im.quantity * pb.unit_cost), 0) "
                << "FROM " << TABLE_NAME << " im "
                << "JOIN product_batches pb ON pb.id = im.batch_id "
                << "WHERE im.movement_type = $1 "
                << "AND im.movement_date >= $2 AND im.movement_date <= $3",
            use(typeStrCopy),
            use(startDateCopy),
            use(endDateCopy),
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
        throw std::runtime_error("Database error in getTotalValueMoved: " + e.displayText());
    }
}

Poco::JSON::Array InventoryMovementRepository::getMovementStatistics()
{
    auto connection = acquireConnection();
    Poco::JSON::Array jsonArray;
    
    try
    {
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT movement_type, status, "
                  "COUNT(*) as count, COALESCE(SUM(quantity), 0) as total_quantity "
                  "FROM " << TABLE_NAME << " "
                  "GROUP BY movement_type, status "
                  "ORDER BY movement_type, status",
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            Poco::JSON::Object stats;
            stats.set("movement_type", rs.value("movement_type").isEmpty() ? "" : rs.value("movement_type").convert<std::string>());
            stats.set("status", rs.value("status").isEmpty() ? "" : rs.value("status").convert<std::string>());
            stats.set("count", rs.value("count").isEmpty() ? 0 : rs.value("count").convert<int>());
            stats.set("total_quantity", rs.value("total_quantity").isEmpty() ? 0 : rs.value("total_quantity").convert<int>());
            
            jsonArray.add(stats);
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in getMovementStatistics: " + e.displayText());
    }
    
    return jsonArray;
}

Poco::JSON::Array InventoryMovementRepository::getMovementReport(const std::string& startDate, const std::string& endDate)
{
    auto connection = acquireConnection();
    Poco::JSON::Array jsonArray;
    
    try
    {
        std::string startDateCopy = startDate;
        std::string endDateCopy = endDate;
        
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT im.movement_type, im.status, "
                  "p.sku, p.name as product_name, "
                  "pb.batch_number, "
                  "im.quantity, "
                  "im.movement_date, "
                  "u.full_name as performed_by_name, "
                  "im.reason, "
                  "wc_from.cell_code as from_cell_code, "
                  "wc_to.cell_code as to_cell_code "
                  "FROM " << TABLE_NAME << " im "
                  "JOIN products p ON p.id = im.product_id "
                  "JOIN product_batches pb ON pb.id = im.batch_id "
                  "LEFT JOIN warehouse_cells wc_from ON wc_from.id = im.from_cell_id "
                  "LEFT JOIN warehouse_cells wc_to ON wc_to.id = im.to_cell_id "
                  "LEFT JOIN users u ON u.id = im.performed_by "
                  "WHERE im.movement_date >= $1 AND im.movement_date <= $2 "
                  "ORDER BY im.movement_date DESC",
            use(startDateCopy),
            use(endDateCopy),
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            Poco::JSON::Object report;
            report.set("movement_type", rs.value("movement_type").isEmpty() ? "" : rs.value("movement_type").convert<std::string>());
            report.set("status", rs.value("status").isEmpty() ? "" : rs.value("status").convert<std::string>());
            report.set("sku", rs.value("sku").isEmpty() ? "" : rs.value("sku").convert<std::string>());
            report.set("product_name", rs.value("product_name").isEmpty() ? "" : rs.value("product_name").convert<std::string>());
            report.set("batch_number", rs.value("batch_number").isEmpty() ? "" : rs.value("batch_number").convert<std::string>());
            report.set("quantity", rs.value("quantity").isEmpty() ? 0 : rs.value("quantity").convert<int>());
            report.set("movement_date", rs.value("movement_date").isEmpty() ? "" : rs.value("movement_date").convert<std::string>());
            report.set("performed_by_name", rs.value("performed_by_name").isEmpty() ? "" : rs.value("performed_by_name").convert<std::string>());
            report.set("reason", rs.value("reason").isEmpty() ? "" : rs.value("reason").convert<std::string>());
            report.set("from_cell_code", rs.value("from_cell_code").isEmpty() ? "" : rs.value("from_cell_code").convert<std::string>());
            report.set("to_cell_code", rs.value("to_cell_code").isEmpty() ? "" : rs.value("to_cell_code").convert<std::string>());
            
            jsonArray.add(report);
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in getMovementReport: " + e.displayText());
    }
    
    return jsonArray;
}

Poco::JSON::Array InventoryMovementRepository::getProductMovementHistory(long long productId)
{
    auto movements = findByProductId(productId);
    Poco::JSON::Array jsonArray;
    
    for (const auto& movement : movements)
    {
        if (movement)
        {
            jsonArray.add(movement->toJson());
        }
    }
    
    return jsonArray;
}

Poco::JSON::Array InventoryMovementRepository::getCellMovementHistory(long long cellId)
{
    auto connection = acquireConnection();
    Poco::JSON::Array jsonArray;
    
    try
    {
        long long cellIdCopy = cellId;
        
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT im.id, im.movement_type, im.product_id, im.batch_id, "
                  "im.from_cell_id, im.to_cell_id, im.quantity, im.reference_id, "
                  "im.reference_type, im.movement_date, im.performed_by, "
                  "im.reason, im.status, "
                  "p.name as product_name, p.sku as product_sku, "
                  "pb.batch_number, "
                  "wc_from.cell_code as from_cell_code, "
                  "wc_to.cell_code as to_cell_code, "
                  "u.full_name as performed_by_name "
                  "FROM " << TABLE_NAME << " im "
                  "JOIN products p ON p.id = im.product_id "
                  "JOIN product_batches pb ON pb.id = im.batch_id "
                  "LEFT JOIN warehouse_cells wc_from ON wc_from.id = im.from_cell_id "
                  "LEFT JOIN warehouse_cells wc_to ON wc_to.id = im.to_cell_id "
                  "LEFT JOIN users u ON u.id = im.performed_by "
                  "WHERE im.from_cell_id = $1 OR im.to_cell_id = $1 "
                  "ORDER BY im.movement_date DESC",
            use(cellIdCopy),
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            Poco::JSON::Object movementJson;
            movementJson.set("id", rs.value("id").isEmpty() ? 0 : rs.value("id").convert<long long>());
            movementJson.set("movement_type", rs.value("movement_type").isEmpty() ? "" : rs.value("movement_type").convert<std::string>());
            movementJson.set("product_name", rs.value("product_name").isEmpty() ? "" : rs.value("product_name").convert<std::string>());
            movementJson.set("product_sku", rs.value("product_sku").isEmpty() ? "" : rs.value("product_sku").convert<std::string>());
            movementJson.set("batch_number", rs.value("batch_number").isEmpty() ? "" : rs.value("batch_number").convert<std::string>());
            movementJson.set("quantity", rs.value("quantity").isEmpty() ? 0 : rs.value("quantity").convert<int>());
            movementJson.set("from_cell_code", rs.value("from_cell_code").isEmpty() ? "" : rs.value("from_cell_code").convert<std::string>());
            movementJson.set("to_cell_code", rs.value("to_cell_code").isEmpty() ? "" : rs.value("to_cell_code").convert<std::string>());
            movementJson.set("movement_date", rs.value("movement_date").isEmpty() ? "" : rs.value("movement_date").convert<std::string>());
            movementJson.set("performed_by_name", rs.value("performed_by_name").isEmpty() ? "" : rs.value("performed_by_name").convert<std::string>());
            movementJson.set("reason", rs.value("reason").isEmpty() ? "" : rs.value("reason").convert<std::string>());
            movementJson.set("status", rs.value("status").isEmpty() ? "" : rs.value("status").convert<std::string>());
            
            jsonArray.add(movementJson);
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in getCellMovementHistory: " + e.displayText());
    }
    
    return jsonArray;
}

bool InventoryMovementRepository::createTransferMovement(long long productId, long long batchId, 
                                                        long long fromCellId, long long toCellId, 
                                                        int quantity, long long performedBy, 
                                                        const std::string& reason)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        models::InventoryMovement movement;
        movement.movementType = models::MovementType::TRANSFER;
        movement.productId = productId;
        movement.batchId = batchId;
        movement.fromCellId = fromCellId;
        movement.toCellId = toCellId;
        movement.quantity = quantity;
        movement.performedBy = performedBy;
        movement.reason = reason;
        movement.status = models::MovementStatus::PLANNED;
        
        long long movementId = create(movement);
        
        commitTransaction(*connection);
        return movementId > 0;
    }
    catch (const Poco::Exception& e)
    {
        rollbackTransaction(*connection);
        throw std::runtime_error("Database error in createTransferMovement: " + e.displayText());
    }
}

bool InventoryMovementRepository::createAdjustmentMovement(long long productId, long long batchId, 
                                                          long long cellId, int quantity, 
                                                          long long performedBy, const std::string& reason)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        models::InventoryMovement movement;
        movement.movementType = models::MovementType::ADJUSTMENT;
        movement.productId = productId;
        movement.batchId = batchId;
        
        if (quantity > 0)
        {
            movement.toCellId = cellId;
        }
        else
        {
            movement.fromCellId = cellId;
            movement.quantity = std::abs(quantity);
        }
        
        movement.quantity = std::abs(quantity);
        movement.performedBy = performedBy;
        movement.reason = reason;
        movement.status = models::MovementStatus::PLANNED;
        
        long long movementId = create(movement);
        
        commitTransaction(*connection);
        return movementId > 0;
    }
    catch (const Poco::Exception& e)
    {
        rollbackTransaction(*connection);
        throw std::runtime_error("Database error in createAdjustmentMovement: " + e.displayText());
    }
}

std::vector<std::pair<long long, std::string>> InventoryMovementRepository::getRecentMovements(int limit)
{
    auto connection = acquireConnection();
    std::vector<std::pair<long long, std::string>> result;
    
    try
    {
        int limitCopy = limit;
        
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT im.id, CONCAT(p.name, ' - ', im.quantity, ' units (', "
                  "im.movement_type, ')') as movement_description "
                  "FROM " << TABLE_NAME << " im "
                  "JOIN products p ON p.id = im.product_id "
                  "ORDER BY im.movement_date DESC "
                  "LIMIT $1",
            use(limitCopy),
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            result.emplace_back(
                rs.value("id").isEmpty() ? 0 : rs.value("id").convert<long long>(),
                rs.value("movement_description").isEmpty() ? "" : rs.value("movement_description").convert<std::string>()
            );
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in getRecentMovements: " + e.displayText());
    }
    
    return result;
}

models::InventoryMovement InventoryMovementRepository::mapRowToMovement(Poco::Data::Row& row) const
{
    models::InventoryMovement movement;
    movement.id = row.get(0).isEmpty() ? 0 : row.get(0).convert<long long>();
    
    std::string movementTypeStr = row.get(1).isEmpty() ? "" : row.get(1).convert<std::string>();
    movement.movementType = models::InventoryMovement::stringToMovementType(movementTypeStr);
    
    movement.productId = row.get(2).isEmpty() ? 0 : row.get(2).convert<long long>();
    movement.batchId = row.get(3).isEmpty() ? 0 : row.get(3).convert<long long>();
    
    if (row.get(4).isEmpty()) {
        movement.fromCellId = Poco::Nullable<Poco::Int64>();
    } else {
        movement.fromCellId = row.get(4).convert<long long>();
    }
    
    if (row.get(5).isEmpty()) {
        movement.toCellId = Poco::Nullable<Poco::Int64>();
    } else {
        movement.toCellId = row.get(5).convert<long long>();
    }
    
    movement.quantity = row.get(6).isEmpty() ? 0 : row.get(6).convert<int>();
    
    if (row.get(7).isEmpty()) {
        movement.referenceId = Poco::Nullable<Poco::Int64>();
    } else {
        movement.referenceId = row.get(7).convert<long long>();
    }
    
    if (row.get(8).isEmpty()) {
        movement.referenceType = Poco::Nullable<std::string>();
    } else {
        movement.referenceType = row.get(8).convert<std::string>();
    }
    
    movement.movementDate = row.get(9).isEmpty() ? "" : row.get(9).convert<std::string>();
    movement.performedBy = row.get(10).isEmpty() ? 0 : row.get(10).convert<long long>();
    
    if (row.get(11).isEmpty()) {
        movement.reason = Poco::Nullable<std::string>();
    } else {
        movement.reason = row.get(11).convert<std::string>();
    }
    
    std::string statusStr = row.get(12).isEmpty() ? "" : row.get(12).convert<std::string>();
    movement.status = models::InventoryMovement::stringToMovementStatus(statusStr);
    
    return movement;
}

} // namespace database::repositories
