#include "ApiRoutes.hpp"
#include "../controllers/AuthController.hpp"
#include "../controllers/UserController.hpp"
#include "../controllers/ProductController.hpp"
#include "../controllers/OrderController.hpp"
#include "../controllers/SupplierController.hpp"
#include "../controllers/CategoryController.hpp"
#include "../controllers/WarehouseCellController.hpp"
#include "../controllers/ProductBatchController.hpp"
#include "../controllers/InventoryMovementController.hpp"
#include "../controllers/ShipmentController.hpp"
#include "../controllers/OrderPaymentController.hpp"
#include "../controllers/BatchImportController.hpp"
#include <Poco/Net/HTTPServerRequest.h>
#include <Poco/Net/HTTPServerResponse.h>
#include <Poco/URI.h>
#include <Poco/StringTokenizer.h>
#include <Poco/RegularExpression.h>
#include <Poco/JSON/Object.h>
#include <memory>
#include <fstream>
#include <sstream>

namespace warehouse_backend::routes
{

// Константы API
const std::string ApiRoutes::API_BASE_PATH = "/api";
const std::string ApiRoutes::API_VERSION = "/v1";
const std::string ApiRoutes::FULL_API_BASE_PATH = API_BASE_PATH + API_VERSION;

// Auth routes (+)
const std::string ApiRoutes::ROUTE_AUTH_REGISTER = FULL_API_BASE_PATH + "/auth/register";
const std::string ApiRoutes::ROUTE_AUTH_LOGIN = FULL_API_BASE_PATH + "/auth/login";
const std::string ApiRoutes::ROUTE_AUTH_REFRESH = FULL_API_BASE_PATH + "/auth/refresh";
const std::string ApiRoutes::ROUTE_AUTH_LOGOUT = FULL_API_BASE_PATH + "/auth/logout";
const std::string ApiRoutes::ROUTE_AUTH_FORGOT_PASSWORD = FULL_API_BASE_PATH + "/auth/forgot-password";
const std::string ApiRoutes::ROUTE_AUTH_RESET_PASSWORD = FULL_API_BASE_PATH + "/auth/reset-password";
const std::string ApiRoutes::ROUTE_AUTH_CHANGE_PASSWORD = FULL_API_BASE_PATH + "/auth/change-password";
const std::string ApiRoutes::ROUTE_AUTH_PROFILE = FULL_API_BASE_PATH + "/auth/profile";
const std::string ApiRoutes::ROUTE_AUTH_VERIFY_EMAIL = FULL_API_BASE_PATH + "/auth/verify-email";

// User routes (+)
const std::string ApiRoutes::ROUTE_USERS_LIST = FULL_API_BASE_PATH + "/users";
const std::string ApiRoutes::ROUTE_USERS_CREATE = FULL_API_BASE_PATH + "/users";
const std::string ApiRoutes::ROUTE_USERS_GET = FULL_API_BASE_PATH + "/users/{id}";
const std::string ApiRoutes::ROUTE_USERS_UPDATE = FULL_API_BASE_PATH + "/users/{id}";
const std::string ApiRoutes::ROUTE_USERS_DELETE = FULL_API_BASE_PATH + "/users/{id}";
const std::string ApiRoutes::ROUTE_USERS_SEARCH = FULL_API_BASE_PATH + "/users/search";
const std::string ApiRoutes::ROUTE_USERS_STATISTICS = FULL_API_BASE_PATH + "/users/statistics";
const std::string ApiRoutes::ROUTE_USERS_IMPORT = FULL_API_BASE_PATH + "/users/import";
const std::string ApiRoutes::ROUTE_USERS_EXPORT = FULL_API_BASE_PATH + "/users/export";

// Product routes (+)
const std::string ApiRoutes::ROUTE_PRODUCTS_LIST = FULL_API_BASE_PATH + "/products";
const std::string ApiRoutes::ROUTE_PRODUCTS_CREATE = FULL_API_BASE_PATH + "/products";
const std::string ApiRoutes::ROUTE_PRODUCTS_GET = FULL_API_BASE_PATH + "/products/{id}";
const std::string ApiRoutes::ROUTE_PRODUCTS_UPDATE = FULL_API_BASE_PATH + "/products/{id}";
const std::string ApiRoutes::ROUTE_PRODUCTS_DELETE = FULL_API_BASE_PATH + "/products/{id}";
const std::string ApiRoutes::ROUTE_PRODUCTS_SEARCH = FULL_API_BASE_PATH + "/products/search";
const std::string ApiRoutes::ROUTE_PRODUCTS_CATEGORY = FULL_API_BASE_PATH + "/products/category/{id}";
const std::string ApiRoutes::ROUTE_PRODUCTS_STOCK = FULL_API_BASE_PATH + "/products/{id}/stock";

// Order routes (+)
const std::string ApiRoutes::ROUTE_ORDERS_LIST = FULL_API_BASE_PATH + "/orders";
const std::string ApiRoutes::ROUTE_ORDERS_CREATE = FULL_API_BASE_PATH + "/orders";
const std::string ApiRoutes::ROUTE_ORDERS_GET = FULL_API_BASE_PATH + "/orders/{id}";
const std::string ApiRoutes::ROUTE_ORDERS_UPDATE = FULL_API_BASE_PATH + "/orders/{id}";
const std::string ApiRoutes::ROUTE_ORDERS_DELETE = FULL_API_BASE_PATH + "/orders/{id}";
const std::string ApiRoutes::ROUTE_ORDERS_CANCEL = FULL_API_BASE_PATH + "/orders/{id}/cancel";
const std::string ApiRoutes::ROUTE_ORDERS_STATUS = FULL_API_BASE_PATH + "/orders/{id}/status";
const std::string ApiRoutes::ROUTE_ORDERS_ITEMS_GET = FULL_API_BASE_PATH + "/orders/{id}/items";
const std::string ApiRoutes::ROUTE_ORDERS_ITEMS_CREATE = FULL_API_BASE_PATH + "/orders/{id}/items";
const std::string ApiRoutes::ROUTE_ORDERS_ITEMS_UPDATE = FULL_API_BASE_PATH + "/orders/{id}/items/{id}";
const std::string ApiRoutes::ROUTE_ORDERS_ITEMS_DELETE = FULL_API_BASE_PATH + "/orders/{id}/items/{id}";
const std::string ApiRoutes::ROUTE_ORDERS_PRIORITY = FULL_API_BASE_PATH + "/orders/{id}/priority";
const std::string ApiRoutes::ROUTE_ORDERS_SEARCH = FULL_API_BASE_PATH + "/orders/search";
const std::string ApiRoutes::ROUTE_ORDERS_CUSTOMER = FULL_API_BASE_PATH + "/orders/customer";
const std::string ApiRoutes::ROUTE_ORDERS_BY_STATUS = FULL_API_BASE_PATH + "/orders/status";
const std::string ApiRoutes::ROUTE_ORDERS_PENDING = FULL_API_BASE_PATH + "/orders/pending";
const std::string ApiRoutes::ROUTE_ORDERS_URGENT = FULL_API_BASE_PATH + "/orders/urgent";
const std::string ApiRoutes::ROUTE_ORDERS_STATISTICS = FULL_API_BASE_PATH + "/orders/statistics";
const std::string ApiRoutes::ROUTE_ORDERS_REVENUE_REPORT = FULL_API_BASE_PATH + "/orders/revenue-report";
const std::string ApiRoutes::ROUTE_ORDERS_CUSTOMER_HISTORY = FULL_API_BASE_PATH + "/orders/customer-history";
const std::string ApiRoutes::ROUTE_ORDERS_SHIP = FULL_API_BASE_PATH + "/orders/{id}/ship";
const std::string ApiRoutes::ROUTE_ORDERS_DELIVER = FULL_API_BASE_PATH + "/orders/{id}/deliver";

// Category routes (+)
const std::string ApiRoutes::ROUTE_CATEGORIES_LIST = FULL_API_BASE_PATH + "/categories";
const std::string ApiRoutes::ROUTE_CATEGORIES_CREATE = FULL_API_BASE_PATH + "/categories";
const std::string ApiRoutes::ROUTE_CATEGORIES_GET = FULL_API_BASE_PATH + "/categories/{id}";
const std::string ApiRoutes::ROUTE_CATEGORIES_UPDATE = FULL_API_BASE_PATH + "/categories/{id}";
const std::string ApiRoutes::ROUTE_CATEGORIES_DELETE = FULL_API_BASE_PATH + "/categories/{id}";
const std::string ApiRoutes::ROUTE_CATEGORIES_TREE = FULL_API_BASE_PATH + "/categories/tree";
const std::string ApiRoutes::ROUTE_CATEGORIES_TREE_ID = FULL_API_BASE_PATH + "/categories/{id}/tree";
const std::string ApiRoutes::ROUTE_CATEGORIES_ROOTS = FULL_API_BASE_PATH + "/categories/roots";
const std::string ApiRoutes::ROUTE_CATEGORIES_CHILDREN = FULL_API_BASE_PATH + "/categories/children";
const std::string ApiRoutes::ROUTE_CATEGORIES_PRODUCTS = FULL_API_BASE_PATH + "/categories/{id}/products";
const std::string ApiRoutes::ROUTE_CATEGORIES_STATISTICS = FULL_API_BASE_PATH + "/categories/statistics";
const std::string ApiRoutes::ROUTE_CATEGORIES_IMPORT = FULL_API_BASE_PATH + "/categories/import";
const std::string ApiRoutes::ROUTE_CATEGORIES_EXPORT = FULL_API_BASE_PATH + "/categories/export";
const std::string ApiRoutes::ROUTE_CATEGORIES_MOVE = FULL_API_BASE_PATH + "/categories/{id}/move";

// Supplier routes (+ -)
const std::string ApiRoutes::ROUTE_SUPPLIERS_LIST = FULL_API_BASE_PATH + "/suppliers";
const std::string ApiRoutes::ROUTE_SUPPLIERS_CREATE = FULL_API_BASE_PATH + "/suppliers";
const std::string ApiRoutes::ROUTE_SUPPLIERS_GET = FULL_API_BASE_PATH + "/suppliers/{id}";
const std::string ApiRoutes::ROUTE_SUPPLIERS_UPDATE = FULL_API_BASE_PATH + "/suppliers/{id}";
const std::string ApiRoutes::ROUTE_SUPPLIERS_DELETE = FULL_API_BASE_PATH + "/suppliers/{id}";

// Product Batch routes
const std::string ApiRoutes::ROUTE_PRODUCT_BATCHES_LIST = FULL_API_BASE_PATH + "/product-batches";
const std::string ApiRoutes::ROUTE_PRODUCT_BATCHES_CREATE = FULL_API_BASE_PATH + "/product-batches";
const std::string ApiRoutes::ROUTE_PRODUCT_BATCHES_GET = FULL_API_BASE_PATH + "/product-batches/{id}";
const std::string ApiRoutes::ROUTE_PRODUCT_BATCHES_UPDATE = FULL_API_BASE_PATH + "/product-batches/{id}";
const std::string ApiRoutes::ROUTE_PRODUCT_BATCHES_DELETE = FULL_API_BASE_PATH + "/product-batches/{id}";
const std::string ApiRoutes::ROUTE_PRODUCT_BATCHES_QUALITY_UPDATE = FULL_API_BASE_PATH + "/product-batches/{id}/quality";
const std::string ApiRoutes::ROUTE_PRODUCT_BATCHES_BY_NUMBER = FULL_API_BASE_PATH + "/product-batches/number";
const std::string ApiRoutes::ROUTE_PRODUCT_BATCHES_BY_PRODUCT = FULL_API_BASE_PATH + "/product-batches/product";
const std::string ApiRoutes::ROUTE_PRODUCT_BATCHES_BY_SUPPLIER = FULL_API_BASE_PATH + "/product-batches/supplier";
const std::string ApiRoutes::ROUTE_PRODUCT_BATCHES_EXPIRING = FULL_API_BASE_PATH + "/product-batches/expiring";
const std::string ApiRoutes::ROUTE_PRODUCT_BATCHES_NEEDING_INSPECTION = FULL_API_BASE_PATH + "/product-batches/needing-inspection";
const std::string ApiRoutes::ROUTE_PRODUCT_BATCHES_STATISTICS = FULL_API_BASE_PATH + "/product-batches/statistics";
const std::string ApiRoutes::ROUTE_PRODUCT_BATCHES_EXPIRATION_REPORT = FULL_API_BASE_PATH + "/product-batches/expiration-report";
const std::string ApiRoutes::ROUTE_PRODUCT_BATCHES_QUALITY_REPORT = FULL_API_BASE_PATH + "/product-batches/quality-report";
const std::string ApiRoutes::ROUTE_PRODUCT_BATCHES_CHECK_AVAILABILITY = FULL_API_BASE_PATH + "/product-batches/check-availability";
const std::string ApiRoutes::ROUTE_PRODUCT_BATCHES_SEARCH = FULL_API_BASE_PATH + "/product-batches/search";

// Order Payment routes
const std::string ApiRoutes::ROUTE_PAYMENTS_LIST = FULL_API_BASE_PATH + "/payments";
const std::string ApiRoutes::ROUTE_PAYMENTS_CREATE = FULL_API_BASE_PATH + "/payments";
const std::string ApiRoutes::ROUTE_PAYMENTS_GET = FULL_API_BASE_PATH + "/payments/{id}";
const std::string ApiRoutes::ROUTE_PAYMENTS_UPDATE = FULL_API_BASE_PATH + "/payments/{id}";
const std::string ApiRoutes::ROUTE_PAYMENTS_DELETE = FULL_API_BASE_PATH + "/payments/{id}";
const std::string ApiRoutes::ROUTE_PAYMENTS_STATUS_UPDATE = FULL_API_BASE_PATH + "/payments/{id}/status";
const std::string ApiRoutes::ROUTE_PAYMENTS_BY_ORDER = FULL_API_BASE_PATH + "/payments/order/{orderId}";
const std::string ApiRoutes::ROUTE_PAYMENTS_SEARCH = FULL_API_BASE_PATH + "/payments/search";
const std::string ApiRoutes::ROUTE_PAYMENTS_STATISTICS = FULL_API_BASE_PATH + "/payments/statistics";
const std::string ApiRoutes::ROUTE_PAYMENTS_REPORT = FULL_API_BASE_PATH + "/payments/report";
const std::string ApiRoutes::ROUTE_PAYMENTS_HISTORY = FULL_API_BASE_PATH + "/payments/history";
const std::string ApiRoutes::ROUTE_PAYMENTS_ANALYSIS_METHODS = FULL_API_BASE_PATH + "/payments/analysis/methods";
const std::string ApiRoutes::ROUTE_PAYMENTS_PROCESS = FULL_API_BASE_PATH + "/payments/{id}/process";
const std::string ApiRoutes::ROUTE_PAYMENTS_REFUND = FULL_API_BASE_PATH + "/payments/{id}/refund";
const std::string ApiRoutes::ROUTE_PAYMENTS_PENDING = FULL_API_BASE_PATH + "/payments/pending";
const std::string ApiRoutes::ROUTE_PAYMENTS_PAID = FULL_API_BASE_PATH + "/payments/paid";
const std::string ApiRoutes::ROUTE_PAYMENTS_FAILED = FULL_API_BASE_PATH + "/payments/failed";
const std::string ApiRoutes::ROUTE_PAYMENTS_REFUNDED = FULL_API_BASE_PATH + "/payments/refunded";
const std::string ApiRoutes::ROUTE_PAYMENTS_OVERDUE = FULL_API_BASE_PATH + "/payments/overdue";
const std::string ApiRoutes::ROUTE_PAYMENTS_MARK_PAID = FULL_API_BASE_PATH + "/payments/{id}/paid";
const std::string ApiRoutes::ROUTE_PAYMENTS_MARK_FAILED = FULL_API_BASE_PATH + "/payments/{id}/failed";
const std::string ApiRoutes::ROUTE_PAYMENTS_MARK_REFUNDED = FULL_API_BASE_PATH + "/payments/{id}/refunded";
const std::string ApiRoutes::ROUTE_PAYMENTS_MARK_PENDING = FULL_API_BASE_PATH + "/payments/{id}/pending";

// Warehouse Cell routes
const std::string ApiRoutes::ROUTE_WAREHOUSE_CELLS_LIST = FULL_API_BASE_PATH + "/warehouse-cells";
const std::string ApiRoutes::ROUTE_WAREHOUSE_CELLS_CREATE = FULL_API_BASE_PATH + "/warehouse-cells";
const std::string ApiRoutes::ROUTE_WAREHOUSE_CELLS_GET = FULL_API_BASE_PATH + "/warehouse-cells/{id}";
const std::string ApiRoutes::ROUTE_WAREHOUSE_CELLS_UPDATE = FULL_API_BASE_PATH + "/warehouse-cells/{id}";
const std::string ApiRoutes::ROUTE_WAREHOUSE_CELLS_DELETE = FULL_API_BASE_PATH + "/warehouse-cells/{id}";
const std::string ApiRoutes::ROUTE_WAREHOUSE_CELLS_CLEAR = FULL_API_BASE_PATH + "/warehouse-cells/{id}/clear";
const std::string ApiRoutes::ROUTE_WAREHOUSE_CELLS_BLOCK = FULL_API_BASE_PATH + "/warehouse-cells/{id}/block";
const std::string ApiRoutes::ROUTE_WAREHOUSE_CELLS_UNBLOCK = FULL_API_BASE_PATH + "/warehouse-cells/{id}/unblock";
const std::string ApiRoutes::ROUTE_WAREHOUSE_CELLS_BY_CODE = FULL_API_BASE_PATH + "/warehouse-cells/code";
const std::string ApiRoutes::ROUTE_WAREHOUSE_CELLS_AVAILABLE = FULL_API_BASE_PATH + "/warehouse-cells/available";
const std::string ApiRoutes::ROUTE_WAREHOUSE_CELLS_BY_ZONE = FULL_API_BASE_PATH + "/warehouse-cells/zone";
const std::string ApiRoutes::ROUTE_WAREHOUSE_CELLS_BY_STATUS = FULL_API_BASE_PATH + "/warehouse-cells/status";
const std::string ApiRoutes::ROUTE_WAREHOUSE_CELLS_BATCHES = FULL_API_BASE_PATH + "/warehouse-cells/{id}/batches";
const std::string ApiRoutes::ROUTE_WAREHOUSE_CELLS_FIND_BEST = FULL_API_BASE_PATH + "/warehouse-cells/find-best";
const std::string ApiRoutes::ROUTE_WAREHOUSE_CELLS_STATISTICS = FULL_API_BASE_PATH + "/warehouse-cells/statistics";
const std::string ApiRoutes::ROUTE_WAREHOUSE_CELLS_OCCUPANCY_REPORT = FULL_API_BASE_PATH + "/warehouse-cells/occupancy-report";
const std::string ApiRoutes::ROUTE_WAREHOUSE_CELLS_SEARCH = FULL_API_BASE_PATH + "/warehouse-cells/search";

// Shipment routes
const std::string ApiRoutes::ROUTE_SHIPMENTS_LIST = FULL_API_BASE_PATH + "/shipments";
const std::string ApiRoutes::ROUTE_SHIPMENTS_CREATE = FULL_API_BASE_PATH + "/shipments";
const std::string ApiRoutes::ROUTE_SHIPMENTS_GET = FULL_API_BASE_PATH + "/shipments/{id}";
const std::string ApiRoutes::ROUTE_SHIPMENTS_UPDATE = FULL_API_BASE_PATH + "/shipments/{id}";
const std::string ApiRoutes::ROUTE_SHIPMENTS_STATUS_UPDATE = FULL_API_BASE_PATH + "/shipments/{id}/status";
const std::string ApiRoutes::ROUTE_SHIPMENTS_TRACKING_UPDATE = FULL_API_BASE_PATH + "/shipments/{id}/tracking";
const std::string ApiRoutes::ROUTE_SHIPMENTS_CANCEL = FULL_API_BASE_PATH + "/shipments/{id}/cancel";
const std::string ApiRoutes::ROUTE_SHIPMENTS_DELIVERED = FULL_API_BASE_PATH + "/shipments/{id}/delivered";
const std::string ApiRoutes::ROUTE_SHIPMENTS_BY_ORDER = FULL_API_BASE_PATH + "/shipments/order";
const std::string ApiRoutes::ROUTE_SHIPMENTS_BY_STATUS = FULL_API_BASE_PATH + "/shipments/status";
const std::string ApiRoutes::ROUTE_SHIPMENTS_BY_CARRIER = FULL_API_BASE_PATH + "/shipments/carrier";
const std::string ApiRoutes::ROUTE_SHIPMENTS_STATISTICS = FULL_API_BASE_PATH + "/shipments/statistics";
const std::string ApiRoutes::ROUTE_SHIPMENTS_CARRIER_PERFORMANCE = FULL_API_BASE_PATH + "/shipments/carrier-performance";
const std::string ApiRoutes::ROUTE_SHIPMENTS_COST_ANALYSIS = FULL_API_BASE_PATH + "/shipments/shipping-cost-analysis";
const std::string ApiRoutes::ROUTE_SHIPMENTS_DELAYED = FULL_API_BASE_PATH + "/shipments/delayed";
const std::string ApiRoutes::ROUTE_SHIPMENTS_DUE_TODAY = FULL_API_BASE_PATH + "/shipments/due-today";

// Inventory Movement routes
const std::string ApiRoutes::ROUTE_INVENTORY_MOVEMENTS_LIST = FULL_API_BASE_PATH + "/inventory/movements";
const std::string ApiRoutes::ROUTE_INVENTORY_MOVEMENTS_GET = FULL_API_BASE_PATH + "/inventory/movements/{id}";
const std::string ApiRoutes::ROUTE_INVENTORY_MOVEMENTS_RECEIPT = FULL_API_BASE_PATH + "/inventory/movements/receipt";
const std::string ApiRoutes::ROUTE_INVENTORY_MOVEMENTS_TRANSFER = FULL_API_BASE_PATH + "/inventory/movements/transfer";
const std::string ApiRoutes::ROUTE_INVENTORY_MOVEMENTS_ADJUSTMENT = FULL_API_BASE_PATH + "/inventory/movements/adjustment";
const std::string ApiRoutes::ROUTE_INVENTORY_MOVEMENTS_CANCEL = FULL_API_BASE_PATH + "/inventory/movements/{id}/cancel";
const std::string ApiRoutes::ROUTE_INVENTORY_MOVEMENTS_STATUS = FULL_API_BASE_PATH + "/inventory/movements/{id}/status";
const std::string ApiRoutes::ROUTE_INVENTORY_MOVEMENTS_PRODUCT = FULL_API_BASE_PATH + "/inventory/movements/product";
const std::string ApiRoutes::ROUTE_INVENTORY_MOVEMENTS_CELL = FULL_API_BASE_PATH + "/inventory/movements/cell";
const std::string ApiRoutes::ROUTE_INVENTORY_MOVEMENTS_STATISTICS = FULL_API_BASE_PATH + "/inventory/movements/statistics";
const std::string ApiRoutes::ROUTE_INVENTORY_MOVEMENTS_REPORT = FULL_API_BASE_PATH + "/inventory/movements/report";
const std::string ApiRoutes::ROUTE_INVENTORY_CHECK_STOCK = FULL_API_BASE_PATH + "/inventory/check-stock";
const std::string ApiRoutes::ROUTE_INVENTORY_EXPIRING = FULL_API_BASE_PATH + "/inventory/expiring";
const std::string ApiRoutes::ROUTE_INVENTORY_LOW_STOCK = FULL_API_BASE_PATH + "/inventory/low-stock";
const std::string ApiRoutes::ROUTE_INVENTORY_STATISTICS = FULL_API_BASE_PATH + "/inventory/statistics";

// Batch Import routes
const std::string ApiRoutes::ROUTE_USERS_BATCH_UPLOAD = FULL_API_BASE_PATH + "/users/batch/upload";
const std::string ApiRoutes::ROUTE_USERS_BATCH_TEMPLATE = FULL_API_BASE_PATH + "/users/batch/template";
const std::string ApiRoutes::ROUTE_PRODUCTS_BATCH_UPLOAD = FULL_API_BASE_PATH + "/products/batch/upload";
const std::string ApiRoutes::ROUTE_PRODUCTS_BATCH_TEMPLATE = FULL_API_BASE_PATH + "/products/batch/template";
const std::string ApiRoutes::ROUTE_CATEGORIES_BATCH_UPLOAD = FULL_API_BASE_PATH + "/categories/batch/upload";
const std::string ApiRoutes::ROUTE_CATEGORIES_BATCH_TEMPLATE = FULL_API_BASE_PATH + "/categories/batch/template";
const std::string ApiRoutes::ROUTE_WAREHOUSE_CELLS_BATCH_UPLOAD = FULL_API_BASE_PATH + "/warehouse-cells/batch/upload";
const std::string ApiRoutes::ROUTE_WAREHOUSE_CELLS_BATCH_TEMPLATE = FULL_API_BASE_PATH + "/warehouse-cells/batch/template";
const std::string ApiRoutes::ROUTE_SUPPLIERS_BATCH_UPLOAD = FULL_API_BASE_PATH + "/suppliers/batch/upload";
const std::string ApiRoutes::ROUTE_SUPPLIERS_BATCH_TEMPLATE = FULL_API_BASE_PATH + "/suppliers/batch/template";

// Report routes
const std::string ApiRoutes::ROUTE_REPORTS_GENERATE = FULL_API_BASE_PATH + "/reports/generate";
const std::string ApiRoutes::ROUTE_REPORTS_DOWNLOAD = FULL_API_BASE_PATH + "/reports/{id}/download";
const std::string ApiRoutes::ROUTE_REPORTS_STATISTICS = FULL_API_BASE_PATH + "/reports/statistics";

// System routes
const std::string ApiRoutes::ROUTE_HEALTH_CHECK = "/health";
const std::string ApiRoutes::ROUTE_SYSTEM_INFO = "/system/info";
const std::string ApiRoutes::ROUTE_API_DOCS = "/api-docs";

ApiRoutes::ApiRoutes()
{
    initializeRoutes();
}

void ApiRoutes::initializeRoutes()
{
    // Auth routes (no authentication required)
    routes.push_back({"POST", ROUTE_AUTH_REGISTER, "AuthController", false, {}});
    routes.push_back({"POST", ROUTE_AUTH_LOGIN, "AuthController", false, {}});
    routes.push_back({"POST", ROUTE_AUTH_REFRESH, "AuthController", false, {}});
    routes.push_back({"POST", ROUTE_AUTH_FORGOT_PASSWORD, "AuthController", false, {}});
    routes.push_back({"POST", ROUTE_AUTH_RESET_PASSWORD, "AuthController", false, {}});
    
    // Auth routes (authentication required)
    routes.push_back({"POST", ROUTE_AUTH_LOGOUT, "AuthController", true, {}});
    routes.push_back({"POST", ROUTE_AUTH_CHANGE_PASSWORD, "AuthController", true, {}});
    routes.push_back({"GET", ROUTE_AUTH_PROFILE, "AuthController", true, {}});
    routes.push_back({"PUT", ROUTE_AUTH_PROFILE, "AuthController", true, {}});
    routes.push_back({"POST", ROUTE_AUTH_VERIFY_EMAIL, "AuthController", true, {}});
    
    // User routes (admin only)
    routes.push_back({"GET", ROUTE_USERS_LIST, "UserController", true, {"admin", "manager"}});
    routes.push_back({"POST", ROUTE_USERS_CREATE, "UserController", true, {"admin"}});
    routes.push_back({"GET", ROUTE_USERS_GET, "UserController", true, {"admin", "manager"}});
    routes.push_back({"PUT", ROUTE_USERS_UPDATE, "UserController", true, {"admin", "manager"}});
    routes.push_back({"DELETE", ROUTE_USERS_DELETE, "UserController", true, {"admin"}});
    routes.push_back({"POST", ROUTE_USERS_SEARCH, "UserController", true, {"admin", "manager"}});
    routes.push_back({"GET", ROUTE_USERS_STATISTICS, "UserController", true, {"admin"}});
    routes.push_back({"POST", ROUTE_USERS_IMPORT, "UserController", true, {"admin"}});
    routes.push_back({"GET", ROUTE_USERS_EXPORT, "UserController", true, {"admin"}});

    // Product routes (authentication required)
    routes.push_back({"GET", ROUTE_PRODUCTS_LIST, "ProductController", true, {"admin", "manager", "worker"}});
    routes.push_back({"POST", ROUTE_PRODUCTS_CREATE, "ProductController", true, {"admin", "manager"}});
    routes.push_back({"GET", ROUTE_PRODUCTS_GET, "ProductController", true, {"admin", "manager", "worker"}});
    routes.push_back({"PUT", ROUTE_PRODUCTS_UPDATE, "ProductController", true, {"admin", "manager"}});
    routes.push_back({"DELETE", ROUTE_PRODUCTS_DELETE, "ProductController", true, {"admin", "manager"}});
    routes.push_back({"GET", ROUTE_PRODUCTS_SEARCH, "ProductController", true, {"admin", "manager", "worker"}});
    routes.push_back({"GET", ROUTE_PRODUCTS_CATEGORY, "ProductController", true, {"admin", "manager", "worker"}});
    routes.push_back({"PUT", ROUTE_PRODUCTS_STOCK, "ProductController", true, {"admin", "manager"}});
    
    // Order routes (authentication required)
    routes.push_back({"GET", ROUTE_ORDERS_LIST, "OrderController", true, {"admin", "manager", "worker"}});
    routes.push_back({"POST", ROUTE_ORDERS_CREATE, "OrderController", true, {"admin", "manager", "worker"}});
    routes.push_back({"GET", ROUTE_ORDERS_GET, "OrderController", true, {"admin", "manager", "worker"}});
    routes.push_back({"PUT", ROUTE_ORDERS_UPDATE, "OrderController", true, {"admin", "manager"}});
    routes.push_back({"DELETE", ROUTE_ORDERS_DELETE, "OrderController", true, {"admin", "manager"}});
    routes.push_back({"POST", ROUTE_ORDERS_CANCEL, "OrderController", true, {"admin", "manager"}});
    routes.push_back({"PUT", ROUTE_ORDERS_STATUS, "OrderController", true, {"admin", "manager", "worker"}});
    routes.push_back({"PUT", ROUTE_ORDERS_PRIORITY, "OrderController", true, {"admin", "manager"}});
    routes.push_back({"GET", ROUTE_ORDERS_ITEMS_GET, "OrderController", true, {"admin", "manager", "worker"}});
    routes.push_back({"POST", ROUTE_ORDERS_ITEMS_CREATE, "OrderController", true, {"admin", "manager", "worker"}});
    routes.push_back({"PUT", ROUTE_ORDERS_ITEMS_UPDATE, "OrderController", true, {"admin", "manager", "worker"}});
    routes.push_back({"DELETE", ROUTE_ORDERS_ITEMS_DELETE, "OrderController", true, {"admin", "manager", "worker"}});
    routes.push_back({"GET", ROUTE_ORDERS_SEARCH, "OrderController", true, {"admin", "manager", "worker"}});
    routes.push_back({"GET", ROUTE_ORDERS_CUSTOMER, "OrderController", true, {"admin", "manager", "auditor"}});
    routes.push_back({"GET", ROUTE_ORDERS_BY_STATUS, "OrderController", true, {"admin", "manager", "worker"}});
    routes.push_back({"GET", ROUTE_ORDERS_PENDING, "OrderController", true, {"admin", "manager", "worker"}});
    routes.push_back({"GET", ROUTE_ORDERS_URGENT, "OrderController", true, {"admin", "manager", "worker"}});
    routes.push_back({"GET", ROUTE_ORDERS_STATISTICS, "OrderController", true, {"admin", "manager", "auditor"}});
    routes.push_back({"GET", ROUTE_ORDERS_REVENUE_REPORT, "OrderController", true, {"admin", "manager", "auditor"}});
    routes.push_back({"GET", ROUTE_ORDERS_CUSTOMER_HISTORY, "OrderController", true, {"admin", "manager", "auditor"}});
    routes.push_back({"POST", ROUTE_ORDERS_SHIP, "OrderController", true, {"admin", "manager", "worker"}});
    routes.push_back({"POST", ROUTE_ORDERS_DELIVER, "OrderController", true, {"admin", "manager", "worker"}});

    // Supplier routes (authentication required)
    routes.push_back({"GET", ROUTE_SUPPLIERS_LIST, "SupplierController", true, {"admin", "manager", "worker", "auditor"}});
    routes.push_back({"POST", ROUTE_SUPPLIERS_CREATE, "SupplierController", true, {"admin", "manager"}});
    routes.push_back({"GET", ROUTE_SUPPLIERS_GET, "SupplierController", true, {"admin", "manager", "worker", "auditor"}});
    routes.push_back({"PUT", ROUTE_SUPPLIERS_UPDATE, "SupplierController", true, {"admin", "manager"}});
    routes.push_back({"DELETE", ROUTE_SUPPLIERS_DELETE, "SupplierController", true, {"admin", "manager"}});
    routes.push_back({"GET", FULL_API_BASE_PATH + "/suppliers/active", "SupplierController", true, {"admin", "manager", "worker", "auditor"}});
    routes.push_back({"GET", FULL_API_BASE_PATH + "/suppliers/statistics", "SupplierController", true, {"admin", "manager", "auditor"}});
    routes.push_back({"GET", FULL_API_BASE_PATH + "/suppliers/search", "SupplierController", true, {"admin", "manager", "worker", "auditor"}});
    routes.push_back({"GET", FULL_API_BASE_PATH + "/suppliers/tax-id", "SupplierController", true, {"admin", "manager", "worker", "auditor"}});
    routes.push_back({"GET", FULL_API_BASE_PATH + "/suppliers/{id}/products", "SupplierController", true, {"admin", "manager", "worker", "auditor"}});
    routes.push_back({"GET", FULL_API_BASE_PATH + "/suppliers/{id}/batches", "SupplierController", true, {"admin", "manager", "worker", "auditor"}});
    routes.push_back({"PUT", FULL_API_BASE_PATH + "/suppliers/{id}/activate", "SupplierController", true, {"admin", "manager"}});
    routes.push_back({"PUT", FULL_API_BASE_PATH + "/suppliers/{id}/deactivate", "SupplierController", true, {"admin", "manager"}});

    // Category routes (authentication required)
    routes.push_back({"GET", ROUTE_CATEGORIES_LIST, "CategoryController", true, {"admin", "manager", "worker", "auditor"}});
    routes.push_back({"POST", ROUTE_CATEGORIES_CREATE, "CategoryController", true, {"admin", "manager"}});
    routes.push_back({"GET", ROUTE_CATEGORIES_GET, "CategoryController", true, {"admin", "manager", "worker", "auditor"}});
    routes.push_back({"PUT", ROUTE_CATEGORIES_UPDATE, "CategoryController", true, {"admin", "manager"}});
    routes.push_back({"DELETE", ROUTE_CATEGORIES_DELETE, "CategoryController", true, {"admin", "manager"}});
    routes.push_back({"GET", ROUTE_CATEGORIES_TREE, "CategoryController", true, {"admin", "manager", "worker", "auditor"}});
    routes.push_back({"GET", ROUTE_CATEGORIES_TREE_ID, "CategoryController", true, {"admin", "manager", "worker", "auditor"}});
    routes.push_back({"GET", ROUTE_CATEGORIES_ROOTS, "CategoryController", true, {"admin", "manager", "worker", "auditor"}});
    routes.push_back({"GET", ROUTE_CATEGORIES_CHILDREN, "CategoryController", true, {"admin", "manager", "worker", "auditor"}});
    routes.push_back({"GET", ROUTE_CATEGORIES_PRODUCTS, "CategoryController", true, {"admin", "manager", "worker", "auditor"}});
    routes.push_back({"GET", ROUTE_CATEGORIES_STATISTICS, "CategoryController", true, {"admin", "manager", "auditor"}});
    routes.push_back({"POST", ROUTE_CATEGORIES_IMPORT, "CategoryController", true, {"admin", "manager"}});
    routes.push_back({"GET", ROUTE_CATEGORIES_EXPORT, "CategoryController", true, {"admin", "manager"}});
    routes.push_back({"PUT", ROUTE_CATEGORIES_MOVE, "CategoryController", true, {"admin", "manager"}});
    
    // Shipment routes (authentication required)
    routes.push_back({"GET", ROUTE_SHIPMENTS_LIST, "ShipmentController", true, {"admin", "manager", "worker", "auditor"}});
    routes.push_back({"POST", ROUTE_SHIPMENTS_CREATE, "ShipmentController", true, {"admin", "manager", "worker"}});
    routes.push_back({"GET", ROUTE_SHIPMENTS_GET, "ShipmentController", true, {"admin", "manager", "worker", "auditor"}});
    routes.push_back({"PUT", ROUTE_SHIPMENTS_UPDATE, "ShipmentController", true, {"admin", "manager"}});
    routes.push_back({"PUT", ROUTE_SHIPMENTS_STATUS_UPDATE, "ShipmentController", true, {"admin", "manager", "worker"}});
    routes.push_back({"PUT", ROUTE_SHIPMENTS_TRACKING_UPDATE, "ShipmentController", true, {"admin", "manager", "worker"}});
    routes.push_back({"POST", ROUTE_SHIPMENTS_CANCEL, "ShipmentController", true, {"admin", "manager"}});
    routes.push_back({"POST", ROUTE_SHIPMENTS_DELIVERED, "ShipmentController", true, {"admin", "manager", "worker"}});
    routes.push_back({"GET", ROUTE_SHIPMENTS_BY_ORDER, "ShipmentController", true, {"admin", "manager", "worker", "auditor"}});
    routes.push_back({"GET", ROUTE_SHIPMENTS_BY_STATUS, "ShipmentController", true, {"admin", "manager", "worker", "auditor"}});
    routes.push_back({"GET", ROUTE_SHIPMENTS_BY_CARRIER, "ShipmentController", true, {"admin", "manager", "worker", "auditor"}});
    routes.push_back({"GET", ROUTE_SHIPMENTS_STATISTICS, "ShipmentController", true, {"admin", "manager", "auditor"}});
    routes.push_back({"GET", ROUTE_SHIPMENTS_CARRIER_PERFORMANCE, "ShipmentController", true, {"admin", "manager", "auditor"}});
    routes.push_back({"GET", ROUTE_SHIPMENTS_COST_ANALYSIS, "ShipmentController", true, {"admin", "manager", "auditor"}});
    routes.push_back({"GET", ROUTE_SHIPMENTS_DELAYED, "ShipmentController", true, {"admin", "manager", "worker", "auditor"}});
    routes.push_back({"GET", ROUTE_SHIPMENTS_DUE_TODAY, "ShipmentController", true, {"admin", "manager", "worker", "auditor"}});

    // Product Batch routes (authentication required)
    routes.push_back({"GET", ROUTE_PRODUCT_BATCHES_LIST, "ProductBatchController", true, {"admin", "manager", "worker", "auditor"}});
    routes.push_back({"POST", ROUTE_PRODUCT_BATCHES_CREATE, "ProductBatchController", true, {"admin", "manager", "worker"}});
    routes.push_back({"GET", ROUTE_PRODUCT_BATCHES_GET, "ProductBatchController", true, {"admin", "manager", "worker", "auditor"}});
    routes.push_back({"PUT", ROUTE_PRODUCT_BATCHES_UPDATE, "ProductBatchController", true, {"admin", "manager"}});
    routes.push_back({"DELETE", ROUTE_PRODUCT_BATCHES_DELETE, "ProductBatchController", true, {"admin", "manager"}});
    routes.push_back({"PUT", ROUTE_PRODUCT_BATCHES_QUALITY_UPDATE, "ProductBatchController", true, {"admin", "manager", "worker"}});
    routes.push_back({"GET", ROUTE_PRODUCT_BATCHES_BY_NUMBER, "ProductBatchController", true, {"admin", "manager", "worker", "auditor"}});
    routes.push_back({"GET", ROUTE_PRODUCT_BATCHES_BY_PRODUCT, "ProductBatchController", true, {"admin", "manager", "worker", "auditor"}});
    routes.push_back({"GET", ROUTE_PRODUCT_BATCHES_BY_SUPPLIER, "ProductBatchController", true, {"admin", "manager", "worker", "auditor"}});
    routes.push_back({"GET", ROUTE_PRODUCT_BATCHES_EXPIRING, "ProductBatchController", true, {"admin", "manager", "worker", "auditor"}});
    routes.push_back({"GET", ROUTE_PRODUCT_BATCHES_NEEDING_INSPECTION, "ProductBatchController", true, {"admin", "manager", "worker", "auditor"}});
    routes.push_back({"GET", ROUTE_PRODUCT_BATCHES_STATISTICS, "ProductBatchController", true, {"admin", "manager", "auditor"}});
    routes.push_back({"GET", ROUTE_PRODUCT_BATCHES_EXPIRATION_REPORT, "ProductBatchController", true, {"admin", "manager", "auditor"}});
    routes.push_back({"GET", ROUTE_PRODUCT_BATCHES_QUALITY_REPORT, "ProductBatchController", true, {"admin", "manager", "auditor"}});
    routes.push_back({"GET", ROUTE_PRODUCT_BATCHES_CHECK_AVAILABILITY, "ProductBatchController", true, {"admin", "manager", "worker", "auditor"}});
    routes.push_back({"GET", ROUTE_PRODUCT_BATCHES_SEARCH, "ProductBatchController", true, {"admin", "manager", "worker", "auditor"}});
    
    // Order Payment routes (authentication required)
    routes.push_back({"GET", ROUTE_PAYMENTS_LIST, "OrderPaymentController", true, {"admin", "manager", "auditor"}});
    routes.push_back({"POST", ROUTE_PAYMENTS_CREATE, "OrderPaymentController", true, {"admin", "manager", "worker"}});
    routes.push_back({"GET", ROUTE_PAYMENTS_GET, "OrderPaymentController", true, {"admin", "manager", "auditor"}});
    routes.push_back({"PUT", ROUTE_PAYMENTS_UPDATE, "OrderPaymentController", true, {"admin", "manager"}});
    routes.push_back({"DELETE", ROUTE_PAYMENTS_DELETE, "OrderPaymentController", true, {"admin", "manager"}});
    routes.push_back({"PUT", ROUTE_PAYMENTS_STATUS_UPDATE, "OrderPaymentController", true, {"admin", "manager"}});
    routes.push_back({"GET", ROUTE_PAYMENTS_BY_ORDER, "OrderPaymentController", true, {"admin", "manager", "worker", "auditor"}});
    routes.push_back({"GET", ROUTE_PAYMENTS_SEARCH, "OrderPaymentController", true, {"admin", "manager", "auditor"}});
    routes.push_back({"POST", ROUTE_PAYMENTS_MARK_PAID, "OrderPaymentController", true, {"admin", "manager"}});
    routes.push_back({"POST", ROUTE_PAYMENTS_MARK_FAILED, "OrderPaymentController", true, {"admin", "manager"}});
    routes.push_back({"POST", ROUTE_PAYMENTS_MARK_REFUNDED, "OrderPaymentController", true, {"admin", "manager"}});
    routes.push_back({"POST", ROUTE_PAYMENTS_MARK_PENDING, "OrderPaymentController", true, {"admin", "manager"}});
    routes.push_back({"POST", ROUTE_PAYMENTS_PROCESS, "OrderPaymentController", true, {"admin", "manager"}});
    routes.push_back({"POST", ROUTE_PAYMENTS_REFUND, "OrderPaymentController", true, {"admin", "manager"}});
    routes.push_back({"GET", ROUTE_PAYMENTS_PENDING, "OrderPaymentController", true, {"admin", "manager", "worker", "auditor"}});
    routes.push_back({"GET", ROUTE_PAYMENTS_PAID, "OrderPaymentController", true, {"admin", "manager", "worker", "auditor"}});
    routes.push_back({"GET", ROUTE_PAYMENTS_FAILED, "OrderPaymentController", true, {"admin", "manager", "worker", "auditor"}});
    routes.push_back({"GET", ROUTE_PAYMENTS_REFUNDED, "OrderPaymentController", true, {"admin", "manager", "worker", "auditor"}});
    routes.push_back({"GET", ROUTE_PAYMENTS_OVERDUE, "OrderPaymentController", true, {"admin", "manager", "worker", "auditor"}});
    routes.push_back({"GET", ROUTE_PAYMENTS_STATISTICS, "OrderPaymentController", true, {"admin", "manager", "auditor"}});
    routes.push_back({"GET", ROUTE_PAYMENTS_REPORT, "OrderPaymentController", true, {"admin", "manager", "auditor"}});
    routes.push_back({"GET", ROUTE_PAYMENTS_HISTORY, "OrderPaymentController", true, {"admin", "manager", "worker", "auditor"}});
    routes.push_back({"GET", ROUTE_PAYMENTS_ANALYSIS_METHODS, "OrderPaymentController", true, {"admin", "manager", "auditor"}});
    
    // Warehouse Cell routes (authentication required)
    routes.push_back({"GET", ROUTE_WAREHOUSE_CELLS_LIST, "WarehouseCellController", true, {"admin", "manager", "worker", "auditor"}});
    routes.push_back({"POST", ROUTE_WAREHOUSE_CELLS_CREATE, "WarehouseCellController", true, {"admin", "manager"}});
    routes.push_back({"GET", ROUTE_WAREHOUSE_CELLS_GET, "WarehouseCellController", true, {"admin", "manager", "worker", "auditor"}});
    routes.push_back({"PUT", ROUTE_WAREHOUSE_CELLS_UPDATE, "WarehouseCellController", true, {"admin", "manager"}});
    routes.push_back({"DELETE", ROUTE_WAREHOUSE_CELLS_DELETE, "WarehouseCellController", true, {"admin", "manager"}});
    routes.push_back({"POST", ROUTE_WAREHOUSE_CELLS_CLEAR, "WarehouseCellController", true, {"admin", "manager"}});
    routes.push_back({"POST", ROUTE_WAREHOUSE_CELLS_BLOCK, "WarehouseCellController", true, {"admin", "manager"}});
    routes.push_back({"POST", ROUTE_WAREHOUSE_CELLS_UNBLOCK, "WarehouseCellController", true, {"admin", "manager"}});
    routes.push_back({"GET", ROUTE_WAREHOUSE_CELLS_BY_CODE, "WarehouseCellController", true, {"admin", "manager", "worker", "auditor"}});
    routes.push_back({"GET", ROUTE_WAREHOUSE_CELLS_AVAILABLE, "WarehouseCellController", true, {"admin", "manager", "worker"}});
    routes.push_back({"GET", ROUTE_WAREHOUSE_CELLS_BY_ZONE, "WarehouseCellController", true, {"admin", "manager", "worker", "auditor"}});
    routes.push_back({"GET", ROUTE_WAREHOUSE_CELLS_BY_STATUS, "WarehouseCellController", true, {"admin", "manager", "worker", "auditor"}});
    routes.push_back({"GET", ROUTE_WAREHOUSE_CELLS_BATCHES, "WarehouseCellController", true, {"admin", "manager", "worker", "auditor"}});
    routes.push_back({"GET", ROUTE_WAREHOUSE_CELLS_FIND_BEST, "WarehouseCellController", true, {"admin", "manager", "worker"}});
    routes.push_back({"GET", ROUTE_WAREHOUSE_CELLS_STATISTICS, "WarehouseCellController", true, {"admin", "manager", "auditor"}});
    routes.push_back({"GET", ROUTE_WAREHOUSE_CELLS_OCCUPANCY_REPORT, "WarehouseCellController", true, {"admin", "manager", "auditor"}});
    routes.push_back({"GET", ROUTE_WAREHOUSE_CELLS_SEARCH, "WarehouseCellController", true, {"admin", "manager", "worker", "auditor"}});

    // Inventory Movement routes (authentication required)
    routes.push_back({"GET", ROUTE_INVENTORY_MOVEMENTS_LIST, "InventoryMovementController", true, {"admin", "manager", "worker", "auditor"}});
    routes.push_back({"GET", ROUTE_INVENTORY_MOVEMENTS_GET, "InventoryMovementController", true, {"admin", "manager", "worker", "auditor"}});
    routes.push_back({"POST", ROUTE_INVENTORY_MOVEMENTS_RECEIPT, "InventoryMovementController", true, {"admin", "manager", "worker"}});
    routes.push_back({"POST", ROUTE_INVENTORY_MOVEMENTS_TRANSFER, "InventoryMovementController", true, {"admin", "manager", "worker"}});
    routes.push_back({"POST", ROUTE_INVENTORY_MOVEMENTS_ADJUSTMENT, "InventoryMovementController", true, {"admin", "manager"}});
    routes.push_back({"POST", ROUTE_INVENTORY_MOVEMENTS_CANCEL, "InventoryMovementController", true, {"admin", "manager"}});
    routes.push_back({"PUT", ROUTE_INVENTORY_MOVEMENTS_STATUS, "InventoryMovementController", true, {"admin", "manager", "worker"}});
    routes.push_back({"GET", ROUTE_INVENTORY_MOVEMENTS_PRODUCT, "InventoryMovementController", true, {"admin", "manager", "worker", "auditor"}});
    routes.push_back({"GET", ROUTE_INVENTORY_MOVEMENTS_CELL, "InventoryMovementController", true, {"admin", "manager", "worker", "auditor"}});
    routes.push_back({"GET", ROUTE_INVENTORY_MOVEMENTS_STATISTICS, "InventoryMovementController", true, {"admin", "manager", "auditor"}});
    routes.push_back({"GET", ROUTE_INVENTORY_MOVEMENTS_REPORT, "InventoryMovementController", true, {"admin", "manager", "auditor"}});
    routes.push_back({"GET", ROUTE_INVENTORY_CHECK_STOCK, "InventoryMovementController", true, {"admin", "manager", "worker", "auditor"}});
    routes.push_back({"GET", ROUTE_INVENTORY_EXPIRING, "InventoryMovementController", true, {"admin", "manager", "worker", "auditor"}});
    routes.push_back({"GET", ROUTE_INVENTORY_LOW_STOCK, "InventoryMovementController", true, {"admin", "manager", "worker", "auditor"}});
    routes.push_back({"GET", ROUTE_INVENTORY_STATISTICS, "InventoryMovementController", true, {"admin", "manager", "auditor"}});

    // Batch Import routes
    routes.push_back({"POST", ROUTE_USERS_BATCH_UPLOAD, "BatchImportController", true, {"admin", "manager"}});
    routes.push_back({"GET", ROUTE_USERS_BATCH_TEMPLATE, "BatchImportController", true, {"admin", "manager"}});
    routes.push_back({"POST", ROUTE_PRODUCTS_BATCH_UPLOAD, "BatchImportController", true, {"admin", "manager"}});
    routes.push_back({"GET", ROUTE_PRODUCTS_BATCH_TEMPLATE, "BatchImportController", true, {"admin", "manager"}});
    routes.push_back({"POST", ROUTE_CATEGORIES_BATCH_UPLOAD, "BatchImportController", true, {"admin", "manager"}});
    routes.push_back({"GET", ROUTE_CATEGORIES_BATCH_TEMPLATE, "BatchImportController", true, {"admin", "manager"}});
    routes.push_back({"POST", ROUTE_WAREHOUSE_CELLS_BATCH_UPLOAD, "BatchImportController", true, {"admin", "manager"}});
    routes.push_back({"GET", ROUTE_WAREHOUSE_CELLS_BATCH_TEMPLATE, "BatchImportController", true, {"admin", "manager"}});
    routes.push_back({"POST", ROUTE_SUPPLIERS_BATCH_UPLOAD, "BatchImportController", true, {"admin", "manager"}});
    routes.push_back({"GET", ROUTE_SUPPLIERS_BATCH_TEMPLATE, "BatchImportController", true, {"admin", "manager"}});

    // System routes (no authentication required)
    routes.push_back({"GET", ROUTE_HEALTH_CHECK, "SystemController", false, {}});
    routes.push_back({"GET", ROUTE_SYSTEM_INFO, "SystemController", false, {}});
    routes.push_back({"GET", ROUTE_API_DOCS, "SystemController", false, {}});
    
    for (const auto& route : routes)
    {
        routeMap[getRouteKey(route.method, route.path)] = route;
    }
}

class ApiInfoHandler : public Poco::Net::HTTPRequestHandler
{
public:
    void handleRequest(Poco::Net::HTTPServerRequest& request, 
                      Poco::Net::HTTPServerResponse& response) override
    {
        if (request.getMethod() != "GET")
        {
            response.setStatus(Poco::Net::HTTPResponse::HTTP_METHOD_NOT_ALLOWED);
            response.send();
            return;
        }
        
        Poco::JSON::Object::Ptr json = new Poco::JSON::Object;
        json->set("success", true);
        json->set("api", "Warehouse Management System API");
        json->set("version", "v1");
        json->set("basePath", "/api/v1");
        
        Poco::JSON::Object::Ptr endpoints = new Poco::JSON::Object;
        endpoints->set("auth", "/api/v1/auth");
        endpoints->set("users", "/api/v1/users");
        endpoints->set("products", "/api/v1/products");
        endpoints->set("orders", "/api/v1/orders");
        endpoints->set("categories", "/api/v1/categories");
        endpoints->set("suppliers", "/api/v1/suppliers");
        endpoints->set("shipments", "/api/v1/shipments");
        endpoints->set("product-batches", "/api/v1/product-batches");
        endpoints->set("payments", "/api/v1/payments");
        endpoints->set("warehouse-cells", "/api/v1/warehouse-cells");
        endpoints->set("inventory-movements", "/api/v1/inventory/movements");
        endpoints->set("inventory", "/api/v1/inventory");
        endpoints->set("reports", "/api/v1/reports");
        endpoints->set("health", "/health");
        endpoints->set("system", "/system/info");
        
        json->set("endpoints", endpoints);
        
        response.setStatus(Poco::Net::HTTPResponse::HTTP_OK);
        response.setContentType("application/json");
        response.setChunkedTransferEncoding(true);
        
        std::ostream& ostr = response.send();
        json->stringify(ostr);
    }
};

class SystemHandler : public Poco::Net::HTTPRequestHandler
{
private:
    std::string route_;
    
public:
    SystemHandler(const std::string& route) : route_(route) {}
    
    void handleRequest(Poco::Net::HTTPServerRequest& request, 
                      Poco::Net::HTTPServerResponse& response) override
    {
        if (route_ == "/health")
        {
            Poco::JSON::Object::Ptr json = new Poco::JSON::Object;
            json->set("status", "ok");
            json->set("service", "warehouse-backend");
            
            response.setStatus(Poco::Net::HTTPResponse::HTTP_OK);
            response.setContentType("application/json");
            response.setChunkedTransferEncoding(true);
            
            std::ostream& ostr = response.send();
            json->stringify(ostr);
        }
        else if (route_ == "/system/info")
        {
            Poco::JSON::Object::Ptr json = new Poco::JSON::Object;
            json->set("name", "Warehouse Management System");
            json->set("version", "1.0.0");
            json->set("apiVersion", "v1");
            
            response.setStatus(Poco::Net::HTTPResponse::HTTP_OK);
            response.setContentType("application/json");
            response.setChunkedTransferEncoding(true);
            
            std::ostream& ostr = response.send();
            json->stringify(ostr);
        }
        else if (route_ == "/api-docs")
        {
            static const char* html =
                "<!DOCTYPE html>\n"
                "<html lang=\"en\">\n"
                "<head>\n"
                "  <meta charset=\"UTF-8\">\n"
                "  <title>Warehouse API Docs</title>\n"
                "  <link rel=\"stylesheet\" href=\"https://unpkg.com/swagger-ui-dist@5/swagger-ui.css\" />\n"
                "</head>\n"
                "<body>\n"
                "  <div id=\"swagger-ui\"></div>\n"
                "  <script src=\"https://unpkg.com/swagger-ui-dist@5/swagger-ui-bundle.js\"></script>\n"
                "  <script>\n"
                "    window.onload = () => {\n"
                "      SwaggerUIBundle({\n"
                "        url: '/api-docs/openapi.json',\n"
                "        dom_id: '#swagger-ui',\n"
                "        presets: [SwaggerUIBundle.presets.apis],\n"
                "        layout: 'BaseLayout'\n"
                "      });\n"
                "    };\n"
                "  </script>\n"
                "</body>\n"
                "</html>\n";
            
            response.setStatus(Poco::Net::HTTPResponse::HTTP_OK);
            response.setContentType("text/html; charset=utf-8");
            response.setChunkedTransferEncoding(true);
            
            std::ostream& ostr = response.send();
            ostr << html;
        }
        else if (route_ == "/api-docs/openapi.json")
        {
            std::ifstream file("../../config/openapi.json");
            
            if (!file.is_open())
            {
                response.setStatus(Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
                response.setContentType("application/json");
                std::ostream& ostr = response.send();
                
                Poco::JSON::Object::Ptr err = new Poco::JSON::Object;
                err->set("success", false);
                err->set("message", "Failed to open OpenAPI specification file");
                err->stringify(ostr);
                return;
            }
            
            std::ostringstream buffer;
            buffer << file.rdbuf();
            
            response.setStatus(Poco::Net::HTTPResponse::HTTP_OK);
            response.setContentType("application/json");
            response.setChunkedTransferEncoding(true);
            
            std::ostream& ostr = response.send();
            ostr << buffer.str();
        }
        else
        {
            response.setStatus(Poco::Net::HTTPResponse::HTTP_NOT_FOUND);
            response.send();
        }
    }
};

Poco::Net::HTTPRequestHandler* ApiRoutes::createRequestHandler(const Poco::Net::HTTPServerRequest& request)
{
    Poco::URI uri(request.getURI());
    std::string path = uri.getPath();
    std::string method = request.getMethod();
    
    if (path == FULL_API_BASE_PATH && method == "GET")
    {
        return new ApiInfoHandler();
    }
    
    if (path == "/health" && method == "GET")
    {
        return new SystemHandler("/health");
    }
    else if (path == "/system/info" && method == "GET")
    {
        return new SystemHandler("/system/info");
    }
    else if (path == "/api-docs" && method == "GET")
    {
        return new SystemHandler("/api-docs");
    }
    else if (path == "/api-docs/openapi.json" && method == "GET")
    {
        return new SystemHandler("/api-docs/openapi.json");
    }
    
    std::string routeKey = getRouteKey(method, path);
    
    for (const auto& route : routes)
    {
        if (route.method == method && matchesPattern(route.path, path))
        {
            if (route.handlerName == "AuthController")
            {
                return new controllers::AuthController();
            }
            else if (route.handlerName == "UserController")
            {
                return new controllers::UserController();
            }
            else if (route.handlerName == "ProductController")
            {
                return new controllers::ProductController();
            }
            else if (route.handlerName == "OrderController")
            {
                return new controllers::OrderController();
            }
            else if (route.handlerName == "SupplierController")
            {
                return new controllers::SupplierController();
            }
            else if (route.handlerName == "CategoryController")
            {
                return new controllers::CategoryController();
            }
            else if (route.handlerName == "ShipmentController")
            {
                return new controllers::ShipmentController();
            }
            else if (route.handlerName == "ProductBatchController")
            {
                return new controllers::ProductBatchController();
            }
            else if (route.handlerName == "OrderPaymentController")
            {
                return new controllers::OrderPaymentController();
            }
            else if (route.handlerName == "WarehouseCellController")
            {
                return new controllers::WarehouseCellController();
            }
            else if (route.handlerName == "InventoryMovementController")
            {
                return new controllers::InventoryMovementController();
            }
            else if (route.handlerName == "BatchImportController")
            {
                return new controllers::BatchImportController();
            }
        }
    }
    
    return new controllers::BaseController();
}

std::string ApiRoutes::getRouteKey(const std::string& method, const std::string& path)
{
    return method + ":" + path;
}

bool ApiRoutes::matchesPattern(const std::string& routePattern, const std::string& requestPath)
{
    if (routePattern == requestPath)
    {
        return true;
    }
    
    Poco::StringTokenizer patternTokens(routePattern, "/", Poco::StringTokenizer::TOK_IGNORE_EMPTY);
    Poco::StringTokenizer pathTokens(requestPath, "/", Poco::StringTokenizer::TOK_IGNORE_EMPTY);
    
    if (patternTokens.count() != pathTokens.count())
    {
        return false;
    }
    
    for (size_t i = 0; i < patternTokens.count(); ++i)
    {
        const std::string& patternToken = patternTokens[i];
        const std::string& pathToken = pathTokens[i];
        
        if (patternToken.find('{') == 0 && patternToken.find('}') == patternToken.length() - 1)
        {
            continue;
        }
        
        if (patternToken != pathToken)
        {
            return false;
        }
    }
    
    return true;
}

std::map<std::string, std::string> ApiRoutes::extractPathParams(const std::string& routePattern, const std::string& requestPath)
{
    std::map<std::string, std::string> params;
    
    Poco::StringTokenizer patternTokens(routePattern, "/", Poco::StringTokenizer::TOK_IGNORE_EMPTY);
    Poco::StringTokenizer pathTokens(requestPath, "/", Poco::StringTokenizer::TOK_IGNORE_EMPTY);
    
    if (patternTokens.count() != pathTokens.count())
    {
        return params;
    }
    
    for (size_t i = 0; i < patternTokens.count(); ++i)
    {
        const std::string& patternToken = patternTokens[i];
        const std::string& pathToken = pathTokens[i];
        
        if (patternToken.find('{') == 0 && patternToken.find('}') == patternToken.length() - 1)
        {
            std::string paramName = patternToken.substr(1, patternToken.length() - 2);
            params[paramName] = pathToken;
        }
    }
    
    return params;
}

} // namespace routes
