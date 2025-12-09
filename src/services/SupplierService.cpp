#include "SupplierService.hpp"
#include "../database/ConnectionPool.hpp"
#include <Poco/JSON/Parser.h>
#include <Poco/JSON/Stringifier.h>
#include <Poco/DateTimeFormatter.h>
#include <Poco/DateTimeFormat.h>
#include <algorithm>
#include <sstream>

namespace warehouse_backend::services
{

SupplierServiceResult::SupplierServiceResult()
    : success(false), supplierId(0), supplier(nullptr)
{
    
}

Poco::JSON::Object SupplierServiceResult::toJson() const
{
    Poco::JSON::Object result;
    result.set("success", success);
    result.set("message", message);
    result.set("supplierId", static_cast<Poco::Int64>(supplierId));
    
    if (supplier)
    {
        result.set("supplier", supplier->toJson());
    }
    
    if (!data.size())
    {
        result.set("data", data);
    }
    
    return result;
}

SupplierService::SupplierService()
    : supplierRepository(std::make_unique<database::repositories::SupplierRepository>()),
      productRepository(std::make_unique<database::repositories::ProductRepository>()),
      batchRepository(std::make_unique<database::repositories::ProductBatchRepository>()),
      auditRepository(std::make_unique<database::repositories::AuditLogRepository>())
{
}

SupplierServiceResult SupplierService::createSupplier(const database::models::Supplier& supplier, long long createdBy)
{
    SupplierServiceResult result;
    
    std::string validationError;
    if (!validateSupplierData(supplier, validationError))
    {
        result.success = false;
        result.message = "Validation failed: " + validationError;
        return result;
    }
    
    if (!isTaxIdAvailable(supplier.taxId))
    {
        result.success = false;
        result.message = "Tax ID '" + supplier.taxId + "' is already registered";
        return result;
    }
    
    try
    {
        long long supplierId = supplierRepository->create(supplier);
        
        if (supplierId > 0)
        {
            result.success = true;
            result.supplierId = supplierId;
            result.message = "Supplier created successfully";
            
            result.supplier = supplierRepository->findById(supplierId);
            
            logAuditEvent("suppliers", supplierId, 
                         database::models::AuditAction::INSERT,
                         "", 
                         supplierToJsonString(*result.supplier),
                         createdBy,
                         "Supplier created");
        }
        else
        {
            result.success = false;
            result.message = "Failed to create supplier";
        }
    }
    catch (const std::exception& e)
    {
        result.success = false;
        result.message = "Error creating supplier: " + std::string(e.what());
    }
    
    return result;
}

SupplierServiceResult SupplierService::getSupplierById(long long supplierId)
{
    SupplierServiceResult result;
    
    try
    {
        auto supplier = supplierRepository->findById(supplierId);
        if (supplier)
        {
            result.success = true;
            result.supplierId = supplierId;
            result.message = "Supplier found";
            result.supplier = enrichSupplierWithDetails(std::move(supplier));
        }
        else
        {
            result.success = false;
            result.message = "Supplier not found with ID: " + std::to_string(supplierId);
        }
    }
    catch (const std::exception& e)
    {
        result.success = false;
        result.message = "Error retrieving supplier: " + std::string(e.what());
    }
    
    return result;
}

SupplierServiceResult SupplierService::getSupplierByTaxId(const std::string& taxId)
{
    SupplierServiceResult result;
    
    try
    {
        auto suppliers = supplierRepository->search(taxId, {"tax_id"});
        if (!suppliers.empty())
        {
            result.success = true;
            result.supplierId = suppliers[0]->id;
            result.message = "Supplier found";
            result.supplier = enrichSupplierWithDetails(std::move(suppliers[0]));
        }
        else
        {
            result.success = false;
            result.message = "Supplier not found with Tax ID: " + taxId;
        }
    }
    catch (const std::exception& e)
    {
        result.success = false;
        result.message = "Error retrieving supplier: " + std::string(e.what());
    }
    
    return result;
}

SupplierServiceResult SupplierService::updateSupplier(long long supplierId, 
                                                     const database::models::Supplier& updatedSupplier, 
                                                     long long updatedBy)
{
    SupplierServiceResult result;
    
    try
    {
        auto currentSupplier = supplierRepository->findById(supplierId);
        if (!currentSupplier)
        {
            result.success = false;
            result.message = "Supplier not found with ID: " + std::to_string(supplierId);
            return result;
        }
        
        std::string validationError;
        if (!validateSupplierData(updatedSupplier, validationError))
        {
            result.success = false;
            result.message = "Validation failed: " + validationError;
            return result;
        }
        
        if (currentSupplier->taxId != updatedSupplier.taxId && 
            !isTaxIdAvailable(updatedSupplier.taxId, supplierId))
        {
            result.success = false;
            result.message = "Tax ID '" + updatedSupplier.taxId + "' is already registered";
            return result;
        }
        
        std::string oldValues = supplierToJsonString(*currentSupplier);
        
        bool updateSuccess = supplierRepository->update(supplierId, updatedSupplier);
        
        if (updateSuccess)
        {
            result.success = true;
            result.supplierId = supplierId;
            result.message = "Supplier updated successfully";
            
            result.supplier = supplierRepository->findById(supplierId);
            
            std::string newValues = supplierToJsonString(*result.supplier);
            logAuditEvent("suppliers", supplierId, 
                         database::models::AuditAction::UPDATE,
                         oldValues, 
                         newValues,
                         updatedBy,
                         "Supplier updated");
        }
        else
        {
            result.success = false;
            result.message = "Failed to update supplier";
        }
    }
    catch (const std::exception& e)
    {
        result.success = false;
        result.message = "Error updating supplier: " + std::string(e.what());
    }
    
    return result;
}

SupplierServiceResult SupplierService::deactivateSupplier(long long supplierId, long long deactivatedBy)
{
    SupplierServiceResult result;
    
    try
    {
        auto currentSupplier = supplierRepository->findById(supplierId);
        if (!currentSupplier)
        {
            result.success = false;
            result.message = "Supplier not found with ID: " + std::to_string(supplierId);
            return result;
        }
        
        if (!currentSupplier->isActive)
        {
            result.success = false;
            result.message = "Supplier is already deactivated";
            return result;
        }
        
        std::string oldValues = supplierToJsonString(*currentSupplier);
        
        database::models::Supplier updatedSupplier = *currentSupplier;
        updatedSupplier.isActive = false;
        
        bool updateSuccess = supplierRepository->update(supplierId, updatedSupplier);
        
        if (updateSuccess)
        {
            result.success = true;
            result.supplierId = supplierId;
            result.message = "Supplier deactivated successfully";
            
            result.supplier = supplierRepository->findById(supplierId);
            
            std::string newValues = supplierToJsonString(*result.supplier);
            logAuditEvent("suppliers", supplierId, 
                         database::models::AuditAction::UPDATE,
                         oldValues, 
                         newValues,
                         deactivatedBy,
                         "Supplier deactivated");
        }
        else
        {
            result.success = false;
            result.message = "Failed to deactivate supplier";
        }
    }
    catch (const std::exception& e)
    {
        result.success = false;
        result.message = "Error deactivating supplier: " + std::string(e.what());
    }
    
    return result;
}

SupplierServiceResult SupplierService::activateSupplier(long long supplierId, long long activatedBy)
{
    SupplierServiceResult result;
    
    try
    {
        auto currentSupplier = supplierRepository->findById(supplierId);
        if (!currentSupplier)
        {
            result.success = false;
            result.message = "Supplier not found with ID: " + std::to_string(supplierId);
            return result;
        }
        
        if (currentSupplier->isActive)
        {
            result.success = false;
            result.message = "Supplier is already active";
            return result;
        }
        
        std::string oldValues = supplierToJsonString(*currentSupplier);
        
        database::models::Supplier updatedSupplier = *currentSupplier;
        updatedSupplier.isActive = true;
        
        bool updateSuccess = supplierRepository->update(supplierId, updatedSupplier);
        
        if (updateSuccess)
        {
            result.success = true;
            result.supplierId = supplierId;
            result.message = "Supplier activated successfully";
            
            result.supplier = supplierRepository->findById(supplierId);
            
            std::string newValues = supplierToJsonString(*result.supplier);
            logAuditEvent("suppliers", supplierId, 
                         database::models::AuditAction::UPDATE,
                         oldValues, 
                         newValues,
                         activatedBy,
                         "Supplier activated");
        }
        else
        {
            result.success = false;
            result.message = "Failed to activate supplier";
        }
    }
    catch (const std::exception& e)
    {
        result.success = false;
        result.message = "Error activating supplier: " + std::string(e.what());
    }
    
    return result;
}

Poco::JSON::Array SupplierService::getActiveSuppliers()
{
    Poco::JSON::Array result;
    
    try
    {
        auto suppliers = supplierRepository->findActiveSuppliers();
        
        for (auto& supplier : suppliers)
        {
            auto enrichedSupplier = enrichSupplierWithDetails(std::move(supplier));
            result.add(enrichedSupplier->toJson());
        }
    }
    catch (const std::exception& e)
    {
        Poco::JSON::Object error;
        error.set("error", "Error retrieving active suppliers: " + std::string(e.what()));
        result.add(error);
    }
    
    return result;
}

Poco::JSON::Array SupplierService::getSupplierProducts(long long supplierId)
{
    Poco::JSON::Array result;
    
    try
    {
        auto products = productRepository->findBySupplier(supplierId);
        
        for (auto& product : products)
        {
            result.add(product->toJson());
        }
        
        if (result.size() == 0)
        {
            Poco::JSON::Object info;
            info.set("message", "No products found for supplier ID: " + std::to_string(supplierId));
            info.set("supplierId", static_cast<Poco::Int64>(supplierId));
            result.add(info);
        }
    }
    catch (const std::exception& e)
    {
        Poco::JSON::Object error;
        error.set("error", "Error retrieving supplier products: " + std::string(e.what()));
        result.add(error);
    }
    
    return result;
}

Poco::JSON::Array SupplierService::getSupplierBatches(long long supplierId)
{
    Poco::JSON::Array result;
    
    try
    {
        auto batches = batchRepository->findBySupplier(supplierId);
        
        for (auto& batch : batches)
        {
            result.add(batch->toJson());
        }
        
        if (result.size() == 0)
        {
            Poco::JSON::Object info;
            info.set("message", "No batches found for supplier ID: " + std::to_string(supplierId));
            info.set("supplierId", static_cast<Poco::Int64>(supplierId));
            result.add(info);
        }
    }
    catch (const std::exception& e)
    {
        Poco::JSON::Object error;
        error.set("error", "Error retrieving supplier batches: " + std::string(e.what()));
        result.add(error);
    }
    
    return result;
}

bool SupplierService::validateSupplierData(const database::models::Supplier& supplier, std::string& errorMessage)
{
    if (supplier.name.empty())
    {
        errorMessage = "Supplier name is required";
        return false;
    }
    
    if (supplier.name.length() > 150)
    {
        errorMessage = "Supplier name cannot exceed 150 characters";
        return false;
    }
    
    if (!supplier.taxId.empty() && supplier.taxId.length() > 20)
    {
        errorMessage = "Tax ID cannot exceed 20 characters";
        return false;
    }
    
    if (!supplier.email.isNull())
    {
        if (!utils::Validator::isValidEmail(supplier.email))
        {
            errorMessage = "Invalid email format";
            return false;
        }
        
        if (supplier.email.value().length() > 100)
        {
            errorMessage = "Email cannot exceed 100 characters";
            return false;
        }
    }
    
    if (!supplier.contactPerson.isNull() && supplier.contactPerson.value().length() > 100)
    {
        errorMessage = "Contact person name cannot exceed 100 characters";
        return false;
    }
    
    if (!supplier.phone.isNull() && supplier.phone.value().length() > 20)
    {
        errorMessage = "Phone number cannot exceed 20 characters";
        return false;
    }
    
    if (supplier.rating < 0.0 || supplier.rating > 5.0)
    {
        errorMessage = "Rating must be between 0.0 and 5.0";
        return false;
    }
    
    return true;
}

bool SupplierService::isTaxIdAvailable(const std::string& taxId, long long excludeSupplierId)
{
    if (taxId.empty())
    {
        return true;
    }
    
    try
    {
        auto suppliers = supplierRepository->search(taxId, {"tax_id"});
        
        if (suppliers.empty())
        {
            return true;
        }
        
        if (excludeSupplierId > 0)
        {
            for (const auto& supplier : suppliers)
            {
                if (supplier->id != excludeSupplierId)
                {
                    return false;
                }
            }
            return true;
        }
        
        return false;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

void SupplierService::logAuditEvent(const std::string& tableName,
                                   long long recordId,
                                   database::models::AuditAction action,
                                   const std::string& oldValues,
                                   const std::string& newValues,
                                   long long changedBy,
                                   const std::string& description)
{
    try
    {
        database::models::AuditLog auditLog;
        auditLog.tableName = tableName;
        auditLog.recordId = recordId;
        auditLog.action = action;
        auditLog.oldValues = oldValues;
        auditLog.newValues = newValues;
        auditLog.changedBy = changedBy;
        auditLog.ipAddress = "127.0.0.1";
        auditLog.userAgent = "SupplierService";
        auditLog.description = description;
        
        auditRepository->create(auditLog);
    }
    catch (const std::exception&)
    {
    }
}

std::string SupplierService::supplierToJsonString(const database::models::Supplier& supplier) const
{
    try
    {
        auto json = supplier.toJson();
        return utils::JsonUtils::objectToString(json, false);
    }
    catch (const std::exception&)
    {
        return "{}";
    }
}

std::unique_ptr<database::models::Supplier> SupplierService::enrichSupplierWithDetails(
    std::unique_ptr<database::models::Supplier> supplier)
{
    if (!supplier)
    {
        return nullptr;
    }
    
    try
    {
        int productCount = productRepository->countProductsBySupplier(supplier->id);
        int batchCount = batchRepository->countBySupplier(supplier->id);
        
        Poco::JSON::Object data;
        data.set("productCount", productCount);
        data.set("batchCount", batchCount);
        
        supplier->rating = supplier->rating;
        
        return supplier;
    }
    catch (const std::exception&)
    {
        return supplier;
    }
}

} // namespace warehouse_backend::services
