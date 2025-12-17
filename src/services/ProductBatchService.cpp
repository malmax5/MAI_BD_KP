#include "ProductBatchService.hpp"
#include "../database/ConnectionPool.hpp"
#include "../utils/JsonUtils.hpp"
#include "../utils/Validator.hpp"
#include <Poco/JSON/Parser.h>
#include <Poco/JSON/Stringifier.h>
#include <Poco/DateTimeFormatter.h>
#include <Poco/DateTimeFormat.h>
#include <Poco/Random.h>
#include <Poco/NumberFormatter.h>
#include <Poco/FileStream.h>
#include <Poco/File.h>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <fstream>

namespace warehouse_backend::services
{

using namespace warehouse_backend::utils;
using namespace warehouse_backend::database;
using namespace warehouse_backend::database::models;

Poco::JSON::Object ProductBatchCreationResult::toJson() const
{
    Poco::JSON::Object result;
    result.set("success", success);
    result.set("message", message);
    result.set("batchId", static_cast<Poco::Int64>(batchId));
    result.set("batchNumber", batchNumber);
    
    if (batch)
    {
        result.set("batch", batch->toJson());
    }
    
    if (batchDetails.size())
    {
        result.set("batchDetails", batchDetails);
    }
    
    return result;
}

Poco::JSON::Object ProductBatchUpdateResult::toJson() const
{
    Poco::JSON::Object result;
    result.set("success", success);
    result.set("message", message);
    
    if (updatedBatch)
    {
        result.set("updatedBatch", updatedBatch->toJson());
    }
    
    Poco::JSON::Array changesArray;
    for (const auto& change : changes)
    {
        changesArray.add(change);
    }
    result.set("changes", changesArray);
    
    return result;
}

Poco::JSON::Object BatchQualityInspectionResult::toJson() const
{
    Poco::JSON::Object result;
    result.set("success", success);
    result.set("message", message);
    result.set("previousStatus", previousStatus);
    result.set("newStatus", newStatus);
    
    if (inspectedBatch)
    {
        result.set("inspectedBatch", inspectedBatch->toJson());
    }
    
    result.set("inspectionNotes", inspectionNotes);
    result.set("inspectionDate", DateUtils::formatDateTime(DateUtils::now()));
    
    return result;
}

Poco::JSON::Object BatchTransferResult::toJson() const
{
    Poco::JSON::Object result;
    result.set("success", success);
    result.set("message", message);
    
    if (transferredBatch)
    {
        result.set("transferredBatch", transferredBatch->toJson());
    }
    
    result.set("fromCellId", static_cast<Poco::Int64>(fromCellId));
    result.set("toCellId", static_cast<Poco::Int64>(toCellId));
    result.set("fromCellCode", fromCellCode);
    result.set("toCellCode", toCellCode);
    result.set("transferDate", DateUtils::formatDateTime(DateUtils::now()));
    
    return result;
}

ProductBatchService::ProductBatchService()
    : batchRepository(std::make_unique<database::repositories::ProductBatchRepository>()),
      productRepository(std::make_unique<database::repositories::ProductRepository>()),
      supplierRepository(std::make_unique<database::repositories::SupplierRepository>()),
      cellRepository(std::make_unique<database::repositories::WarehouseCellRepository>()),
      auditRepository(std::make_unique<database::repositories::AuditLogRepository>())
{
}

ProductBatchCreationResult ProductBatchService::createProductBatch(const ProductBatch& batch, long long createdBy)
{
    ProductBatchCreationResult result;
    
    std::string validationError;
    if (!validateBatchForCreation(batch, validationError))
    {
        result.success = false;
        result.message = "Invalid batch data: " + validationError;
        return result;
    }
    
    auto product = productRepository->findById(batch.productId);
    if (!product)
    {
        result.success = false;
        result.message = "Product not found with ID: " + std::to_string(batch.productId);
        return result;
    }
    
    auto supplier = supplierRepository->findById(batch.supplierId);
    if (!supplier)
    {
        result.success = false;
        result.message = "Supplier not found with ID: " + std::to_string(batch.supplierId);
        return result;
    }
    
    auto storageCell = cellRepository->findById(batch.storageCellId);
    if (!storageCell)
    {
        result.success = false;
        result.message = "Storage cell not found with ID: " + std::to_string(batch.storageCellId);
        return result;
    }
    
    if (storageCell->status == warehouse_backend::database::models::CellStatus::BLOCKED)
    {
        result.success = false;
        result.message = "Storage cell is blocked and cannot be used";
        return result;
    }
    
    if (!checkCellCapacity(batch.storageCellId, batch.productId, batch.quantityReceived))
    {
        result.success = false;
        result.message = "Storage cell does not have enough capacity for this batch";
        return result;
    }
    
    if (batchRepository->batchNumberExists(batch.batchNumber))
    {
        result.success = false;
        result.message = "Batch number already exists: " + batch.batchNumber;
        return result;
    }
    
    if (batch.quantityAvailable > batch.quantityReceived)
    {
        result.success = false;
        result.message = "Available quantity cannot exceed received quantity";
        return result;
    }
    
    auto connection = ConnectionPool::getInstance().acquireConnection();
    if (!connection)
    {
        result.success = false;
        result.message = "Failed to acquire database connection";
        return result;
    }
    
    try
    {
        connection->beginTransaction();
        
        ProductBatch batchToCreate = batch;
        if (batchToCreate.batchNumber.empty())
        {
            batchToCreate.batchNumber = generateBatchNumber(product->sku);
        }
        
        if (batchToCreate.arrivalDate.isNull() || batchToCreate.arrivalDate.value().empty())
        {
            batchToCreate.arrivalDate = DateUtils::formatDateTime(DateUtils::now());
        }
        
        if (!batchToCreate.validate())
        {
            connection->rollbackTransaction();
            result.success = false;
            result.message = "Batch validation failed";
            return result;
        }
        
        long long batchId = batchRepository->create(batchToCreate);
        
        if (batchId <= 0)
        {
            connection->rollbackTransaction();
            result.success = false;
            result.message = "Failed to create batch";
            return result;
        }
        
        auto createdBatch = batchRepository->findById(batchId);
        
        if (!createdBatch)
        {
            connection->rollbackTransaction();
            result.success = false;
            result.message = "Failed to retrieve created batch";
            return result;
        }
        
        logBatchAudit("CREATE", batchId, createdBy, 
                     "Batch created: " + createdBatch->batchNumber + 
                     " for product: " + product->name + 
                     " (" + product->sku + ")");
        
        connection->commitTransaction();
        
        result.success = true;
        result.batchId = batchId;
        result.batchNumber = batchToCreate.batchNumber;
        result.message = "Batch created successfully";
        result.batch = std::move(createdBatch);
        
        result.batchDetails.set("batchId", static_cast<Poco::Int64>(batchId));
        result.batchDetails.set("batchNumber", batchToCreate.batchNumber);
        result.batchDetails.set("productName", product->name);
        result.batchDetails.set("productSku", product->sku);
        result.batchDetails.set("supplierName", supplier->name);
        result.batchDetails.set("storageCellCode", storageCell->cellCode);
        result.batchDetails.set("quantityReceived", batchToCreate.quantityReceived);
        result.batchDetails.set("quantityAvailable", batchToCreate.quantityAvailable);
        result.batchDetails.set("unitCost", batchToCreate.unitCost);
        result.batchDetails.set("totalValue", batchToCreate.quantityAvailable * batchToCreate.unitCost);
        result.batchDetails.set("createdAt", batchToCreate.arrivalDate.value());
        
    }
    catch (const std::exception& e)
    {
        if (connection->isTransactionActive())
        {
            connection->rollbackTransaction();
        }
        result.success = false;
        result.message = "Error creating batch: " + std::string(e.what());
    }
    
    return result;
}

ProductBatchUpdateResult ProductBatchService::updateProductBatch(long long batchId, const ProductBatch& updatedBatch, long long updatedBy)
{
    ProductBatchUpdateResult result;
    
    auto connection = ConnectionPool::getInstance().acquireConnection();
    if (!connection)
    {
        result.success = false;
        result.message = "Failed to acquire database connection";
        return result;
    }
    
    try
    {
        connection->beginTransaction();
        
        auto existingBatch = batchRepository->findById(batchId);
        if (!existingBatch)
        {
            connection->rollbackTransaction();
            result.success = false;
            result.message = "Batch not found with ID: " + std::to_string(batchId);
            return result;
        }
        
        if (existingBatch->batchNumber != updatedBatch.batchNumber)
        {
            if (batchRepository->batchNumberExists(updatedBatch.batchNumber))
            {
                connection->rollbackTransaction();
                result.success = false;
                result.message = "Batch number already exists: " + updatedBatch.batchNumber;
                return result;
            }
            result.changes.push_back("Batch number changed from " + existingBatch->batchNumber + " to " + updatedBatch.batchNumber);
        }
        
        if (existingBatch->productId != updatedBatch.productId)
        {
            auto product = productRepository->findById(updatedBatch.productId);
            if (!product)
            {
                connection->rollbackTransaction();
                result.success = false;
                result.message = "Product not found with ID: " + std::to_string(updatedBatch.productId);
                return result;
            }
            result.changes.push_back("Product changed from ID " + std::to_string(existingBatch->productId) + 
                                    " to ID " + std::to_string(updatedBatch.productId));
        }
        
        if (existingBatch->supplierId != updatedBatch.supplierId)
        {
            auto supplier = supplierRepository->findById(updatedBatch.supplierId);
            if (!supplier)
            {
                connection->rollbackTransaction();
                result.success = false;
                result.message = "Supplier not found with ID: " + std::to_string(updatedBatch.supplierId);
                return result;
            }
            result.changes.push_back("Supplier changed from ID " + std::to_string(existingBatch->supplierId) + 
                                    " to ID " + std::to_string(updatedBatch.supplierId));
        }
        
        if (existingBatch->storageCellId != updatedBatch.storageCellId)
        {
            auto storageCell = cellRepository->findById(updatedBatch.storageCellId);
            if (!storageCell)
            {
                connection->rollbackTransaction();
                result.success = false;
                result.message = "Storage cell not found with ID: " + std::to_string(updatedBatch.storageCellId);
                return result;
            }
            
            if (storageCell->status == warehouse_backend::database::models::CellStatus::BLOCKED)
            {
                connection->rollbackTransaction();
                result.success = false;
                result.message = "Storage cell is blocked and cannot be used";
                return result;
            }
            
            if (!checkCellCapacity(updatedBatch.storageCellId, updatedBatch.productId, updatedBatch.quantityReceived))
            {
                connection->rollbackTransaction();
                result.success = false;
                result.message = "Storage cell does not have enough capacity for this batch";
                return result;
            }
            result.changes.push_back("Storage cell changed from ID " + std::to_string(existingBatch->storageCellId) + 
                                    " to ID " + std::to_string(updatedBatch.storageCellId));
        }
        
        if (existingBatch->quantityReceived != updatedBatch.quantityReceived)
        {
            if (updatedBatch.quantityReceived < existingBatch->quantityAvailable)
            {
                connection->rollbackTransaction();
                result.success = false;
                result.message = "Cannot reduce received quantity below available quantity";
                return result;
            }
            result.changes.push_back("Quantity received changed from " + std::to_string(existingBatch->quantityReceived) + 
                                    " to " + std::to_string(updatedBatch.quantityReceived));
        }
        
        if (existingBatch->quantityAvailable != updatedBatch.quantityAvailable)
        {
            if (updatedBatch.quantityAvailable > updatedBatch.quantityReceived)
            {
                connection->rollbackTransaction();
                result.success = false;
                result.message = "Available quantity cannot exceed received quantity";
                return result;
            }
            result.changes.push_back("Quantity available changed from " + std::to_string(existingBatch->quantityAvailable) + 
                                    " to " + std::to_string(updatedBatch.quantityAvailable));
        }
        
        if (existingBatch->unitCost != updatedBatch.unitCost)
        {
            result.changes.push_back("Unit cost changed from " + std::to_string(existingBatch->unitCost) + 
                                    " to " + std::to_string(updatedBatch.unitCost));
        }
        
        ProductBatch batchToUpdate = updatedBatch;
        batchToUpdate.id = batchId;
        
        if (!batchToUpdate.validate())
        {
            connection->rollbackTransaction();
            result.success = false;
            result.message = "Batch validation failed";
            return result;
        }
        
        bool updateSuccess = batchRepository->update(batchId, batchToUpdate);
        
        if (!updateSuccess)
        {
            connection->rollbackTransaction();
            result.success = false;
            result.message = "Failed to update batch";
            return result;
        }
        
        auto updatedBatchData = batchRepository->findById(batchId);
        
        logBatchAudit("UPDATE", batchId, updatedBy, 
                     "Batch updated: " + updatedBatchData->batchNumber + 
                     " (" + std::to_string(result.changes.size()) + " changes)");
        
        connection->commitTransaction();
        
        result.success = true;
        result.message = "Batch updated successfully";
        result.updatedBatch = std::move(updatedBatchData);
        
    }
    catch (const std::exception& e)
    {
        if (connection->isTransactionActive())
        {
            connection->rollbackTransaction();
        }
        result.success = false;
        result.message = "Error updating batch: " + std::string(e.what());
    }
    
    return result;
}

bool ProductBatchService::deleteProductBatch(long long batchId, long long deletedBy, const std::string& reason)
{
    auto connection = ConnectionPool::getInstance().acquireConnection();
    if (!connection)
    {
        return false;
    }
    
    try
    {
        connection->beginTransaction();
        
        auto batch = batchRepository->findById(batchId);
        if (!batch)
        {
            connection->rollbackTransaction();
            return false;
        }
        
        if (batch->quantityAvailable > 0)
        {
            connection->rollbackTransaction();
            return false;
        }
        
        bool deleteSuccess = batchRepository->remove(batchId);
        
        if (!deleteSuccess)
        {
            connection->rollbackTransaction();
            return false;
        }
        
        logBatchAudit("DELETE", batchId, deletedBy, 
                     "Batch deleted: " + batch->batchNumber + 
                     (reason.empty() ? "" : " Reason: " + reason));
        
        connection->commitTransaction();
        return true;
        
    }
    catch (const std::exception&)
    {
        if (connection->isTransactionActive())
        {
            connection->rollbackTransaction();
        }
        return false;
    }
}

std::shared_ptr<ProductBatch> ProductBatchService::getBatchById(long long batchId)
{
    try
    {
        return batchRepository->findById(batchId);
    }
    catch (const std::exception&)
    {
        return nullptr;
    }
}

std::shared_ptr<ProductBatch> ProductBatchService::getBatchByNumber(const std::string& batchNumber)
{
    try
    {
        return batchRepository->findByBatchNumber(batchNumber);
    }
    catch (const std::exception&)
    {
        return nullptr;
    }
}

Poco::JSON::Array ProductBatchService::getBatchesByProduct(long long productId, int page, int pageSize)
{
    Poco::JSON::Array result;
    
    try
    {
        auto batches = batchRepository->findByProduct(productId);
        
        int startIndex = (page - 1) * pageSize;
        int endIndex = std::min(static_cast<int>(batches.size()), startIndex + pageSize);
        
        for (int i = startIndex; i < endIndex && i < static_cast<int>(batches.size()); ++i)
        {
            if (batches[i])
            {
                result.add(batches[i]->toJson());
            }
        }
        
        if (batches.size() > 0)
        {
            Poco::JSON::Object metadata;
            metadata.set("totalBatches", static_cast<int>(batches.size()));
            metadata.set("page", page);
            metadata.set("pageSize", pageSize);
            metadata.set("totalPages", (batches.size() + pageSize - 1) / pageSize);
            metadata.set("productId", static_cast<Poco::Int64>(productId));
            result.add(metadata);
        }
        
    }
    catch (const std::exception& e)
    {
        Poco::JSON::Object error;
        error.set("error", "Error retrieving batches by product: " + std::string(e.what()));
        result.add(error);
    }
    
    return result;
}

Poco::JSON::Array ProductBatchService::getBatchesBySupplier(long long supplierId, int page, int pageSize)
{
    Poco::JSON::Array result;
    
    try
    {
        auto batches = batchRepository->findBySupplier(supplierId);
        
        int startIndex = (page - 1) * pageSize;
        int endIndex = std::min(static_cast<int>(batches.size()), startIndex + pageSize);
        
        for (int i = startIndex; i < endIndex && i < static_cast<int>(batches.size()); ++i)
        {
            if (batches[i])
            {
                result.add(batches[i]->toJson());
            }
        }
        
        if (batches.size() > 0)
        {
            Poco::JSON::Object metadata;
            metadata.set("totalBatches", static_cast<int>(batches.size()));
            metadata.set("page", page);
            metadata.set("pageSize", pageSize);
            metadata.set("totalPages", (batches.size() + pageSize - 1) / pageSize);
            metadata.set("supplierId", static_cast<Poco::Int64>(supplierId));
            result.add(metadata);
        }
        
    }
    catch (const std::exception& e)
    {
        Poco::JSON::Object error;
        error.set("error", "Error retrieving batches by supplier: " + std::string(e.what()));
        result.add(error);
    }
    
    return result;
}

Poco::JSON::Array ProductBatchService::getBatchesByStorageCell(long long cellId, int page, int pageSize)
{
    Poco::JSON::Array result;
    
    try
    {
        auto batches = batchRepository->findByStorageCell(cellId);
        
        int startIndex = (page - 1) * pageSize;
        int endIndex = std::min(static_cast<int>(batches.size()), startIndex + pageSize);
        
        for (int i = startIndex; i < endIndex && i < static_cast<int>(batches.size()); ++i)
        {
            if (batches[i])
            {
                result.add(batches[i]->toJson());
            }
        }
        
        if (batches.size() > 0)
        {
            Poco::JSON::Object metadata;
            metadata.set("totalBatches", static_cast<int>(batches.size()));
            metadata.set("page", page);
            metadata.set("pageSize", pageSize);
            metadata.set("totalPages", (batches.size() + pageSize - 1) / pageSize);
            metadata.set("storageCellId", static_cast<Poco::Int64>(cellId));
            result.add(metadata);
        }
        
    }
    catch (const std::exception& e)
    {
        Poco::JSON::Object error;
        error.set("error", "Error retrieving batches by storage cell: " + std::string(e.what()));
        result.add(error);
    }
    
    return result;
}

BatchQualityInspectionResult ProductBatchService::inspectBatch(long long batchId, const std::string& newStatus, 
                                                               long long inspectedBy, const std::string& notes)
{
    BatchQualityInspectionResult result;
    
    try
    {
        auto batch = batchRepository->findById(batchId);
        if (!batch)
        {
            result.success = false;
            result.message = "Batch not found with ID: " + std::to_string(batchId);
            return result;
        }
        
        if (batch->qualityStatus == ProductBatch::stringToQualityStatus(newStatus))
        {
            result.success = false;
            result.message = "Batch already has status: " + newStatus;
            return result;
        }
        
        if (batch->qualityStatus == QualityStatus::REJECTED && newStatus != "rejected")
        {
            result.success = false;
            result.message = "Cannot change status from rejected";
            return result;
        }
        
        std::string oldStatus = ProductBatch::qualityStatusToString(batch->qualityStatus);
        
        bool updateSuccess = batchRepository->updateQualityStatus(batchId, newStatus);
        
        if (updateSuccess)
        {
            result.success = true;
            result.message = "Batch inspection completed successfully";
            result.previousStatus = oldStatus;
            result.newStatus = newStatus;
            result.inspectedBatch = batchRepository->findById(batchId);
            result.inspectionNotes = notes;
            
            logBatchAudit("INSPECT", batchId, inspectedBy, 
                         "Quality status changed from " + oldStatus + " to " + newStatus + 
                         (notes.empty() ? "" : " Notes: " + notes));
        }
        else
        {
            result.success = false;
            result.message = "Failed to update batch quality status";
        }
    }
    catch (const std::exception& e)
    {
        result.success = false;
        result.message = "Error inspecting batch: " + std::string(e.what());
    }
    
    return result;
}

Poco::JSON::Array ProductBatchService::getBatchesNeedingInspection(int page, int pageSize)
{
    Poco::JSON::Array result;
    
    try
    {
        auto batches = batchRepository->findBatchesNeedingInspection();
        
        int startIndex = (page - 1) * pageSize;
        int endIndex = std::min(static_cast<int>(batches.size()), startIndex + pageSize);
        
        for (int i = startIndex; i < endIndex && i < static_cast<int>(batches.size()); ++i)
        {
            if (batches[i])
            {
                result.add(batches[i]->toJson());
            }
        }
        
        if (batches.size() > 0)
        {
            Poco::JSON::Object metadata;
            metadata.set("totalBatches", static_cast<int>(batches.size()));
            metadata.set("page", page);
            metadata.set("pageSize", pageSize);
            metadata.set("totalPages", (batches.size() + pageSize - 1) / pageSize);
            metadata.set("retrievedAt", DateUtils::formatDateTime(DateUtils::now()));
            result.add(metadata);
        }
        
    }
    catch (const std::exception& e)
    {
        Poco::JSON::Object error;
        error.set("error", "Error retrieving batches needing inspection: " + std::string(e.what()));
        result.add(error);
    }
    
    return result;
}

ProductBatchUpdateResult ProductBatchService::useBatchQuantity(long long batchId, int quantity, long long usedBy, 
                                                               const std::string& reason)
{
    ProductBatchUpdateResult result;
    
    auto connection = ConnectionPool::getInstance().acquireConnection();
    if (!connection)
    {
        result.success = false;
        result.message = "Failed to acquire database connection";
        return result;
    }
    
    try
    {
        connection->beginTransaction();
        
        auto batch = batchRepository->findById(batchId);
        if (!batch)
        {
            connection->rollbackTransaction();
            result.success = false;
            result.message = "Batch not found with ID: " + std::to_string(batchId);
            return result;
        }
        
        if (batch->qualityStatus != QualityStatus::APPROVED)
        {
            connection->rollbackTransaction();
            result.success = false;
            result.message = "Batch is not approved for use. Current status: " + 
                            ProductBatch::qualityStatusToString(batch->qualityStatus);
            return result;
        }
        
        if (batch->isExpired())
        {
            connection->rollbackTransaction();
            result.success = false;
            result.message = "Batch has expired and cannot be used";
            return result;
        }
        
        if (batch->quantityAvailable < quantity)
        {
            connection->rollbackTransaction();
            result.success = false;
            result.message = "Insufficient quantity available. Available: " + 
                            std::to_string(batch->quantityAvailable) + 
                            ", Requested: " + std::to_string(quantity);
            return result;
        }
        
        bool useSuccess = batchRepository->useQuantity(batchId, quantity);
        
        if (!useSuccess)
        {
            connection->rollbackTransaction();
            result.success = false;
            result.message = "Failed to use batch quantity";
            return result;
        }
        
        auto updatedBatch = batchRepository->findById(batchId);
        
        logBatchAudit("USE_QUANTITY", batchId, usedBy, 
                     "Used " + std::to_string(quantity) + " from batch " + batch->batchNumber + 
                     ". Remaining: " + std::to_string(updatedBatch->quantityAvailable) +
                     (reason.empty() ? "" : " Reason: " + reason));
        
        connection->commitTransaction();
        
        result.success = true;
        result.message = "Batch quantity used successfully";
        result.updatedBatch = std::move(updatedBatch);
        result.changes.push_back("Used quantity: " + std::to_string(quantity));
        result.changes.push_back("Remaining quantity: " + std::to_string(result.updatedBatch->quantityAvailable));
        
    }
    catch (const std::exception& e)
    {
        if (connection->isTransactionActive())
        {
            connection->rollbackTransaction();
        }
        result.success = false;
        result.message = "Error using batch quantity: " + std::string(e.what());
    }
    
    return result;
}

ProductBatchUpdateResult ProductBatchService::returnBatchQuantity(long long batchId, int quantity, long long returnedBy,
                                                                  const std::string& reason)
{
    ProductBatchUpdateResult result;
    
    auto connection = ConnectionPool::getInstance().acquireConnection();
    if (!connection)
    {
        result.success = false;
        result.message = "Failed to acquire database connection";
        return result;
    }
    
    try
    {
        connection->beginTransaction();
        
        auto batch = batchRepository->findById(batchId);
        if (!batch)
        {
            connection->rollbackTransaction();
            result.success = false;
            result.message = "Batch not found with ID: " + std::to_string(batchId);
            return result;
        }
        
        if (batch->qualityStatus != QualityStatus::APPROVED)
        {
            connection->rollbackTransaction();
            result.success = false;
            result.message = "Batch is not approved. Current status: " + 
                            ProductBatch::qualityStatusToString(batch->qualityStatus);
            return result;
        }
        
        if (batch->quantityAvailable + quantity > batch->quantityReceived)
        {
            connection->rollbackTransaction();
            result.success = false;
            result.message = "Returned quantity would exceed original received quantity";
            return result;
        }
        
        bool returnSuccess = batchRepository->returnQuantity(batchId, quantity);
        
        if (!returnSuccess)
        {
            connection->rollbackTransaction();
            result.success = false;
            result.message = "Failed to return batch quantity";
            return result;
        }
        
        auto updatedBatch = batchRepository->findById(batchId);
        
        logBatchAudit("RETURN_QUANTITY", batchId, returnedBy, 
                     "Returned " + std::to_string(quantity) + " to batch " + batch->batchNumber + 
                     ". New available: " + std::to_string(updatedBatch->quantityAvailable) +
                     (reason.empty() ? "" : " Reason: " + reason));
        
        connection->commitTransaction();
        
        result.success = true;
        result.message = "Batch quantity returned successfully";
        result.updatedBatch = std::move(updatedBatch);
        result.changes.push_back("Returned quantity: " + std::to_string(quantity));
        result.changes.push_back("New available quantity: " + std::to_string(result.updatedBatch->quantityAvailable));
        
    }
    catch (const std::exception& e)
    {
        if (connection->isTransactionActive())
        {
            connection->rollbackTransaction();
        }
        result.success = false;
        result.message = "Error returning batch quantity: " + std::string(e.what());
    }
    
    return result;
}

ProductBatchUpdateResult ProductBatchService::adjustBatchQuantity(long long batchId, int newQuantity, long long adjustedBy,
                                                                  const std::string& reason)
{
    ProductBatchUpdateResult result;
    
    auto connection = ConnectionPool::getInstance().acquireConnection();
    if (!connection)
    {
        result.success = false;
        result.message = "Failed to acquire database connection";
        return result;
    }
    
    try
    {
        connection->beginTransaction();
        
        auto batch = batchRepository->findById(batchId);
        if (!batch)
        {
            connection->rollbackTransaction();
            result.success = false;
            result.message = "Batch not found with ID: " + std::to_string(batchId);
            return result;
        }
        
        if (newQuantity > batch->quantityReceived)
        {
            connection->rollbackTransaction();
            result.success = false;
            result.message = "Adjusted quantity cannot exceed received quantity";
            return result;
        }
        
        int oldQuantity = batch->quantityAvailable;
        
        bool adjustSuccess = batchRepository->updateQuantity(batchId, batch->quantityReceived, newQuantity);
        
        if (!adjustSuccess)
        {
            connection->rollbackTransaction();
            result.success = false;
            result.message = "Failed to adjust batch quantity";
            return result;
        }
        
        auto updatedBatch = batchRepository->findById(batchId);
        
        logBatchAudit("ADJUST_QUANTITY", batchId, adjustedBy, 
                     "Adjusted quantity from " + std::to_string(oldQuantity) + " to " + 
                     std::to_string(newQuantity) + " for batch " + batch->batchNumber +
                     (reason.empty() ? "" : " Reason: " + reason));
        
        connection->commitTransaction();
        
        result.success = true;
        result.message = "Batch quantity adjusted successfully";
        result.updatedBatch = std::move(updatedBatch);
        result.changes.push_back("Quantity adjusted from " + std::to_string(oldQuantity) + 
                                 " to " + std::to_string(newQuantity));
        
    }
    catch (const std::exception& e)
    {
        if (connection->isTransactionActive())
        {
            connection->rollbackTransaction();
        }
        result.success = false;
        result.message = "Error adjusting batch quantity: " + std::string(e.what());
    }
    
    return result;
}

BatchTransferResult ProductBatchService::transferBatch(long long batchId, long long newCellId, long long transferredBy,
                                                       const std::string& reason)
{
    BatchTransferResult result;
    
    auto connection = ConnectionPool::getInstance().acquireConnection();
    if (!connection)
    {
        result.success = false;
        result.message = "Failed to acquire database connection";
        return result;
    }
    
    try
    {
        connection->beginTransaction();
        
        auto batch = batchRepository->findById(batchId);
        if (!batch)
        {
            connection->rollbackTransaction();
            result.success = false;
            result.message = "Batch not found with ID: " + std::to_string(batchId);
            return result;
        }
        
        if (batch->storageCellId == newCellId)
        {
            connection->rollbackTransaction();
            result.success = false;
            result.message = "Batch is already in the specified cell";
            return result;
        }
        
        auto newCell = cellRepository->findById(newCellId);
        if (!newCell)
        {
            connection->rollbackTransaction();
            result.success = false;
            result.message = "New storage cell not found with ID: " + std::to_string(newCellId);
            return result;
        }
        
        if (newCell->status == warehouse_backend::database::models::CellStatus::BLOCKED)
        {
            connection->rollbackTransaction();
            result.success = false;
            result.message = "New storage cell is blocked and cannot be used";
            return result;
        }
        
        if (!checkCellCapacity(newCellId, batch->productId, batch->quantityReceived))
        {
            connection->rollbackTransaction();
            result.success = false;
            result.message = "New storage cell does not have enough capacity for this batch";
            return result;
        }
        
        auto oldCell = cellRepository->findById(batch->storageCellId);
        
        bool transferSuccess = batchRepository->updateStorageCell(batchId, newCellId);
        
        if (!transferSuccess)
        {
            connection->rollbackTransaction();
            result.success = false;
            result.message = "Failed to transfer batch";
            return result;
        }
        
        auto transferredBatch = batchRepository->findById(batchId);
        
        logBatchAudit("TRANSFER", batchId, transferredBy, 
                     "Batch transferred from cell " + (oldCell ? oldCell->cellCode : "unknown") + 
                     " to cell " + newCell->cellCode +
                     (reason.empty() ? "" : " Reason: " + reason));
        
        connection->commitTransaction();
        
        result.success = true;
        result.message = "Batch transferred successfully";
        result.transferredBatch = std::move(transferredBatch);
        result.fromCellId = batch->storageCellId;
        result.toCellId = newCellId;
        result.fromCellCode = oldCell ? oldCell->cellCode : "";
        result.toCellCode = newCell->cellCode;
        
    }
    catch (const std::exception& e)
    {
        if (connection->isTransactionActive())
        {
            connection->rollbackTransaction();
        }
        result.success = false;
        result.message = "Error transferring batch: " + std::string(e.what());
    }
    
    return result;
}

Poco::JSON::Object ProductBatchService::findBestCellForBatch(long long batchId)
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
        if (!product)
        {
            result.set("error", "Product not found for batch");
            return result;
        }
        
        auto currentCell = cellRepository->findById(batch->storageCellId);
        if (!currentCell)
        {
            result.set("error", "Current storage cell not found");
            return result;
        }
        
        result.set("batchId", static_cast<Poco::Int64>(batchId));
        result.set("batchNumber", batch->batchNumber);
        result.set("currentCellId", static_cast<Poco::Int64>(batch->storageCellId));
        result.set("currentCellCode", currentCell->cellCode);
        
        Poco::JSON::Array alternativeCells;
        
        auto availableCells = cellRepository->findAvailableCells(0, 0);
        for (const auto& cell : availableCells)
        {
            if (cell->id == batch->storageCellId)
                continue;
                
            if (cell->status == warehouse_backend::database::models::CellStatus::BLOCKED)
                continue;
                
            Poco::JSON::Object cellInfo;
            cellInfo.set("cellId", static_cast<Poco::Int64>(cell->id));
            cellInfo.set("cellCode", cell->cellCode);
            cellInfo.set("zone", cell->zone);
            cellInfo.set("temperatureZone", cell->temperatureZone);
            cellInfo.set("status", cell->status);
            cellInfo.set("currentOccupancy", cell->currentOccupancy);
            cellInfo.set("maxVolume", cell->maxVolume);
            cellInfo.set("maxWeight", cell->maxWeight);
            
            alternativeCells.add(cellInfo);
        }
        
        result.set("alternativeCells", alternativeCells);
        result.set("searchTimestamp", DateUtils::formatDateTime(DateUtils::now()));
        
    }
    catch (const std::exception& e)
    {
        result.set("error", "Error finding best cell for batch: " + std::string(e.what()));
    }
    
    return result;
}

Poco::JSON::Object ProductBatchService::getBatchStatistics()
{
    Poco::JSON::Object stats;
    
    try
    {
        auto statistics = batchRepository->getBatchStatistics();
        
        if (statistics.size() > 0)
        {
            auto data = statistics.getObject(0);
            
            stats.set("totalBatches", data->get("total_batches"));
            stats.set("totalReceived", data->get("total_received"));
            stats.set("totalAvailable", data->get("total_available"));
            stats.set("totalValue", data->get("total_value"));
            stats.set("averageUnitCost", data->get("avg_unit_cost"));
            stats.set("approvedBatches", data->get("approved_batches"));
            stats.set("pendingBatches", data->get("pending_batches"));
            stats.set("quarantineBatches", data->get("quarantine_batches"));
            stats.set("rejectedBatches", data->get("rejected_batches"));
            stats.set("expiredBatches", data->get("expired_batches"));
            
            double utilizationRate = 0.0;
            if (data->get("total_received").convert<int>() > 0)
            {
                utilizationRate = (data->get("total_available").convert<double>() / 
                                   data->get("total_received").convert<double>()) * 100.0;
            }
            
            stats.set("stockUtilizationRate", utilizationRate);
            stats.set("calculatedAt", DateUtils::formatDateTime(DateUtils::now()));
        }
        
    }
    catch (const std::exception& e)
    {
        stats.set("error", "Error calculating batch statistics: " + std::string(e.what()));
    }
    
    return stats;
}

Poco::JSON::Array ProductBatchService::getExpiringBatchesReport(int daysThreshold)
{
    Poco::JSON::Array result;
    
    try
    {
        auto batches = batchRepository->findExpiringBatches(daysThreshold);
        
        for (const auto& batch : batches)
        {
            if (batch)
            {
                auto json = batch->toJson();
                
                int daysUntilExpiration = batch->daysUntilExpiration();
                json.set("days_until_expiration", daysUntilExpiration);
                json.set("expiration_priority", 
                         daysUntilExpiration <= 7 ? "HIGH" : 
                         daysUntilExpiration <= 14 ? "MEDIUM" : "LOW");
                
                result.add(json);
            }
        }
        
        if (batches.size() > 0)
        {
            Poco::JSON::Object summary;
            summary.set("totalBatches", static_cast<int>(batches.size()));
            summary.set("daysThreshold", daysThreshold);
            summary.set("reportGenerated", DateUtils::formatDateTime(DateUtils::now()));
            
            int highPriority = 0, mediumPriority = 0, lowPriority = 0;
            for (const auto& batch : batches)
            {
                if (batch)
                {
                    int days = batch->daysUntilExpiration();
                    if (days <= 7) highPriority++;
                    else if (days <= 14) mediumPriority++;
                    else lowPriority++;
                }
            }
            
            summary.set("highPriorityCount", highPriority);
            summary.set("mediumPriorityCount", mediumPriority);
            summary.set("lowPriorityCount", lowPriority);
            
            result.add(summary);
        }
        
    }
    catch (const std::exception& e)
    {
        Poco::JSON::Object error;
        error.set("error", "Error generating expiring batches report: " + std::string(e.what()));
        result.add(error);
    }
    
    return result;
}

Poco::JSON::Array ProductBatchService::getExpiredBatchesReport()
{
    Poco::JSON::Array result;
    
    try
    {
        auto batches = batchRepository->findExpiredBatches();
        
        for (const auto& batch : batches)
        {
            if (batch)
            {
                auto json = batch->toJson();
                
                int daysExpired = -batch->daysUntilExpiration();
                json.set("days_expired", daysExpired);
                json.set("expired_value", batch->quantityAvailable * batch->unitCost);
                
                result.add(json);
            }
        }
        
        if (batches.size() > 0)
        {
            Poco::JSON::Object summary;
            summary.set("totalExpiredBatches", static_cast<int>(batches.size()));
            
            int totalQuantity = 0;
            double totalValue = 0.0;
            
            for (const auto& batch : batches)
            {
                if (batch)
                {
                    totalQuantity += batch->quantityAvailable;
                    totalValue += batch->quantityAvailable * batch->unitCost;
                }
            }
            
            summary.set("totalExpiredQuantity", totalQuantity);
            summary.set("totalExpiredValue", totalValue);
            summary.set("reportGenerated", DateUtils::formatDateTime(DateUtils::now()));
            
            result.add(summary);
        }
        
    }
    catch (const std::exception& e)
    {
        Poco::JSON::Object error;
        error.set("error", "Error generating expired batches report: " + std::string(e.what()));
        result.add(error);
    }
    
    return result;
}

Poco::JSON::Array ProductBatchService::getQualityStatusReport()
{
    Poco::JSON::Array result;
    
    try
    {
        result = batchRepository->getQualityStatusReport();
        
        if (result.size() > 0)
        {
            int totalBatches = 0;
            double totalValue = 0.0;
            
            for (size_t i = 0; i < result.size(); ++i)
            {
                auto statusData = result.getObject(i);
                totalBatches += statusData->get("batch_count").convert<int>();
                totalValue += statusData->get("total_value").convert<double>();
            }
            
            Poco::JSON::Object summary;
            summary.set("totalBatches", totalBatches);
            summary.set("totalValue", totalValue);
            summary.set("reportGenerated", DateUtils::formatDateTime(DateUtils::now()));
            
            result.add(summary);
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

Poco::JSON::Array ProductBatchService::getSupplierBatchReport(long long supplierId)
{
    Poco::JSON::Array result;
    
    try
    {
        result = batchRepository->getSupplierBatchReport(supplierId);
        
        if (result.size() > 0)
        {
            auto supplier = supplierRepository->findById(supplierId);
            
            Poco::JSON::Object header;
            header.set("supplierId", static_cast<Poco::Int64>(supplierId));
            header.set("supplierName", supplier ? supplier->name : "Unknown");
            header.set("reportGenerated", DateUtils::formatDateTime(DateUtils::now()));
            
            int totalProducts = 0;
            int totalBatches = 0;
            int totalQuantity = 0;
            double totalValue = 0.0;
            
            for (size_t i = 0; i < result.size(); ++i)
            {
                auto productData = result.getObject(i);
                totalProducts++;
                totalBatches += productData->get("batch_count").convert<int>();
                totalQuantity += productData->get("total_available").convert<int>();
                totalValue += productData->get("total_value").convert<double>();
            }
            
            header.set("totalProducts", totalProducts);
            header.set("totalBatches", totalBatches);
            header.set("totalQuantity", totalQuantity);
            header.set("totalValue", totalValue);
            
            result.add(header);
        }
        
    }
    catch (const std::exception& e)
    {
        Poco::JSON::Object error;
        error.set("error", "Error generating supplier batch report: " + std::string(e.what()));
        result.add(error);
    }
    
    return result;
}

Poco::JSON::Array ProductBatchService::getProductStockSummary(long long productId)
{
    Poco::JSON::Array result;
    
    try
    {
        if (productId > 0)
        {
            auto batches = batchRepository->findByProduct(productId);
            
            int totalAvailable = 0;
            int totalReceived = 0;
            double totalValue = 0.0;
            
            for (const auto& batch : batches)
            {
                if (batch)
                {
                    Poco::JSON::Object batchSummary;
                    batchSummary.set("batchId", static_cast<Poco::Int64>(batch->id));
                    batchSummary.set("batchNumber", batch->batchNumber);
                    batchSummary.set("quantityAvailable", batch->quantityAvailable);
                    batchSummary.set("quantityReceived", batch->quantityReceived);
                    batchSummary.set("unitCost", batch->unitCost);
                    batchSummary.set("batchValue", batch->quantityAvailable * batch->unitCost);
                    batchSummary.set("qualityStatus", ProductBatch::qualityStatusToString(batch->qualityStatus));
                    batchSummary.set("expirationDate", batch->expirationDate.isNull() ? "" : batch->expirationDate.value());
                    batchSummary.set("daysUntilExpiration", batch->daysUntilExpiration());
                    
                    totalAvailable += batch->quantityAvailable;
                    totalReceived += batch->quantityReceived;
                    totalValue += batch->quantityAvailable * batch->unitCost;
                    
                    result.add(batchSummary);
                }
            }
            
            if (batches.size() > 0)
            {
                auto product = productRepository->findById(productId);
                
                Poco::JSON::Object summary;
                summary.set("productId", static_cast<Poco::Int64>(productId));
                summary.set("productName", product ? product->name : "Unknown");
                summary.set("productSku", product ? product->sku : "");
                summary.set("totalBatches", static_cast<int>(batches.size()));
                summary.set("totalAvailable", totalAvailable);
                summary.set("totalReceived", totalReceived);
                summary.set("totalValue", totalValue);
                summary.set("calculatedAt", DateUtils::formatDateTime(DateUtils::now()));
                
                result.add(summary);
            }
        }
        else
        {
            auto stockSummary = batchRepository->getProductStockSummary();
            
            for (const auto& [prodId, stock] : stockSummary)
            {
                auto product = productRepository->findById(prodId);
                if (product)
                {
                    Poco::JSON::Object productSummary;
                    productSummary.set("productId", static_cast<Poco::Int64>(prodId));
                    productSummary.set("productName", product->name);
                    productSummary.set("productSku", product->sku);
                    productSummary.set("currentStock", stock);
                    productSummary.set("minStockLevel", product->minStockLevel);
                    productSummary.set("maxStockLevel", product->maxStockLevel);
                    
                    std::string stockStatus = "NORMAL";
                    if (stock <= product->minStockLevel)
                        stockStatus = "CRITICAL";
                    else if (stock <= product->minStockLevel * 1.5)
                        stockStatus = "LOW";
                    else if (stock >= product->maxStockLevel * 0.9)
                        stockStatus = "HIGH";
                    
                    productSummary.set("stockStatus", stockStatus);
                    
                    result.add(productSummary);
                }
            }
            
            if (stockSummary.size() > 0)
            {
                Poco::JSON::Object overallSummary;
                overallSummary.set("totalProducts", static_cast<int>(stockSummary.size()));
                overallSummary.set("reportGenerated", DateUtils::formatDateTime(DateUtils::now()));
                
                result.add(overallSummary);
            }
        }
        
    }
    catch (const std::exception& e)
    {
        Poco::JSON::Object error;
        error.set("error", "Error generating product stock summary: " + std::string(e.what()));
        result.add(error);
    }
    
    return result;
}

Poco::JSON::Array ProductBatchService::getStockValueAnalysis()
{
    Poco::JSON::Array result;
    
    try
    {
        double totalBatchValue = batchRepository->getTotalBatchValue();
        
        Poco::JSON::Object totalValue;
        totalValue.set("analysisType", "TOTAL_STOCK_VALUE");
        totalValue.set("value", totalBatchValue);
        totalValue.set("currency", "USD");
        totalValue.set("calculatedAt", DateUtils::formatDateTime(DateUtils::now()));
        
        result.add(totalValue);
        
        auto batches = batchRepository->findAll();
        std::map<std::string, double> valueByStatus;
        std::map<std::string, int> countByStatus;
        
        for (const auto& batch : batches)
        {
            if (batch)
            {
                std::string status = ProductBatch::qualityStatusToString(batch->qualityStatus);
                double batchValue = batch->quantityAvailable * batch->unitCost;
                
                valueByStatus[status] += batchValue;
                countByStatus[status]++;
            }
        }
        
        for (const auto& [status, value] : valueByStatus)
        {
            Poco::JSON::Object statusValue;
            statusValue.set("analysisType", "VALUE_BY_STATUS");
            statusValue.set("status", status);
            statusValue.set("batchCount", countByStatus[status]);
            statusValue.set("value", value);
            statusValue.set("percentage", (value / totalBatchValue) * 100.0);
            
            result.add(statusValue);
        }
        
    }
    catch (const std::exception& e)
    {
        Poco::JSON::Object error;
        error.set("error", "Error generating stock value analysis: " + std::string(e.what()));
        result.add(error);
    }
    
    return result;
}

Poco::JSON::Array ProductBatchService::getBatchAgingReport()
{
    Poco::JSON::Array result;
    
    try
    {
        auto batches = batchRepository->findAll();
        
        std::map<std::string, int> agingCategories = {
            {"0-30 days", 0},
            {"31-90 days", 0},
            {"91-180 days", 0},
            {"181-365 days", 0},
            {"Over 1 year", 0}
        };
        
        std::map<std::string, double> valueByAge = {
            {"0-30 days", 0.0},
            {"31-90 days", 0.0},
            {"91-180 days", 0.0},
            {"181-365 days", 0.0},
            {"Over 1 year", 0.0}
        };
        
        auto now = DateUtils::now();
        
        for (const auto& batch : batches)
        {
            if (batch && batch->qualityStatus == QualityStatus::APPROVED && batch->quantityAvailable > 0)
            {
                try
                {
                    Poco::DateTime arrivalDate = DateUtils::parseDate(batch->arrivalDate);
                    int daysInStock = DateUtils::daysBetween(arrivalDate, now);
                    
                    double batchValue = batch->quantityAvailable * batch->unitCost;
                    
                    if (daysInStock <= 30)
                    {
                        agingCategories["0-30 days"]++;
                        valueByAge["0-30 days"] += batchValue;
                    }
                    else if (daysInStock <= 90)
                    {
                        agingCategories["31-90 days"]++;
                        valueByAge["31-90 days"] += batchValue;
                    }
                    else if (daysInStock <= 180)
                    {
                        agingCategories["91-180 days"]++;
                        valueByAge["91-180 days"] += batchValue;
                    }
                    else if (daysInStock <= 365)
                    {
                        agingCategories["181-365 days"]++;
                        valueByAge["181-365 days"] += batchValue;
                    }
                    else
                    {
                        agingCategories["Over 1 year"]++;
                        valueByAge["Over 1 year"] += batchValue;
                    }
                    
                    Poco::JSON::Object batchAging;
                    batchAging.set("batchId", static_cast<Poco::Int64>(batch->id));
                    batchAging.set("batchNumber", batch->batchNumber);
                    batchAging.set("productName", batch->productName);
                    batchAging.set("arrivalDate", batch->arrivalDate);
                    batchAging.set("daysInStock", daysInStock);
                    batchAging.set("quantityAvailable", batch->quantityAvailable);
                    batchAging.set("batchValue", batchValue);
                    batchAging.set("agingCategory", 
                        daysInStock <= 30 ? "0-30 days" :
                        daysInStock <= 90 ? "31-90 days" :
                        daysInStock <= 180 ? "91-180 days" :
                        daysInStock <= 365 ? "181-365 days" : "Over 1 year");
                    
                    result.add(batchAging);
                }
                catch (...)
                {
                    // Пропускаем партии с невалидными датами
                }
            }
        }
        
        Poco::JSON::Object summary;
        summary.set("totalBatchesAnalyzed", static_cast<int>(batches.size()));
        
        for (const auto& [category, count] : agingCategories)
        {
            summary.set("count_" + category, count);
            summary.set("value_" + category, valueByAge[category]);
        }
        
        summary.set("reportGenerated", DateUtils::formatDateTime(DateUtils::now()));
        result.add(summary);
        
    }
    catch (const std::exception& e)
    {
        Poco::JSON::Object error;
        error.set("error", "Error generating batch aging report: " + std::string(e.what()));
        result.add(error);
    }
    
    return result;
}

Poco::JSON::Array ProductBatchService::searchBatches(const std::string& query, int page, int pageSize)
{
    Poco::JSON::Array result;
    
    try
    {
        std::vector<std::string> searchFields = {"batch_number", "invoice_number"};
        auto batches = batchRepository->search(query, searchFields);
        
        int startIndex = (page - 1) * pageSize;
        int endIndex = std::min(static_cast<int>(batches.size()), startIndex + pageSize);
        
        for (int i = startIndex; i < endIndex && i < static_cast<int>(batches.size()); ++i)
        {
            if (batches[i])
            {
                result.add(batches[i]->toJson());
            }
        }
        
        if (batches.size() > 0)
        {
            Poco::JSON::Object metadata;
            metadata.set("totalResults", static_cast<int>(batches.size()));
            metadata.set("page", page);
            metadata.set("pageSize", pageSize);
            metadata.set("totalPages", (batches.size() + pageSize - 1) / pageSize);
            metadata.set("searchQuery", query);
            metadata.set("searchTimestamp", DateUtils::formatDateTime(DateUtils::now()));
            result.add(metadata);
        }
        
    }
    catch (const std::exception& e)
    {
        Poco::JSON::Object error;
        error.set("error", "Error searching batches: " + std::string(e.what()));
        result.add(error);
    }
    
    return result;
}

Poco::JSON::Array ProductBatchService::findAvailableBatchesForProduct(long long productId, int requiredQuantity)
{
    Poco::JSON::Array result;
    
    try
    {
        auto batches = batchRepository->findAvailableBatches(productId, requiredQuantity);
        
        for (const auto& batch : batches)
        {
            if (batch)
            {
                result.add(batch->toJson());
            }
        }
        
        if (batches.size() > 0)
        {
            Poco::JSON::Object summary;
            summary.set("productId", static_cast<Poco::Int64>(productId));
            summary.set("requiredQuantity", requiredQuantity);
            summary.set("availableBatches", static_cast<int>(batches.size()));
            
            int totalAvailable = 0;
            for (const auto& batch : batches)
            {
                if (batch)
                {
                    totalAvailable += batch->quantityAvailable;
                }
            }
            
            summary.set("totalAvailableQuantity", totalAvailable);
            summary.set("sufficientStock", totalAvailable >= requiredQuantity);
            summary.set("searchTimestamp", DateUtils::formatDateTime(DateUtils::now()));
            
            result.add(summary);
        }
        
    }
    catch (const std::exception& e)
    {
        Poco::JSON::Object error;
        error.set("error", "Error finding available batches: " + std::string(e.what()));
        result.add(error);
    }
    
    return result;
}

Poco::JSON::Array ProductBatchService::findBatchesForOrder(long long productId, int quantity)
{
    Poco::JSON::Array result;
    
    try
    {
        auto batches = batchRepository->findBatchesForOrderItem(productId, quantity);
        
        int allocatedQuantity = 0;
        int remainingQuantity = quantity;
        
        for (const auto& batch : batches)
        {
            int toAllocate = std::min(batch.quantityAvailable, remainingQuantity);
            
            Poco::JSON::Object allocation;
            allocation.set("batchId", static_cast<Poco::Int64>(batch.id));
            allocation.set("batchNumber", batch.batchNumber);
            allocation.set("quantityAvailable", batch.quantityAvailable);
            allocation.set("quantityToAllocate", toAllocate);
            allocation.set("unitCost", batch.unitCost);
            allocation.set("allocationValue", toAllocate * batch.unitCost);
            allocation.set("expirationDate", batch.expirationDate.isNull() ? "" : batch.expirationDate.value());
            allocation.set("daysUntilExpiration", batch.daysUntilExpiration());
            allocation.set("storageCellCode", batch.storageCellCode);
            
            allocatedQuantity += toAllocate;
            remainingQuantity -= toAllocate;
            
            result.add(allocation);
            
            if (remainingQuantity <= 0)
                break;
        }
        
        Poco::JSON::Object summary;
        summary.set("productId", static_cast<Poco::Int64>(productId));
        summary.set("requestedQuantity", quantity);
        summary.set("allocatedQuantity", allocatedQuantity);
        summary.set("remainingQuantity", remainingQuantity);
        summary.set("sufficientStock", remainingQuantity == 0);
        summary.set("batchesUsed", static_cast<int>(batches.size()));
        summary.set("allocationTimestamp", DateUtils::formatDateTime(DateUtils::now()));
        
        result.add(summary);
        
    }
    catch (const std::exception& e)
    {
        Poco::JSON::Object error;
        error.set("error", "Error finding batches for order: " + std::string(e.what()));
        result.add(error);
    }
    
    return result;
}

Poco::JSON::Array ProductBatchService::importBatches(const Poco::JSON::Array& batchesJson, long long importedBy)
{
    Poco::JSON::Array result;
    
    auto connection = ConnectionPool::getInstance().acquireConnection();
    if (!connection)
    {
        Poco::JSON::Object error;
        error.set("error", "Failed to acquire database connection");
        result.add(error);
        return result;
    }
    
    try
    {
        connection->beginTransaction();
        
        int successCount = 0;
        int failureCount = 0;
        int totalBatches = static_cast<int>(batchesJson.size());
        
        for (size_t i = 0; i < batchesJson.size(); ++i)
        {
            try
            {
                auto batchJson = batchesJson.getObject(i);
                ProductBatch batch(*batchJson);
                
                std::string validationError;
                if (!validateBatchForCreation(batch, validationError))
                {
                    Poco::JSON::Object failure;
                    failure.set("index", static_cast<int>(i));
                    failure.set("batchNumber", batch.batchNumber);
                    failure.set("status", "FAILED");
                    failure.set("error", "Validation failed: " + validationError);
                    result.add(failure);
                    failureCount++;
                    continue;
                }
                
                auto product = productRepository->findById(batch.productId);
                if (!product)
                {
                    Poco::JSON::Object failure;
                    failure.set("index", static_cast<int>(i));
                    failure.set("batchNumber", batch.batchNumber);
                    failure.set("status", "FAILED");
                    failure.set("error", "Product not found: " + std::to_string(batch.productId));
                    result.add(failure);
                    failureCount++;
                    continue;
                }
                
                auto supplier = supplierRepository->findById(batch.supplierId);
                if (!supplier)
                {
                    Poco::JSON::Object failure;
                    failure.set("index", static_cast<int>(i));
                    failure.set("batchNumber", batch.batchNumber);
                    failure.set("status", "FAILED");
                    failure.set("error", "Supplier not found: " + std::to_string(batch.supplierId));
                    result.add(failure);
                    failureCount++;
                    continue;
                }
                
                if (batchRepository->batchNumberExists(batch.batchNumber))
                {
                    Poco::JSON::Object failure;
                    failure.set("index", static_cast<int>(i));
                    failure.set("batchNumber", batch.batchNumber);
                    failure.set("status", "FAILED");
                    failure.set("error", "Batch number already exists");
                    result.add(failure);
                    failureCount++;
                    continue;
                }
                
                if (batch.arrivalDate.isNull() || batch.arrivalDate.value().empty())
                {
                    batch.arrivalDate = DateUtils::formatDateTime(DateUtils::now());
                }
                
                long long batchId = batchRepository->create(batch);
                
                if (batchId > 0)
                {
                    logBatchAudit("IMPORT", batchId, importedBy, 
                                 "Batch imported: " + batch.batchNumber);
                    
                    Poco::JSON::Object success;
                    success.set("index", static_cast<int>(i));
                    success.set("batchId", static_cast<Poco::Int64>(batchId));
                    success.set("batchNumber", batch.batchNumber);
                    success.set("status", "SUCCESS");
                    result.add(success);
                    successCount++;
                }
                else
                {
                    Poco::JSON::Object failure;
                    failure.set("index", static_cast<int>(i));
                    failure.set("batchNumber", batch.batchNumber);
                    failure.set("status", "FAILED");
                    failure.set("error", "Failed to create batch in database");
                    result.add(failure);
                    failureCount++;
                }
            }
            catch (const std::exception& e)
            {
                Poco::JSON::Object failure;
                failure.set("index", static_cast<int>(i));
                failure.set("status", "FAILED");
                failure.set("error", std::string(e.what()));
                result.add(failure);
                failureCount++;
            }
        }
        
        connection->commitTransaction();
        
        Poco::JSON::Object summary;
        summary.set("totalBatches", totalBatches);
        summary.set("successCount", successCount);
        summary.set("failureCount", failureCount);
        summary.set("successRate", totalBatches > 0 ? (successCount * 100.0 / totalBatches) : 0.0);
        summary.set("importedBy", static_cast<Poco::Int64>(importedBy));
        summary.set("importTimestamp", DateUtils::formatDateTime(DateUtils::now()));
        
        result.add(summary);
        
    }
    catch (const std::exception& e)
    {
        if (connection->isTransactionActive())
        {
            connection->rollbackTransaction();
        }
        
        Poco::JSON::Object error;
        error.set("error", "Error importing batches: " + std::string(e.what()));
        result.add(error);
    }
    
    return result;
}

bool ProductBatchService::exportBatchesToJson(const std::string& filePath, const std::vector<long long>& batchIds)
{
    try
    {
        Poco::JSON::Array batchesArray;
        
        if (batchIds.empty())
        {
            auto allBatches = batchRepository->findAll();
            for (const auto& batch : allBatches)
            {
                if (batch)
                {
                    batchesArray.add(batch->toJson());
                }
            }
        }
        else
        {
            for (long long batchId : batchIds)
            {
                auto batch = batchRepository->findById(batchId);
                if (batch)
                {
                    batchesArray.add(batch->toJson());
                }
            }
        }
        
        Poco::JSON::Object exportData;
        exportData.set("exportTimestamp", DateUtils::formatDateTime(DateUtils::now()));
        exportData.set("totalBatches", static_cast<int>(batchesArray.size()));
        exportData.set("batches", batchesArray);
        
        std::ofstream file(filePath);
        if (!file.is_open())
        {
            return false;
        }
        
        Poco::JSON::Stringifier::stringify(exportData, file, 2);
        file.close();
        
        return true;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

bool ProductBatchService::validateBatchForCreation(const ProductBatch& batch, std::string& errorMessage)
{
    if (!batch.validate())
    {
        errorMessage = "Basic validation failed";
        return false;
    }
    
    if (batch.batchNumber.empty() || batch.batchNumber.length() > 100)
    {
        errorMessage = "Batch number must be between 1 and 100 characters";
        return false;
    }
    
    if (batch.productId <= 0)
    {
        errorMessage = "Invalid product ID";
        return false;
    }
    
    if (batch.supplierId <= 0)
    {
        errorMessage = "Invalid supplier ID";
        return false;
    }
    
    if (batch.storageCellId <= 0)
    {
        errorMessage = "Invalid storage cell ID";
        return false;
    }
    
    if (batch.quantityReceived <= 0)
    {
        errorMessage = "Received quantity must be greater than 0";
        return false;
    }
    
    if (batch.quantityAvailable < 0)
    {
        errorMessage = "Available quantity cannot be negative";
        return false;
    }
    
    if (batch.quantityAvailable > batch.quantityReceived)
    {
        errorMessage = "Available quantity cannot exceed received quantity";
        return false;
    }
    
    if (batch.unitCost < 0)
    {
        errorMessage = "Unit cost cannot be negative";
        return false;
    }
    
    if (!batch.manufactureDate.isNull() && !Validator::isValidDate(batch.manufactureDate.value()))
    {
        errorMessage = "Invalid manufacture date format";
        return false;
    }
    
    if (!batch.expirationDate.isNull() && !Validator::isValidDate(batch.expirationDate.value()))
    {
        errorMessage = "Invalid expiration date format";
        return false;
    }
    
    if (!batch.manufactureDate.isNull() && !batch.expirationDate.isNull())
    {
        try
        {
            Poco::DateTime manufacture = DateUtils::parseDate(batch.manufactureDate);
            Poco::DateTime expiration = DateUtils::parseDate(batch.expirationDate);
            
            if (expiration <= manufacture)
            {
                errorMessage = "Expiration date must be after manufacture date";
                return false;
            }
        }
        catch (...)
        {
            errorMessage = "Invalid date format";
            return false;
        }
    }
    
    return true;
}

bool ProductBatchService::canBatchBeUsed(long long batchId, int quantity)
{
    auto batch = batchRepository->findById(batchId);
    if (!batch)
    {
        return false;
    }
    
    return batch->canBeUsed(quantity);
}

bool ProductBatchService::isBatchExpired(long long batchId)
{
    auto batch = batchRepository->findById(batchId);
    if (!batch)
    {
        return false;
    }
    
    return batch->isExpired();
}

int ProductBatchService::getDaysUntilExpiration(long long batchId)
{
    auto batch = batchRepository->findById(batchId);
    if (!batch)
    {
        return INT_MAX;
    }
    
    return batch->daysUntilExpiration();
}

double ProductBatchService::calculateBatchValue(long long batchId)
{
    auto batch = batchRepository->findById(batchId);
    if (!batch)
    {
        return 0.0;
    }
    
    return batch->quantityAvailable * batch->unitCost;
}

Poco::JSON::Object ProductBatchService::getBatchDetails(long long batchId)
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
        
        result = batch->toJson();
        
        auto product = productRepository->findById(batch->productId);
        if (product)
        {
            result.set("productDetails", product->toJson());
        }
        
        auto supplier = supplierRepository->findById(batch->supplierId);
        if (supplier)
        {
            result.set("supplierDetails", supplier->toJson());
        }
        
        auto cell = cellRepository->findById(batch->storageCellId);
        if (cell)
        {
            result.set("storageCellDetails", cell->toJson());
        }
        
        result.set("retrievedAt", DateUtils::formatDateTime(DateUtils::now()));
        
    }
    catch (const std::exception& e)
    {
        result.set("error", "Error retrieving batch details: " + std::string(e.what()));
    }
    
    return result;
}

std::string ProductBatchService::generateBatchNumber(const std::string& productSku)
{
    Poco::Random rng;
    rng.seed();
    
    auto now = DateUtils::now();
    std::string datePart = Poco::DateTimeFormatter::format(now, "%Y%m%d");
    
    int randomPart = rng.next(9999);
    
    std::string prefix = "BATCH";
    if (!productSku.empty())
    {
        prefix = productSku.substr(0, 3);
    }
    
    return prefix + "-" + datePart + "-" + std::to_string(randomPart);
}

bool ProductBatchService::checkCellCapacity(long long cellId, long long productId, int quantity)
{
    try
    {
        auto cell = cellRepository->findById(cellId);
        if (!cell)
        {
            return false;
        }
        
        if (cell->status == warehouse_backend::database::models::CellStatus::FULL)
        {
            return false;
        }
        
        auto product = productRepository->findById(productId);
        if (!product)
        {
            return false;
        }
        
        double estimatedWeight = 0.0;
        double estimatedVolume = 0.0;
        
        if (product->weight > 0)
        {
            estimatedWeight = product->weight * quantity;
        }
        
        if (!product->dimensions.isNull())
        {
            // Упрощенный расчет объема
            estimatedVolume = 0.01 * quantity; // Заглушка
        }
        
        return cell->maxWeight >= estimatedWeight && cell->maxVolume >= estimatedVolume;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

void ProductBatchService::logBatchAudit(const std::string& action, long long batchId, long long userId, 
                                       const std::string& details, const std::string& ipAddress)
{
    try
    {
        AuditLog auditLog;
        auditLog.tableName = "product_batches";
        auditLog.recordId = batchId;
        auditLog.action = AuditAction::UPDATE;
        auditLog.changedBy = userId;
        auditLog.description = "Batch " + action + ": " + details;
        auditLog.ipAddress = ipAddress;
        auditLog.userAgent = "ProductBatchService";
        
        auditRepository->create(auditLog);
    }
    catch (const std::exception&)
    {
        // Игнорируем ошибки логирования
    }
}

void ProductBatchService::enrichBatchWithDetails(ProductBatch& batch)
{
    if (batch.productId > 0 && batch.productName.empty())
    {
        auto product = productRepository->findById(batch.productId);
        if (product)
        {
            batch.productName = product->name;
            batch.productSku = product->sku;
        }
    }
    
    if (batch.supplierId > 0 && batch.supplierName.empty())
    {
        auto supplier = supplierRepository->findById(batch.supplierId);
        if (supplier)
        {
            batch.supplierName = supplier->name;
        }
    }
    
    if (batch.storageCellId > 0 && batch.storageCellCode.empty())
    {
        auto cell = cellRepository->findById(batch.storageCellId);
        if (cell)
        {
            batch.storageCellCode = cell->cellCode;
        }
    }
}

Poco::JSON::Object ProductBatchService::convertBatchToJson(const ProductBatch& batch, bool includeDetails)
{
    Poco::JSON::Object json = batch.toJson();
    
    if (includeDetails)
    {
        if (batch.productId > 0)
        {
            auto product = productRepository->findById(batch.productId);
            if (product)
            {
                json.set("productDetails", product->toJson());
            }
        }
        
        if (batch.supplierId > 0)
        {
            auto supplier = supplierRepository->findById(batch.supplierId);
            if (supplier)
            {
                json.set("supplierDetails", supplier->toJson());
            }
        }
    }
    
    return json;
}

} // namespace warehouse_backend::services
