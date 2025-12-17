#include "WarehouseCellService.hpp"
#include "../database/ConnectionPool.hpp"
#include "../utils/DateUtils.hpp"
#include <Poco/JSON/Parser.h>
#include <Poco/JSON/Stringifier.h>
#include <Poco/DateTimeFormatter.h>
#include <algorithm>
#include <sstream>

namespace warehouse_backend::services
{

WarehouseCellServiceResult::WarehouseCellServiceResult()
    : success(false), cellId(0), cell(nullptr)
{
}

Poco::JSON::Object WarehouseCellServiceResult::toJson() const
{
    Poco::JSON::Object result;
    result.set("success", success);
    result.set("message", message);
    result.set("cellId", static_cast<Poco::Int64>(cellId));
    
    if (cell)
    {
        result.set("cell", cell->toJson());
    }
    
    if (!data.size())
    {
        result.set("data", data);
    }
    
    return result;
}

WarehouseCellService::WarehouseCellService()
    : cellRepository(std::make_unique<database::repositories::WarehouseCellRepository>()),
      batchRepository(std::make_unique<database::repositories::ProductBatchRepository>()),
      auditRepository(std::make_unique<database::repositories::AuditLogRepository>())
{
}

WarehouseCellServiceResult WarehouseCellService::createCell(
    const database::models::WarehouseCell& cell, long long createdBy)
{
    WarehouseCellServiceResult result;
    
    std::string validationError;
    if (!validateCellData(cell, validationError))
    {
        result.success = false;
        result.message = "Validation failed: " + validationError;
        return result;
    }
    
    try
    {
        long long cellId = cellRepository->create(cell);
        
        if (cellId > 0)
        {
            result.success = true;
            result.cellId = cellId;
            result.message = "Warehouse cell created successfully";
            
            result.cell = cellRepository->findById(cellId);
            
            database::models::AuditLog auditLog;
            auditLog.tableName = "warehouse_cells";
            auditLog.recordId = cellId;
            auditLog.action = database::models::AuditAction::INSERT;
            auditLog.newValues = warehouseCellToJsonString(*result.cell);
            auditLog.changedBy = createdBy;
            auditLog.description = "Warehouse cell created";
            
            auditRepository->create(auditLog);
            
            result.data.set("cellCode", result.cell->cellCode);
            result.data.set("zone", result.cell->zone);
            result.data.set("status", database::models::WarehouseCell::statusToString(result.cell->status));
        }
        else
        {
            result.success = false;
            result.message = "Failed to create warehouse cell";
        }
    }
    catch (const std::exception& e)
    {
        result.success = false;
        result.message = "Error creating warehouse cell: " + std::string(e.what());
    }
    
    return result;
}

WarehouseCellServiceResult WarehouseCellService::getCellById(long long cellId)
{
    WarehouseCellServiceResult result;
    
    try
    {
        auto cell = cellRepository->findById(cellId);
        if (cell)
        {
            result.success = true;
            result.cellId = cellId;
            result.message = "Warehouse cell found";
            result.cell = std::move(cell);
            
            auto batches = batchRepository->findByStorageCell(cellId);
            result.data.set("batchCount", static_cast<int>(batches.size()));
            
            Poco::JSON::Array batchArray;
            for (const auto& batch : batches)
            {
                Poco::JSON::Object batchObj;
                batchObj.set("batchNumber", batch->batchNumber);
                batchObj.set("productName", batch->productName);
                batchObj.set("quantityAvailable", batch->quantityAvailable);
                batchArray.add(batchObj);
            }
            result.data.set("batches", batchArray);
        }
        else
        {
            result.success = false;
            result.message = "Warehouse cell not found with ID: " + std::to_string(cellId);
        }
    }
    catch (const std::exception& e)
    {
        result.success = false;
        result.message = "Error retrieving warehouse cell: " + std::string(e.what());
    }
    
    return result;
}

WarehouseCellServiceResult WarehouseCellService::getCellByCode(const std::string& cellCode)
{
    WarehouseCellServiceResult result;
    
    try
    {
        auto cell = cellRepository->findByCellCode(cellCode);
        if (cell)
        {
            result.success = true;
            result.cellId = cell->id;
            result.message = "Warehouse cell found";
            result.cell = std::move(cell);
            
            auto batches = batchRepository->findByStorageCell(result.cellId);
            result.data.set("batchCount", static_cast<int>(batches.size()));
            
            result.data.set("availableVolume", result.cell->getAvailableVolume());
            result.data.set("availableWeight", result.cell->getAvailableWeight());
        }
        else
        {
            result.success = false;
            result.message = "Warehouse cell not found with code: " + cellCode;
        }
    }
    catch (const std::exception& e)
    {
        result.success = false;
        result.message = "Error retrieving warehouse cell: " + std::string(e.what());
    }
    
    return result;
}

WarehouseCellServiceResult WarehouseCellService::updateCell(
    long long cellId, 
    const database::models::WarehouseCell& updatedCell,
    long long updatedBy)
{
    WarehouseCellServiceResult result;
    
    try
    {
        auto currentCell = cellRepository->findById(cellId);
        if (!currentCell)
        {
            result.success = false;
            result.message = "Warehouse cell not found with ID: " + std::to_string(cellId);
            return result;
        }
        
        std::string validationError;
        database::models::WarehouseCell cellToUpdate = updatedCell;
        cellToUpdate.id = cellId;
        cellToUpdate.currentOccupancy = currentCell->currentOccupancy;
        
        if (!validateCellData(cellToUpdate, validationError))
        {
            result.success = false;
            result.message = "Validation failed: " + validationError;
            return result;
        }
        
        std::string oldValues = warehouseCellToJsonString(*currentCell);
        
        bool updateSuccess = cellRepository->update(cellId, cellToUpdate);
        
        if (updateSuccess)
        {
            result.success = true;
            result.cellId = cellId;
            result.message = "Warehouse cell updated successfully";
            
            result.cell = cellRepository->findById(cellId);
            
            database::models::AuditLog auditLog;
            auditLog.tableName = "warehouse_cells";
            auditLog.recordId = cellId;
            auditLog.action = database::models::AuditAction::UPDATE;
            auditLog.oldValues = oldValues;
            std::string oldValues = warehouseCellToJsonString(*result.cell);
            auditLog.changedBy = updatedBy;
            auditLog.description = "Warehouse cell updated";
            
            auditRepository->create(auditLog);
            
            std::vector<std::string> changes;
            if (currentCell->cellCode != result.cell->cellCode)
                changes.push_back("cellCode");
            if (currentCell->zone != result.cell->zone)
                changes.push_back("zone");
            if (currentCell->maxVolume != result.cell->maxVolume)
                changes.push_back("maxVolume");
            if (currentCell->maxWeight != result.cell->maxWeight)
                changes.push_back("maxWeight");
            
            Poco::JSON::Array changesArray;
            for (const auto& change : changes)
            {
                changesArray.add(change);
            }
            result.data.set("changes", changesArray);
        }
        else
        {
            result.success = false;
            result.message = "Failed to update warehouse cell";
        }
    }
    catch (const std::exception& e)
    {
        result.success = false;
        result.message = "Error updating warehouse cell: " + std::string(e.what());
    }
    
    return result;
}

WarehouseCellServiceResult WarehouseCellService::blockCell(long long cellId, long long blockedBy)
{
    WarehouseCellServiceResult result;
    
    try
    {
        auto currentCell = cellRepository->findById(cellId);
        if (!currentCell)
        {
            result.success = false;
            result.message = "Warehouse cell not found with ID: " + std::to_string(cellId);
            return result;
        }
        
        auto batches = batchRepository->findByStorageCell(cellId);
        if (!batches.empty())
        {
            result.success = false;
            result.message = "Cannot block cell that contains batches";
            result.data.set("batchCount", static_cast<int>(batches.size()));
            return result;
        }
        
        std::string oldValues = warehouseCellToJsonString(*currentCell);
        
        bool blockSuccess = cellRepository->blockCell(cellId);
        
        if (blockSuccess)
        {
            result.success = true;
            result.cellId = cellId;
            result.message = "Warehouse cell blocked successfully";
            
            result.cell = cellRepository->findById(cellId);
            
            database::models::AuditLog auditLog;
            auditLog.tableName = "warehouse_cells";
            auditLog.recordId = cellId;
            auditLog.action = database::models::AuditAction::UPDATE;
            auditLog.oldValues = oldValues;
            auditLog.newValues = warehouseCellToJsonString(*result.cell);
            auditLog.changedBy = blockedBy;
            auditLog.description = "Warehouse cell blocked";
            
            auditRepository->create(auditLog);
            
            result.data.set("previousStatus", database::models::WarehouseCell::statusToString(
                database::models::WarehouseCell::stringToStatus(oldValues)));
            result.data.set("newStatus", database::models::WarehouseCell::statusToString(result.cell->status));
        }
        else
        {
            result.success = false;
            result.message = "Failed to block warehouse cell";
        }
    }
    catch (const std::exception& e)
    {
        result.success = false;
        result.message = "Error blocking warehouse cell: " + std::string(e.what());
    }
    
    return result;
}

WarehouseCellServiceResult WarehouseCellService::unblockCell(long long cellId, long long unblockedBy)
{
    WarehouseCellServiceResult result;
    
    try
    {
        auto currentCell = cellRepository->findById(cellId);
        if (!currentCell)
        {
            result.success = false;
            result.message = "Warehouse cell not found with ID: " + std::to_string(cellId);
            return result;
        }
        
        if (currentCell->status != database::models::CellStatus::BLOCKED)
        {
            result.success = false;
            result.message = "Cell is not blocked";
            result.data.set("currentStatus", database::models::WarehouseCell::statusToString(currentCell->status));
            return result;
        }
        
        std::string oldValues = warehouseCellToJsonString(*currentCell);
        
        bool unblockSuccess = cellRepository->unblockCell(cellId);
        
        if (unblockSuccess)
        {
            result.success = true;
            result.cellId = cellId;
            result.message = "Warehouse cell unblocked successfully";
            
            result.cell = cellRepository->findById(cellId);
            
            database::models::AuditLog auditLog;
            auditLog.tableName = "warehouse_cells";
            auditLog.recordId = cellId;
            auditLog.action = database::models::AuditAction::UPDATE;
            auditLog.oldValues = oldValues;
            auditLog.newValues = warehouseCellToJsonString(*result.cell);
            auditLog.changedBy = unblockedBy;
            auditLog.description = "Warehouse cell unblocked";
            
            auditRepository->create(auditLog);
            
            result.data.set("newStatus", database::models::WarehouseCell::statusToString(result.cell->status));
        }
        else
        {
            result.success = false;
            result.message = "Failed to unblock warehouse cell";
        }
    }
    catch (const std::exception& e)
    {
        result.success = false;
        result.message = "Error unblocking warehouse cell: " + std::string(e.what());
    }
    
    return result;
}

WarehouseCellServiceResult WarehouseCellService::clearCell(long long cellId, long long clearedBy)
{
    WarehouseCellServiceResult result;
    
    try
    {
        auto currentCell = cellRepository->findById(cellId);
        if (!currentCell)
        {
            result.success = false;
            result.message = "Warehouse cell not found with ID: " + std::to_string(cellId);
            return result;
        }
        
        std::string oldValues = warehouseCellToJsonString(*currentCell);
        
        bool clearSuccess = cellRepository->clearCell(cellId);
        
        if (clearSuccess)
        {
            result.success = true;
            result.cellId = cellId;
            result.message = "Warehouse cell cleared successfully";
            
            result.cell = cellRepository->findById(cellId);
            
            database::models::AuditLog auditLog;
            auditLog.tableName = "warehouse_cells";
            auditLog.recordId = cellId;
            auditLog.action = database::models::AuditAction::UPDATE;
            auditLog.oldValues = oldValues;
            auditLog.newValues = warehouseCellToJsonString(*result.cell);
            auditLog.changedBy = clearedBy;
            auditLog.description = "Warehouse cell cleared of all batches";
            
            auditRepository->create(auditLog);
            
            result.data.set("previousOccupancy", currentCell->currentOccupancy);
            result.data.set("newOccupancy", 0.0);
            result.data.set("status", "empty");
        }
        else
        {
            result.success = false;
            result.message = "Failed to clear warehouse cell";
        }
    }
    catch (const std::exception& e)
    {
        result.success = false;
        result.message = "Error clearing warehouse cell: " + std::string(e.what());
    }
    
    return result;
}

WarehouseCellServiceResult WarehouseCellService::deleteCell(long long cellId, long long deletedBy)
{
    WarehouseCellServiceResult result;
    
    try
    {
        auto currentCell = cellRepository->findById(cellId);
        if (!currentCell)
        {
            result.success = false;
            result.message = "Warehouse cell not found with ID: " + std::to_string(cellId);
            return result;
        }
        
        std::string oldValues = warehouseCellToJsonString(*currentCell);
        
        if (currentCell->currentOccupancy > 0)
        {
            result.success = false;
            result.message = "Cannot delete non-empty cell. Clear it first or use block operation.";
            result.data.set("currentOccupancy", currentCell->currentOccupancy);
            return result;
        }
        
        bool deleteSuccess = cellRepository->remove(cellId);
        
        if (deleteSuccess)
        {
            result.success = true;
            result.cellId = cellId;
            result.message = "Warehouse cell deleted successfully";
            
            database::models::AuditLog auditLog;
            auditLog.tableName = "warehouse_cells";
            auditLog.recordId = cellId;
            auditLog.action = database::models::AuditAction::DELETE;
            auditLog.oldValues = oldValues;
            auditLog.changedBy = deletedBy;
            auditLog.description = "Warehouse cell deleted";
            
            auditRepository->create(auditLog);
            
            result.data.set("cellCode", currentCell->cellCode);
            result.data.set("zone", currentCell->zone);
        }
        else
        {
            result.success = false;
            result.message = "Failed to delete warehouse cell";
        }
    }
    catch (const std::exception& e)
    {
        result.success = false;
        result.message = "Error deleting warehouse cell: " + std::string(e.what());
    }
    
    return result;
}

Poco::JSON::Array WarehouseCellService::getAvailableCells(double requiredVolume, double requiredWeight)
{
    Poco::JSON::Array result;
    
    try
    {
        auto cells = cellRepository->findAvailableCells(requiredVolume, requiredWeight);
        
        for (auto& cell : cells)
        {
            Poco::JSON::Object cellObj;
            cellObj.set("id", static_cast<Poco::Int64>(cell->id));
            cellObj.set("cellCode", cell->cellCode);
            cellObj.set("zone", cell->zone);
            cellObj.set("rack", cell->rack);
            cellObj.set("shelf", cell->shelf);
            cellObj.set("position", cell->position);
            cellObj.set("maxVolume", cell->maxVolume);
            cellObj.set("maxWeight", cell->maxWeight);
            cellObj.set("currentOccupancy", cell->currentOccupancy);
            cellObj.set("status", database::models::WarehouseCell::statusToString(cell->status));
            cellObj.set("temperatureZone", database::models::WarehouseCell::temperatureZoneToString(cell->temperatureZone));
            cellObj.set("availableVolume", cell->getAvailableVolume());
            cellObj.set("availableWeight", cell->getAvailableWeight());
            
            double volumeUtilization = (cell->maxVolume > 0) ? 
                (100.0 - (cell->getAvailableVolume() / cell->maxVolume * 100.0)) : 0.0;
            double weightUtilization = (cell->maxWeight > 0) ? 
                (100.0 - (cell->getAvailableWeight() / cell->maxWeight * 100.0)) : 0.0;
            
            cellObj.set("volumeUtilization", volumeUtilization);
            cellObj.set("weightUtilization", weightUtilization);
            
            result.add(cellObj);
        }
    }
    catch (const std::exception& e)
    {
        Poco::JSON::Object error;
        error.set("error", "Error retrieving available cells: " + std::string(e.what()));
        result.add(error);
    }
    
    return result;
}

Poco::JSON::Array WarehouseCellService::getCellsByZone(const std::string& zone)
{
    Poco::JSON::Array result;
    
    try
    {
        auto cells = cellRepository->findByZone(zone);
        
        for (auto& cell : cells)
        {
            Poco::JSON::Object cellObj = cell->toJson();
            
            auto batches = batchRepository->findByStorageCell(cell->id);
            cellObj.set("batchCount", static_cast<int>(batches.size()));
            
            result.add(cellObj);
        }
    }
    catch (const std::exception& e)
    {
        Poco::JSON::Object error;
        error.set("error", "Error retrieving cells by zone: " + std::string(e.what()));
        result.add(error);
    }
    
    return result;
}

Poco::JSON::Array WarehouseCellService::getCellsByStatus(const std::string& status)
{
    Poco::JSON::Array result;
    
    try
    {
        auto cells = cellRepository->findByStatus(status);
        
        for (auto& cell : cells)
        {
            Poco::JSON::Object cellObj = cell->toJson();
            
            cellObj.set("availableVolume", cell->getAvailableVolume());
            cellObj.set("availableWeight", cell->getAvailableWeight());
            
            result.add(cellObj);
        }
    }
    catch (const std::exception& e)
    {
        Poco::JSON::Object error;
        error.set("error", "Error retrieving cells by status: " + std::string(e.what()));
        result.add(error);
    }
    
    return result;
}

Poco::JSON::Array WarehouseCellService::getCellBatches(long long cellId)
{
    Poco::JSON::Array result;
    
    try
    {
        auto batches = batchRepository->findByStorageCell(cellId);
        
        for (auto& batch : batches)
        {
            Poco::JSON::Object batchObj;
            batchObj.set("id", static_cast<Poco::Int64>(batch->id));
            batchObj.set("batchNumber", batch->batchNumber);
            batchObj.set("productName", batch->productName);
            batchObj.set("productSku", batch->productSku);
            batchObj.set("supplierName", batch->supplierName);
            batchObj.set("quantityReceived", batch->quantityReceived);
            batchObj.set("quantityAvailable", batch->quantityAvailable);
            batchObj.set("unitCost", batch->unitCost);

            if (!batch->manufactureDate.isNull())
                batchObj.set("manufactureDate", batch->manufactureDate.value());

            if (!batch->expirationDate.isNull())
                batchObj.set("expirationDate", batch->expirationDate.value());

            if (!batch->arrivalDate.isNull())
                batchObj.set("arrivalDate", batch->arrivalDate.value());

            batchObj.set("qualityStatus", database::models::ProductBatch::qualityStatusToString(batch->qualityStatus));

            if (!batch->invoiceNumber.isNull())
                batchObj.set("invoiceNumber", batch->invoiceNumber.value());
            
            double batchValue = batch->quantityAvailable * batch->unitCost;
            batchObj.set("totalValue", batchValue);
            
            if (!batch->expirationDate.isNull())
            {
                try
                {
                    auto expirationDate = utils::DateUtils::parseDate(batch->expirationDate);
                    auto currentDate = utils::DateUtils::now();
                    int daysUntilExpiration = utils::DateUtils::daysBetween(currentDate, expirationDate);
                    batchObj.set("daysUntilExpiration", daysUntilExpiration);
                }
                catch (...)
                {
                    batchObj.set("daysUntilExpiration", -1);
                }
            }
            
            result.add(batchObj);
        }
    }
    catch (const std::exception& e)
    {
        Poco::JSON::Object error;
        error.set("error", "Error retrieving cell batches: " + std::string(e.what()));
        result.add(error);
    }
    
    return result;
}

bool WarehouseCellService::validateCellData(const database::models::WarehouseCell& cell, std::string& errorMessage)
{
    if (cell.cellCode.empty())
    {
        errorMessage = "Cell code is required";
        return false;
    }
    
    if (cell.zone.empty())
    {
        errorMessage = "Zone is required";
        return false;
    }
    
    if (cell.rack.empty())
    {
        errorMessage = "Rack is required";
        return false;
    }
    
    if (cell.shelf.empty())
    {
        errorMessage = "Shelf is required";
        return false;
    }
    
    if (cell.position.empty())
    {
        errorMessage = "Position is required";
        return false;
    }
    
    if (cell.maxVolume <= 0)
    {
        errorMessage = "Maximum volume must be greater than 0";
        return false;
    }
    
    if (cell.maxWeight <= 0)
    {
        errorMessage = "Maximum weight must be greater than 0";
        return false;
    }
    
    if (cell.currentOccupancy < 0 || cell.currentOccupancy > 100)
    {
        errorMessage = "Current occupancy must be between 0 and 100";
        return false;
    }
    
    if (cell.cellCode.length() > 50)
    {
        errorMessage = "Cell code cannot exceed 50 characters";
        return false;
    }
    
    return true;
}

bool WarehouseCellService::canStoreInCell(long long cellId, double volume, double weight)
{
    try
    {
        auto cell = cellRepository->findById(cellId);
        if (!cell)
        {
            return false;
        }
        
        return cell->canStore(volume, weight);
    }
    catch (const std::exception&)
    {
        return false;
    }
}

WarehouseCellServiceResult WarehouseCellService::findBestCellForStorage(
    double volume, double weight, const std::string& temperatureZone)
{
    WarehouseCellServiceResult result;
    
    try
    {
        database::models::WarehouseCell bestCell;
        
        if (!temperatureZone.empty())
        {
            database::models::TemperatureZone tempZone = 
                database::models::WarehouseCell::stringToTemperatureZone(temperatureZone);
            
            auto availableCells = cellRepository->findAvailableCells(volume, weight);
            
            std::vector<database::models::WarehouseCell> filteredCells;
            for (const auto& cell : availableCells)
            {
                if (cell->temperatureZone == tempZone)
                {
                    filteredCells.push_back(*cell);
                }
            }
            
            if (!filteredCells.empty())
            {
                std::sort(filteredCells.begin(), filteredCells.end(),
                    [](const database::models::WarehouseCell& a, const database::models::WarehouseCell& b) {
                        return a.getAvailableVolume() < b.getAvailableVolume() &&
                               a.getAvailableWeight() < b.getAvailableWeight();
                    });
                
                bestCell = filteredCells[0];
            }
        }
        else
        {
            bestCell = cellRepository->findBestCellForStorage(volume, weight);
        }
        
        if (bestCell.id > 0)
        {
            result.success = true;
            result.cellId = bestCell.id;
            result.message = "Best cell found for storage";
            
            result.cell = std::make_unique<database::models::WarehouseCell>(bestCell);
            
            result.data.set("availableVolume", bestCell.getAvailableVolume());
            result.data.set("availableWeight", bestCell.getAvailableWeight());
            result.data.set("volumeUtilization", 100.0 - (bestCell.getAvailableVolume() / bestCell.maxVolume * 100.0));
            result.data.set("weightUtilization", 100.0 - (bestCell.getAvailableWeight() / bestCell.maxWeight * 100.0));
            
            bool canStore = bestCell.canStore(volume, weight);
            result.data.set("canStore", canStore);
            
            if (!canStore)
            {
                result.data.set("volumeDeficit", volume - bestCell.getAvailableVolume());
                result.data.set("weightDeficit", weight - bestCell.getAvailableWeight());
            }
        }
        else
        {
            result.success = false;
            result.message = "No suitable cell found for storage requirements";
            result.data.set("requiredVolume", volume);
            result.data.set("requiredWeight", weight);
            
            if (!temperatureZone.empty())
            {
                result.data.set("temperatureZone", temperatureZone);
            }
        }
    }
    catch (const std::exception& e)
    {
        result.success = false;
        result.message = "Error finding best cell for storage: " + std::string(e.what());
    }
    
    return result;
}

std::string WarehouseCellService::warehouseCellToJsonString(const database::models::WarehouseCell& warehouseCell) const
{
    try
    {
        auto json = warehouseCell.toJson();
        return utils::JsonUtils::objectToString(json, false);
    }
    catch (const std::exception&)
    {
        return "{}";
    }
}

} // namespace warehouse_backend::services
