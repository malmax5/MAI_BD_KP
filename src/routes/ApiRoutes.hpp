#pragma once

#include <Poco/Net/HTTPRequestHandlerFactory.h>
#include <Poco/Net/HTTPRequestHandler.h>
#include <Poco/Net/HTTPServerRequest.h>
#include <Poco/URI.h>
#include <memory>
#include <string>
#include <map>

namespace warehouse_backend::routes
{

class ApiRoutes : public Poco::Net::HTTPRequestHandlerFactory
{
public:
    ApiRoutes();
    virtual ~ApiRoutes() = default;
    
    Poco::Net::HTTPRequestHandler* createRequestHandler(const Poco::Net::HTTPServerRequest& request) override;
    
    static std::string getRouteKey(const std::string& method, const std::string& path);
    static bool matchesPattern(const std::string& routePattern, const std::string& requestPath);
    static std::map<std::string, std::string> extractPathParams(const std::string& routePattern, const std::string& requestPath);
    
    static const std::string API_BASE_PATH;
    static const std::string API_VERSION;
    static const std::string FULL_API_BASE_PATH;
    
    // Auth routes
    static const std::string ROUTE_AUTH_REGISTER;
    static const std::string ROUTE_AUTH_LOGIN;
    static const std::string ROUTE_AUTH_REFRESH;
    static const std::string ROUTE_AUTH_LOGOUT;
    static const std::string ROUTE_AUTH_FORGOT_PASSWORD;
    static const std::string ROUTE_AUTH_RESET_PASSWORD;
    static const std::string ROUTE_AUTH_CHANGE_PASSWORD;
    static const std::string ROUTE_AUTH_PROFILE;
    static const std::string ROUTE_AUTH_VERIFY_EMAIL;
    
    // User routes
    static const std::string ROUTE_USERS_LIST;
    static const std::string ROUTE_USERS_CREATE;
    static const std::string ROUTE_USERS_GET;
    static const std::string ROUTE_USERS_UPDATE;
    static const std::string ROUTE_USERS_DELETE;
    static const std::string ROUTE_USERS_SEARCH;
    static const std::string ROUTE_USERS_STATISTICS;
    static const std::string ROUTE_USERS_IMPORT;
    static const std::string ROUTE_USERS_EXPORT;
    
    // Product routes
    static const std::string ROUTE_PRODUCTS_LIST;
    static const std::string ROUTE_PRODUCTS_CREATE;
    static const std::string ROUTE_PRODUCTS_GET;
    static const std::string ROUTE_PRODUCTS_UPDATE;
    static const std::string ROUTE_PRODUCTS_DELETE;
    static const std::string ROUTE_PRODUCTS_SEARCH;
    static const std::string ROUTE_PRODUCTS_CATEGORY;
    static const std::string ROUTE_PRODUCTS_STOCK;
    
    // Order routes
    static const std::string ROUTE_ORDERS_LIST;
    static const std::string ROUTE_ORDERS_CREATE;
    static const std::string ROUTE_ORDERS_GET;
    static const std::string ROUTE_ORDERS_UPDATE;
    static const std::string ROUTE_ORDERS_DELETE;
    static const std::string ROUTE_ORDERS_CANCEL;
    static const std::string ROUTE_ORDERS_STATUS;
    static const std::string ROUTE_ORDERS_ITEMS_GET;
    static const std::string ROUTE_ORDERS_ITEMS_CREATE;
    static const std::string ROUTE_ORDERS_ITEMS_UPDATE;
    static const std::string ROUTE_ORDERS_ITEMS_DELETE;
    static const std::string ROUTE_ORDERS_PRIORITY;
    static const std::string ROUTE_ORDERS_SEARCH;
    static const std::string ROUTE_ORDERS_CUSTOMER;
    static const std::string ROUTE_ORDERS_BY_STATUS;
    static const std::string ROUTE_ORDERS_PENDING;
    static const std::string ROUTE_ORDERS_URGENT;
    static const std::string ROUTE_ORDERS_STATISTICS;
    static const std::string ROUTE_ORDERS_REVENUE_REPORT;
    static const std::string ROUTE_ORDERS_CUSTOMER_HISTORY;
    static const std::string ROUTE_ORDERS_SHIP;
    static const std::string ROUTE_ORDERS_DELIVER;
    
    // Categories routes
    static const std::string ROUTE_CATEGORIES_LIST;
    static const std::string ROUTE_CATEGORIES_CREATE;
    static const std::string ROUTE_CATEGORIES_GET;
    static const std::string ROUTE_CATEGORIES_UPDATE;
    static const std::string ROUTE_CATEGORIES_DELETE;
    static const std::string ROUTE_CATEGORIES_TREE;
    static const std::string ROUTE_CATEGORIES_TREE_ID;
    static const std::string ROUTE_CATEGORIES_ROOTS;
    static const std::string ROUTE_CATEGORIES_CHILDREN;
    static const std::string ROUTE_CATEGORIES_PRODUCTS;
    static const std::string ROUTE_CATEGORIES_STATISTICS;
    static const std::string ROUTE_CATEGORIES_IMPORT;
    static const std::string ROUTE_CATEGORIES_EXPORT;
    static const std::string ROUTE_CATEGORIES_MOVE;
    
    // Suppliers routes
    static const std::string ROUTE_SUPPLIERS_LIST;
    static const std::string ROUTE_SUPPLIERS_CREATE;
    static const std::string ROUTE_SUPPLIERS_GET;
    static const std::string ROUTE_SUPPLIERS_UPDATE;
    static const std::string ROUTE_SUPPLIERS_DELETE;
    
    // ProductBatch routes
    static const std::string ROUTE_PRODUCT_BATCHES_LIST;
    static const std::string ROUTE_PRODUCT_BATCHES_CREATE;
    static const std::string ROUTE_PRODUCT_BATCHES_GET;
    static const std::string ROUTE_PRODUCT_BATCHES_UPDATE;
    static const std::string ROUTE_PRODUCT_BATCHES_DELETE;
    static const std::string ROUTE_PRODUCT_BATCHES_QUALITY_UPDATE;
    static const std::string ROUTE_PRODUCT_BATCHES_BY_NUMBER;
    static const std::string ROUTE_PRODUCT_BATCHES_BY_PRODUCT;
    static const std::string ROUTE_PRODUCT_BATCHES_BY_SUPPLIER;
    static const std::string ROUTE_PRODUCT_BATCHES_EXPIRING;
    static const std::string ROUTE_PRODUCT_BATCHES_NEEDING_INSPECTION;
    static const std::string ROUTE_PRODUCT_BATCHES_STATISTICS;
    static const std::string ROUTE_PRODUCT_BATCHES_EXPIRATION_REPORT;
    static const std::string ROUTE_PRODUCT_BATCHES_QUALITY_REPORT;
    static const std::string ROUTE_PRODUCT_BATCHES_CHECK_AVAILABILITY;
    static const std::string ROUTE_PRODUCT_BATCHES_SEARCH;

    // Order Payment routes
    static const std::string ROUTE_PAYMENTS_LIST;
    static const std::string ROUTE_PAYMENTS_CREATE;
    static const std::string ROUTE_PAYMENTS_GET;
    static const std::string ROUTE_PAYMENTS_UPDATE;
    static const std::string ROUTE_PAYMENTS_DELETE;
    static const std::string ROUTE_PAYMENTS_STATUS_UPDATE;
    static const std::string ROUTE_PAYMENTS_BY_ORDER;
    static const std::string ROUTE_PAYMENTS_SEARCH;
    static const std::string ROUTE_PAYMENTS_STATISTICS;
    static const std::string ROUTE_PAYMENTS_REPORT;
    static const std::string ROUTE_PAYMENTS_HISTORY;
    static const std::string ROUTE_PAYMENTS_ANALYSIS_METHODS;
    static const std::string ROUTE_PAYMENTS_PENDING;
    static const std::string ROUTE_PAYMENTS_PAID;
    static const std::string ROUTE_PAYMENTS_FAILED;
    static const std::string ROUTE_PAYMENTS_REFUNDED;
    static const std::string ROUTE_PAYMENTS_OVERDUE;
    static const std::string ROUTE_PAYMENTS_PROCESS;
    static const std::string ROUTE_PAYMENTS_REFUND;
    static const std::string ROUTE_PAYMENTS_MARK_PAID;
    static const std::string ROUTE_PAYMENTS_MARK_FAILED;
    static const std::string ROUTE_PAYMENTS_MARK_REFUNDED;
    static const std::string ROUTE_PAYMENTS_MARK_PENDING;

    // Warehouse Cell routes
    static const std::string ROUTE_WAREHOUSE_CELLS_LIST;
    static const std::string ROUTE_WAREHOUSE_CELLS_CREATE;
    static const std::string ROUTE_WAREHOUSE_CELLS_GET;
    static const std::string ROUTE_WAREHOUSE_CELLS_UPDATE;
    static const std::string ROUTE_WAREHOUSE_CELLS_DELETE;
    static const std::string ROUTE_WAREHOUSE_CELLS_CLEAR;
    static const std::string ROUTE_WAREHOUSE_CELLS_BLOCK;
    static const std::string ROUTE_WAREHOUSE_CELLS_UNBLOCK;
    static const std::string ROUTE_WAREHOUSE_CELLS_BY_CODE;
    static const std::string ROUTE_WAREHOUSE_CELLS_AVAILABLE;
    static const std::string ROUTE_WAREHOUSE_CELLS_BY_ZONE;
    static const std::string ROUTE_WAREHOUSE_CELLS_BY_STATUS;
    static const std::string ROUTE_WAREHOUSE_CELLS_BATCHES;
    static const std::string ROUTE_WAREHOUSE_CELLS_FIND_BEST;
    static const std::string ROUTE_WAREHOUSE_CELLS_STATISTICS;
    static const std::string ROUTE_WAREHOUSE_CELLS_OCCUPANCY_REPORT;
    static const std::string ROUTE_WAREHOUSE_CELLS_SEARCH;

    // Shipment routes
    static const std::string ROUTE_SHIPMENTS_LIST;
    static const std::string ROUTE_SHIPMENTS_CREATE;
    static const std::string ROUTE_SHIPMENTS_GET;
    static const std::string ROUTE_SHIPMENTS_UPDATE;
    static const std::string ROUTE_SHIPMENTS_TRACK;
    static const std::string ROUTE_SHIPMENTS_STATUS_UPDATE;
    static const std::string ROUTE_SHIPMENTS_TRACKING_UPDATE;
    static const std::string ROUTE_SHIPMENTS_CANCEL;
    static const std::string ROUTE_SHIPMENTS_DELIVERED;
    static const std::string ROUTE_SHIPMENTS_BY_ORDER;
    static const std::string ROUTE_SHIPMENTS_BY_STATUS;
    static const std::string ROUTE_SHIPMENTS_BY_CARRIER;
    static const std::string ROUTE_SHIPMENTS_STATISTICS;
    static const std::string ROUTE_SHIPMENTS_CARRIER_PERFORMANCE;
    static const std::string ROUTE_SHIPMENTS_COST_ANALYSIS;
    static const std::string ROUTE_SHIPMENTS_DELAYED;
    static const std::string ROUTE_SHIPMENTS_DUE_TODAY;
    
    // Inventory Movement routes
    static const std::string ROUTE_INVENTORY_MOVEMENTS_LIST;
    static const std::string ROUTE_INVENTORY_MOVEMENTS_GET;
    static const std::string ROUTE_INVENTORY_MOVEMENTS_RECEIPT;
    static const std::string ROUTE_INVENTORY_MOVEMENTS_TRANSFER;
    static const std::string ROUTE_INVENTORY_MOVEMENTS_ADJUSTMENT;
    static const std::string ROUTE_INVENTORY_MOVEMENTS_CANCEL;
    static const std::string ROUTE_INVENTORY_MOVEMENTS_STATUS;
    static const std::string ROUTE_INVENTORY_MOVEMENTS_PRODUCT;
    static const std::string ROUTE_INVENTORY_MOVEMENTS_CELL;
    static const std::string ROUTE_INVENTORY_MOVEMENTS_STATISTICS;
    static const std::string ROUTE_INVENTORY_MOVEMENTS_REPORT;
    static const std::string ROUTE_INVENTORY_CHECK_STOCK;
    static const std::string ROUTE_INVENTORY_EXPIRING;
    static const std::string ROUTE_INVENTORY_LOW_STOCK;
    static const std::string ROUTE_INVENTORY_STATISTICS;

    // Batch Import
    static const std::string ROUTE_USERS_BATCH_UPLOAD;
    static const std::string ROUTE_USERS_BATCH_TEMPLATE;
    static const std::string ROUTE_PRODUCTS_BATCH_UPLOAD;
    static const std::string ROUTE_PRODUCTS_BATCH_TEMPLATE;
    static const std::string ROUTE_CATEGORIES_BATCH_UPLOAD;
    static const std::string ROUTE_CATEGORIES_BATCH_TEMPLATE;
    static const std::string ROUTE_WAREHOUSE_CELLS_BATCH_UPLOAD;
    static const std::string ROUTE_WAREHOUSE_CELLS_BATCH_TEMPLATE;
    static const std::string ROUTE_SUPPLIERS_BATCH_UPLOAD;
    static const std::string ROUTE_SUPPLIERS_BATCH_TEMPLATE;
    
    static const std::string ROUTE_REPORTS_GENERATE;
    static const std::string ROUTE_REPORTS_DOWNLOAD;
    static const std::string ROUTE_REPORTS_STATISTICS;
    
    static const std::string ROUTE_HEALTH_CHECK;
    static const std::string ROUTE_SYSTEM_INFO;
    static const std::string ROUTE_API_DOCS;
    
private:
    void initializeRoutes();
    
    struct RouteInfo
    {
        std::string method;
        std::string path;
        std::string handlerName;
        bool requiresAuth;
        std::vector<std::string> requiredPermissions;
    };
    
    std::vector<RouteInfo> routes;
    std::map<std::string, RouteInfo> routeMap;
};

} // namespace routes
