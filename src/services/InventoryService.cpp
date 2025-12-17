#include "InventoryService.hpp"
#include "../database/ConnectionPool.hpp"
#include <Poco/JSON/Parser.h>
#include <Poco/JSON/Stringifier.h>
#include <Poco/DateTimeFormatter.h>
#include <Poco/DateTimeFormat.h>
#include <Poco/NumberFormatter.h>
#include <algorithm>
#include <sstream>
#include <cmath>

namespace warehouse_backend::services
{

InventoryMovementResult::InventoryMovementResult()
    : success(false), movementId(0), movement(nullptr)
{
    
}

Poco::JSON::Object InventoryMovementResult::toJson() const
{
    Poco::JSON::Object result;
    result.set("success", success);
    result.set("message", message);
    result.set("movementId", static_cast<Poco::Int64>(movementId));
    
    if (movement)
    {
        result.set("movement", movement->toJson());
    }
    
    if (!details.size())
    {
        result.set("details", details);
    }
    
    return result;
}

Poco::JSON::Object StockCheckResult::toJson() const
{
    Poco::JSON::Object result;
    result.set("available", available);
    result.set("quantityAvailable", quantityAvailable);
    result.set("quantityNeeded", quantityNeeded);
    result.set("message", message);
    
    Poco::JSON::Array batchIdsArray;
    for (const auto& batchId : availableBatchIds)
    {
        batchIdsArray.add(static_cast<Poco::Int64>(batchId));
    }
    result.set("availableBatchIds", batchIdsArray);
    
    return result;
}

InventoryService::InventoryService()
    : batchRepository(std::make_unique<database::repositories::ProductBatchRepository>()),
      cellRepository(std::make_unique<database::repositories::WarehouseCellRepository>()),
      movementRepository(std::make_unique<database::repositories::InventoryMovementRepository>()),
      productRepository(std::make_unique<database::repositories::ProductRepository>()),
      auditRepository(std::make_unique<database::repositories::AuditLogRepository>())
{
}

InventoryMovementResult InventoryService::receiveProduct(long long productId,
                                                        long long supplierId,
                                                        int quantity,
                                                        double unitCost,
                                                        const std::string& batchNumber,
                                                        const std::string& expirationDate,
                                                        long long storageCellId,
                                                        long long receivedBy)
{
    InventoryMovementResult result;
    
    try
    {
        auto connection = database::ConnectionPool::getInstance().acquireConnection();
        if (!connection)
        {
            result.success = false;
            result.message = "Failed to acquire database connection";
            return result;
        }
        
        connection->beginTransaction();
        
        auto product = productRepository->findById(productId);
        if (!product)
        {
            connection->rollbackTransaction();
            result.success = false;
            result.message = "Product not found with ID: " + std::to_string(productId);
            return result;
        }
        
        if (quantity <= 0)
        {
            connection->rollbackTransaction();
            result.success = false;
            result.message = "Quantity must be greater than 0";
            return result;
        }
        
        if (unitCost < 0)
        {
            connection->rollbackTransaction();
            result.success = false;
            result.message = "Unit cost cannot be negative";
            return result;
        }
        
        std::string actualBatchNumber = batchNumber;
        if (actualBatchNumber.empty())
        {
            actualBatchNumber = generateBatchNumber(productId);
        }
        
        long long cellId = storageCellId;
        if (cellId == 0)
        {
            auto bestCell = findBestStorageCell(product->weight * quantity, 
                                                product->weight * quantity);
            if (bestCell.has("cellId"))
            {
                cellId = bestCell.get("cellId");
            }
        }
        
        if (cellId == 0)
        {
            connection->rollbackTransaction();
            result.success = false;
            result.message = "No suitable storage cell found for product";
            return result;
        }
        
        database::models::ProductBatch newBatch;
        newBatch.batchNumber = actualBatchNumber;
        newBatch.productId = productId;
        newBatch.supplierId = supplierId;
        newBatch.quantityReceived = quantity;
        newBatch.quantityAvailable = quantity;
        newBatch.unitCost = unitCost;

        if (!expirationDate.empty())
        {
            newBatch.expirationDate = expirationDate;
        }

        newBatch.storageCellId = cellId;
        newBatch.qualityStatus = database::models::QualityStatus::PENDING;

        newBatch.arrivalDate = utils::DateUtils::formatDateTime(utils::DateUtils::now());
        
        long long batchId = batchRepository->create(newBatch);
        
        if (batchId <= 0)
        {
            connection->rollbackTransaction();
            result.success = false;
            result.message = "Failed to create product batch";
            return result;
        }
        
        database::models::InventoryMovement movement;
        movement.movementType = database::models::MovementType::RECEIPT;
        movement.productId = productId;
        movement.batchId = batchId;
        
        if (cellId > 0)
        {
            movement.toCellId = cellId;
        }

        movement.quantity = quantity;
        movement.performedBy = receivedBy;
        movement.status = database::models::MovementStatus::COMPLETED;
        movement.reason = "Product receipt";
        movement.movementDate = utils::DateUtils::formatDateTime(utils::DateUtils::now());
        
        long long movementId = movementRepository->create(movement);
        
        updateCellOccupancy(cellId);
        
        connection->commitTransaction();
        
        result.success = true;
        result.movementId = movementId;
        result.message = "Product received successfully";
        result.movement = movementRepository->findById(movementId);
        
        Poco::JSON::Object details;
        details.set("batchId", static_cast<Poco::Int64>(batchId));
        details.set("batchNumber", actualBatchNumber);
        details.set("storageCellId", static_cast<Poco::Int64>(cellId));
        details.set("quantityReceived", quantity);
        details.set("unitCost", unitCost);
        result.details = details;
        
        logInventoryAudit("RECEIPT", movementId, receivedBy, 
                         "Received " + std::to_string(quantity) + 
                         " units of product ID " + std::to_string(productId));
    }
    catch (const std::exception& e)
    {
        result.success = false;
        result.message = "Error receiving product: " + std::string(e.what());
    }
    
    return result;
}

InventoryMovementResult InventoryService::transferProduct(long long batchId,
                                                         long long fromCellId,
                                                         long long toCellId,
                                                         int quantity,
                                                         long long performedBy,
                                                         const std::string& reason)
{
    InventoryMovementResult result;
    
    try
    {
        auto connection = database::ConnectionPool::getInstance().acquireConnection();
        if (!connection)
        {
            result.success = false;
            result.message = "Failed to acquire database connection";
            return result;
        }
        
        connection->beginTransaction();
        
        auto batch = batchRepository->findById(batchId);
        if (!batch)
        {
            connection->rollbackTransaction();
            result.success = false;
            result.message = "Batch not found with ID: " + std::to_string(batchId);
            return result;
        }
        
        if (batch->quantityAvailable < quantity)
        {
            connection->rollbackTransaction();
            result.success = false;
            result.message = "Insufficient quantity in batch. Available: " + 
                           std::to_string(batch->quantityAvailable) + 
                           ", Requested: " + std::to_string(quantity);
            return result;
        }
        
        if (fromCellId == toCellId)
        {
            connection->rollbackTransaction();
            result.success = false;
            result.message = "Source and destination cells cannot be the same";
            return result;
        }
        
        auto fromCell = cellRepository->findById(fromCellId);
        if (!fromCell)
        {
            connection->rollbackTransaction();
            result.success = false;
            result.message = "Source cell not found with ID: " + std::to_string(fromCellId);
            return result;
        }
        
        auto toCell = cellRepository->findById(toCellId);
        if (!toCell)
        {
            connection->rollbackTransaction();
            result.success = false;
            result.message = "Destination cell not found with ID: " + std::to_string(toCellId);
            return result;
        }
        
        if (toCell->status == database::models::CellStatus::BLOCKED)
        {
            connection->rollbackTransaction();
            result.success = false;
            result.message = "Destination cell is blocked";
            return result;
        }
        
        auto product = productRepository->findById(batch->productId);
        if (product)
        {
            double requiredWeight = product->weight * quantity;
            double requiredVolume = 0.0;
            
            if (!product->dimensions.isNull())
            {
                std::vector<std::string> dims;
                std::stringstream ss(product->dimensions);
                std::string item;
                while (std::getline(ss, item, 'x'))
                {
                    dims.push_back(item);
                }
                
                if (dims.size() == 3)
                {
                    requiredVolume = std::stod(dims[0]) * std::stod(dims[1]) * 
                                   std::stod(dims[2]) * quantity / 1000000.0;
                }
            }
            else
            {
                requiredVolume = 0.01 * quantity;
            }
            
            if (toCell->getAvailableWeight() < requiredWeight ||
                toCell->getAvailableVolume() < requiredVolume)
            {
                connection->rollbackTransaction();
                result.success = false;
                result.message = "Destination cell does not have enough capacity";
                return result;
            }
        }
        
        database::models::InventoryMovement movement;
        movement.movementType = database::models::MovementType::TRANSFER;
        movement.productId = batch->productId;
        movement.batchId = batchId;
        movement.quantity = quantity;
        movement.performedBy = performedBy;
        movement.status = database::models::MovementStatus::COMPLETED;
        movement.movementDate = utils::DateUtils::formatDateTime(utils::DateUtils::now());

        if (fromCellId > 0)
        {
            movement.fromCellId = fromCellId;
        }

        if (toCellId > 0)
        {
            movement.toCellId = toCellId;
        }

        std::string finalReason = reason.empty() ? "Product transfer" : reason;
        if (!finalReason.empty())
        {
            movement.reason = finalReason;
        }
        
        long long movementId = movementRepository->create(movement);
        
        batch->quantityAvailable -= quantity;
        batch->storageCellId = toCellId;
        batchRepository->update(batchId, *batch);
        
        updateCellOccupancy(fromCellId);
        updateCellOccupancy(toCellId);
        
        connection->commitTransaction();
        
        result.success = true;
        result.movementId = movementId;
        result.message = "Product transferred successfully";
        result.movement = movementRepository->findById(movementId);
        
        Poco::JSON::Object details;
        details.set("batchId", static_cast<Poco::Int64>(batchId));
        details.set("fromCellId", static_cast<Poco::Int64>(fromCellId));
        details.set("toCellId", static_cast<Poco::Int64>(toCellId));
        details.set("quantityTransferred", quantity);
        details.set("newBatchQuantity", batch->quantityAvailable);
        result.details = details;
        
        logInventoryAudit("TRANSFER", movementId, performedBy,
                         "Transferred " + std::to_string(quantity) + 
                         " units of batch ID " + std::to_string(batchId));
    }
    catch (const std::exception& e)
    {
        result.success = false;
        result.message = "Error transferring product: " + std::string(e.what());
    }
    
    return result;
}

InventoryMovementResult InventoryService::adjustInventory(long long productId,
                                                         long long batchId,
                                                         long long cellId,
                                                         int quantityAdjustment,
                                                         long long performedBy,
                                                         const std::string& reason)
{
    InventoryMovementResult result;
    
    try
    {
        auto connection = database::ConnectionPool::getInstance().acquireConnection();
        if (!connection)
        {
            result.success = false;
            result.message = "Failed to acquire database connection";
            return result;
        }
        
        connection->beginTransaction();
        
        std::unique_ptr<database::models::ProductBatch> batch;
        if (batchId > 0)
        {
            batch = batchRepository->findById(batchId);
            if (!batch)
            {
                connection->rollbackTransaction();
                result.success = false;
                result.message = "Batch not found with ID: " + std::to_string(batchId);
                return result;
            }
            
            if (batch->productId != productId && productId > 0)
            {
                connection->rollbackTransaction();
                result.success = false;
                result.message = "Batch does not belong to specified product";
                return result;
            }
            
            productId = batch->productId;
        }
        else
        {
            if (productId <= 0)
            {
                connection->rollbackTransaction();
                result.success = false;
                result.message = "Product ID must be specified";
                return result;
            }
            
            auto product = productRepository->findById(productId);
            if (!product)
            {
                connection->rollbackTransaction();
                result.success = false;
                result.message = "Product not found with ID: " + std::to_string(productId);
                return result;
            }
            
            if (cellId <= 0)
            {
                connection->rollbackTransaction();
                result.success = false;
                result.message = "Cell ID must be specified";
                return result;
            }
            
            auto batches = batchRepository->findByProduct(productId);
            for (const auto& b : batches)
            {
                if (b->storageCellId == cellId)
                {
                    batch = std::make_unique<database::models::ProductBatch>(*b);
                    break;
                }
            }
            
            if (!batch)
            {
                connection->rollbackTransaction();
                result.success = false;
                result.message = "No batch found for product in specified cell";
                return result;
            }
            
            batchId = batch->id;
        }
        
        if (quantityAdjustment == 0)
        {
            connection->rollbackTransaction();
            result.success = false;
            result.message = "Quantity adjustment cannot be zero";
            return result;
        }
        
        if (quantityAdjustment < 0 && batch->quantityAvailable < std::abs(quantityAdjustment))
        {
            connection->rollbackTransaction();
            result.success = false;
            result.message = "Cannot adjust by negative quantity exceeding available stock";
            return result;
        }
        
        database::models::InventoryMovement movement;

        movement.movementType = database::models::MovementType::ADJUSTMENT;
        movement.productId = productId;
        movement.batchId = batchId;
        movement.quantity = std::abs(quantityAdjustment);
        movement.performedBy = performedBy;
        movement.status = database::models::MovementStatus::COMPLETED;
        movement.movementDate = utils::DateUtils::formatDateTime(utils::DateUtils::now());

        Poco::Int64 cellId = batch->storageCellId;
        if (cellId > 0)
        {
            movement.fromCellId = cellId;
            movement.toCellId = cellId;
        }

        std::string finalReason = reason.empty() ? "Inventory adjustment" : reason;
        if (!finalReason.empty())
        {
            movement.reason = finalReason;
        }
        
        long long movementId = movementRepository->create(movement);
        
        batch->quantityReceived += quantityAdjustment;
        batch->quantityAvailable += quantityAdjustment;
        
        if (batch->quantityAvailable < 0)
        {
            batch->quantityAvailable = 0;
        }
        
        if (batch->quantityReceived < 0)
        {
            batch->quantityReceived = 0;
        }
        
        batchRepository->update(batchId, *batch);
        
        updateCellOccupancy(batch->storageCellId);
        
        connection->commitTransaction();
        
        result.success = true;
        result.movementId = movementId;
        result.message = "Inventory adjusted successfully";
        result.movement = movementRepository->findById(movementId);
        
        Poco::JSON::Object details;
        details.set("batchId", static_cast<Poco::Int64>(batchId));
        details.set("cellId", static_cast<Poco::Int64>(batch->storageCellId));
        details.set("quantityAdjustment", quantityAdjustment);
        details.set("newBatchQuantity", batch->quantityAvailable);
        details.set("newBatchReceived", batch->quantityReceived);
        result.details = details;
        
        logInventoryAudit("ADJUSTMENT", movementId, performedBy,
                         "Adjusted inventory by " + std::to_string(quantityAdjustment) + 
                         " units for batch ID " + std::to_string(batchId));
    }
    catch (const std::exception& e)
    {
        result.success = false;
        result.message = "Error adjusting inventory: " + std::string(e.what());
    }
    
    return result;
}

StockCheckResult InventoryService::checkStockAvailability(long long productId, int quantity)
{
    StockCheckResult result;
    
    try
    {
        auto product = productRepository->findById(productId);
        if (!product)
        {
            result.available = false;
            result.message = "Product not found with ID: " + std::to_string(productId);
            return result;
        }
        
        if (quantity <= 0)
        {
            result.available = false;
            result.message = "Quantity must be greater than 0";
            return result;
        }
        
        auto batches = batchRepository->findAvailableBatches(productId, quantity);
        
        int totalAvailable = 0;
        std::vector<long long> availableBatchIds;
        
        for (const auto& batch : batches)
        {
            if (batch->qualityStatus == database::models::QualityStatus::APPROVED &&
                batch->quantityAvailable > 0)
            {
                totalAvailable += batch->quantityAvailable;
                availableBatchIds.push_back(batch->id);
                
                if (totalAvailable >= quantity)
                {
                    break;
                }
            }
        }
        
        result.available = totalAvailable >= quantity;
        result.quantityAvailable = totalAvailable;
        result.quantityNeeded = quantity;
        result.availableBatchIds = availableBatchIds;
        
        if (result.available)
        {
            result.message = "Sufficient stock available";
        }
        else
        {
            result.message = "Insufficient stock. Available: " + 
                           std::to_string(totalAvailable) + 
                           ", Needed: " + std::to_string(quantity);
        }
    }
    catch (const std::exception& e)
    {
        result.available = false;
        result.message = "Error checking stock availability: " + std::string(e.what());
    }
    
    return result;
}

StockCheckResult InventoryService::checkBatchAvailability(long long batchId, int quantity)
{
    StockCheckResult result;
    
    try
    {
        auto batch = batchRepository->findById(batchId);
        if (!batch)
        {
            result.available = false;
            result.message = "Batch not found with ID: " + std::to_string(batchId);
            return result;
        }
        
        if (quantity <= 0)
        {
            result.available = false;
            result.message = "Quantity must be greater than 0";
            return result;
        }
        
        result.available = batch->quantityAvailable >= quantity;
        result.quantityAvailable = batch->quantityAvailable;
        result.quantityNeeded = quantity;
        
        if (batch->quantityAvailable > 0)
        {
            result.availableBatchIds.push_back(batchId);
        }
        
        if (result.available)
        {
            result.message = "Sufficient quantity in batch";
        }
        else
        {
            result.message = "Insufficient quantity in batch. Available: " + 
                           std::to_string(batch->quantityAvailable) + 
                           ", Needed: " + std::to_string(quantity);
        }
    }
    catch (const std::exception& e)
    {
        result.available = false;
        result.message = "Error checking batch availability: " + std::string(e.what());
    }
    
    return result;
}

Poco::JSON::Array InventoryService::getMovements(int page, int pageSize, 
                                                const std::map<std::string, std::string>& filters)
{
    Poco::JSON::Array result;
    
    try
    {
        if (filters.find("productId") != filters.end())
        {
            long long productId = std::stoll(filters.at("productId"));
            auto movements = movementRepository->findByProductId(productId);
            for (const auto& movement : movements)
            {
                if (movement)
                {
                    result.add(movement->toJson());
                }
            }
        }
        else if (filters.find("batchId") != filters.end())
        {
            long long batchId = std::stoll(filters.at("batchId"));
            auto movements = movementRepository->findByBatchId(batchId);
            for (const auto& movement : movements)
            {
                if (movement)
                {
                    result.add(movement->toJson());
                }
            }
        }
        else if (filters.find("movementType") != filters.end())
        {
            std::string typeStr = filters.at("movementType");
            auto movementType = database::models::InventoryMovement::stringToMovementType(typeStr);
            auto movements = movementRepository->findByMovementType(movementType);
            for (const auto& movement : movements)
            {
                if (movement)
                {
                    result.add(movement->toJson());
                }
            }
        }
        else
        {
            auto movements = movementRepository->findPaginated(page, pageSize);
            for (const auto& movement : movements)
            {
                if (movement)
                {
                    result.add(movement->toJson());
                }
            }
        }
    }
    catch (const std::exception& e)
    {
        Poco::JSON::Object error;
        error.set("error", "Error getting movements: " + std::string(e.what()));
        result.add(error);
    }
    
    return result;
}

Poco::JSON::Object InventoryService::getProductStockInfo(long long productId)
{
    Poco::JSON::Object result;
    
    try
    {
        auto product = productRepository->findById(productId);
        if (!product)
        {
            result.set("error", "Product not found with ID: " + std::to_string(productId));
            return result;
        }
        
        auto batches = batchRepository->findByProduct(productId);
        
        int totalAvailable = 0;
        int totalReceived = 0;
        double totalValue = 0.0;
        
        Poco::JSON::Array batchesArray;
        Poco::JSON::Array lowStockBatchesArray;
        Poco::JSON::Array expiringBatchesArray;
        
        for (const auto& batch : batches)
        {
            Poco::JSON::Object batchJson = batch->toJson();
            batchesArray.add(batchJson);
            
            totalAvailable += batch->quantityAvailable;
            totalReceived += batch->quantityReceived;
            totalValue += batch->quantityAvailable * batch->unitCost;
            
            if (batch->quantityAvailable < product->minStockLevel * 0.5)
            {
                lowStockBatchesArray.add(batchJson);
            }
            
            if (!batch->expirationDate.isNull())
            {
                auto expirationDate = utils::DateUtils::parseDate(batch->expirationDate);
                auto today = utils::DateUtils::now();
                int daysUntilExpiration = utils::DateUtils::daysBetween(today, expirationDate);
                
                if (daysUntilExpiration <= 30 && daysUntilExpiration >= 0)
                {
                    expiringBatchesArray.add(batchJson);
                }
            }
        }
        
        result.set("productId", static_cast<Poco::Int64>(productId));
        result.set("productName", product->name);
        result.set("sku", product->sku);
        result.set("totalAvailable", totalAvailable);
        result.set("totalReceived", totalReceived);
        result.set("totalValue", totalValue);
        result.set("minStockLevel", product->minStockLevel);
        result.set("maxStockLevel", product->maxStockLevel);
        result.set("batches", batchesArray);
        result.set("lowStockBatches", lowStockBatchesArray);
        result.set("expiringBatches", expiringBatchesArray);
        
        std::string stockStatus;
        if (totalAvailable <= product->minStockLevel)
        {
            stockStatus = "CRITICAL";
        }
        else if (totalAvailable <= product->minStockLevel * 1.5)
        {
            stockStatus = "LOW";
        }
        else if (totalAvailable >= product->maxStockLevel * 0.9)
        {
            stockStatus = "HIGH";
        }
        else
        {
            stockStatus = "NORMAL";
        }
        
        result.set("stockStatus", stockStatus);
        result.set("needsReorder", totalAvailable <= product->minStockLevel);
    }
    catch (const std::exception& e)
    {
        result.set("error", "Error getting product stock info: " + std::string(e.what()));
    }
    
    return result;
}

Poco::JSON::Object InventoryService::getBatchStockInfo(long long batchId)
{
    Poco::JSON::Object result;
    
    try
    {
        auto batch = batchRepository->findById(batchId);
        if (!batch)
        {
            result.set("error", "Batch not found with ID: " + std::to_string(batchId));
            return result;
        }
        
        auto product = productRepository->findById(batch->productId);
        auto cell = cellRepository->findById(batch->storageCellId);
        
        result = batch->toJson();
        
        if (product)
        {
            result.set("productDetails", product->toJson());
        }
        
        if (cell)
        {
            result.set("cellDetails", cell->toJson());
        }
        
        if (!batch->expirationDate.isNull())
        {
            auto expirationDate = utils::DateUtils::parseDate(batch->expirationDate);
            auto today = utils::DateUtils::now();
            int daysUntilExpiration = utils::DateUtils::daysBetween(today, expirationDate);
            
            result.set("daysUntilExpiration", daysUntilExpiration);
            result.set("isExpired", daysUntilExpiration < 0);
            result.set("isExpiringSoon", daysUntilExpiration <= 30 && daysUntilExpiration >= 0);
        }
        
        result.set("batchValue", batch->quantityAvailable * batch->unitCost);
        result.set("utilizationPercentage", 
                  (static_cast<double>(batch->quantityAvailable) / batch->quantityReceived) * 100);
    }
    catch (const std::exception& e)
    {
        result.set("error", "Error getting batch stock info: " + std::string(e.what()));
    }
    
    return result;
}

Poco::JSON::Object InventoryService::getCellStockInfo(long long cellId)
{
    Poco::JSON::Object result;
    
    try
    {
        auto cell = cellRepository->findById(cellId);
        if (!cell)
        {
            result.set("error", "Cell not found with ID: " + std::to_string(cellId));
            return result;
        }
        
        auto batches = batchRepository->findByStorageCell(cellId);
        
        double totalValue = 0.0;
        double totalWeight = 0.0;
        double totalVolume = 0.0;
        int totalItems = 0;
        
        Poco::JSON::Array batchesArray;
        Poco::JSON::Array productsArray;
        
        std::map<long long, Poco::JSON::Object> productMap;
        
        for (const auto& batch : batches)
        {
            Poco::JSON::Object batchJson = batch->toJson();
            batchesArray.add(batchJson);
            
            totalValue += batch->quantityAvailable * batch->unitCost;
            totalItems += batch->quantityAvailable;
            
            auto product = productRepository->findById(batch->productId);
            if (product)
            {
                totalWeight += product->weight * batch->quantityAvailable;
                
                if (!product->dimensions.isNull())
                {
                    std::vector<std::string> dims;
                    std::stringstream ss(product->dimensions);
                    std::string item;
                    while (std::getline(ss, item, 'x'))
                    {
                        dims.push_back(item);
                    }
                    
                    if (dims.size() == 3)
                    {
                        totalVolume += std::stod(dims[0]) * std::stod(dims[1]) * 
                                     std::stod(dims[2]) * batch->quantityAvailable / 1000000.0;
                    }
                }
                else
                {
                    totalVolume += 0.01 * batch->quantityAvailable;
                }
                
                if (productMap.find(product->id) == productMap.end())
                {
                    Poco::JSON::Object productInfo;
                    productInfo.set("productId", static_cast<Poco::Int64>(product->id));
                    productInfo.set("sku", product->sku);
                    productInfo.set("name", product->name);
                    productInfo.set("totalQuantity", batch->quantityAvailable);
                    productInfo.set("batches", Poco::JSON::Array());
                    
                    productMap[product->id] = productInfo;
                }
                else
                {
                    auto& productInfo = productMap[product->id];
                    int currentQty = productInfo.get("totalQuantity");
                    productInfo.set("totalQuantity", currentQty + batch->quantityAvailable);
                }
                
                auto& productInfo = productMap[product->id];
                auto batchesArray = productInfo.get("batches").extract<Poco::JSON::Array::Ptr>();
                batchesArray->add(batchJson);
            }
        }
        
        for (const auto& [productId, productInfo] : productMap)
        {
            productsArray.add(productInfo);
        }
        
        result.set("cellId", static_cast<Poco::Int64>(cellId));
        result.set("cellCode", cell->cellCode);
        result.set("zone", cell->zone);
        result.set("status", database::models::WarehouseCell::statusToString(cell->status));
        result.set("temperatureZone", 
                  database::models::WarehouseCell::temperatureZoneToString(cell->temperatureZone));
        result.set("maxVolume", cell->maxVolume);
        result.set("maxWeight", cell->maxWeight);
        result.set("currentOccupancy", cell->currentOccupancy);
        result.set("availableVolume", cell->getAvailableVolume());
        result.set("availableWeight", cell->getAvailableWeight());
        result.set("totalValue", totalValue);
        result.set("totalWeight", totalWeight);
        result.set("totalVolume", totalVolume);
        result.set("totalItems", totalItems);
        result.set("batchesCount", static_cast<int>(batches.size()));
        result.set("productsCount", static_cast<int>(productMap.size()));
        result.set("batches", batchesArray);
        result.set("products", productsArray);
        
        double volumeUtilization = (totalVolume / cell->maxVolume) * 100;
        double weightUtilization = (totalWeight / cell->maxWeight) * 100;
        result.set("volumeUtilization", volumeUtilization);
        result.set("weightUtilization", weightUtilization);
    }
    catch (const std::exception& e)
    {
        result.set("error", "Error getting cell stock info: " + std::string(e.what()));
    }
    
    return result;
}

Poco::JSON::Array InventoryService::findExpiringProducts(int daysThreshold)
{
    Poco::JSON::Array result;
    
    try
    {
        auto batches = batchRepository->findExpiringBatches(daysThreshold);
        
        for (const auto& batch : batches)
        {
            Poco::JSON::Object item;
            item.set("batchId", static_cast<Poco::Int64>(batch->id));
            item.set("batchNumber", batch->batchNumber);
            item.set("productId", static_cast<Poco::Int64>(batch->productId));

            auto product = productRepository->findById(batch->productId);
            if (product)
            {
                item.set("productName", product->name);
                item.set("sku", product->sku);
            }

            item.set("quantityAvailable", batch->quantityAvailable);
            item.set("unitCost", batch->unitCost);

            if (!batch->expirationDate.isNull())
            {
                item.set("expirationDate", batch->expirationDate.value());

                auto expirationDate = utils::DateUtils::parseDate(batch->expirationDate.value());
                auto today = utils::DateUtils::now();
                int daysUntilExpiration = utils::DateUtils::daysBetween(today, expirationDate);
                item.set("daysUntilExpiration", daysUntilExpiration);
            }
        
            item.set("storageCellId", static_cast<Poco::Int64>(batch->storageCellId));

            result.add(item);
        }
    }
    catch (const std::exception& e)
    {
        Poco::JSON::Object error;
        error.set("error", "Error finding expiring products: " + std::string(e.what()));
        result.add(error);
    }
    
    return result;
}

Poco::JSON::Array InventoryService::findLowStockProducts(int threshold)
{
    Poco::JSON::Array result;
    
    try
    {
        auto products = productRepository->findProductsWithLowStock();
        
        for (const auto& product : products)
        {
            int totalAvailable = 0;
            auto batches = batchRepository->findByProduct(product->id);
            
            for (const auto& batch : batches)
            {
                if (batch->qualityStatus == database::models::QualityStatus::APPROVED)
                {
                    totalAvailable += batch->quantityAvailable;
                }
            }
            
            if (totalAvailable <= threshold || totalAvailable <= product->minStockLevel)
            {
                Poco::JSON::Object item;
                item.set("productId", static_cast<Poco::Int64>(product->id));
                item.set("sku", product->sku);
                item.set("productName", product->name);
                item.set("currentStock", totalAvailable);
                item.set("minStockLevel", product->minStockLevel);
                item.set("maxStockLevel", product->maxStockLevel);
                item.set("needsReorder", totalAvailable <= product->minStockLevel);
                
                result.add(item);
            }
        }
    }
    catch (const std::exception& e)
    {
        Poco::JSON::Object error;
        error.set("error", "Error finding low stock products: " + std::string(e.what()));
        result.add(error);
    }
    
    return result;
}

Poco::JSON::Array InventoryService::findProductsNeedingQualityInspection()
{
    Poco::JSON::Array result;
    
    try
    {
        auto batches = batchRepository->findBatchesNeedingInspection();
        
        for (const auto& batch : batches)
        {
            Poco::JSON::Object item;
            item.set("batchId", static_cast<Poco::Int64>(batch->id));
            item.set("batchNumber", batch->batchNumber);
            item.set("productId", static_cast<Poco::Int64>(batch->productId));

            auto product = productRepository->findById(batch->productId);
            if (product)
            {
                item.set("productName", product->name);
                item.set("sku", product->sku);
            }

            item.set("quantityAvailable", batch->quantityAvailable);

            if (!batch->arrivalDate.isNull())
            {
                item.set("arrivalDate", batch->arrivalDate.value());
            }

            item.set("qualityStatus", 
                     database::models::ProductBatch::qualityStatusToString(batch->qualityStatus));
            item.set("storageCellId", static_cast<Poco::Int64>(batch->storageCellId));

            auto cell = cellRepository->findById(batch->storageCellId);
            if (cell)
            {
                item.set("cellCode", cell->cellCode);
            }

            result.add(item);
        }
    }
    catch (const std::exception& e)
    {
        Poco::JSON::Object error;
        error.set("error", "Error finding products needing quality inspection: " + std::string(e.what()));
        result.add(error);
    }
    
    return result;
}

bool InventoryService::updateBatchQualityStatus(long long batchId,
                                               const std::string& qualityStatus,
                                               long long updatedBy)
{
    try
    {
        auto batch = batchRepository->findById(batchId);
        if (!batch)
        {
            return false;
        }
        
        auto status = database::models::ProductBatch::stringToQualityStatus(qualityStatus);
        bool success = batchRepository->updateQualityStatus(batchId, qualityStatus);
        
        if (success)
        {
            logInventoryAudit("QUALITY_UPDATE", batchId, updatedBy,
                            "Updated quality status to " + qualityStatus + 
                            " for batch ID " + std::to_string(batchId));
        }
        
        return success;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

Poco::JSON::Array InventoryService::getInventoryMovementHistory(long long productId,
                                                               const std::string& startDate,
                                                               const std::string& endDate)
{
    Poco::JSON::Array result;
    
    try
    {
        auto movements = movementRepository->findByProductId(productId);
        
        for (const auto& movement : movements)
        {
            if (!startDate.empty())
            {
                auto movementDate = utils::DateUtils::parseDateTime(movement->movementDate);
                auto filterStartDate = utils::DateUtils::parseDateTime(startDate);
                
                if (movementDate < filterStartDate)
                {
                    continue;
                }
            }
            
            if (!endDate.empty())
            {
                auto movementDate = utils::DateUtils::parseDateTime(movement->movementDate);
                auto filterEndDate = utils::DateUtils::parseDateTime(endDate);
                
                if (movementDate > filterEndDate)
                {
                    continue;
                }
            }
            
            result.add(movement->toJson());
        }
    }
    catch (const std::exception& e)
    {
        Poco::JSON::Object error;
        error.set("error", "Error getting inventory movement history: " + std::string(e.what()));
        result.add(error);
    }
    
    return result;
}

Poco::JSON::Array InventoryService::getCellMovementHistory(long long cellId,
                                                          const std::string& startDate,
                                                          const std::string& endDate)
{
    Poco::JSON::Array result;
    
    try
    {
        auto fromMovements = movementRepository->findByFromCell(cellId);
        auto toMovements = movementRepository->findByToCell(cellId);
        
        std::vector<std::unique_ptr<database::models::InventoryMovement>> allMovements;
        allMovements.reserve(fromMovements.size() + toMovements.size());
        
        for (auto& movement : fromMovements)
        {
            allMovements.push_back(std::move(movement));
        }
        
        for (auto& movement : toMovements)
        {
            allMovements.push_back(std::move(movement));
        }
        
        std::sort(allMovements.begin(), allMovements.end(),
                 [](const std::unique_ptr<database::models::InventoryMovement>& a,
                    const std::unique_ptr<database::models::InventoryMovement>& b) {
                     return a->movementDate > b->movementDate;
                 });
        
        for (const auto& movement : allMovements)
        {
            if (!startDate.empty())
            {
                auto movementDate = utils::DateUtils::parseDateTime(movement->movementDate);
                auto filterStartDate = utils::DateUtils::parseDateTime(startDate);
                
                if (movementDate < filterStartDate)
                {
                    continue;
                }
            }
            
            if (!endDate.empty())
            {
                auto movementDate = utils::DateUtils::parseDateTime(movement->movementDate);
                auto filterEndDate = utils::DateUtils::parseDateTime(endDate);
                
                if (movementDate > filterEndDate)
                {
                    continue;
                }
            }
            
            result.add(movement->toJson());
        }
    }
    catch (const std::exception& e)
    {
        Poco::JSON::Object error;
        error.set("error", "Error getting cell movement history: " + std::string(e.what()));
        result.add(error);
    }
    
    return result;
}

Poco::JSON::Object InventoryService::getInventoryStatistics()
{
    Poco::JSON::Object stats;
    
    try
    {
        auto allProducts = productRepository->findAll();
        auto allBatches = batchRepository->findAll();
        auto allCells = cellRepository->findAll();
        auto allMovements = movementRepository->findAll();
        
        int totalProducts = static_cast<int>(allProducts.size());
        int totalBatches = static_cast<int>(allBatches.size());
        int totalCells = static_cast<int>(allCells.size());
        int totalMovements = static_cast<int>(allMovements.size());
        
        double totalStockValue = 0.0;
        int totalStockQuantity = 0;
        int approvedBatches = 0;
        int pendingBatches = 0;
        int quarantineBatches = 0;
        int rejectedBatches = 0;
        
        for (const auto& batch : allBatches)
        {
            totalStockValue += batch->quantityAvailable * batch->unitCost;
            totalStockQuantity += batch->quantityAvailable;
            
            switch (batch->qualityStatus)
            {
                case database::models::QualityStatus::APPROVED:
                    approvedBatches++;
                    break;
                case database::models::QualityStatus::PENDING:
                    pendingBatches++;
                    break;
                case database::models::QualityStatus::QUARANTINE:
                    quarantineBatches++;
                    break;
                case database::models::QualityStatus::REJECTED:
                    rejectedBatches++;
                    break;
            }
        }
        
        int emptyCells = 0;
        int partiallyOccupiedCells = 0;
        int fullCells = 0;
        int blockedCells = 0;
        
        for (const auto& cell : allCells)
        {
            switch (cell->status)
            {
                case database::models::CellStatus::EMPTY:
                    emptyCells++;
                    break;
                case database::models::CellStatus::PARTIALLY_OCCUPIED:
                    partiallyOccupiedCells++;
                    break;
                case database::models::CellStatus::FULL:
                    fullCells++;
                    break;
                case database::models::CellStatus::BLOCKED:
                    blockedCells++;
                    break;
            }
        }
        
        int receiptMovements = 0;
        int shipmentMovements = 0;
        int transferMovements = 0;
        int adjustmentMovements = 0;
        
        for (const auto& movement : allMovements)
        {
            switch (movement->movementType)
            {
                case database::models::MovementType::RECEIPT:
                    receiptMovements++;
                    break;
                case database::models::MovementType::SHIPMENT:
                    shipmentMovements++;
                    break;
                case database::models::MovementType::TRANSFER:
                    transferMovements++;
                    break;
                case database::models::MovementType::ADJUSTMENT:
                    adjustmentMovements++;
                    break;
                default:
                    break;
            }
        }
        
        stats.set("totalProducts", totalProducts);
        stats.set("totalBatches", totalBatches);
        stats.set("totalCells", totalCells);
        stats.set("totalMovements", totalMovements);
        stats.set("totalStockValue", totalStockValue);
        stats.set("totalStockQuantity", totalStockQuantity);
        stats.set("averageStockValue", totalBatches > 0 ? totalStockValue / totalBatches : 0);
        stats.set("averageBatchQuantity", totalBatches > 0 ? totalStockQuantity / totalBatches : 0);
        
        stats.set("approvedBatches", approvedBatches);
        stats.set("pendingBatches", pendingBatches);
        stats.set("quarantineBatches", quarantineBatches);
        stats.set("rejectedBatches", rejectedBatches);
        
        stats.set("emptyCells", emptyCells);
        stats.set("partiallyOccupiedCells", partiallyOccupiedCells);
        stats.set("fullCells", fullCells);
        stats.set("blockedCells", blockedCells);
        
        stats.set("receiptMovements", receiptMovements);
        stats.set("shipmentMovements", shipmentMovements);
        stats.set("transferMovements", transferMovements);
        stats.set("adjustmentMovements", adjustmentMovements);
        
        stats.set("lastUpdated", utils::DateUtils::formatDateTime(utils::DateUtils::now()));
    }
    catch (const std::exception& e)
    {
        stats.set("error", "Error calculating inventory statistics: " + std::string(e.what()));
    }
    
    return stats;
}

Poco::JSON::Array InventoryService::getStockValueReport()
{
    Poco::JSON::Array result;
    
    try
    {
        auto products = productRepository->findAll();
        
        for (const auto& product : products)
        {
            auto batches = batchRepository->findByProduct(product->id);
            
            double productValue = 0.0;
            int productQuantity = 0;
            
            for (const auto& batch : batches)
            {
                if (batch->qualityStatus == database::models::QualityStatus::APPROVED)
                {
                    productValue += batch->quantityAvailable * batch->unitCost;
                    productQuantity += batch->quantityAvailable;
                }
            }
            
            if (productValue > 0)
            {
                Poco::JSON::Object item;
                item.set("productId", static_cast<Poco::Int64>(product->id));
                item.set("sku", product->sku);
                item.set("productName", product->name);
                item.set("stockValue", productValue);
                item.set("stockQuantity", productQuantity);
                item.set("unitPrice", product->unitPrice);
                item.set("averageUnitCost", productQuantity > 0 ? productValue / productQuantity : 0);
                
                result.add(item);
            }
        }
    }
    catch (const std::exception& e)
    {
        Poco::JSON::Object error;
        error.set("error", "Error generating stock value report: " + std::string(e.what()));
        result.add(error);
    }
    
    return result;
}

Poco::JSON::Array InventoryService::getExpirationReport()
{
    Poco::JSON::Array result;
    
    try
    {
        auto batches = batchRepository->findAll();
        
        std::map<std::string, Poco::JSON::Object> monthMap;
        
        for (const auto& batch : batches)
        {
            if (!batch->expirationDate.isNull())
            {
                auto expirationDate = utils::DateUtils::parseDate(batch->expirationDate);
                std::string monthKey = utils::DateUtils::formatDate(expirationDate, "%Y-%m");
                
                if (monthMap.find(monthKey) == monthMap.end())
                {
                    Poco::JSON::Object monthInfo;
                    monthInfo.set("month", monthKey);
                    monthInfo.set("totalBatches", 0);
                    monthInfo.set("totalQuantity", 0);
                    monthInfo.set("totalValue", 0.0);
                    monthInfo.set("batches", Poco::JSON::Array::Ptr(new Poco::JSON::Array()));
                    
                    monthMap[monthKey] = monthInfo;
                }
                
                auto& monthInfo = monthMap[monthKey];
                int totalBatches = monthInfo.get("totalBatches");
                int totalQuantity = monthInfo.get("totalQuantity");
                double totalValue = monthInfo.get("totalValue");
                
                monthInfo.set("totalBatches", totalBatches + 1);
                monthInfo.set("totalQuantity", totalQuantity + batch->quantityAvailable);
                monthInfo.set("totalValue", totalValue + (batch->quantityAvailable * batch->unitCost));
                
                auto batchesArray = monthInfo.get("batches").extract<Poco::JSON::Array::Ptr>();
                
                Poco::JSON::Object batchInfo;
                batchInfo.set("batchId", static_cast<Poco::Int64>(batch->id));
                batchInfo.set("batchNumber", batch->batchNumber);
                batchInfo.set("productId", static_cast<Poco::Int64>(batch->productId));
                
                auto product = productRepository->findById(batch->productId);
                if (product)
                {
                    batchInfo.set("productName", product->name);
                }
                
                batchInfo.set("quantity", batch->quantityAvailable);
                batchInfo.set("value", batch->quantityAvailable * batch->unitCost);

                if (!batch->expirationDate.isNull())
                    batchInfo.set("expirationDate", batch->expirationDate.value());
                
                batchesArray->add(batchInfo);
            }
        }
        
        for (const auto& [monthKey, monthInfo] : monthMap)
        {
            result.add(monthInfo);
        }
    }
    catch (const std::exception& e)
    {
        Poco::JSON::Object error;
        error.set("error", "Error generating expiration report: " + std::string(e.what()));
        result.add(error);
    }
    
    return result;
}

Poco::JSON::Array InventoryService::getQualityStatusReport()
{
    Poco::JSON::Array result;
    
    try
    {
        auto batches = batchRepository->findAll();
        
        std::map<std::string, Poco::JSON::Object> statusMap;
        
        for (const auto& batch : batches)
        {
            std::string status = database::models::ProductBatch::qualityStatusToString(batch->qualityStatus);
            
            if (statusMap.find(status) == statusMap.end())
            {
                Poco::JSON::Object statusInfo;
                statusInfo.set("status", status);
                statusInfo.set("totalBatches", 0);
                statusInfo.set("totalQuantity", 0);
                statusInfo.set("totalValue", 0.0);
                statusInfo.set("batches", Poco::JSON::Array::Ptr(new Poco::JSON::Array()));
                
                statusMap[status] = statusInfo;
            }
            
            auto& statusInfo = statusMap[status];
            int totalBatches = statusInfo.get("totalBatches");
            int totalQuantity = statusInfo.get("totalQuantity");
            double totalValue = statusInfo.get("totalValue");
            
            statusInfo.set("totalBatches", totalBatches + 1);
            statusInfo.set("totalQuantity", totalQuantity + batch->quantityAvailable);
            statusInfo.set("totalValue", totalValue + (batch->quantityAvailable * batch->unitCost));
            
            auto batchesArray = statusInfo.get("batches").extract<Poco::JSON::Array::Ptr>();
            
            Poco::JSON::Object batchInfo;
            batchInfo.set("batchId", static_cast<Poco::Int64>(batch->id));
            batchInfo.set("batchNumber", batch->batchNumber);
            batchInfo.set("productId", static_cast<Poco::Int64>(batch->productId));
            
            auto product = productRepository->findById(batch->productId);
            if (product)
            {
                batchInfo.set("productName", product->name);
            }
            
            batchInfo.set("quantity", batch->quantityAvailable);
            batchInfo.set("value", batch->quantityAvailable * batch->unitCost);

            if (!batch->arrivalDate.isNull())
                batchInfo.set("arrivalDate", batch->arrivalDate.value());
            
            batchesArray->add(batchInfo);
        }
        
        for (const auto& [status, statusInfo] : statusMap)
        {
            result.add(statusInfo);
        }
    }
    catch (const std::exception& e)
    {
        Poco::JSON::Object error;
        error.set("error", "Error generating quality status report: " + std::string(e.what()));
        result.add(error);
    }
    
    return result;
}

Poco::JSON::Object InventoryService::findBestStorageCell(double volume,
                                                        double weight,
                                                        const std::string& temperatureZone)
{
    Poco::JSON::Object result;
    
    try
    {
        std::vector<std::unique_ptr<database::models::WarehouseCell>> availableCells;
        
        if (temperatureZone.empty())
        {
            availableCells = cellRepository->findAvailableCells(volume, weight);
        }
        else
        {
            auto tempZone = database::models::WarehouseCell::stringToTemperatureZone(temperatureZone);
            auto cellsByZone = cellRepository->findByTemperatureZone(temperatureZone);
            
            for (auto& cell : cellsByZone)
            {
                if (cell->canStore(volume, weight) && 
                    cell->status != database::models::CellStatus::BLOCKED)
                {
                    availableCells.push_back(std::move(cell));
                }
            }
        }
        
        if (availableCells.empty())
        {
            result.set("found", false);
            result.set("message", "No suitable storage cells found");
            return result;
        }
        
        database::models::WarehouseCell* bestCell = availableCells[0].get();
        double bestScore = 0.0;
        
        for (const auto& cell : availableCells)
        {
            double volumeUtilization = (volume / cell->maxVolume) * 100;
            double weightUtilization = (weight / cell->maxWeight) * 100;
            double occupancyFactor = 100 - cell->currentOccupancy;
            
            double score = (occupancyFactor * 0.5) + 
                          ((100 - volumeUtilization) * 0.25) + 
                          ((100 - weightUtilization) * 0.25);
            
            if (score > bestScore)
            {
                bestScore = score;
                bestCell = cell.get();
            }
        }
        
        result.set("found", true);
        result.set("cellId", static_cast<Poco::Int64>(bestCell->id));
        result.set("cellCode", bestCell->cellCode);
        result.set("zone", bestCell->zone);
        result.set("temperatureZone", 
                  database::models::WarehouseCell::temperatureZoneToString(bestCell->temperatureZone));
        result.set("maxVolume", bestCell->maxVolume);
        result.set("maxWeight", bestCell->maxWeight);
        result.set("currentOccupancy", bestCell->currentOccupancy);
        result.set("availableVolume", bestCell->getAvailableVolume());
        result.set("availableWeight", bestCell->getAvailableWeight());
        result.set("score", bestScore);
        
        double projectedVolumeUtilization = ((bestCell->maxVolume - bestCell->getAvailableVolume() + volume) / 
                                           bestCell->maxVolume) * 100;
        double projectedWeightUtilization = ((bestCell->maxWeight - bestCell->getAvailableWeight() + weight) / 
                                           bestCell->maxWeight) * 100;
        
        result.set("projectedVolumeUtilization", projectedVolumeUtilization);
        result.set("projectedWeightUtilization", projectedWeightUtilization);
    }
    catch (const std::exception& e)
    {
        result.set("error", "Error finding best storage cell: " + std::string(e.what()));
    }
    
    return result;
}

bool InventoryService::reserveStock(long long batchId, int quantity, long long reservedBy)
{
    try
    {
        auto batch = batchRepository->findById(batchId);
        if (!batch)
        {
            return false;
        }
        
        if (batch->quantityAvailable < quantity)
        {
            return false;
        }
        
        batch->quantityAvailable -= quantity;
        bool success = batchRepository->update(batchId, *batch);
        
        if (success)
        {
            updateCellOccupancy(batch->storageCellId);
            logInventoryAudit("RESERVE", batchId, reservedBy,
                            "Reserved " + std::to_string(quantity) + 
                            " units from batch ID " + std::to_string(batchId));
        }
        
        return success;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

bool InventoryService::releaseStock(long long batchId, int quantity, long long releasedBy)
{
    try
    {
        auto batch = batchRepository->findById(batchId);
        if (!batch)
        {
            return false;
        }
        
        batch->quantityAvailable += quantity;
        
        if (batch->quantityAvailable > batch->quantityReceived)
        {
            batch->quantityAvailable = batch->quantityReceived;
        }
        
        bool success = batchRepository->update(batchId, *batch);
        
        if (success)
        {
            updateCellOccupancy(batch->storageCellId);
            logInventoryAudit("RELEASE", batchId, releasedBy,
                            "Released " + std::to_string(quantity) + 
                            " units to batch ID " + std::to_string(batchId));
        }
        
        return success;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

std::string InventoryService::generateBatchNumber(long long productId)
{
    try
    {
        auto product = productRepository->findById(productId);
        if (!product)
        {
            return "BATCH-UNKNOWN-" + std::to_string(time(nullptr));
        }
        
        std::string prefix = "BATCH";
        std::string sku = product->sku;
        
        time_t now = time(nullptr);
        struct tm* timeinfo = localtime(&now);
        char timestamp[20];
        strftime(timestamp, sizeof(timestamp), "%Y%m%d%H%M%S", timeinfo);
        
        return prefix + "-" + sku + "-" + timestamp;
    }
    catch (const std::exception&)
    {
        return "BATCH-ERROR-" + std::to_string(time(nullptr));
    }
}

void InventoryService::updateCellOccupancy(long long cellId)
{
    try
    {
        auto cell = cellRepository->findById(cellId);
        if (!cell)
        {
            return;
        }
        
        auto batches = batchRepository->findByStorageCell(cellId);
        
        double totalWeight = 0.0;
        double totalVolume = 0.0;
        
        for (const auto& batch : batches)
        {
            auto product = productRepository->findById(batch->productId);
            if (product)
            {
                totalWeight += product->weight * batch->quantityAvailable;
                
                if (!product->dimensions.isNull())
                {
                    std::vector<std::string> dims;
                    std::stringstream ss(product->dimensions);
                    std::string item;
                    while (std::getline(ss, item, 'x'))
                    {
                        dims.push_back(item);
                    }
                    
                    if (dims.size() == 3)
                    {
                        totalVolume += std::stod(dims[0]) * std::stod(dims[1]) * 
                                     std::stod(dims[2]) * batch->quantityAvailable / 1000000.0;
                    }
                }
                else
                {
                    totalVolume += 0.01 * batch->quantityAvailable;
                }
            }
        }
        
        double volumeOccupancy = (totalVolume / cell->maxVolume) * 100;
        double weightOccupancy = (totalWeight / cell->maxWeight) * 100;
        double occupancy = std::max(volumeOccupancy, weightOccupancy);
        
        database::models::CellStatus newStatus;
        if (occupancy >= 95.0)
        {
            newStatus = database::models::CellStatus::FULL;
        }
        else if (occupancy >= 20.0)
        {
            newStatus = database::models::CellStatus::PARTIALLY_OCCUPIED;
        }
        else
        {
            newStatus = database::models::CellStatus::EMPTY;
        }
        
        cell->currentOccupancy = std::min(occupancy, 100.0);
        cell->status = newStatus;
        
        cellRepository->update(cellId, *cell);
    }
    catch (const std::exception&)
    {
    }
}

void InventoryService::logInventoryAudit(const std::string& action,
                                        long long recordId,
                                        long long changedBy,
                                        const std::string& details)
{
    try
    {
        database::models::AuditLog auditLog;
        auditLog.tableName = "inventory_movements";
        auditLog.recordId = recordId;
        auditLog.action = database::models::AuditAction::UPDATE;
        auditLog.changedBy = changedBy;
        auditLog.description = action + ": " + details;
        auditLog.changedAt = utils::DateUtils::formatDateTime(utils::DateUtils::now());
        
        auditRepository->create(auditLog);
    }
    catch (const std::exception&)
    {
    }
}

} // namespace services
