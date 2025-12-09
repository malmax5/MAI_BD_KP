#include "ReportService.hpp"
#include "../database/ConnectionPool.hpp"
#include "../database/repositories/WarehouseCellRepository.hpp"
#include "../database/repositories/SupplierRepository.hpp"
#include "../database/repositories/OrderPaymentRepository.hpp"
#include "../database/repositories/OrderItemRepository.hpp"
#include "../utils/DateUtils.hpp"
#include "../utils/JsonUtils.hpp"
#include "../utils/Validator.hpp"
#include <Poco/JSON/Parser.h>
#include <Poco/JSON/Stringifier.h>
#include <Poco/DateTimeFormatter.h>
#include <Poco/DateTimeFormat.h>
#include <sstream>
#include <iomanip>
#include <cmath>

namespace warehouse_backend::services
{

ReportResult::ReportResult()
    : success(false), message("")
{
}

Poco::JSON::Object ReportResult::toJson() const
{
    Poco::JSON::Object result;
    result.set("success", success);
    result.set("message", message);
    
    if (!reportData.size())
    {
        result.set("reportData", reportData);
    }
    
    return result;
}

ReportService::ReportService()
    : orderRepository(std::make_unique<database::repositories::CustomerOrderRepository>()),
      productRepository(std::make_unique<database::repositories::ProductRepository>()),
      movementRepository(std::make_unique<database::repositories::InventoryMovementRepository>()),
      shipmentRepository(std::make_unique<database::repositories::ShipmentRepository>())
{
}

ReportResult ReportService::getWarehouseOccupancyReport()
{
    ReportResult result;
    
    try
    {
        auto connection = database::ConnectionPool::getInstance().acquireConnection();
        if (!connection)
        {
            result.success = false;
            result.message = "Failed to acquire database connection";
            return result;
        }
        
        std::string sql = R"(
            SELECT 
                zone,
                temperature_zone,
                cell_status,
                total_cells,
                empty_cells,
                partially_occupied_cells,
                full_cells,
                blocked_cells,
                avg_occupancy_percent,
                total_max_volume,
                total_max_weight,
                used_volume,
                used_weight,
                ROUND((used_volume / NULLIF(total_max_volume, 0) * 100)::numeric, 2) as volume_utilization_percent,
                ROUND((used_weight / NULLIF(total_max_weight, 0) * 100)::numeric, 2) as weight_utilization_percent
            FROM v_warehouse_occupancy
            ORDER BY zone, temperature_zone, cell_status
        )";
        
        auto session = connection->getSession();
        Poco::Data::Statement select(session);
        
        std::string zone, temperatureZone, cellStatus;
        int totalCells, emptyCells, partiallyOccupiedCells, fullCells, blockedCells;
        double avgOccupancyPercent, totalMaxVolume, totalMaxWeight, usedVolume, usedWeight;
        double volumeUtilizationPercent, weightUtilizationPercent;
        
        select << sql,
            Poco::Data::Keywords::into(zone),
            Poco::Data::Keywords::into(temperatureZone),
            Poco::Data::Keywords::into(cellStatus),
            Poco::Data::Keywords::into(totalCells),
            Poco::Data::Keywords::into(emptyCells),
            Poco::Data::Keywords::into(partiallyOccupiedCells),
            Poco::Data::Keywords::into(fullCells),
            Poco::Data::Keywords::into(blockedCells),
            Poco::Data::Keywords::into(avgOccupancyPercent),
            Poco::Data::Keywords::into(totalMaxVolume),
            Poco::Data::Keywords::into(totalMaxWeight),
            Poco::Data::Keywords::into(usedVolume),
            Poco::Data::Keywords::into(usedWeight),
            Poco::Data::Keywords::into(volumeUtilizationPercent),
            Poco::Data::Keywords::into(weightUtilizationPercent);
        
        Poco::JSON::Array occupancyData;
        double totalOverallVolume = 0;
        double totalOverallWeight = 0;
        double totalUsedVolume = 0;
        double totalUsedWeight = 0;
        int totalAllCells = 0;
        int totalEmptyCells = 0;
        int totalOccupiedCells = 0;
        
        select.execute();
        
        while (select.nextDataSet())
        {
            Poco::JSON::Object item;
            item.set("zone", zone);
            item.set("temperature_zone", temperatureZone);
            item.set("cell_status", cellStatus);
            item.set("total_cells", totalCells);
            item.set("empty_cells", emptyCells);
            item.set("partially_occupied_cells", partiallyOccupiedCells);
            item.set("full_cells", fullCells);
            item.set("blocked_cells", blockedCells);
            item.set("avg_occupancy_percent", avgOccupancyPercent);
            item.set("total_max_volume", totalMaxVolume);
            item.set("total_max_weight", totalMaxWeight);
            item.set("used_volume", usedVolume);
            item.set("used_weight", usedWeight);
            item.set("volume_utilization_percent", volumeUtilizationPercent);
            item.set("weight_utilization_percent", weightUtilizationPercent);
            
            occupancyData.add(item);
            
            totalAllCells += totalCells;
            totalEmptyCells += emptyCells;
            totalOccupiedCells += (partiallyOccupiedCells + fullCells);
            totalOverallVolume += totalMaxVolume;
            totalOverallWeight += totalMaxWeight;
            totalUsedVolume += usedVolume;
            totalUsedWeight += usedWeight;
        }
        
        Poco::JSON::Object summary;
        summary.set("total_cells", totalAllCells);
        summary.set("empty_cells", totalEmptyCells);
        summary.set("occupied_cells", totalOccupiedCells);
        summary.set("blocked_cells", totalAllCells - totalEmptyCells - totalOccupiedCells);
        summary.set("occupancy_rate_percent", totalAllCells > 0 ? 
            std::round((static_cast<double>(totalOccupiedCells) / totalAllCells * 100) * 100) / 100 : 0);
        summary.set("total_max_volume", totalOverallVolume);
        summary.set("total_max_weight", totalOverallWeight);
        summary.set("total_used_volume", totalUsedVolume);
        summary.set("total_used_weight", totalUsedWeight);
        summary.set("overall_volume_utilization_percent", totalOverallVolume > 0 ? 
            std::round((totalUsedVolume / totalOverallVolume * 100) * 100) / 100 : 0);
        summary.set("overall_weight_utilization_percent", totalOverallWeight > 0 ? 
            std::round((totalUsedWeight / totalOverallWeight * 100) * 100) / 100 : 0);
        summary.set("report_generated", utils::DateUtils::formatDateTime(utils::DateUtils::now()));
        
        result.reportData.set("summary", summary);
        result.reportData.set("detailed_data", occupancyData);
        
        result.success = true;
        result.message = "Warehouse occupancy report generated successfully";
    }
    catch (const std::exception& e)
    {
        result.success = false;
        result.message = "Error generating warehouse occupancy report: " + std::string(e.what());
    }
    
    return result;
}

ReportResult ReportService::getInventoryValueReport()
{
    ReportResult result;
    
    try
    {
        auto connection = database::ConnectionPool::getInstance().acquireConnection();
        if (!connection)
        {
            result.success = false;
            result.message = "Failed to acquire database connection";
            return result;
        }
        
        std::string totalValueSql = "SELECT get_stock_value()";
        auto session = connection->getSession();
        
        double totalInventoryValue = 0;
        Poco::Data::Statement totalValueStmt(session);
        totalValueStmt << totalValueSql, Poco::Data::Keywords::into(totalInventoryValue);
        totalValueStmt.execute();
        totalValueStmt.nextDataSet();
        
        std::string zoneValueSql = R"(
            SELECT 
                wc.zone,
                COALESCE(SUM(pb.quantity_available * pb.unit_cost), 0) as zone_value,
                COUNT(DISTINCT pb.id) as batch_count,
                SUM(pb.quantity_available) as total_quantity
            FROM warehouse_cells wc
            LEFT JOIN product_batches pb ON pb.storage_cell_id = wc.id 
                AND pb.quality_status = 'approved'
            GROUP BY wc.zone
            ORDER BY wc.zone
        )";
        
        Poco::Data::Statement zoneStmt(session);
        std::string zone;
        double zoneValue;
        int batchCount, totalQuantity;
        
        zoneStmt << zoneValueSql,
            Poco::Data::Keywords::into(zone),
            Poco::Data::Keywords::into(zoneValue),
            Poco::Data::Keywords::into(batchCount),
            Poco::Data::Keywords::into(totalQuantity);
        
        Poco::JSON::Array zoneData;
        zoneStmt.execute();
        
        while (zoneStmt.nextDataSet())
        {
            Poco::JSON::Object zoneItem;
            zoneItem.set("zone", zone);
            zoneItem.set("value", zoneValue);
            zoneItem.set("percentage_of_total", totalInventoryValue > 0 ? 
                std::round((zoneValue / totalInventoryValue * 100) * 100) / 100 : 0);
            zoneItem.set("batch_count", batchCount);
            zoneItem.set("total_quantity", totalQuantity);
            
            zoneData.add(zoneItem);
        }
        
        std::string categoryValueSql = R"(
            SELECT 
                c.name as category_name,
                COALESCE(SUM(pb.quantity_available * pb.unit_cost), 0) as category_value,
                COUNT(DISTINCT p.id) as product_count,
                COUNT(DISTINCT pb.id) as batch_count,
                SUM(pb.quantity_available) as total_quantity
            FROM categories c
            LEFT JOIN products p ON p.category_id = c.id AND p.is_active = TRUE
            LEFT JOIN product_batches pb ON pb.product_id = p.id 
                AND pb.quality_status = 'approved'
            GROUP BY c.id, c.name
            HAVING COALESCE(SUM(pb.quantity_available * pb.unit_cost), 0) > 0
            ORDER BY category_value DESC
        )";
        
        Poco::Data::Statement categoryStmt(session);
        std::string categoryName;
        double categoryValue;
        int productCount, catBatchCount, catTotalQuantity;
        
        categoryStmt << categoryValueSql,
            Poco::Data::Keywords::into(categoryName),
            Poco::Data::Keywords::into(categoryValue),
            Poco::Data::Keywords::into(productCount),
            Poco::Data::Keywords::into(catBatchCount),
            Poco::Data::Keywords::into(catTotalQuantity);
        
        Poco::JSON::Array categoryData;
        categoryStmt.execute();
        
        while (categoryStmt.nextDataSet())
        {
            Poco::JSON::Object categoryItem;
            categoryItem.set("category_name", categoryName);
            categoryItem.set("value", categoryValue);
            categoryItem.set("percentage_of_total", totalInventoryValue > 0 ? 
                std::round((categoryValue / totalInventoryValue * 100) * 100) / 100 : 0);
            categoryItem.set("product_count", productCount);
            categoryItem.set("batch_count", catBatchCount);
            categoryItem.set("total_quantity", catTotalQuantity);
            
            categoryData.add(categoryItem);
        }
        
        std::string topProductsSql = R"(
            SELECT 
                p.sku,
                p.name as product_name,
                c.name as category_name,
                COALESCE(SUM(pb.quantity_available * pb.unit_cost), 0) as product_value,
                SUM(pb.quantity_available) as total_quantity,
                ROUND(AVG(pb.unit_cost)::numeric, 2) as avg_unit_cost,
                COUNT(DISTINCT pb.id) as batch_count
            FROM products p
            LEFT JOIN categories c ON c.id = p.category_id
            LEFT JOIN product_batches pb ON pb.product_id = p.id 
                AND pb.quality_status = 'approved'
            WHERE p.is_active = TRUE
            GROUP BY p.id, p.sku, p.name, c.name
            HAVING COALESCE(SUM(pb.quantity_available * pb.unit_cost), 0) > 0
            ORDER BY product_value DESC
            LIMIT 10
        )";
        
        Poco::Data::Statement topProductsStmt(session);
        std::string sku, productName, topCategoryName;
        double productValue, avgUnitCost;
        int topTotalQuantity, topBatchCount;
        
        topProductsStmt << topProductsSql,
            Poco::Data::Keywords::into(sku),
            Poco::Data::Keywords::into(productName),
            Poco::Data::Keywords::into(topCategoryName),
            Poco::Data::Keywords::into(productValue),
            Poco::Data::Keywords::into(topTotalQuantity),
            Poco::Data::Keywords::into(avgUnitCost),
            Poco::Data::Keywords::into(topBatchCount);
        
        Poco::JSON::Array topProductsData;
        topProductsStmt.execute();
        
        while (topProductsStmt.nextDataSet())
        {
            Poco::JSON::Object productItem;
            productItem.set("sku", sku);
            productItem.set("product_name", productName);
            productItem.set("category_name", topCategoryName);
            productItem.set("value", productValue);
            productItem.set("total_quantity", topTotalQuantity);
            productItem.set("avg_unit_cost", avgUnitCost);
            productItem.set("batch_count", topBatchCount);
            productItem.set("percentage_of_total", totalInventoryValue > 0 ? 
                std::round((productValue / totalInventoryValue * 100) * 100) / 100 : 0);
            
            topProductsData.add(productItem);
        }
        
        Poco::JSON::Object summary;
        summary.set("total_inventory_value", totalInventoryValue);
        summary.set("currency", "USD");
        
        std::string countsSql = R"(
            SELECT 
                COUNT(DISTINCT p.id) as total_products,
                COUNT(DISTINCT pb.id) as total_batches,
                COALESCE(SUM(pb.quantity_available), 0) as total_items
            FROM products p
            LEFT JOIN product_batches pb ON pb.product_id = p.id 
                AND pb.quality_status = 'approved'
            WHERE p.is_active = TRUE
        )";
        
        Poco::Data::Statement countsStmt(session);
        int totalProducts, totalBatches, totalItems;
        
        countsStmt << countsSql,
            Poco::Data::Keywords::into(totalProducts),
            Poco::Data::Keywords::into(totalBatches),
            Poco::Data::Keywords::into(totalItems);
        
        countsStmt.execute();
        countsStmt.nextDataSet();
        
        summary.set("total_products", totalProducts);
        summary.set("total_batches", totalBatches);
        summary.set("total_items", totalItems);
        summary.set("average_value_per_product", totalProducts > 0 ? 
            totalInventoryValue / totalProducts : 0);
        summary.set("average_value_per_item", totalItems > 0 ? 
            totalInventoryValue / totalItems : 0);
        summary.set("report_generated", utils::DateUtils::formatDateTime(utils::DateUtils::now()));
        
        result.reportData.set("summary", summary);
        result.reportData.set("by_zone", zoneData);
        result.reportData.set("by_category", categoryData);
        result.reportData.set("top_valuable_products", topProductsData);
        
        result.success = true;
        result.message = "Inventory value report generated successfully";
    }
    catch (const std::exception& e)
    {
        result.success = false;
        result.message = "Error generating inventory value report: " + std::string(e.what());
    }
    
    return result;
}

ReportResult ReportService::getDailyStatisticsReport(const std::string& date)
{
    ReportResult result;
    
    try
    {
        if (!utils::Validator::isValidDate(date))
        {
            result.success = false;
            result.message = "Invalid date format. Expected YYYY-MM-DD";
            return result;
        }
        
        auto connection = database::ConnectionPool::getInstance().acquireConnection();
        if (!connection)
        {
            result.success = false;
            result.message = "Failed to acquire database connection";
            return result;
        }
        
        std::string sql = R"(
            SELECT * FROM v_daily_statistics 
            WHERE date = ?::date
        )";
        
        auto session = connection->getSession();
        Poco::Data::Statement select(session);
        
        std::string reportDate;
        int totalOrders, shipmentsCount, uniqueCustomers;
        double dailyRevenue, avgOrderValue, totalShippingCost;
        int totalItemsOrdered, totalItemsShipped;
        int newOrders, processingOrders, deliveredOrders, cancelledOrders;

        std::string dateCopy = date;

        select << sql,
            Poco::Data::Keywords::into(reportDate),
            Poco::Data::Keywords::into(totalOrders),
            Poco::Data::Keywords::into(dailyRevenue),
            Poco::Data::Keywords::into(avgOrderValue),
            Poco::Data::Keywords::into(uniqueCustomers),
            Poco::Data::Keywords::into(shipmentsCount),
            Poco::Data::Keywords::into(totalShippingCost),
            Poco::Data::Keywords::into(totalItemsOrdered),
            Poco::Data::Keywords::into(totalItemsShipped),
            Poco::Data::Keywords::into(newOrders),
            Poco::Data::Keywords::into(processingOrders),
            Poco::Data::Keywords::into(deliveredOrders),
            Poco::Data::Keywords::into(cancelledOrders),
            Poco::Data::Keywords::use(dateCopy);
        
        Poco::JSON::Object dailyStats;
        
        select.execute();
        
        if (select.nextDataSet())
        {
            dailyStats.set("date", reportDate);
            dailyStats.set("total_orders", totalOrders);
            dailyStats.set("daily_revenue", dailyRevenue);
            dailyStats.set("avg_order_value", avgOrderValue);
            dailyStats.set("unique_customers", uniqueCustomers);
            dailyStats.set("shipments_count", shipmentsCount);
            dailyStats.set("total_shipping_cost", totalShippingCost);
            dailyStats.set("total_items_ordered", totalItemsOrdered);
            dailyStats.set("total_items_shipped", totalItemsShipped);
            dailyStats.set("new_orders", newOrders);
            dailyStats.set("processing_orders", processingOrders);
            dailyStats.set("delivered_orders", deliveredOrders);
            dailyStats.set("cancelled_orders", cancelledOrders);
            dailyStats.set("fulfillment_rate_percent", totalItemsOrdered > 0 ? 
                std::round((static_cast<double>(totalItemsShipped) / totalItemsOrdered * 100) * 100) / 100 : 0);
        }
        else
        {
            dailyStats.set("date", date);
            dailyStats.set("total_orders", 0);
            dailyStats.set("daily_revenue", 0.0);
            dailyStats.set("avg_order_value", 0.0);
            dailyStats.set("unique_customers", 0);
            dailyStats.set("shipments_count", 0);
            dailyStats.set("total_shipping_cost", 0.0);
            dailyStats.set("total_items_ordered", 0);
            dailyStats.set("total_items_shipped", 0);
            dailyStats.set("new_orders", 0);
            dailyStats.set("processing_orders", 0);
            dailyStats.set("delivered_orders", 0);
            dailyStats.set("cancelled_orders", 0);
            dailyStats.set("fulfillment_rate_percent", 0);
        }
        
        std::string prioritySql = R"(
            SELECT 
                priority,
                COUNT(*) as order_count,
                SUM(total_amount) as total_amount
            FROM customer_orders
            WHERE DATE(order_date) = ?::date
            GROUP BY priority
            ORDER BY priority
        )";
        
        Poco::Data::Statement priorityStmt(session);
        std::string priority;
        int priorityOrderCount;
        double priorityTotalAmount;
        
        priorityStmt << prioritySql,
            Poco::Data::Keywords::into(priority),
            Poco::Data::Keywords::into(priorityOrderCount),
            Poco::Data::Keywords::into(priorityTotalAmount),
            Poco::Data::Keywords::use(dateCopy);
        
        Poco::JSON::Array priorityStats;
        priorityStmt.execute();
        
        while (priorityStmt.nextDataSet())
        {
            Poco::JSON::Object priorityItem;
            priorityItem.set("priority", priority);
            priorityItem.set("order_count", priorityOrderCount);
            priorityItem.set("total_amount", priorityTotalAmount);
            
            priorityStats.add(priorityItem);
        }
        
        std::string topProductsSql = R"(
            SELECT 
                p.sku,
                p.name as product_name,
                SUM(oi.quantity_ordered) as total_quantity,
                SUM(oi.quantity_ordered * oi.unit_price * (1 - oi.discount_percent/100)) as total_value,
                COUNT(DISTINCT oi.order_id) as order_count
            FROM order_items oi
            JOIN products p ON p.id = oi.product_id
            JOIN customer_orders co ON co.id = oi.order_id
            WHERE DATE(co.order_date) = ?::date
            GROUP BY p.id, p.sku, p.name
            ORDER BY total_quantity DESC
            LIMIT 10
        )";
        
        Poco::Data::Statement topProductsStmt(session);
        std::string productSku, productName;
        int productTotalQuantity, productOrderCount;
        double productTotalValue;
        
        topProductsStmt << topProductsSql,
            Poco::Data::Keywords::into(productSku),
            Poco::Data::Keywords::into(productName),
            Poco::Data::Keywords::into(productTotalQuantity),
            Poco::Data::Keywords::into(productTotalValue),
            Poco::Data::Keywords::into(productOrderCount),
            Poco::Data::Keywords::use(dateCopy);
        
        Poco::JSON::Array topProducts;
        topProductsStmt.execute();
        
        while (topProductsStmt.nextDataSet())
        {
            Poco::JSON::Object productItem;
            productItem.set("sku", productSku);
            productItem.set("product_name", productName);
            productItem.set("total_quantity", productTotalQuantity);
            productItem.set("total_value", productTotalValue);
            productItem.set("order_count", productOrderCount);
            productItem.set("avg_quantity_per_order", productOrderCount > 0 ? 
                static_cast<double>(productTotalQuantity) / productOrderCount : 0);
            
            topProducts.add(productItem);
        }
        
        std::string supplierSql = R"(
            SELECT 
                s.name as supplier_name,
                COUNT(DISTINCT pb.id) as batch_count,
                SUM(pb.quantity_received) as total_quantity_received,
                SUM(pb.quantity_received * pb.unit_cost) as total_value
            FROM product_batches pb
            JOIN suppliers s ON s.id = pb.supplier_id
            WHERE DATE(pb.arrival_date) = ?::date
            GROUP BY s.id, s.name
            ORDER BY total_value DESC
        )";
        
        Poco::Data::Statement supplierStmt(session);
        std::string supplierName;
        int supplierBatchCount, supplierTotalQuantity;
        double supplierTotalValue;
        
        supplierStmt << supplierSql,
            Poco::Data::Keywords::into(supplierName),
            Poco::Data::Keywords::into(supplierBatchCount),
            Poco::Data::Keywords::into(supplierTotalQuantity),
            Poco::Data::Keywords::into(supplierTotalValue),
            Poco::Data::Keywords::use(dateCopy);
        
        Poco::JSON::Array supplierStats;
        supplierStmt.execute();
        
        while (supplierStmt.nextDataSet())
        {
            Poco::JSON::Object supplierItem;
            supplierItem.set("supplier_name", supplierName);
            supplierItem.set("batch_count", supplierBatchCount);
            supplierItem.set("total_quantity_received", supplierTotalQuantity);
            supplierItem.set("total_value", supplierTotalValue);
            
            supplierStats.add(supplierItem);
        }
        
        result.reportData.set("daily_summary", dailyStats);
        result.reportData.set("order_priority_stats", priorityStats);
        result.reportData.set("top_products", topProducts);
        result.reportData.set("supplier_receipts", supplierStats);
        result.reportData.set("report_generated", utils::DateUtils::formatDateTime(utils::DateUtils::now()));
        
        result.success = true;
        result.message = "Daily statistics report generated successfully";
    }
    catch (const std::exception& e)
    {
        result.success = false;
        result.message = "Error generating daily statistics report: " + std::string(e.what());
    }
    
    return result;
}

ReportResult ReportService::getSalesReport(const std::string& startDate, const std::string& endDate)
{
    ReportResult result;
    
    try
    {
        if (!utils::Validator::isValidDate(startDate) || !utils::Validator::isValidDate(endDate))
        {
            result.success = false;
            result.message = "Invalid date format. Expected YYYY-MM-DD";
            return result;
        }
        
        auto connection = database::ConnectionPool::getInstance().acquireConnection();
        if (!connection)
        {
            result.success = false;
            result.message = "Failed to acquire database connection";
            return result;
        }
        
        auto session = connection->getSession();
        
        std::string salesSql = R"(
            SELECT 
                COUNT(DISTINCT co.id) as total_orders,
                SUM(co.total_amount) as total_revenue,
                AVG(co.total_amount) as avg_order_value,
                COUNT(DISTINCT co.customer_email) as unique_customers,
                MIN(co.order_date) as first_order_date,
                MAX(co.order_date) as last_order_date
            FROM customer_orders co
            WHERE co.order_date::date BETWEEN ?::date AND ?::date
                AND co.status NOT IN ('cancelled')
        )";
        
        Poco::Data::Statement salesStmt(session);
        int totalOrders, uniqueCustomers;
        double totalRevenue, avgOrderValue;
        std::string firstOrderDate, lastOrderDate;

        std::string startDateCopy = startDate;
        std::string endDateCopy = endDate;
        
        salesStmt << salesSql,
            Poco::Data::Keywords::into(totalOrders),
            Poco::Data::Keywords::into(totalRevenue),
            Poco::Data::Keywords::into(avgOrderValue),
            Poco::Data::Keywords::into(uniqueCustomers),
            Poco::Data::Keywords::into(firstOrderDate),
            Poco::Data::Keywords::into(lastOrderDate),
            Poco::Data::Keywords::use(startDateCopy),
            Poco::Data::Keywords::use(endDateCopy);
        
        salesStmt.execute();
        salesStmt.nextDataSet();
        
        Poco::JSON::Object salesSummary;
        salesSummary.set("period_start", startDate);
        salesSummary.set("period_end", endDate);
        salesSummary.set("total_orders", totalOrders);
        salesSummary.set("total_revenue", totalRevenue);
        salesSummary.set("avg_order_value", avgOrderValue);
        salesSummary.set("unique_customers", uniqueCustomers);
        salesSummary.set("first_order_date", firstOrderDate);
        salesSummary.set("last_order_date", lastOrderDate);
        salesSummary.set("avg_revenue_per_customer", uniqueCustomers > 0 ? 
            totalRevenue / uniqueCustomers : 0);
        
        std::string dailySalesSql = R"(
            SELECT 
                DATE(co.order_date) as sale_date,
                COUNT(DISTINCT co.id) as daily_orders,
                SUM(co.total_amount) as daily_revenue,
                COUNT(DISTINCT co.customer_email) as daily_customers
            FROM customer_orders co
            WHERE co.order_date::date BETWEEN ?::date AND ?::date
                AND co.status NOT IN ('cancelled')
            GROUP BY DATE(co.order_date)
            ORDER BY sale_date
        )";
        
        Poco::Data::Statement dailyStmt(session);
        std::string saleDate;
        int dailyOrders, dailyCustomers;
        double dailyRevenue;
        
        dailyStmt << dailySalesSql,
            Poco::Data::Keywords::into(saleDate),
            Poco::Data::Keywords::into(dailyOrders),
            Poco::Data::Keywords::into(dailyRevenue),
            Poco::Data::Keywords::into(dailyCustomers),
            Poco::Data::Keywords::use(startDateCopy),
            Poco::Data::Keywords::use(endDateCopy);
        
        Poco::JSON::Array dailySales;
        dailyStmt.execute();
        
        while (dailyStmt.nextDataSet())
        {
            Poco::JSON::Object dailyItem;
            dailyItem.set("date", saleDate);
            dailyItem.set("orders", dailyOrders);
            dailyItem.set("revenue", dailyRevenue);
            dailyItem.set("customers", dailyCustomers);
            dailyItem.set("avg_order_value", dailyOrders > 0 ? dailyRevenue / dailyOrders : 0);
            
            dailySales.add(dailyItem);
        }
        
        std::string categorySalesSql = R"(
            SELECT 
                c.name as category_name,
                COUNT(DISTINCT co.id) as order_count,
                SUM(oi.quantity_ordered) as total_quantity,
                SUM(oi.quantity_ordered * oi.unit_price * (1 - oi.discount_percent/100)) as total_revenue,
                COUNT(DISTINCT oi.product_id) as unique_products
            FROM categories c
            JOIN products p ON p.category_id = c.id
            JOIN order_items oi ON oi.product_id = p.id
            JOIN customer_orders co ON co.id = oi.order_id
            WHERE co.order_date::date BETWEEN ?::date AND ?::date
                AND co.status NOT IN ('cancelled')
            GROUP BY c.id, c.name
            ORDER BY total_revenue DESC
        )";
        
        Poco::Data::Statement categoryStmt(session);
        std::string categoryName;
        int categoryOrderCount, categoryTotalQuantity, categoryUniqueProducts;
        double categoryTotalRevenue;
        
        categoryStmt << categorySalesSql,
            Poco::Data::Keywords::into(categoryName),
            Poco::Data::Keywords::into(categoryOrderCount),
            Poco::Data::Keywords::into(categoryTotalQuantity),
            Poco::Data::Keywords::into(categoryTotalRevenue),
            Poco::Data::Keywords::into(categoryUniqueProducts),
            Poco::Data::Keywords::use(startDateCopy),
            Poco::Data::Keywords::use(endDateCopy);
        
        Poco::JSON::Array categorySales;
        categoryStmt.execute();
        
        while (categoryStmt.nextDataSet())
        {
            Poco::JSON::Object categoryItem;
            categoryItem.set("category_name", categoryName);
            categoryItem.set("order_count", categoryOrderCount);
            categoryItem.set("total_quantity", categoryTotalQuantity);
            categoryItem.set("total_revenue", categoryTotalRevenue);
            categoryItem.set("unique_products", categoryUniqueProducts);
            categoryItem.set("percentage_of_total", totalRevenue > 0 ? 
                std::round((categoryTotalRevenue / totalRevenue * 100) * 100) / 100 : 0);
            categoryItem.set("avg_quantity_per_order", categoryOrderCount > 0 ? 
                static_cast<double>(categoryTotalQuantity) / categoryOrderCount : 0);
            
            categorySales.add(categoryItem);
        }
        
        std::string topCustomersSql = R"(
            SELECT 
                co.customer_email,
                co.customer_name,
                COUNT(DISTINCT co.id) as order_count,
                SUM(co.total_amount) as total_spent,
                MIN(co.order_date) as first_order_date,
                MAX(co.order_date) as last_order_date
            FROM customer_orders co
            WHERE co.order_date::date BETWEEN ?::date AND ?::date
                AND co.status NOT IN ('cancelled')
                AND co.customer_email IS NOT NULL
            GROUP BY co.customer_email, co.customer_name
            ORDER BY total_spent DESC
            LIMIT 10
        )";
        
        Poco::Data::Statement topCustomersStmt(session);
        std::string customerEmail, customerName, custFirstOrderDate, custLastOrderDate;
        int custOrderCount;
        double custTotalSpent;
        
        topCustomersStmt << topCustomersSql,
            Poco::Data::Keywords::into(customerEmail),
            Poco::Data::Keywords::into(customerName),
            Poco::Data::Keywords::into(custOrderCount),
            Poco::Data::Keywords::into(custTotalSpent),
            Poco::Data::Keywords::into(custFirstOrderDate),
            Poco::Data::Keywords::into(custLastOrderDate),
            Poco::Data::Keywords::use(startDateCopy),
            Poco::Data::Keywords::use(endDateCopy);
        
        Poco::JSON::Array topCustomers;
        topCustomersStmt.execute();
        
        while (topCustomersStmt.nextDataSet())
        {
            Poco::JSON::Object customerItem;
            customerItem.set("customer_email", customerEmail);
            customerItem.set("customer_name", customerName);
            customerItem.set("order_count", custOrderCount);
            customerItem.set("total_spent", custTotalSpent);
            customerItem.set("first_order_date", custFirstOrderDate);
            customerItem.set("last_order_date", custLastOrderDate);
            customerItem.set("avg_order_value", custOrderCount > 0 ? 
                custTotalSpent / custOrderCount : 0);
            customerItem.set("percentage_of_total", totalRevenue > 0 ? 
                std::round((custTotalSpent / totalRevenue * 100) * 100) / 100 : 0);
            
            topCustomers.add(customerItem);
        }
        
        std::string statusStatsSql = R"(
            SELECT 
                co.status,
                COUNT(*) as order_count,
                SUM(co.total_amount) as total_amount
            FROM customer_orders co
            WHERE co.order_date::date BETWEEN ?::date AND ?::date
            GROUP BY co.status
            ORDER BY co.status
        )";
        
        Poco::Data::Statement statusStmt(session);
        std::string orderStatus;
        int statusOrderCount;
        double statusTotalAmount;
        
        statusStmt << statusStatsSql,
            Poco::Data::Keywords::into(orderStatus),
            Poco::Data::Keywords::into(statusOrderCount),
            Poco::Data::Keywords::into(statusTotalAmount),
            Poco::Data::Keywords::use(startDateCopy),
            Poco::Data::Keywords::use(endDateCopy);
        
        Poco::JSON::Array statusStats;
        statusStmt.execute();
        
        while (statusStmt.nextDataSet())
        {
            Poco::JSON::Object statusItem;
            statusItem.set("status", orderStatus);
            statusItem.set("order_count", statusOrderCount);
            statusItem.set("total_amount", statusTotalAmount);
            statusItem.set("percentage_of_orders", totalOrders > 0 ? 
                std::round((static_cast<double>(statusOrderCount) / totalOrders * 100) * 100) / 100 : 0);
            
            statusStats.add(statusItem);
        }
        
        result.reportData.set("sales_summary", salesSummary);
        result.reportData.set("daily_sales", dailySales);
        result.reportData.set("category_sales", categorySales);
        result.reportData.set("top_customers", topCustomers);
        result.reportData.set("order_status_stats", statusStats);
        result.reportData.set("report_generated", utils::DateUtils::formatDateTime(utils::DateUtils::now()));
        
        result.success = true;
        result.message = "Sales report generated successfully";
    }
    catch (const std::exception& e)
    {
        result.success = false;
        result.message = "Error generating sales report: " + std::string(e.what());
    }
    
    return result;
}

ReportResult ReportService::getStockMovementReport(const std::string& startDate, const std::string& endDate)
{
    ReportResult result;
    
    try
    {
        if (!utils::Validator::isValidDate(startDate) || !utils::Validator::isValidDate(endDate))
        {
            result.success = false;
            result.message = "Invalid date format. Expected YYYY-MM-DD";
            return result;
        }
        
        auto movementsArray = movementRepository->getMovementReport(startDate, endDate);
        
        if (movementsArray.size() == 0)
        {
            auto connection = database::ConnectionPool::getInstance().acquireConnection();
            if (!connection)
            {
                result.success = false;
                result.message = "Failed to acquire database connection";
                return result;
            }
            
            auto session = connection->getSession();
            
            std::string movementStatsSql = R"(
                SELECT 
                    im.movement_type,
                    COUNT(*) as movement_count,
                    SUM(im.quantity) as total_quantity,
                    COUNT(DISTINCT im.product_id) as unique_products,
                    COUNT(DISTINCT im.performed_by) as unique_users
                FROM inventory_movements im
                WHERE im.movement_date::date BETWEEN ?::date AND ?::date
                GROUP BY im.movement_type
                ORDER BY movement_count DESC
            )";
            
            Poco::Data::Statement statsStmt(session);
            std::string movementType;
            int movementCount, totalQuantity, uniqueProducts, uniqueUsers;

            std::string startDateCopy = startDate;
            std::string endDateCopy = endDate;
            
            statsStmt << movementStatsSql,
                Poco::Data::Keywords::into(movementType),
                Poco::Data::Keywords::into(movementCount),
                Poco::Data::Keywords::into(totalQuantity),
                Poco::Data::Keywords::into(uniqueProducts),
                Poco::Data::Keywords::into(uniqueUsers),
                Poco::Data::Keywords::use(startDateCopy),
                Poco::Data::Keywords::use(endDateCopy);
            
            Poco::JSON::Array movementStats;
            statsStmt.execute();
            
            int totalMovements = 0;
            int totalQuantityAll = 0;
            
            while (statsStmt.nextDataSet())
            {
                Poco::JSON::Object statItem;
                statItem.set("movement_type", movementType);
                statItem.set("movement_count", movementCount);
                statItem.set("total_quantity", totalQuantity);
                statItem.set("unique_products", uniqueProducts);
                statItem.set("unique_users", uniqueUsers);
                
                movementStats.add(statItem);
                
                totalMovements += movementCount;
                totalQuantityAll += totalQuantity;
            }
            
            std::string detailedSql = R"(
                SELECT 
                    im.id,
                    im.movement_type,
                    p.sku,
                    p.name as product_name,
                    pb.batch_number,
                    wc_from.cell_code as from_cell,
                    wc_to.cell_code as to_cell,
                    im.quantity,
                    im.movement_date,
                    u.full_name as performed_by_name,
                    im.reason,
                    im.status
                FROM inventory_movements im
                JOIN products p ON p.id = im.product_id
                JOIN product_batches pb ON pb.id = im.batch_id
                LEFT JOIN warehouse_cells wc_from ON wc_from.id = im.from_cell_id
                LEFT JOIN warehouse_cells wc_to ON wc_to.id = im.to_cell_id
                JOIN users u ON u.id = im.performed_by
                WHERE im.movement_date::date BETWEEN ?::date AND ?::date
                ORDER BY im.movement_date DESC
                LIMIT 100
            )";
            
            Poco::Data::Statement detailedStmt(session);
            long long movementId;
            std::string detailedMovementType, sku, productName, batchNumber;
            std::string fromCell, toCell, movementDate, performedByName, reason, status;
            int quantity;
            
            detailedStmt << detailedSql,
                Poco::Data::Keywords::into(movementId),
                Poco::Data::Keywords::into(detailedMovementType),
                Poco::Data::Keywords::into(sku),
                Poco::Data::Keywords::into(productName),
                Poco::Data::Keywords::into(batchNumber),
                Poco::Data::Keywords::into(fromCell),
                Poco::Data::Keywords::into(toCell),
                Poco::Data::Keywords::into(quantity),
                Poco::Data::Keywords::into(movementDate),
                Poco::Data::Keywords::into(performedByName),
                Poco::Data::Keywords::into(reason),
                Poco::Data::Keywords::into(status),
                Poco::Data::Keywords::use(startDateCopy),
                Poco::Data::Keywords::use(endDateCopy);
            
            Poco::JSON::Array detailedMovements;
            detailedStmt.execute();
            
            while (detailedStmt.nextDataSet())
            {
                Poco::JSON::Object movementItem;
                movementItem.set("id", static_cast<Poco::Int64>(movementId));
                movementItem.set("movement_type", detailedMovementType);
                movementItem.set("sku", sku);
                movementItem.set("product_name", productName);
                movementItem.set("batch_number", batchNumber);
                movementItem.set("from_cell", fromCell);
                movementItem.set("to_cell", toCell);
                movementItem.set("quantity", quantity);
                movementItem.set("movement_date", movementDate);
                movementItem.set("performed_by", performedByName);
                movementItem.set("reason", reason);
                movementItem.set("status", status);
                
                detailedMovements.add(movementItem);
            }
            
            std::string userStatsSql = R"(
                SELECT 
                    u.full_name,
                    COUNT(DISTINCT im.id) as movement_count,
                    SUM(im.quantity) as total_quantity,
                    COUNT(DISTINCT im.product_id) as unique_products
                FROM inventory_movements im
                JOIN users u ON u.id = im.performed_by
                WHERE im.movement_date::date BETWEEN ?::date AND ?::date
                GROUP BY u.id, u.full_name
                ORDER BY movement_count DESC
                LIMIT 10
            )";
            
            Poco::Data::Statement userStmt(session);
            std::string userName;
            int userMovementCount, userTotalQuantity, userUniqueProducts;
            
            userStmt << userStatsSql,
                Poco::Data::Keywords::into(userName),
                Poco::Data::Keywords::into(userMovementCount),
                Poco::Data::Keywords::into(userTotalQuantity),
                Poco::Data::Keywords::into(userUniqueProducts),
                Poco::Data::Keywords::use(startDateCopy),
                Poco::Data::Keywords::use(endDateCopy);
            
            Poco::JSON::Array userStats;
            userStmt.execute();
            
            while (userStmt.nextDataSet())
            {
                Poco::JSON::Object userItem;
                userItem.set("user_name", userName);
                userItem.set("movement_count", userMovementCount);
                userItem.set("total_quantity", userTotalQuantity);
                userItem.set("unique_products", userUniqueProducts);
                userItem.set("avg_quantity_per_movement", userMovementCount > 0 ? 
                    static_cast<double>(userTotalQuantity) / userMovementCount : 0);
                
                userStats.add(userItem);
            }
            
            Poco::JSON::Object summary;
            summary.set("period_start", startDate);
            summary.set("period_end", endDate);
            summary.set("total_movements", totalMovements);
            summary.set("total_quantity_moved", totalQuantityAll);
            summary.set("avg_quantity_per_movement", totalMovements > 0 ? 
                static_cast<double>(totalQuantityAll) / totalMovements : 0);
            summary.set("report_generated", utils::DateUtils::formatDateTime(utils::DateUtils::now()));
            
            result.reportData.set("summary", summary);
            result.reportData.set("movement_statistics", movementStats);
            result.reportData.set("detailed_movements", detailedMovements);
            result.reportData.set("user_statistics", userStats);
        }
        else
        {
            result.reportData.set("movement_data", movementsArray);
            
            Poco::JSON::Object summary;
            summary.set("period_start", startDate);
            summary.set("period_end", endDate);
            summary.set("total_records", static_cast<int>(movementsArray.size()));
            summary.set("report_generated", utils::DateUtils::formatDateTime(utils::DateUtils::now()));
            
            result.reportData.set("summary", summary);
        }
        
        result.success = true;
        result.message = "Stock movement report generated successfully";
    }
    catch (const std::exception& e)
    {
        result.success = false;
        result.message = "Error generating stock movement report: " + std::string(e.what());
    }
    
    return result;
}

ReportResult ReportService::getExpiringProductsReport(int daysThreshold)
{
    ReportResult result;
    
    try
    {
        if (daysThreshold <= 0)
        {
            result.success = false;
            result.message = "Days threshold must be positive";
            return result;
        }
        
        auto connection = database::ConnectionPool::getInstance().acquireConnection();
        if (!connection)
        {
            result.success = false;
            result.message = "Failed to acquire database connection";
            return result;
        }
        
        std::string sql = "SELECT * FROM get_expiring_products(?)";
        
        auto session = connection->getSession();
        Poco::Data::Statement select(session);
        
        long long batchId;
        std::string batchNumber, productSku, productName, expirationDate, storageCellCode, temperatureZone;
        int daysUntilExpiration, quantityAvailable;
        
        select << sql,
            Poco::Data::Keywords::into(batchId),
            Poco::Data::Keywords::into(batchNumber),
            Poco::Data::Keywords::into(productSku),
            Poco::Data::Keywords::into(productName),
            Poco::Data::Keywords::into(expirationDate),
            Poco::Data::Keywords::into(daysUntilExpiration),
            Poco::Data::Keywords::into(quantityAvailable),
            Poco::Data::Keywords::into(storageCellCode),
            Poco::Data::Keywords::into(temperatureZone),
            Poco::Data::Keywords::use(daysThreshold);
        
        Poco::JSON::Array expiringProducts;
        select.execute();
        
        double totalValue = 0;
        int totalQuantity = 0;
        int criticalCount = 0;
        int warningCount = 0;
        int noticeCount = 0;
        
        while (select.nextDataSet())
        {
            std::string costSql = "SELECT unit_cost FROM product_batches WHERE id = ?";
            double unitCost = 0;
            Poco::Data::Statement costStmt(session);
            costStmt << costSql, Poco::Data::Keywords::into(unitCost), Poco::Data::Keywords::use(batchId);
            costStmt.execute();
            costStmt.nextDataSet();
            
            double batchValue = unitCost * quantityAvailable;
            totalValue += batchValue;
            totalQuantity += quantityAvailable;
            
            std::string urgencyLevel;
            if (daysUntilExpiration <= 7)
            {
                urgencyLevel = "CRITICAL";
                criticalCount++;
            }
            else if (daysUntilExpiration <= 30)
            {
                urgencyLevel = "WARNING";
                warningCount++;
            }
            else
            {
                urgencyLevel = "NOTICE";
                noticeCount++;
            }
            
            Poco::JSON::Object productItem;
            productItem.set("batch_id", static_cast<Poco::Int64>(batchId));
            productItem.set("batch_number", batchNumber);
            productItem.set("sku", productSku);
            productItem.set("product_name", productName);
            productItem.set("expiration_date", expirationDate);
            productItem.set("days_until_expiration", daysUntilExpiration);
            productItem.set("quantity_available", quantityAvailable);
            productItem.set("unit_cost", unitCost);
            productItem.set("total_value", batchValue);
            productItem.set("storage_cell", storageCellCode);
            productItem.set("temperature_zone", temperatureZone);
            productItem.set("urgency_level", urgencyLevel);
            
            expiringProducts.add(productItem);
        }
        
        std::string zoneStatsSql = R"(
            SELECT 
                wc.temperature_zone,
                COUNT(DISTINCT pb.id) as batch_count,
                SUM(pb.quantity_available) as total_quantity,
                SUM(pb.quantity_available * pb.unit_cost) as total_value
            FROM product_batches pb
            JOIN warehouse_cells wc ON wc.id = pb.storage_cell_id
            WHERE pb.expiration_date IS NOT NULL
                AND pb.expiration_date >= CURRENT_DATE
                AND pb.expiration_date <= CURRENT_DATE + ?
                AND pb.quantity_available > 0
                AND pb.quality_status = 'approved'
            GROUP BY wc.temperature_zone
            ORDER BY total_value DESC
        )";
        
        Poco::Data::Statement zoneStmt(session);
        std::string tempZone;
        int zoneBatchCount, zoneTotalQuantity;
        double zoneTotalValue;
        
        zoneStmt << zoneStatsSql,
            Poco::Data::Keywords::into(tempZone),
            Poco::Data::Keywords::into(zoneBatchCount),
            Poco::Data::Keywords::into(zoneTotalQuantity),
            Poco::Data::Keywords::into(zoneTotalValue),
            Poco::Data::Keywords::use(daysThreshold);
        
        Poco::JSON::Array zoneStats;
        zoneStmt.execute();
        
        while (zoneStmt.nextDataSet())
        {
            Poco::JSON::Object zoneItem;
            zoneItem.set("temperature_zone", tempZone);
            zoneItem.set("batch_count", zoneBatchCount);
            zoneItem.set("total_quantity", zoneTotalQuantity);
            zoneItem.set("total_value", zoneTotalValue);
            zoneItem.set("percentage_of_total", totalValue > 0 ? 
                std::round((zoneTotalValue / totalValue * 100) * 100) / 100 : 0);
            
            zoneStats.add(zoneItem);
        }
        
        std::string topProductsSql = R"(
            SELECT 
                p.sku,
                p.name as product_name,
                c.name as category_name,
                COUNT(DISTINCT pb.id) as expiring_batches,
                SUM(pb.quantity_available) as total_quantity,
                MIN(pb.expiration_date) as earliest_expiration,
                MAX(pb.expiration_date) as latest_expiration
            FROM products p
            JOIN categories c ON c.id = p.category_id
            JOIN product_batches pb ON pb.product_id = p.id
            WHERE pb.expiration_date IS NOT NULL
                AND pb.expiration_date >= CURRENT_DATE
                AND pb.expiration_date <= CURRENT_DATE + ?
                AND pb.quantity_available > 0
                AND pb.quality_status = 'approved'
            GROUP BY p.id, p.sku, p.name, c.name
            ORDER BY expiring_batches DESC, total_quantity DESC
            LIMIT 10
        )";
        
        Poco::Data::Statement topProductsStmt(session);
        std::string topSku, topProductName, categoryName, earliestExpiration, latestExpiration;
        int expiringBatches, topTotalQuantity;
        
        topProductsStmt << topProductsSql,
            Poco::Data::Keywords::into(topSku),
            Poco::Data::Keywords::into(topProductName),
            Poco::Data::Keywords::into(categoryName),
            Poco::Data::Keywords::into(expiringBatches),
            Poco::Data::Keywords::into(topTotalQuantity),
            Poco::Data::Keywords::into(earliestExpiration),
            Poco::Data::Keywords::into(latestExpiration),
            Poco::Data::Keywords::use(daysThreshold);
        
        Poco::JSON::Array topProducts;
        topProductsStmt.execute();
        
        while (topProductsStmt.nextDataSet())
        {
            Poco::JSON::Object productItem;
            productItem.set("sku", topSku);
            productItem.set("product_name", topProductName);
            productItem.set("category_name", categoryName);
            productItem.set("expiring_batches", expiringBatches);
            productItem.set("total_quantity", topTotalQuantity);
            productItem.set("earliest_expiration", earliestExpiration);
            productItem.set("latest_expiration", latestExpiration);
            
            topProducts.add(productItem);
        }
        
        Poco::JSON::Object summary;
        summary.set("days_threshold", daysThreshold);
        summary.set("report_date", utils::DateUtils::formatDate(utils::DateUtils::now()));
        summary.set("total_expiring_batches", static_cast<int>(expiringProducts.size()));
        summary.set("total_expiring_quantity", totalQuantity);
        summary.set("total_expiring_value", totalValue);
        summary.set("critical_batches", criticalCount);
        summary.set("warning_batches", warningCount);
        summary.set("notice_batches", noticeCount);
        summary.set("avg_days_until_expiration", expiringProducts.size() > 0 ? 
            daysThreshold / 2 : 0);
        summary.set("report_generated", utils::DateUtils::formatDateTime(utils::DateUtils::now()));
        
        result.reportData.set("summary", summary);
        result.reportData.set("expiring_products", expiringProducts);
        result.reportData.set("temperature_zone_stats", zoneStats);
        result.reportData.set("top_affected_products", topProducts);
        
        result.success = true;
        result.message = "Expiring products report generated successfully";
    }
    catch (const std::exception& e)
    {
        result.success = false;
        result.message = "Error generating expiring products report: " + std::string(e.what());
    }
    
    return result;
}

ReportResult ReportService::getSupplierPerformanceReport()
{
    ReportResult result;
    
    try
    {
        auto connection = database::ConnectionPool::getInstance().acquireConnection();
        if (!connection)
        {
            result.success = false;
            result.message = "Failed to acquire database connection";
            return result;
        }
        
        std::string sql = "SELECT * FROM get_supplier_stats()";
        
        auto session = connection->getSession();
        Poco::Data::Statement select(session);
        
        long long supplierId;
        std::string supplierName, lastDeliveryDate;
        int totalBatches, totalProducts, totalQuantityReceived;
        double totalStockValue, avgRating;
        
        select << sql,
            Poco::Data::Keywords::into(supplierId),
            Poco::Data::Keywords::into(supplierName),
            Poco::Data::Keywords::into(totalBatches),
            Poco::Data::Keywords::into(totalProducts),
            Poco::Data::Keywords::into(totalQuantityReceived),
            Poco::Data::Keywords::into(totalStockValue),
            Poco::Data::Keywords::into(avgRating),
            Poco::Data::Keywords::into(lastDeliveryDate);
        
        Poco::JSON::Array supplierStats;
        select.execute();
        
        double totalAllStockValue = 0;
        int totalAllBatches = 0;
        int totalAllProducts = 0;
        int activeSuppliers = 0;
        
        while (select.nextDataSet())
        {
            totalAllStockValue += totalStockValue;
            totalAllBatches += totalBatches;
            totalAllProducts += totalProducts;
            
            if (totalBatches > 0) activeSuppliers++;
            
            double avgBatchSize = totalBatches > 0 ? 
                static_cast<double>(totalQuantityReceived) / totalBatches : 0;
            
            double avgProductValue = totalProducts > 0 ? 
                totalStockValue / totalProducts : 0;
            
            Poco::JSON::Object supplierItem;
            supplierItem.set("supplier_id", static_cast<Poco::Int64>(supplierId));
            supplierItem.set("supplier_name", supplierName);
            supplierItem.set("total_batches", totalBatches);
            supplierItem.set("total_products", totalProducts);
            supplierItem.set("total_quantity_received", totalQuantityReceived);
            supplierItem.set("total_stock_value", totalStockValue);
            supplierItem.set("avg_rating", avgRating);
            supplierItem.set("last_delivery_date", lastDeliveryDate);
            supplierItem.set("avg_batch_size", avgBatchSize);
            supplierItem.set("avg_product_value", avgProductValue);
            supplierItem.set("percentage_of_total_value", totalAllStockValue > 0 ? 
                std::round((totalStockValue / totalAllStockValue * 100) * 100) / 100 : 0);
            
            std::string performanceRating;
            if (avgRating >= 4.5) performanceRating = "EXCELLENT";
            else if (avgRating >= 4.0) performanceRating = "GOOD";
            else if (avgRating >= 3.0) performanceRating = "AVERAGE";
            else if (avgRating >= 2.0) performanceRating = "POOR";
            else performanceRating = "VERY POOR";
            
            supplierItem.set("performance_rating", performanceRating);
            
            supplierStats.add(supplierItem);
        }
        
        std::string qualityStatsSql = R"(
            SELECT 
                s.name as supplier_name,
                pb.quality_status,
                COUNT(*) as batch_count,
                SUM(pb.quantity_received) as total_quantity,
                ROUND(AVG(pb.unit_cost)::numeric, 2) as avg_unit_cost
            FROM suppliers s
            JOIN product_batches pb ON pb.supplier_id = s.id
            WHERE s.is_active = TRUE
            GROUP BY s.id, s.name, pb.quality_status
            ORDER BY s.name, pb.quality_status
        )";
        
        Poco::Data::Statement qualityStmt(session);
        std::string qualitySupplierName, qualityStatus;
        int qualityBatchCount, qualityTotalQuantity;
        double avgUnitCost;
        
        qualityStmt << qualityStatsSql,
            Poco::Data::Keywords::into(qualitySupplierName),
            Poco::Data::Keywords::into(qualityStatus),
            Poco::Data::Keywords::into(qualityBatchCount),
            Poco::Data::Keywords::into(qualityTotalQuantity),
            Poco::Data::Keywords::into(avgUnitCost);
        
        Poco::JSON::Array qualityStats;
        qualityStmt.execute();
        
        std::map<std::string, Poco::JSON::Object> supplierQualityMap;
        
        while (qualityStmt.nextDataSet())
        {
            if (supplierQualityMap.find(qualitySupplierName) == supplierQualityMap.end())
            {
                Poco::JSON::Object qualityItem;
                qualityItem.set("supplier_name", qualitySupplierName);
                qualityItem.set("approved_batches", 0);
                qualityItem.set("pending_batches", 0);
                qualityItem.set("quarantine_batches", 0);
                qualityItem.set("rejected_batches", 0);
                qualityItem.set("approved_quantity", 0);
                qualityItem.set("pending_quantity", 0);
                qualityItem.set("quarantine_quantity", 0);
                qualityItem.set("rejected_quantity", 0);
                
                supplierQualityMap[qualitySupplierName] = qualityItem;
            }
            
            auto& qualityItem = supplierQualityMap[qualitySupplierName];
            
            if (qualityStatus == "approved")
            {
                qualityItem.set("approved_batches", qualityBatchCount);
                qualityItem.set("approved_quantity", qualityTotalQuantity);
            }
            else if (qualityStatus == "pending")
            {
                qualityItem.set("pending_batches", qualityBatchCount);
                qualityItem.set("pending_quantity", qualityTotalQuantity);
            }
            else if (qualityStatus == "quarantine")
            {
                qualityItem.set("quarantine_batches", qualityBatchCount);
                qualityItem.set("quarantine_quantity", qualityTotalQuantity);
            }
            else if (qualityStatus == "rejected")
            {
                qualityItem.set("rejected_batches", qualityBatchCount);
                qualityItem.set("rejected_quantity", qualityTotalQuantity);
            }
        }
        
        Poco::JSON::Array detailedQualityStats;
        for (const auto& [name, qualityItem] : supplierQualityMap)
        {
            int totalBatchesForSupplier = 
                qualityItem.get("approved_batches") +
                qualityItem.get("pending_batches") +
                qualityItem.get("quarantine_batches") +
                qualityItem.get("rejected_batches");
            
            if (totalBatchesForSupplier > 0)
            {
                double approvalRate = static_cast<double>(qualityItem.get("approved_batches")) / 
                                    totalBatchesForSupplier * 100;
                
                Poco::JSON::Object item = qualityItem;
                item.set("total_batches", totalBatchesForSupplier);
                item.set("approval_rate_percent", std::round(approvalRate * 100) / 100);
                
                detailedQualityStats.add(item);
            }
        }
        
        std::string deliveryStatsSql = R"(
            SELECT 
                s.name as supplier_name,
                COUNT(DISTINCT pb.id) as total_deliveries,
                ROUND(AVG(DATE_PART('day', pb.arrival_date::timestamp - pb.manufacture_date::timestamp))::numeric, 1) as avg_days_from_manufacture,
                MIN(pb.manufacture_date) as earliest_manufacture_date,
                MAX(pb.manufacture_date) as latest_manufacture_date
            FROM suppliers s
            JOIN product_batches pb ON pb.supplier_id = s.id
            WHERE pb.manufacture_date IS NOT NULL
                AND pb.arrival_date IS NOT NULL
                AND s.is_active = TRUE
            GROUP BY s.id, s.name
            HAVING COUNT(DISTINCT pb.id) >= 3
            ORDER BY avg_days_from_manufacture
        )";
        
        Poco::Data::Statement deliveryStmt(session);
        std::string deliverySupplierName, earliestManufactureDate, latestManufactureDate;
        int totalDeliveries;
        double avgDaysFromManufacture;
        
        deliveryStmt << deliveryStatsSql,
            Poco::Data::Keywords::into(deliverySupplierName),
            Poco::Data::Keywords::into(totalDeliveries),
            Poco::Data::Keywords::into(avgDaysFromManufacture),
            Poco::Data::Keywords::into(earliestManufactureDate),
            Poco::Data::Keywords::into(latestManufactureDate);
        
        Poco::JSON::Array deliveryStats;
        deliveryStmt.execute();
        
        while (deliveryStmt.nextDataSet())
        {
            Poco::JSON::Object deliveryItem;
            deliveryItem.set("supplier_name", deliverySupplierName);
            deliveryItem.set("total_deliveries", totalDeliveries);
            deliveryItem.set("avg_days_from_manufacture", avgDaysFromManufacture);
            deliveryItem.set("earliest_manufacture_date", earliestManufactureDate);
            deliveryItem.set("latest_manufacture_date", latestManufactureDate);
            
            std::string deliverySpeed;
            if (avgDaysFromManufacture <= 7) deliverySpeed = "VERY FAST";
            else if (avgDaysFromManufacture <= 14) deliverySpeed = "FAST";
            else if (avgDaysFromManufacture <= 30) deliverySpeed = "AVERAGE";
            else if (avgDaysFromManufacture <= 60) deliverySpeed = "SLOW";
            else deliverySpeed = "VERY SLOW";
            
            deliveryItem.set("delivery_speed_rating", deliverySpeed);
            
            deliveryStats.add(deliveryItem);
        }
        
        Poco::JSON::Object summary;
        summary.set("total_suppliers", static_cast<int>(supplierStats.size()));
        summary.set("active_suppliers", activeSuppliers);
        summary.set("inactive_suppliers", static_cast<int>(supplierStats.size()) - activeSuppliers);
        summary.set("total_stock_value_from_suppliers", totalAllStockValue);
        summary.set("total_batches_from_suppliers", totalAllBatches);
        summary.set("total_products_from_suppliers", totalAllProducts);
        summary.set("avg_batches_per_supplier", static_cast<int>(supplierStats.size()) > 0 ? 
            static_cast<double>(totalAllBatches) / supplierStats.size() : 0);
        summary.set("avg_stock_value_per_supplier", static_cast<int>(supplierStats.size()) > 0 ? 
            totalAllStockValue / supplierStats.size() : 0);
        summary.set("report_generated", utils::DateUtils::formatDateTime(utils::DateUtils::now()));
        
        result.reportData.set("summary", summary);
        result.reportData.set("supplier_performance", supplierStats);
        result.reportData.set("quality_statistics", detailedQualityStats);
        result.reportData.set("delivery_statistics", deliveryStats);
        
        result.success = true;
        result.message = "Supplier performance report generated successfully";
    }
    catch (const std::exception& e)
    {
        result.success = false;
        result.message = "Error generating supplier performance report: " + std::string(e.what());
    }
    
    return result;
}

ReportResult ReportService::getCustomerOrderReport(const std::string& customerEmail)
{
    ReportResult result;
    
    try
    {
        if (customerEmail.empty())
        {
            result.success = false;
            result.message = "Customer email is required";
            return result;
        }
        
        if (!utils::Validator::isValidEmail(customerEmail))
        {
            result.success = false;
            result.message = "Invalid email format";
            return result;
        }
        
        auto connection = database::ConnectionPool::getInstance().acquireConnection();
        if (!connection)
        {
            result.success = false;
            result.message = "Failed to acquire database connection";
            return result;
        }
        
        auto session = connection->getSession();
        
        std::string customerInfoSql = R"(
            SELECT 
                customer_name,
                customer_email,
                customer_phone,
                COUNT(DISTINCT id) as total_orders,
                SUM(total_amount) as total_spent,
                MIN(order_date) as first_order_date,
                MAX(order_date) as last_order_date,
                AVG(total_amount) as avg_order_value
            FROM customer_orders
            WHERE customer_email = ?
            GROUP BY customer_name, customer_email, customer_phone
        )";
        
        Poco::Data::Statement customerStmt(session);
        std::string customerName, customerPhone, firstOrderDate, lastOrderDate;
        int totalOrders;
        double totalSpent, avgOrderValue;

        std::string customerEmailCopy = customerEmail;
        
        customerStmt << customerInfoSql,
            Poco::Data::Keywords::into(customerName),
            Poco::Data::Keywords::into(customerEmailCopy),
            Poco::Data::Keywords::into(customerPhone),
            Poco::Data::Keywords::into(totalOrders),
            Poco::Data::Keywords::into(totalSpent),
            Poco::Data::Keywords::into(firstOrderDate),
            Poco::Data::Keywords::into(lastOrderDate),
            Poco::Data::Keywords::into(avgOrderValue),
            Poco::Data::Keywords::use(customerEmailCopy);
        
        Poco::JSON::Object customerInfo;
        customerStmt.execute();
        
        if (customerStmt.nextDataSet())
        {
            customerInfo.set("customer_name", customerName);
            customerInfo.set("customer_email", customerEmail);
            customerInfo.set("customer_phone", customerPhone);
            customerInfo.set("total_orders", totalOrders);
            customerInfo.set("total_spent", totalSpent);
            customerInfo.set("first_order_date", firstOrderDate);
            customerInfo.set("last_order_date", lastOrderDate);
            customerInfo.set("avg_order_value", avgOrderValue);
            
            if (!firstOrderDate.empty() && !lastOrderDate.empty())
            {
                auto firstDate = utils::DateUtils::parseDate(firstOrderDate);
                auto lastDate = utils::DateUtils::parseDate(lastOrderDate);
                int daysBetween = utils::DateUtils::daysBetween(firstDate, lastDate);
                
                if (daysBetween > 0)
                {
                    double ordersPerMonth = static_cast<double>(totalOrders) / (daysBetween / 30.0);
                    customerInfo.set("orders_per_month", std::round(ordersPerMonth * 100) / 100);
                }
            }
        }
        else
        {
            result.success = false;
            result.message = "Customer not found with email: " + customerEmail;
            return result;
        }
        
        std::string orderHistorySql = R"(
            SELECT 
                co.id,
                co.order_number,
                co.order_date,
                co.status,
                co.priority,
                co.total_amount,
                co.estimated_delivery_date,
                co.actual_delivery_date,
                COUNT(DISTINCT oi.id) as item_count,
                SUM(oi.quantity_ordered) as total_quantity,
                STRING_AGG(DISTINCT s.status::text, ', ') as shipment_statuses
            FROM customer_orders co
            LEFT JOIN order_items oi ON oi.order_id = co.id
            LEFT JOIN shipments s ON s.order_id = co.id
            WHERE co.customer_email = ?
            GROUP BY co.id, co.order_number, co.order_date, co.status, co.priority, 
                     co.total_amount, co.estimated_delivery_date, co.actual_delivery_date
            ORDER BY co.order_date DESC
        )";
        
        Poco::Data::Statement historyStmt(session);
        long long orderId;
        std::string orderNumber, orderDate, status, priority, estimatedDeliveryDate, actualDeliveryDate, shipmentStatuses;
        double orderTotalAmount;
        int itemCount, orderTotalQuantity;
        
        historyStmt << orderHistorySql,
            Poco::Data::Keywords::into(orderId),
            Poco::Data::Keywords::into(orderNumber),
            Poco::Data::Keywords::into(orderDate),
            Poco::Data::Keywords::into(status),
            Poco::Data::Keywords::into(priority),
            Poco::Data::Keywords::into(orderTotalAmount),
            Poco::Data::Keywords::into(estimatedDeliveryDate),
            Poco::Data::Keywords::into(actualDeliveryDate),
            Poco::Data::Keywords::into(itemCount),
            Poco::Data::Keywords::into(orderTotalQuantity),
            Poco::Data::Keywords::into(shipmentStatuses),
            Poco::Data::Keywords::use(customerEmailCopy);
        
        Poco::JSON::Array orderHistory;
        historyStmt.execute();
        
        int completedOrders = 0;
        int cancelledOrders = 0;
        int pendingOrders = 0;
        
        while (historyStmt.nextDataSet())
        {
            Poco::JSON::Object orderItem;
            orderItem.set("order_id", static_cast<Poco::Int64>(orderId));
            orderItem.set("order_number", orderNumber);
            orderItem.set("order_date", orderDate);
            orderItem.set("status", status);
            orderItem.set("priority", priority);
            orderItem.set("total_amount", orderTotalAmount);
            orderItem.set("estimated_delivery_date", estimatedDeliveryDate);
            orderItem.set("actual_delivery_date", actualDeliveryDate);
            orderItem.set("item_count", itemCount);
            orderItem.set("total_quantity", orderTotalQuantity);
            orderItem.set("shipment_statuses", shipmentStatuses);
            orderItem.set("avg_item_price", orderTotalQuantity > 0 ? 
                orderTotalAmount / orderTotalQuantity : 0);
            
            orderHistory.add(orderItem);
            
            if (status == "delivered" || status == "shipped")
                completedOrders++;
            else if (status == "cancelled")
                cancelledOrders++;
            else
                pendingOrders++;
        }
        
        std::string productPreferencesSql = R"(
            SELECT 
                p.sku,
                p.name as product_name,
                c.name as category_name,
                COUNT(DISTINCT oi.order_id) as order_count,
                SUM(oi.quantity_ordered) as total_quantity,
                SUM(oi.quantity_ordered * oi.unit_price * (1 - oi.discount_percent/100)) as total_spent,
                ROUND(AVG(oi.unit_price)::numeric, 2) as avg_unit_price,
                MIN(co.order_date) as first_purchase_date,
                MAX(co.order_date) as last_purchase_date
            FROM products p
            JOIN categories c ON c.id = p.category_id
            JOIN order_items oi ON oi.product_id = p.id
            JOIN customer_orders co ON co.id = oi.order_id
            WHERE co.customer_email = ?
            GROUP BY p.id, p.sku, p.name, c.name
            ORDER BY total_quantity DESC
            LIMIT 10
        )";
        
        Poco::Data::Statement preferencesStmt(session);
        std::string prefSku, prefProductName, prefCategoryName, prefFirstPurchaseDate, prefLastPurchaseDate;
        int prefOrderCount, prefTotalQuantity;
        double prefTotalSpent, prefAvgUnitPrice;
        
        preferencesStmt << productPreferencesSql,
            Poco::Data::Keywords::into(prefSku),
            Poco::Data::Keywords::into(prefProductName),
            Poco::Data::Keywords::into(prefCategoryName),
            Poco::Data::Keywords::into(prefOrderCount),
            Poco::Data::Keywords::into(prefTotalQuantity),
            Poco::Data::Keywords::into(prefTotalSpent),
            Poco::Data::Keywords::into(prefAvgUnitPrice),
            Poco::Data::Keywords::into(prefFirstPurchaseDate),
            Poco::Data::Keywords::into(prefLastPurchaseDate),
            Poco::Data::Keywords::use(customerEmailCopy);
        
        Poco::JSON::Array productPreferences;
        preferencesStmt.execute();
        
        while (preferencesStmt.nextDataSet())
        {
            Poco::JSON::Object preferenceItem;
            preferenceItem.set("sku", prefSku);
            preferenceItem.set("product_name", prefProductName);
            preferenceItem.set("category_name", prefCategoryName);
            preferenceItem.set("order_count", prefOrderCount);
            preferenceItem.set("total_quantity", prefTotalQuantity);
            preferenceItem.set("total_spent", prefTotalSpent);
            preferenceItem.set("avg_unit_price", prefAvgUnitPrice);
            preferenceItem.set("first_purchase_date", prefFirstPurchaseDate);
            preferenceItem.set("last_purchase_date", prefLastPurchaseDate);
            preferenceItem.set("avg_quantity_per_order", prefOrderCount > 0 ? 
                static_cast<double>(prefTotalQuantity) / prefOrderCount : 0);
            
            productPreferences.add(preferenceItem);
        }
        
        std::string categoryPreferencesSql = R"(
            SELECT 
                c.name as category_name,
                COUNT(DISTINCT oi.order_id) as order_count,
                SUM(oi.quantity_ordered) as total_quantity,
                SUM(oi.quantity_ordered * oi.unit_price * (1 - oi.discount_percent/100)) as total_spent,
                COUNT(DISTINCT p.id) as unique_products
            FROM categories c
            JOIN products p ON p.category_id = c.id
            JOIN order_items oi ON oi.product_id = p.id
            JOIN customer_orders co ON co.id = oi.order_id
            WHERE co.customer_email = ?
            GROUP BY c.id, c.name
            ORDER BY total_spent DESC
        )";
        
        Poco::Data::Statement categoryStmt(session);
        std::string catCategoryName;
        int catOrderCount, catTotalQuantity, catUniqueProducts;
        double catTotalSpent;
        
        categoryStmt << categoryPreferencesSql,
            Poco::Data::Keywords::into(catCategoryName),
            Poco::Data::Keywords::into(catOrderCount),
            Poco::Data::Keywords::into(catTotalQuantity),
            Poco::Data::Keywords::into(catTotalSpent),
            Poco::Data::Keywords::into(catUniqueProducts),
            Poco::Data::Keywords::use(customerEmailCopy);
        
        Poco::JSON::Array categoryPreferences;
        categoryStmt.execute();
        
        while (categoryStmt.nextDataSet())
        {
            Poco::JSON::Object categoryItem;
            categoryItem.set("category_name", catCategoryName);
            categoryItem.set("order_count", catOrderCount);
            categoryItem.set("total_quantity", catTotalQuantity);
            categoryItem.set("total_spent", catTotalSpent);
            categoryItem.set("unique_products", catUniqueProducts);
            categoryItem.set("percentage_of_total", totalSpent > 0 ? 
                std::round((catTotalSpent / totalSpent * 100) * 100) / 100 : 0);
            
            categoryPreferences.add(categoryItem);
        }
        
        std::string deliveryStatsSql = R"(
            SELECT 
                s.carrier,
                COUNT(DISTINCT s.id) as shipment_count,
                ROUND(AVG(s.shipping_cost)::numeric, 2) as avg_shipping_cost,
                SUM(s.shipping_cost) as total_shipping_cost,
                ROUND(AVG(DATE_PART('day', s.actual_arrival::timestamp - s.shipment_date::timestamp))::numeric, 1) as avg_delivery_days,
                SUM(CASE WHEN s.status = 'delivered' THEN 1 ELSE 0 END) as delivered_count,
                SUM(CASE WHEN s.status = 'delayed' THEN 1 ELSE 0 END) as delayed_count
            FROM shipments s
            JOIN customer_orders co ON co.id = s.order_id
            WHERE co.customer_email = ?
            GROUP BY s.carrier
            ORDER BY shipment_count DESC
        )";
        
        Poco::Data::Statement deliveryStmt(session);
        std::string carrier;
        int shipmentCount, deliveredCount, delayedCount;
        double avgShippingCost, totalShippingCost, avgDeliveryDays;
        
        deliveryStmt << deliveryStatsSql,
            Poco::Data::Keywords::into(carrier),
            Poco::Data::Keywords::into(shipmentCount),
            Poco::Data::Keywords::into(avgShippingCost),
            Poco::Data::Keywords::into(totalShippingCost),
            Poco::Data::Keywords::into(avgDeliveryDays),
            Poco::Data::Keywords::into(deliveredCount),
            Poco::Data::Keywords::into(delayedCount),
            Poco::Data::Keywords::use(customerEmailCopy);
        
        Poco::JSON::Array deliveryStats;
        deliveryStmt.execute();
        
        while (deliveryStmt.nextDataSet())
        {
            Poco::JSON::Object deliveryItem;
            deliveryItem.set("carrier", carrier);
            deliveryItem.set("shipment_count", shipmentCount);
            deliveryItem.set("avg_shipping_cost", avgShippingCost);
            deliveryItem.set("total_shipping_cost", totalShippingCost);
            deliveryItem.set("avg_delivery_days", avgDeliveryDays);
            deliveryItem.set("delivered_count", deliveredCount);
            deliveryItem.set("delayed_count", delayedCount);
            deliveryItem.set("on_time_rate_percent", shipmentCount > 0 ? 
                std::round((static_cast<double>(deliveredCount) / shipmentCount * 100) * 100) / 100 : 0);
            
            deliveryStats.add(deliveryItem);
        }
        
        Poco::JSON::Object summary;
        summary.set("customer_name", customerName);
        summary.set("customer_email", customerEmail);
        summary.set("total_orders", totalOrders);
        summary.set("completed_orders", completedOrders);
        summary.set("cancelled_orders", cancelledOrders);
        summary.set("pending_orders", pendingOrders);
        summary.set("completion_rate_percent", totalOrders > 0 ? 
            std::round((static_cast<double>(completedOrders) / totalOrders * 100) * 100) / 100 : 0);
        summary.set("total_spent", totalSpent);
        summary.set("avg_order_value", avgOrderValue);
        summary.set("first_order_date", firstOrderDate);
        summary.set("last_order_date", lastOrderDate);
        summary.set("unique_products_ordered", static_cast<int>(productPreferences.size()));
        summary.set("unique_categories_ordered", static_cast<int>(categoryPreferences.size()));
        summary.set("report_generated", utils::DateUtils::formatDateTime(utils::DateUtils::now()));
        
        std::string loyaltyLevel;
        if (totalOrders >= 20 && totalSpent >= 5000) loyaltyLevel = "PLATINUM";
        else if (totalOrders >= 10 && totalSpent >= 1000) loyaltyLevel = "GOLD";
        else if (totalOrders >= 5 && totalSpent >= 500) loyaltyLevel = "SILVER";
        else if (totalOrders >= 2) loyaltyLevel = "BRONZE";
        else loyaltyLevel = "NEW";
        
        summary.set("loyalty_level", loyaltyLevel);
        
        Poco::JSON::Array recommendations;
        
        if (totalOrders == 0)
        {
            recommendations.add("Это новый клиент. Рассмотрите предложение приветственной скидки.");
        }
        else if (totalOrders >= 5 && (utils::DateUtils::daysBetween(
            utils::DateUtils::parseDate(lastOrderDate), 
            utils::DateUtils::now()) > 60))
        {
            recommendations.add("Клиент не делал заказов более 60 дней. Рассмотрите отправку персонализированного предложения.");
        }
        
        if (!categoryPreferences.empty())
        {
            auto firstCategory = categoryPreferences.getObject(0);
            std::string favoriteCategory = firstCategory->get("category_name");
            recommendations.add("Любимая категория клиента: " + favoriteCategory + 
                               ". Рекомендуйте новые товары из этой категории.");
        }
        
        summary.set("recommendations", recommendations);
        
        result.reportData.set("customer_summary", summary);
        result.reportData.set("order_history", orderHistory);
        result.reportData.set("product_preferences", productPreferences);
        result.reportData.set("category_preferences", categoryPreferences);
        result.reportData.set("delivery_statistics", deliveryStats);
        
        result.success = true;
        result.message = "Customer order report generated successfully";
    }
    catch (const std::exception& e)
    {
        result.success = false;
        result.message = "Error generating customer order report: " + std::string(e.what());
    }
    
    return result;
}

} // namespace services
