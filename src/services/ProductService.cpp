#include "ProductService.hpp"
#include "../database/repositories/WarehouseCellRepository.hpp"
#include "../database/repositories/InventoryMovementRepository.hpp"
#include "../utils/DateUtils.hpp"
#include <Poco/Data/RecordSet.h>
#include <Poco/Data/Row.h>
#include <algorithm>

namespace warehouse_backend::services
{

ProductServiceResult::ProductServiceResult()
    : success(false),
      message(""),
      productId(0),
      product(nullptr),
      data(Poco::JSON::Object())
{
}

Poco::JSON::Object ProductServiceResult::toJson() const
{
    Poco::JSON::Object json;
    json.set("success", success);
    json.set("message", message);
    
    if (success)
    {
        json.set("productId", productId);
        if (product)
        {
            json.set("product", product->toJson());
        }
    }
    
    if (!data.size())
    {
        json.set("data", data);
    }
    
    return json;
}

ProductService::ProductService()
    : productRepository(std::make_unique<database::repositories::ProductRepository>()),
      batchRepository(std::make_unique<database::repositories::ProductBatchRepository>()),
      categoryRepository(std::make_unique<database::repositories::CategoryRepository>()),
      supplierRepository(std::make_unique<database::repositories::SupplierRepository>()),
      auditRepository(std::make_unique<database::repositories::AuditLogRepository>())
{
}

ProductServiceResult ProductService::createProduct(const database::models::Product& product,
                                                  long long createdBy)
{
    ProductServiceResult result;
    
    try
    {
        std::string validationError;
        if (!validateProductData(product, validationError))
        {
            result.message = "Ошибка валидации продукта: " + validationError;
            return result;
        }
        
        if (!isSkuAvailable(product.sku))
        {
            result.message = "SKU уже существует: " + product.sku;
            return result;
        }
        
        auto category = categoryRepository->findById(product.categoryId);
        if (!category)
        {
            result.message = "Категория не найдена: " + std::to_string(product.categoryId);
            return result;
        }
        
        auto supplier = supplierRepository->findById(product.supplierId);
        if (!supplier)
        {
            result.message = "Поставщик не найден: " + std::to_string(product.supplierId);
            return result;
        }
        
        long long productId = productRepository->create(product);
        if (productId <= 0)
        {
            result.message = "Ошибка при создании продукта";
            return result;
        }
        
        auto createdProduct = productRepository->findById(productId);
        if (!createdProduct)
        {
            result.message = "Не удалось получить созданный продукт";
            return result;
        }
        
        logProductAudit("CREATE", productId, createdBy);
        
        result.success = true;
        result.message = "Продукт успешно создан";
        result.productId = productId;
        result.product = std::move(createdProduct);
        
        Poco::JSON::Object data;
        data.set("sku", product.sku);
        data.set("name", product.name);
        data.set("categoryName", category->name);
        data.set("supplierName", supplier->name);
        result.data = data;
    }
    catch (const std::exception& e)
    {
        result.message = "Ошибка при создании продукта: " + std::string(e.what());
    }
    catch (...)
    {
        result.message = "Неизвестная ошибка при создании продукта";
    }
    
    return result;
}

ProductServiceResult ProductService::getProductById(long long productId)
{
    ProductServiceResult result;
    
    try
    {
        auto product = productRepository->findById(productId);
        if (!product)
        {
            result.message = "Продукт не найден: " + std::to_string(productId);
            return result;
        }
        
        auto category = categoryRepository->findById(product->categoryId);
        if (category)
        {
            product->categoryName = category->name;
        }
        
        auto supplier = supplierRepository->findById(product->supplierId);
        if (supplier)
        {
            product->supplierName = supplier->name;
        }
        
        auto batches = batchRepository->findByProduct(productId);
        int currentStock = 0;
        for (const auto& batch : batches)
        {
            currentStock += batch->quantityAvailable;
        }
        product->currentStock = currentStock;
        
        result.success = true;
        result.message = "Продукт успешно получен";
        result.productId = productId;
        result.product = std::move(product);
    }
    catch (const std::exception& e)
    {
        result.message = "Ошибка при получении продукта: " + std::string(e.what());
    }
    catch (...)
    {
        result.message = "Неизвестная ошибка при получении продукта";
    }
    
    return result;
}

ProductServiceResult ProductService::getProductBySku(const std::string& sku)
{
    ProductServiceResult result;
    
    try
    {
        auto product = productRepository->findBySku(sku);
        if (!product)
        {
            result.message = "Продукт не найден: " + sku;
            return result;
        }
        
        auto category = categoryRepository->findById(product->categoryId);
        if (category)
        {
            product->categoryName = category->name;
        }
        
        auto supplier = supplierRepository->findById(product->supplierId);
        if (supplier)
        {
            product->supplierName = supplier->name;
        }
        
        auto batches = batchRepository->findByProduct(product->id);
        int currentStock = 0;
        for (const auto& batch : batches)
        {
            currentStock += batch->quantityAvailable;
        }
        product->currentStock = currentStock;
        
        result.success = true;
        result.message = "Продукт успешно получен";
        result.productId = product->id;
        result.product = std::move(product);
    }
    catch (const std::exception& e)
    {
        result.message = "Ошибка при получении продукта: " + std::string(e.what());
    }
    catch (...)
    {
        result.message = "Неизвестная ошибка при получении продукта";
    }
    
    return result;
}

Poco::JSON::Array ProductService::getProductsByCategory(long long categoryId,
                                                       int page,
                                                       int pageSize)
{
    Poco::JSON::Array result;
    
    try
    {
        auto category = categoryRepository->findById(categoryId);
        if (!category)
        {
            return result;
        }
        
        auto products = productRepository->findByCategory(categoryId);
        
        int startIdx = (page - 1) * pageSize;
        int endIdx = std::min(startIdx + pageSize, static_cast<int>(products.size()));
        
        for (int i = startIdx; i < endIdx && i < static_cast<int>(products.size()); ++i)
        {
            auto& product = products[i];
            
            product->categoryName = category->name;
            
            auto supplier = supplierRepository->findById(product->supplierId);
            if (supplier)
            {
                product->supplierName = supplier->name;
            }
            
            auto batches = batchRepository->findByProduct(product->id);
            int currentStock = 0;
            for (const auto& batch : batches)
            {
                currentStock += batch->quantityAvailable;
            }
            product->currentStock = currentStock;
            
            result.add(product->toJson());
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Ошибка при получении продуктов по категории: " << e.what() << std::endl;
    }
    catch (...)
    {
        std::cerr << "Неизвестная ошибка при получении продуктов по категории" << std::endl;
    }
    
    return result;
}

Poco::JSON::Array ProductService::searchProducts(const std::string& query,
                                                int page,
                                                int pageSize)
{
    Poco::JSON::Array result;
    
    try
    {
        std::vector<std::string> searchFields = {"p.sku", "p.name", "p.description"};
        
        auto products = productRepository->search(query, searchFields);
        
        int startIdx = (page - 1) * pageSize;
        int endIdx = std::min(startIdx + pageSize, static_cast<int>(products.size()));
        
        for (int i = startIdx; i < endIdx && i < static_cast<int>(products.size()); ++i)
        {
            auto& product = products[i];
            
            auto category = categoryRepository->findById(product->categoryId);
            if (category)
            {
                product->categoryName = category->name;
            }
            
            auto supplier = supplierRepository->findById(product->supplierId);
            if (supplier)
            {
                product->supplierName = supplier->name;
            }
            
            auto batches = batchRepository->findByProduct(product->id);
            int currentStock = 0;
            for (const auto& batch : batches)
            {
                currentStock += batch->quantityAvailable;
            }
            product->currentStock = currentStock;
            
            result.add(product->toJson());
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Ошибка при поиске продуктов: " << e.what() << std::endl;
    }
    catch (...)
    {
        std::cerr << "Неизвестная ошибка при поиске продуктов" << std::endl;
    }
    
    return result;
}

ProductServiceResult ProductService::updateProduct(long long productId,
                                                  const database::models::Product& updatedProduct,
                                                  long long updatedBy)
{
    ProductServiceResult result;
    
    try
    {
        auto existingProduct = productRepository->findById(productId);
        if (!existingProduct)
        {
            result.message = "Продукт не найден: " + std::to_string(productId);
            return result;
        }
        
        std::string oldValues = productToJsonString(*existingProduct);
        
        if (existingProduct->sku != updatedProduct.sku)
        {
            if (!isSkuAvailable(updatedProduct.sku, productId))
            {
                result.message = "SKU уже используется: " + updatedProduct.sku;
                return result;
            }
        }
        
        if (existingProduct->categoryId != updatedProduct.categoryId)
        {
            auto category = categoryRepository->findById(updatedProduct.categoryId);
            if (!category)
            {
                result.message = "Категория не найдена: " + std::to_string(updatedProduct.categoryId);
                return result;
            }
        }
        
        if (existingProduct->supplierId != updatedProduct.supplierId)
        {
            auto supplier = supplierRepository->findById(updatedProduct.supplierId);
            if (!supplier)
            {
                result.message = "Поставщик не найден: " + std::to_string(updatedProduct.supplierId);
                return result;
            }
        }
        
        std::string validationError;
        if (!validateProductData(updatedProduct, validationError))
        {
            result.message = "Ошибка валидации продукта: " + validationError;
            return result;
        }
        
        bool updateSuccess = productRepository->update(productId, updatedProduct);
        if (!updateSuccess)
        {
            result.message = "Ошибка при обновлении продукта";
            return result;
        }
        
        auto product = productRepository->findById(productId);
        if (!product)
        {
            result.message = "Не удалось получить обновленный продукт";
            return result;
        }
        
        auto category = categoryRepository->findById(product->categoryId);
        if (category)
        {
            product->categoryName = category->name;
        }
        
        auto supplier = supplierRepository->findById(product->supplierId);
        if (supplier)
        {
            product->supplierName = supplier->name;
        }
        
        auto batches = batchRepository->findByProduct(productId);
        int currentStock = 0;
        for (const auto& batch : batches)
        {
            currentStock += batch->quantityAvailable;
        }
        product->currentStock = currentStock;
        
        std::string newValues = productToJsonString(*product);
        logProductAudit("UPDATE", productId, updatedBy, oldValues, newValues);
        
        result.success = true;
        result.message = "Продукт успешно обновлен";
        result.productId = productId;
        result.product = std::move(product);
    }
    catch (const std::exception& e)
    {
        result.message = "Ошибка при обновлении продукта: " + std::string(e.what());
    }
    catch (...)
    {
        result.message = "Неизвестная ошибка при обновлении продукта";
    }
    
    return result;
}

ProductServiceResult ProductService::updateStockLevels(long long productId,
                                                      int minStockLevel,
                                                      int maxStockLevel,
                                                      long long updatedBy)
{
    ProductServiceResult result;
    
    try
    {
        auto product = productRepository->findById(productId);
        if (!product)
        {
            result.message = "Продукт не найден: " + std::to_string(productId);
            return result;
        }
        
        if (minStockLevel < 0 || maxStockLevel < minStockLevel)
        {
            result.message = "Некорректные уровни запаса: min=" + std::to_string(minStockLevel) +
                             ", max=" + std::to_string(maxStockLevel);
            return result;
        }
        
        std::string oldValues = productToJsonString(*product);
        
        bool updateSuccess = productRepository->updateStockLevels(productId, minStockLevel, maxStockLevel);
        if (!updateSuccess)
        {
            result.message = "Ошибка при обновлении уровней запаса";
            return result;
        }
        
        auto updatedProduct = productRepository->findById(productId);
        if (!updatedProduct)
        {
            result.message = "Не удалось получить обновленный продукт";
            return result;
        }
        
        auto category = categoryRepository->findById(updatedProduct->categoryId);
        if (category)
        {
            updatedProduct->categoryName = category->name;
        }
        
        auto supplier = supplierRepository->findById(updatedProduct->supplierId);
        if (supplier)
        {
            updatedProduct->supplierName = supplier->name;
        }
        
        auto batches = batchRepository->findByProduct(productId);
        int currentStock = 0;
        for (const auto& batch : batches)
        {
            currentStock += batch->quantityAvailable;
        }
        updatedProduct->currentStock = currentStock;
        
        std::string newValues = productToJsonString(*updatedProduct);
        logProductAudit("UPDATE_STOCK_LEVELS", productId, updatedBy, oldValues, newValues);
        
        result.success = true;
        result.message = "Уровни запаса успешно обновлены";
        result.productId = productId;
        result.product = std::move(updatedProduct);
    }
    catch (const std::exception& e)
    {
        result.message = "Ошибка при обновлении уровней запаса: " + std::string(e.what());
    }
    catch (...)
    {
        result.message = "Неизвестная ошибка при обновлении уровней запаса";
    }
    
    return result;
}

ProductServiceResult ProductService::updatePrice(long long productId,
                                                double newPrice,
                                                long long updatedBy)
{
    ProductServiceResult result;
    
    try
    {
        auto product = productRepository->findById(productId);
        if (!product)
        {
            result.message = "Продукт не найден: " + std::to_string(productId);
            return result;
        }
        
        if (newPrice < 0)
        {
            result.message = "Некорректная цена: " + std::to_string(newPrice);
            return result;
        }
        
        std::string oldValues = productToJsonString(*product);
        
        bool updateSuccess = productRepository->updatePrice(productId, newPrice);
        if (!updateSuccess)
        {
            result.message = "Ошибка при обновлении цены";
            return result;
        }
        
        auto updatedProduct = productRepository->findById(productId);
        if (!updatedProduct)
        {
            result.message = "Не удалось получить обновленный продукт";
            return result;
        }
        
        auto category = categoryRepository->findById(updatedProduct->categoryId);
        if (category)
        {
            updatedProduct->categoryName = category->name;
        }
        
        auto supplier = supplierRepository->findById(updatedProduct->supplierId);
        if (supplier)
        {
            updatedProduct->supplierName = supplier->name;
        }
        
        auto batches = batchRepository->findByProduct(productId);
        int currentStock = 0;
        for (const auto& batch : batches)
        {
            currentStock += batch->quantityAvailable;
        }
        updatedProduct->currentStock = currentStock;
        
        std::string newValues = productToJsonString(*updatedProduct);
        logProductAudit("UPDATE_PRICE", productId, updatedBy, oldValues, newValues);
        
        result.success = true;
        result.message = "Цена успешно обновлена";
        result.productId = productId;
        result.product = std::move(updatedProduct);
    }
    catch (const std::exception& e)
    {
        result.message = "Ошибка при обновлении цены: " + std::string(e.what());
    }
    catch (...)
    {
        result.message = "Неизвестная ошибка при обновлении цены";
    }
    
    return result;
}

ProductServiceResult ProductService::deactivateProduct(long long productId,
                                                      long long deactivatedBy)
{
    ProductServiceResult result;
    
    try
    {
        auto product = productRepository->findById(productId);
        if (!product)
        {
            result.message = "Продукт не найден: " + std::to_string(productId);
            return result;
        }
        
        if (!product->isActive)
        {
            result.message = "Продукт уже деактивирован";
            return result;
        }
        
        auto batches = batchRepository->findByProduct(productId);
        int totalStock = 0;
        for (const auto& batch : batches)
        {
            totalStock += batch->quantityAvailable;
        }
        
        if (totalStock > 0)
        {
            result.message = "Невозможно деактивировать продукт с остатками на складе: " + 
                             std::to_string(totalStock) + " единиц";
            return result;
        }
        
        std::string oldValues = productToJsonString(*product);
        
        bool updateSuccess = productRepository->updateStatus(productId, false);
        if (!updateSuccess)
        {
            result.message = "Ошибка при деактивации продукта";
            return result;
        }
        
        auto updatedProduct = productRepository->findById(productId);
        if (!updatedProduct)
        {
            result.message = "Не удалось получить обновленный продукт";
            return result;
        }
        
        auto category = categoryRepository->findById(updatedProduct->categoryId);
        if (category)
        {
            updatedProduct->categoryName = category->name;
        }
        
        auto supplier = supplierRepository->findById(updatedProduct->supplierId);
        if (supplier)
        {
            updatedProduct->supplierName = supplier->name;
        }
        
        std::string newValues = productToJsonString(*updatedProduct);
        logProductAudit("DEACTIVATE", productId, deactivatedBy, oldValues, newValues);
        
        result.success = true;
        result.message = "Продукт успешно деактивирован";
        result.productId = productId;
        result.product = std::move(updatedProduct);
    }
    catch (const std::exception& e)
    {
        result.message = "Ошибка при деактивации продукта: " + std::string(e.what());
    }
    catch (...)
    {
        result.message = "Неизвестная ошибка при деактивации продукта";
    }
    
    return result;
}

ProductServiceResult ProductService::activateProduct(long long productId,
                                                    long long activatedBy)
{
    ProductServiceResult result;
    
    try
    {
        auto product = productRepository->findById(productId);
        if (!product)
        {
            result.message = "Продукт не найден: " + std::to_string(productId);
            return result;
        }
        
        if (product->isActive)
        {
            result.message = "Продукт уже активирован";
            return result;
        }
        
        std::string oldValues = productToJsonString(*product);
        
        bool updateSuccess = productRepository->updateStatus(productId, true);
        if (!updateSuccess)
        {
            result.message = "Ошибка при активации продукта";
            return result;
        }
        
        auto updatedProduct = productRepository->findById(productId);
        if (!updatedProduct)
        {
            result.message = "Не удалось получить обновленный продукт";
            return result;
        }
        
        auto category = categoryRepository->findById(updatedProduct->categoryId);
        if (category)
        {
            updatedProduct->categoryName = category->name;
        }
        
        auto supplier = supplierRepository->findById(updatedProduct->supplierId);
        if (supplier)
        {
            updatedProduct->supplierName = supplier->name;
        }
        
        auto batches = batchRepository->findByProduct(productId);
        int currentStock = 0;
        for (const auto& batch : batches)
        {
            currentStock += batch->quantityAvailable;
        }
        updatedProduct->currentStock = currentStock;
        
        std::string newValues = productToJsonString(*updatedProduct);
        logProductAudit("ACTIVATE", productId, activatedBy, oldValues, newValues);
        
        result.success = true;
        result.message = "Продукт успешно активирован";
        result.productId = productId;
        result.product = std::move(updatedProduct);
    }
    catch (const std::exception& e)
    {
        result.message = "Ошибка при активации продукта: " + std::string(e.what());
    }
    catch (...)
    {
        result.message = "Неизвестная ошибка при активации продукта";
    }
    
    return result;
}

Poco::JSON::Array ProductService::getLowStockProducts(int threshold)
{
    Poco::JSON::Array result;
    
    try
    {
        auto products = productRepository->findActiveProducts();
        
        for (const auto& product : products)
        {
            auto batches = batchRepository->findByProduct(product->id);
            int currentStock = 0;
            for (const auto& batch : batches)
            {
                if (batch->qualityStatus == database::models::QualityStatus::APPROVED)
                {
                    currentStock += batch->quantityAvailable;
                }
            }
            
            if (currentStock <= threshold)
            {
                product->currentStock = currentStock;
                
                auto category = categoryRepository->findById(product->categoryId);
                if (category)
                {
                    product->categoryName = category->name;
                }
                
                auto supplier = supplierRepository->findById(product->supplierId);
                if (supplier)
                {
                    product->supplierName = supplier->name;
                }
                
                Poco::JSON::Object productJson = product->toJson();
                productJson.set("currentStock", currentStock);
                productJson.set("stockLevel", 
                               currentStock <= product->minStockLevel ? "CRITICAL" : 
                               currentStock <= product->minStockLevel * 2 ? "LOW" : "NORMAL");
                
                result.add(productJson);
            }
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Ошибка при получении продуктов с низким запасом: " << e.what() << std::endl;
    }
    catch (...)
    {
        std::cerr << "Неизвестная ошибка при получении продуктов с низким запасом" << std::endl;
    }
    
    return result;
}

Poco::JSON::Array ProductService::getProductsNeedingReorder()
{
    Poco::JSON::Array result;
    
    try
    {
        auto products = productRepository->findActiveProducts();
        
        for (const auto& product : products)
        {
            auto batches = batchRepository->findByProduct(product->id);
            int currentStock = 0;
            for (const auto& batch : batches)
            {
                if (batch->qualityStatus == database::models::QualityStatus::APPROVED)
                {
                    currentStock += batch->quantityAvailable;
                }
            }
            
            if (currentStock <= product->minStockLevel)
            {
                product->currentStock = currentStock;
                
                auto category = categoryRepository->findById(product->categoryId);
                if (category)
                {
                    product->categoryName = category->name;
                }
                
                auto supplier = supplierRepository->findById(product->supplierId);
                if (supplier)
                {
                    product->supplierName = supplier->name;
                }
                
                int neededQuantity = product->maxStockLevel - currentStock;
                if (neededQuantity > 0)
                {
                    Poco::JSON::Object productJson = product->toJson();
                    productJson.set("currentStock", currentStock);
                    productJson.set("neededQuantity", neededQuantity);
                    productJson.set("reorderLevel", product->minStockLevel);
                    productJson.set("priority", 
                                   currentStock == 0 ? "URGENT" : 
                                   currentStock <= product->minStockLevel * 0.5 ? "HIGH" : "NORMAL");
                    
                    result.add(productJson);
                }
            }
        }
        
    }
    catch (const std::exception& e)
    {
        std::cerr << "Ошибка при получении продуктов для пополнения: " << e.what() << std::endl;
    }
    catch (...)
    {
        std::cerr << "Неизвестная ошибка при получении продуктов для пополнения" << std::endl;
    }
    
    return result;
}

Poco::JSON::Object ProductService::getProductStockSummary(long long productId)
{
    Poco::JSON::Object result;
    
    try
    {
        auto product = productRepository->findById(productId);
        if (!product)
        {
            result.set("error", "Продукт не найден");
            return result;
        }
        
        auto batches = batchRepository->findByProduct(productId);
        
        int totalReceived = 0;
        int totalAvailable = 0;
        int totalUsed = 0;
        double totalValue = 0.0;
        
        std::map<std::string, int> statusCount;
        std::map<std::string, int> cellDistribution;
        
        for (const auto& batch : batches)
        {
            totalReceived += batch->quantityReceived;
            totalAvailable += batch->quantityAvailable;
            totalUsed += (batch->quantityReceived - batch->quantityAvailable);
            totalValue += (batch->quantityAvailable * batch->unitCost);
            
            std::string status = database::models::ProductBatch::qualityStatusToString(batch->qualityStatus);
            statusCount[status] += batch->quantityAvailable;
            
            cellDistribution[batch->storageCellCode] += batch->quantityAvailable;
        }
        
        int averageDaysInStock = 0;
        if (!batches.empty())
        {
            auto now = utils::DateUtils::now();
            int totalDays = 0;
            
            for (const auto& batch : batches)
            {
                auto arrivalDate = utils::DateUtils::parseDate(batch->arrivalDate);
                totalDays += utils::DateUtils::daysBetween(arrivalDate, now);
            }
            
            averageDaysInStock = totalDays / batches.size();
        }
        
        result.set("productId", productId);
        result.set("productName", product->name);
        result.set("productSku", product->sku);
        result.set("totalReceived", totalReceived);
        result.set("totalAvailable", totalAvailable);
        result.set("totalUsed", totalUsed);
        result.set("totalValue", totalValue);
        result.set("minStockLevel", product->minStockLevel);
        result.set("maxStockLevel", product->maxStockLevel);
        result.set("currentStockLevel", 
                   totalAvailable <= product->minStockLevel ? "CRITICAL" : 
                   totalAvailable <= product->minStockLevel * 1.5 ? "LOW" : 
                   totalAvailable >= product->maxStockLevel * 0.9 ? "HIGH" : "NORMAL");
        result.set("averageDaysInStock", averageDaysInStock);
        
        Poco::JSON::Object statusJson;
        for (const auto& [status, count] : statusCount)
        {
            statusJson.set(status, count);
        }
        result.set("qualityStatusDistribution", statusJson);
        
        Poco::JSON::Object cellJson;
        for (const auto& [cell, count] : cellDistribution)
        {
            cellJson.set(cell, count);
        }
        result.set("cellDistribution", cellJson);
        
        Poco::JSON::Array batchesJson;
        for (const auto& batch : batches)
        {
            batchesJson.add(batch->toJson());
        }
        result.set("batches", batchesJson);
    }
    catch (const std::exception& e)
    {
        result.set("error", "Ошибка при получении сводки по запасам: " + std::string(e.what()));
    }
    catch (...)
    {
        result.set("error", "Неизвестная ошибка при получении сводки по запасам");
    }
    
    return result;
}

Poco::JSON::Object ProductService::getProductValueAnalysis(long long productId)
{
    Poco::JSON::Object result;
    
    try
    {
        auto product = productRepository->findById(productId);
        if (!product)
        {
            result.set("error", "Продукт не найден");
            return result;
        }
        
        auto batches = batchRepository->findByProduct(productId);
        
        int totalStock = 0;
        double totalCost = 0.0;
        double totalPotentialRevenue = 0.0;
        double averageCost = 0.0;
        double minCost = std::numeric_limits<double>::max();
        double maxCost = 0.0;
        
        for (const auto& batch : batches)
        {
            if (batch->qualityStatus == database::models::QualityStatus::APPROVED)
            {
                totalStock += batch->quantityAvailable;
                totalCost += (batch->quantityAvailable * batch->unitCost);
                totalPotentialRevenue += (batch->quantityAvailable * product->unitPrice);
                
                if (batch->unitCost < minCost) minCost = batch->unitCost;
                if (batch->unitCost > maxCost) maxCost = batch->unitCost;
            }
        }
        
        if (totalStock > 0)
        {
            averageCost = totalCost / totalStock;
        }
        
        double grossMargin = totalStock > 0 ? 
            ((totalPotentialRevenue - totalCost) / totalPotentialRevenue * 100) : 0.0;
        
        double turnoverRate = 0.0;
        double stockCoverage = 0.0;
        
        auto now = utils::DateUtils::now();
        Poco::JSON::Object ageAnalysis;
        int youngStock = 0;
        int mediumStock = 0;
        int oldStock = 0;
        
        for (const auto& batch : batches)
        {
            if (batch->qualityStatus == database::models::QualityStatus::APPROVED)
            {
                auto arrivalDate = utils::DateUtils::parseDate(batch->arrivalDate);
                int daysInStock = utils::DateUtils::daysBetween(arrivalDate, now);
                
                if (daysInStock < 30)
                {
                    youngStock += batch->quantityAvailable;
                }
                else if (daysInStock < 90)
                {
                    mediumStock += batch->quantityAvailable;
                }
                else
                {
                    oldStock += batch->quantityAvailable;
                }
            }
        }
        
        ageAnalysis.set("youngStock", youngStock);
        ageAnalysis.set("mediumStock", mediumStock);
        ageAnalysis.set("oldStock", oldStock);
        
        result.set("productId", productId);
        result.set("productName", product->name);
        result.set("productSku", product->sku);
        result.set("totalStock", totalStock);
        result.set("totalCostValue", totalCost);
        result.set("totalPotentialRevenue", totalPotentialRevenue);
        result.set("averageUnitCost", averageCost);
        result.set("minUnitCost", minCost == std::numeric_limits<double>::max() ? 0.0 : minCost);
        result.set("maxUnitCost", maxCost);
        result.set("currentUnitPrice", product->unitPrice);
        result.set("grossMarginPercent", grossMargin);
        result.set("turnoverRate", turnoverRate);
        result.set("stockCoverageDays", stockCoverage);
        result.set("ageAnalysis", ageAnalysis);
        result.set("profitability", 
                   grossMargin > 30 ? "HIGH" : 
                   grossMargin > 15 ? "MEDIUM" : "LOW");
    }
    catch (const std::exception& e)
    {
        result.set("error", "Ошибка при анализе стоимости продукта: " + std::string(e.what()));
    }
    catch (...)
    {
        result.set("error", "Неизвестная ошибка при анализе стоимости продукта");
    }
    
    return result;
}

Poco::JSON::Array ProductService::getProductStatistics()
{
    Poco::JSON::Array result;
    
    try
    {
        auto stats = productRepository->getProductStatistics();
        result = stats;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Ошибка при получении статистики продуктов: " << e.what() << std::endl;
    }
    catch (...)
    {
        std::cerr << "Неизвестная ошибка при получении статистики продуктов" << std::endl;
    }
    
    return result;
}

Poco::JSON::Array ProductService::getCategoryProductReport(long long categoryId)
{
    Poco::JSON::Array result;
    
    try
    {
        auto category = categoryRepository->findById(categoryId);
        if (!category)
        {
            return result;
        }
        
        auto products = productRepository->findByCategory(categoryId);
        
        for (const auto& product : products)
        {
            auto batches = batchRepository->findByProduct(product->id);
            int currentStock = 0;
            double stockValue = 0.0;
            
            for (const auto& batch : batches)
            {
                if (batch->qualityStatus == database::models::QualityStatus::APPROVED)
                {
                    currentStock += batch->quantityAvailable;
                    stockValue += (batch->quantityAvailable * batch->unitCost);
                }
            }
            
            double stockPercentage = product->maxStockLevel > 0 ? 
                (static_cast<double>(currentStock) / product->maxStockLevel * 100) : 0.0;
            
            Poco::JSON::Object reportEntry;
            reportEntry.set("productId", product->id);
            reportEntry.set("sku", product->sku);
            reportEntry.set("name", product->name);
            reportEntry.set("currentStock", currentStock);
            reportEntry.set("minStockLevel", product->minStockLevel);
            reportEntry.set("maxStockLevel", product->maxStockLevel);
            reportEntry.set("stockPercentage", stockPercentage);
            reportEntry.set("stockValue", stockValue);
            reportEntry.set("unitPrice", product->unitPrice);
            reportEntry.set("isActive", product->isActive);
            reportEntry.set("stockStatus", 
                           currentStock <= product->minStockLevel ? "CRITICAL" : 
                           currentStock <= product->minStockLevel * 1.5 ? "LOW" : 
                           stockPercentage >= 90 ? "HIGH" : "NORMAL");
            
            result.add(reportEntry);
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Ошибка при создании отчета по категории: " << e.what() << std::endl;
    }
    catch (...)
    {
        std::cerr << "Неизвестная ошибка при создании отчета по категории" << std::endl;
    }
    
    return result;
}

Poco::JSON::Array ProductService::getSupplierProductReport(long long supplierId)
{
    Poco::JSON::Array result;
    
    try
    {
        auto supplier = supplierRepository->findById(supplierId);
        if (!supplier)
        {
            return result;
        }
        
        auto products = productRepository->findBySupplier(supplierId);
        
        for (const auto& product : products)
        {
            auto batches = batchRepository->findBySupplier(supplierId);
            int supplierStock = 0;
            double supplierStockValue = 0.0;
            int totalReceived = 0;
            
            for (const auto& batch : batches)
            {
                if (batch->productId == product->id && 
                    batch->qualityStatus == database::models::QualityStatus::APPROVED)
                {
                    supplierStock += batch->quantityAvailable;
                    supplierStockValue += (batch->quantityAvailable * batch->unitCost);
                    totalReceived += batch->quantityReceived;
                }
            }
            
            Poco::JSON::Object reportEntry;
            reportEntry.set("productId", product->id);
            reportEntry.set("sku", product->sku);
            reportEntry.set("name", product->name);
            reportEntry.set("supplierStock", supplierStock);
            reportEntry.set("supplierStockValue", supplierStockValue);
            reportEntry.set("totalReceivedFromSupplier", totalReceived);
            reportEntry.set("unitPrice", product->unitPrice);
            reportEntry.set("isActive", product->isActive);
            reportEntry.set("supplierRating", supplier->rating);
            
            result.add(reportEntry);
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Ошибка при создании отчета по поставщику: " << e.what() << std::endl;
    }
    catch (...)
    {
        std::cerr << "Неизвестная ошибка при создании отчета по поставщику" << std::endl;
    }
    
    return result;
}

bool ProductService::validateProductData(const database::models::Product& product,
                                        std::string& errorMessage)
{
    if (product.sku.empty() || product.sku.length() > database::models::Product::MAX_SKU_LENGTH)
    {
        errorMessage = "Некорректный SKU (длина от 1 до " + 
                      std::to_string(database::models::Product::MAX_SKU_LENGTH) + " символов)";
        return false;
    }
    
    if (product.name.empty() || product.name.length() > database::models::Product::MAX_NAME_LENGTH)
    {
        errorMessage = "Некорректное название (длина от 1 до " + 
                      std::to_string(database::models::Product::MAX_NAME_LENGTH) + " символов)";
        return false;
    }
    
    if (product.unitPrice < database::models::Product::MIN_PRICE)
    {
        errorMessage = "Цена не может быть отрицательной";
        return false;
    }
    
    if (product.weight < database::models::Product::MIN_WEIGHT)
    {
        errorMessage = "Вес не может быть отрицательным";
        return false;
    }
    
    if (product.minStockLevel < database::models::Product::MIN_STOCK_LEVEL)
    {
        errorMessage = "Минимальный уровень запаса не может быть отрицательным";
        return false;
    }
    
    if (product.maxStockLevel < product.minStockLevel)
    {
        errorMessage = "Максимальный уровень запаса не может быть меньше минимального";
        return false;
    }
    
    if (product.categoryId <= 0)
    {
        errorMessage = "Необходимо указать категорию";
        return false;
    }
    
    if (product.supplierId <= 0)
    {
        errorMessage = "Необходимо указать поставщика";
        return false;
    }
    
    return true;
}

bool ProductService::isSkuAvailable(const std::string& sku, long long excludeProductId)
{
    try
    {
        auto existingProduct = productRepository->findBySku(sku);
        
        if (!existingProduct)
        {
            return true;
        }
        
        if (excludeProductId > 0 && existingProduct->id == excludeProductId)
        {
            return true;
        }
        
        return false;
    }
    catch (...)
    {
        return false;
    }
}

std::string ProductService::productToJsonString(const database::models::Product& user) const
{
    try
    {
        auto json = user.toJson();
        return utils::JsonUtils::objectToString(json, false);
    }
    catch (const std::exception&)
    {
        return "{}";
    }
}

void ProductService::logProductAudit(const std::string& action,
                                    long long productId,
                                    long long changedBy,
                                    const std::string& oldValues,
                                    const std::string& newValues)
{
    try
    {
        database::models::AuditLog auditLog;
        auditLog.tableName = "products";
        auditLog.recordId = productId;
        auditLog.action = database::models::AuditAction::UPDATE;

        Poco::Nullable<std::string> oldValuesNullable;
        if (!oldValues.empty())
            oldValuesNullable = oldValues;
        auditLog.oldValues = oldValuesNullable;
        
        Poco::Nullable<std::string> newValuesNullable;
        if (!newValues.empty())
            newValuesNullable = newValues;
        auditLog.newValues = newValuesNullable;

        auditLog.changedBy = changedBy;
        auditLog.description = "Product " + action + ": " + std::to_string(productId);
        
        auditRepository->create(auditLog);
    }
    catch (const std::exception& e)
    {
        std::cerr << "Ошибка при записи аудита продукта: " << e.what() << std::endl;
    }
    catch (...)
    {
        std::cerr << "Неизвестная ошибка при записи аудита продукта" << std::endl;
    }
}

} // namespace warehouse_backend::services
