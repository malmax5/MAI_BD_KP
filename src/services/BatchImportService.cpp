#include "BatchImportService.hpp"
#include "../database/ConnectionPool.hpp"
#include "../utils/JsonUtils.hpp"
#include "../utils/DateUtils.hpp"
#include <Poco/JSON/Parser.h>
#include <Poco/JSON/Stringifier.h>
#include <Poco/DateTimeFormatter.h>
#include <Poco/DateTimeFormat.h>
#include <Poco/RegularExpression.h>
#include <algorithm>
#include <sstream>

namespace warehouse_backend::services
{

ImportResult::ImportResult()
    : success(false), importedCount(0), failedCount(0)
{
}

Poco::JSON::Object ImportResult::toJson() const
{
    Poco::JSON::Object result;
    result.set("success", success);
    result.set("message", message);
    result.set("importedCount", importedCount);
    result.set("failedCount", failedCount);
    result.set("failedItems", failedItems);
    return result;
}

BatchImportService::BatchImportService()
    : productRepository(std::make_unique<database::repositories::ProductRepository>()),
      supplierRepository(std::make_unique<database::repositories::SupplierRepository>()),
      batchRepository(std::make_unique<database::repositories::ProductBatchRepository>())
{
}

ImportResult BatchImportService::importProducts(const Poco::JSON::Array& productsData, long long importedBy)
{
    ImportResult result;
    result.importedCount = 0;
    result.failedCount = 0;
    
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
        
        try
        {
            for (size_t i = 0; i < productsData.size(); ++i)
            {
                Poco::JSON::Object failedItem;
                failedItem.set("index", static_cast<int>(i));
                
                try
                {
                    Poco::Dynamic::Var productVar = productsData.get(i);
                    if (productVar.isEmpty())
                    {
                        throw std::runtime_error("Invalid product data format");
                    }

                    Poco::JSON::Object productJson = productVar.extract<Poco::JSON::Object>();
                    
                    if (!productJson.has("sku") || !productJson.has("name"))
                    {
                        throw std::runtime_error("Missing required fields (sku, name)");
                    }
                    
                    std::string sku = utils::JsonUtils::getString(productJson, "sku");
                    std::string name = utils::JsonUtils::getString(productJson, "name");
                    
                    auto existingProduct = productRepository->findBySku(sku);
                    if (existingProduct)
                    {
                        throw std::runtime_error("Product with SKU '" + sku + "' already exists");
                    }
                    
                    database::models::Product product;
                    product.sku = sku;
                    product.name = name;
                    product.description = utils::JsonUtils::getString(productJson, "description", "");
                    product.categoryId = productJson.has("categoryId") ? 
                        productJson.get("categoryId").convert<long long>() : 0;
                    product.supplierId = productJson.has("supplierId") ? 
                        productJson.get("supplierId").convert<long long>() : 0;
                    product.unitPrice = productJson.has("unitPrice") ? 
                        productJson.get("unitPrice").convert<double>() : 0.0;
                    product.weight = productJson.has("weight") ? 
                        productJson.get("weight").convert<double>() : 0.0;
                    product.dimensions = utils::JsonUtils::getString(productJson, "dimensions", "");
                    product.minStockLevel = productJson.has("minStockLevel") ? 
                        productJson.get("minStockLevel").convert<int>() : 0;
                    product.maxStockLevel = productJson.has("maxStockLevel") ? 
                        productJson.get("maxStockLevel").convert<int>() : 1000;
                    product.isActive = productJson.has("isActive") ? 
                        productJson.get("isActive").convert<bool>() : true;
                    
                    if (!product.validate())
                    {
                        throw std::runtime_error("Product validation failed");
                    }
                    
                    long long productId = productRepository->create(product);
                    
                    if (productId > 0)
                    {
                        result.importedCount++;
                        
                        if (importedBy > 0)
                        {
                            Poco::JSON::Object auditData;
                            auditData.set("action", "BATCH_IMPORT_PRODUCT");
                            auditData.set("importedBy", static_cast<Poco::Int64>(importedBy));
                            auditData.set("productId", static_cast<Poco::Int64>(productId));
                            auditData.set("sku", sku);
                        }
                    }
                    else
                    {
                        throw std::runtime_error("Failed to create product in database");
                    }
                }
                catch (const std::exception& e)
                {
                    result.failedCount++;
                    failedItem.set("error", std::string(e.what()));
                    result.failedItems.add(failedItem);
                    
                    continue;
                }
            }
            
            connection->commitTransaction();
            
            result.success = true;
            result.message = "Imported " + std::to_string(result.importedCount) + 
                            " products, failed " + std::to_string(result.failedCount);
        }
        catch (const std::exception& e)
        {
            connection->rollbackTransaction();
            throw;
        }
    }
    catch (const std::exception& e)
    {
        result.success = false;
        result.message = "Import failed: " + std::string(e.what());
    }
    
    return result;
}

ImportResult BatchImportService::importSuppliers(const Poco::JSON::Array& suppliersData, long long importedBy)
{
    ImportResult result;
    result.importedCount = 0;
    result.failedCount = 0;
    
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
        
        try
        {
            for (size_t i = 0; i < suppliersData.size(); ++i)
            {
                Poco::JSON::Object failedItem;
                failedItem.set("index", static_cast<int>(i));
                
                try
                {
                    Poco::Dynamic::Var supplierVar = suppliersData.get(i);
                    if (supplierVar.isEmpty())
                    {
                        throw std::runtime_error("Invalid supplier data format");
                    }

                    Poco::JSON::Object supplierJson = supplierVar.extract<Poco::JSON::Object>();
                    
                    if (!supplierJson.has("name"))
                    {
                        throw std::runtime_error("Missing required field: name");
                    }
                    
                    std::string name = utils::JsonUtils::getString(supplierJson, "name");
                    std::string taxId = utils::JsonUtils::getString(supplierJson, "taxId", "");
                    
                    if (!taxId.empty())
                    {
                        auto suppliers = supplierRepository->search(taxId, {"tax_id"});
                        if (!suppliers.empty())
                        {
                            throw std::runtime_error("Supplier with taxId '" + taxId + "' already exists");
                        }
                    }
                    
                    database::models::Supplier supplier;
                    supplier.name = name;
                    supplier.contactPerson = utils::JsonUtils::getString(supplierJson, "contactPerson", "");
                    supplier.email = utils::JsonUtils::getString(supplierJson, "email", "");
                    supplier.phone = utils::JsonUtils::getString(supplierJson, "phone", "");
                    supplier.address = utils::JsonUtils::getString(supplierJson, "address", "");
                    supplier.taxId = taxId;
                    supplier.paymentTerms = utils::JsonUtils::getString(supplierJson, "paymentTerms", "");
                    supplier.rating = supplierJson.has("rating") ? 
                        supplierJson.get("rating").convert<double>() : 0.0;
                    supplier.isActive = supplierJson.has("isActive") ? 
                        supplierJson.get("isActive").convert<bool>() : true;
                    
                    if (!supplier.email.isNull() && !utils::Validator::isValidEmail(supplier.email.value()))
                    {
                        throw std::runtime_error("Invalid email format");
                    }
                    
                    if (supplier.rating < 0 || supplier.rating > 5)
                    {
                        throw std::runtime_error("Rating must be between 0 and 5");
                    }
                    
                    long long supplierId = supplierRepository->create(supplier);
                    
                    if (supplierId > 0)
                    {
                        result.importedCount++;
                        
                        if (importedBy > 0)
                        {
                            Poco::JSON::Object auditData;
                            auditData.set("action", "BATCH_IMPORT_SUPPLIER");
                            auditData.set("importedBy", static_cast<Poco::Int64>(importedBy));
                            auditData.set("supplierId", static_cast<Poco::Int64>(supplierId));
                            auditData.set("name", name);
                        }
                    }
                    else
                    {
                        throw std::runtime_error("Failed to create supplier in database");
                    }
                }
                catch (const std::exception& e)
                {
                    result.failedCount++;
                    failedItem.set("error", std::string(e.what()));
                    result.failedItems.add(failedItem);
                    
                    continue;
                }
            }
            
            connection->commitTransaction();
            
            result.success = true;
            result.message = "Imported " + std::to_string(result.importedCount) + 
                            " suppliers, failed " + std::to_string(result.failedCount);
        }
        catch (const std::exception& e)
        {
            connection->rollbackTransaction();
            throw;
        }
    }
    catch (const std::exception& e)
    {
        result.success = false;
        result.message = "Import failed: " + std::string(e.what());
    }
    
    return result;
}

ImportResult BatchImportService::importBatches(const Poco::JSON::Array& batchesData, long long importedBy)
{
    ImportResult result;
    result.importedCount = 0;
    result.failedCount = 0;
    
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
        
        try
        {
            for (size_t i = 0; i < batchesData.size(); ++i)
            {
                Poco::JSON::Object failedItem;
                failedItem.set("index", static_cast<int>(i));
                
                try
                {
                    Poco::Dynamic::Var batchVar = batchesData.get(i);
                    if (batchVar.isEmpty())
                    {
                        throw std::runtime_error("Invalid batch data format");
                    }

                    Poco::JSON::Object batchJson = batchVar.extract<Poco::JSON::Object>();
                    
                    if (!batchJson.has("batchNumber") || !batchJson.has("productId") || 
                        !batchJson.has("supplierId") || !batchJson.has("quantityReceived") ||
                        !batchJson.has("unitCost"))
                    {
                        throw std::runtime_error("Missing required fields (batchNumber, productId, supplierId, quantityReceived, unitCost)");
                    }
                    
                    std::string batchNumber = utils::JsonUtils::getString(batchJson, "batchNumber");
                    long long productId = batchJson.get("productId").convert<long long>();
                    long long supplierId = batchJson.get("supplierId").convert<long long>();
                    int quantityReceived = batchJson.get("quantityReceived").convert<int>();
                    double unitCost = batchJson.get("unitCost").convert<double>();
                    
                    auto existingBatch = batchRepository->findByBatchNumber(batchNumber);
                    if (existingBatch)
                    {
                        throw std::runtime_error("Batch with number '" + batchNumber + "' already exists");
                    }
                    
                    auto product = productRepository->findById(productId);
                    if (!product)
                    {
                        throw std::runtime_error("Product with ID " + std::to_string(productId) + " not found");
                    }
                    
                    auto supplier = supplierRepository->findById(supplierId);
                    if (!supplier)
                    {
                        throw std::runtime_error("Supplier with ID " + std::to_string(supplierId) + " not found");
                    }
                    
                    database::models::ProductBatch batch;
                    batch.batchNumber = batchNumber;
                    batch.productId = productId;
                    batch.supplierId = supplierId;
                    batch.quantityReceived = quantityReceived;
                    batch.quantityAvailable = quantityReceived;
                    batch.unitCost = unitCost;
                    
                    if (batchJson.has("manufactureDate"))
                    {
                        std::string manufactureDateStr = utils::JsonUtils::getString(batchJson, "manufactureDate");
                        if (!manufactureDateStr.empty())
                        {
                            batch.manufactureDate = manufactureDateStr;
                        }
                    }
                    
                    if (batchJson.has("expirationDate"))
                    {
                        std::string expirationDateStr = utils::JsonUtils::getString(batchJson, "expirationDate");
                        if (!expirationDateStr.empty())
                        {
                            batch.expirationDate = expirationDateStr;
                        }
                    }
                    
                    batch.storageCellId = batchJson.has("storageCellId") ? 
                        batchJson.get("storageCellId").convert<long long>() : 0;
                    
                    if (batchJson.has("qualityStatus"))
                    {
                        std::string qualityStatusStr = utils::JsonUtils::getString(batchJson, "qualityStatus");
                        batch.qualityStatus = database::models::ProductBatch::stringToQualityStatus(qualityStatusStr);
                    }
                    else
                    {
                        batch.qualityStatus = database::models::QualityStatus::PENDING;
                    }
                    
                    batch.invoiceNumber = utils::JsonUtils::getString(batchJson, "invoiceNumber", "");
                    
                    if (!batch.validate())
                    {
                        throw std::runtime_error("Batch validation failed");
                    }
                    
                    if (batch.quantityAvailable > batch.quantityReceived)
                    {
                        throw std::runtime_error("Available quantity cannot be greater than received quantity");
                    }
                    
                    long long batchId = batchRepository->create(batch);
                    
                    if (batchId > 0)
                    {
                        result.importedCount++;
                        
                        if (importedBy > 0)
                        {
                            Poco::JSON::Object auditData;
                            auditData.set("action", "BATCH_IMPORT_BATCH");
                            auditData.set("importedBy", static_cast<Poco::Int64>(importedBy));
                            auditData.set("batchId", static_cast<Poco::Int64>(batchId));
                            auditData.set("batchNumber", batchNumber);
                            auditData.set("productId", static_cast<Poco::Int64>(productId));
                            auditData.set("supplierId", static_cast<Poco::Int64>(supplierId));
                        }
                    }
                    else
                    {
                        throw std::runtime_error("Failed to create batch in database");
                    }
                }
                catch (const std::exception& e)
                {
                    result.failedCount++;
                    failedItem.set("error", std::string(e.what()));
                    result.failedItems.add(failedItem);
                    
                    continue;
                }
            }
            
            connection->commitTransaction();
            
            result.success = true;
            result.message = "Imported " + std::to_string(result.importedCount) + 
                            " batches, failed " + std::to_string(result.failedCount);
        }
        catch (const std::exception& e)
        {
            connection->rollbackTransaction();
            throw;
        }
    }
    catch (const std::exception& e)
    {
        result.success = false;
        result.message = "Import failed: " + std::string(e.what());
    }
    
    return result;
}

} // namespace services
