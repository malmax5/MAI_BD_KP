#include <iostream>
#include <memory>
#include <cassert>
#include <Poco/JSON/Parser.h>
#include <Poco/JSON/Stringifier.h>

// Подключаем заголовки из src/database/
#include "../src/database/DatabaseException.hpp"
#include "../src/database/DatabaseConnection.hpp"
#include "../src/database/ConnectionPool.hpp"

// Подключаем заголовки моделей
#include "../src/database/models/User.hpp"
#include "../src/database/models/Product.hpp"
#include "../src/database/models/Category.hpp"
#include "../src/database/models/Supplier.hpp"
#include "../src/database/models/ProductBatch.hpp"
#include "../src/database/models/WarehouseCell.hpp"
#include "../src/database/models/CustomerOrder.hpp"
#include "../src/database/models/OrderItem.hpp"
#include "../src/database/models/Shipment.hpp"
#include "../src/database/models/InventoryMovement.hpp"
#include "../src/database/models/AuditLog.hpp"
#include "../src/database/models/OrderPayment.hpp"

using namespace warehouse_backend::database;
using namespace warehouse_backend::database::models;

// ================= Тесты для DatabaseException =================
void testDatabaseException() {
    std::cout << "Testing DatabaseException..." << std::endl;
    
    try {
        throw ConnectionException("Test connection error");
        assert(false && "Should have thrown");
    } catch (const ConnectionException& e) {
        std::cout << "  ✓ ConnectionException caught: " << e.what() << std::endl;
    }
    
    try {
        throw QueryException("Test query error");
        assert(false && "Should have thrown");
    } catch (const QueryException& e) {
        std::cout << "  ✓ QueryException caught: " << e.what() << std::endl;
    }
    
    try {
        throw TransactionException("Test transaction error");
        assert(false && "Should have thrown");
    } catch (const TransactionException& e) {
        std::cout << "  ✓ TransactionException caught: " << e.what() << std::endl;
    }
    
    std::cout << "  ✓ All DatabaseException tests passed!" << std::endl;
}

// ================= Тесты для моделей данных =================
void testUserModel() {
    std::cout << "Testing User model..." << std::endl;
    
    User user;
    user.username = "test_user";
    user.email = "test@example.com";
    user.fullName = "Test User";
    user.role = UserRole::ADMIN;
    user.isActive = true;
    
    // Тестируем toJson()
    auto json = user.toJson();
    assert(json.has("username"));
    assert(json.getValue<std::string>("username") == "test_user");
    assert(json.getValue<std::string>("role") == "admin");
    
    // Тестируем fromJson()
    Poco::JSON::Object jsonObj;
    jsonObj.set("username", "new_user");
    jsonObj.set("email", "new@example.com");
    jsonObj.set("full_name", "New User");
    jsonObj.set("role", "manager");
    jsonObj.set("is_active", true);
    
    User user2(jsonObj);
    assert(user2.username == "new_user");
    assert(user2.role == UserRole::MANAGER);
    
    // Тестируем roleToString и stringToRole
    assert(User::roleToString(UserRole::ADMIN) == "admin");
    assert(User::stringToRole("worker") == UserRole::WORKER);
    
    std::cout << "  ✓ User model tests passed!" << std::endl;
}

void testProductModel() {
    std::cout << "Testing Product model..." << std::endl;
    
    Product product;
    product.sku = "TEST-001";
    product.name = "Test Product";
    product.categoryId = 1;
    product.supplierId = 1;
    product.unitPrice = 99.99;
    product.weight = 1.5;
    product.minStockLevel = 10;
    product.maxStockLevel = 100;
    product.isActive = true;
    
    auto json = product.toJson();
    assert(json.has("sku"));
    assert(json.getValue<std::string>("sku") == "TEST-001");
    assert(json.getValue<double>("unit_price") == 99.99);
    
    // Тестируем needsReorder()
    product.currentStock = 5;  // Ниже minStockLevel
    assert(product.needsReorder() == true);
    
    product.currentStock = 50;  // Выше minStockLevel
    assert(product.needsReorder() == false);
    
    std::cout << "  ✓ Product model tests passed!" << std::endl;
}

void testCategoryModel() {
    std::cout << "Testing Category model..." << std::endl;
    
    Category category;
    category.name = "Test Category";
    category.description = "Test Description";
    category.parentId = 0;
    category.sortOrder = 1;
    
    auto json = category.toJson();
    assert(json.has("name"));
    assert(json.getValue<std::string>("name") == "Test Category");
    
    // Тестируем isRoot()
    assert(category.isRoot() == true);
    
    category.parentId = 1;
    assert(category.isRoot() == false);
    
    std::cout << "  ✓ Category model tests passed!" << std::endl;
}

void testSupplierModel() {
    std::cout << "Testing Supplier model..." << std::endl;
    
    Supplier supplier;
    supplier.name = "Test Supplier";
    supplier.contactPerson = "John Doe";
    supplier.email = "supplier@example.com";
    supplier.phone = "+1234567890";
    supplier.rating = 4.5;
    supplier.isActive = true;
    
    auto json = supplier.toJson();
    assert(json.has("name"));
    assert(json.getValue<std::string>("name") == "Test Supplier");
    assert(json.getValue<double>("rating") == 4.5);
    
    std::cout << "  ✓ Supplier model tests passed!" << std::endl;
}

void testProductBatchModel() {
    std::cout << "Testing ProductBatch model..." << std::endl;
    
    ProductBatch batch;
    batch.batchNumber = "BATCH-2024-001";
    batch.productId = 1;
    batch.supplierId = 1;
    batch.quantityReceived = 100;
    batch.quantityAvailable = 100;
    batch.unitCost = 50.0;
    batch.expirationDate = "2024-12-31";
    batch.storageCellId = 1;
    batch.qualityStatus = QualityStatus::APPROVED;
    
    auto json = batch.toJson();
    assert(json.has("batch_number"));
    assert(json.getValue<std::string>("batch_number") == "BATCH-2024-001");
    assert(json.getValue<std::string>("quality_status") == "approved");
    
    // Тестируем qualityStatusToString
    assert(ProductBatch::qualityStatusToString(QualityStatus::APPROVED) == "approved");
    assert(ProductBatch::stringToQualityStatus("pending") == QualityStatus::PENDING);
    
    std::cout << "  ✓ ProductBatch model tests passed!" << std::endl;
}

void testWarehouseCellModel() {
    std::cout << "Testing WarehouseCell model..." << std::endl;
    
    WarehouseCell cell;
    cell.cellCode = "A-01-01";
    cell.zone = "Zone A";
    cell.rack = "Rack 01";
    cell.shelf = "Shelf 01";
    cell.position = "Position 01";
    cell.maxVolume = 1000.0;
    cell.maxWeight = 500.0;
    cell.currentOccupancy = 0.0;
    cell.status = CellStatus::EMPTY;
    cell.temperatureZone = TemperatureZone::NORMAL;
    
    auto json = cell.toJson();
    assert(json.has("cell_code"));
    assert(json.getValue<std::string>("cell_code") == "A-01-01");
    assert(json.getValue<std::string>("status") == "empty");
    
    // Тестируем canStore()
    assert(cell.canStore(500.0, 200.0) == true);
    
    cell.currentOccupancy = 95.0;
    cell.status = CellStatus::FULL;
    assert(cell.canStore(100.0, 50.0) == false);
    
    // Тестируем статусы
    assert(WarehouseCell::statusToString(CellStatus::FULL) == "full");
    assert(WarehouseCell::stringToStatus("blocked") == CellStatus::BLOCKED);
    
    std::cout << "  ✓ WarehouseCell model tests passed!" << std::endl;
}

void testCustomerOrderModel() {
    std::cout << "Testing CustomerOrder model..." << std::endl;
    
    CustomerOrder order;
    order.orderNumber = "ORD-2024-001";
    order.customerName = "Test Customer";
    order.shippingAddress = "123 Test St";
    order.status = OrderStatus::NEW;
    order.totalAmount = 199.99;
    order.priority = OrderPriority::NORMAL;
    order.createdBy = 1;
    
    auto json = order.toJson();
    assert(json.has("order_number"));
    assert(json.getValue<std::string>("order_number") == "ORD-2024-001");
    assert(json.getValue<std::string>("status") == "new");
    
    // Тестируем статусы
    assert(CustomerOrder::statusToString(OrderStatus::DELIVERED) == "delivered");
    assert(CustomerOrder::stringToStatus("processing") == OrderStatus::PROCESSING);
    
    // Тестируем canBeCancelled()
    order.status = OrderStatus::NEW;
    assert(order.canBeCancelled() == true);
    
    order.status = OrderStatus::DELIVERED;
    assert(order.canBeCancelled() == false);
    
    std::cout << "  ✓ CustomerOrder model tests passed!" << std::endl;
}

void testOrderItemModel() {
    std::cout << "Testing OrderItem model..." << std::endl;
    
    OrderItem item;
    item.orderId = 1;
    item.productId = 1;
    item.batchId = 1;
    item.quantityOrdered = 10;
    item.quantityShipped = 0;
    item.unitPrice = 19.99;
    item.discountPercent = 10.0;
    item.pickingStatus = PickingStatus::NOT_STARTED;
    
    // Тестируем calculateLineTotal()
    item.calculateLineTotal();
    double expected = 10 * 19.99 * (1 - 10.0/100.0);
    assert(item.lineTotal == expected);
    
    auto json = item.toJson();
    assert(json.has("quantity_ordered"));
    assert(json.getValue<int>("quantity_ordered") == 10);
    
    // Тестируем статусы
    assert(OrderItem::pickingStatusToString(PickingStatus::PICKED) == "picked");
    assert(OrderItem::stringToPickingStatus("in_progress") == PickingStatus::IN_PROGRESS);
    
    std::cout << "  ✓ OrderItem model tests passed!" << std::endl;
}

void testShipmentModel() {
    std::cout << "Testing Shipment model..." << std::endl;
    
    Shipment shipment;
    shipment.shipmentNumber = "SHIP-2024-001";
    shipment.orderId = 1;
    shipment.carrier = "Test Carrier";
    shipment.trackingNumber = "TRACK123";
    shipment.shippingCost = 25.0;
    shipment.status = ShipmentStatus::PREPARING;
    shipment.weightTotal = 15.5;
    
    auto json = shipment.toJson();
    assert(json.has("shipment_number"));
    assert(json.getValue<std::string>("shipment_number") == "SHIP-2024-001");
    assert(json.getValue<std::string>("status") == "preparing");
    
    // Тестируем статусы
    assert(Shipment::statusToString(ShipmentStatus::DELIVERED) == "delivered");
    assert(Shipment::stringToStatus("in_transit") == ShipmentStatus::IN_TRANSIT);
    
    std::cout << "  ✓ Shipment model tests passed!" << std::endl;
}

void testInventoryMovementModel() {
    std::cout << "Testing InventoryMovement model..." << std::endl;
    
    InventoryMovement movement;
    movement.movementType = MovementType::TRANSFER;
    movement.productId = 1;
    movement.batchId = 1;
    movement.fromCellId = 1;
    movement.toCellId = 2;
    movement.quantity = 50;
    movement.performedBy = 1;
    movement.status = MovementStatus::PLANNED;
    
    auto json = movement.toJson();
    assert(json.has("movement_type"));
    assert(json.getValue<std::string>("movement_type") == "transfer");
    
    // Тестируем типы перемещений
    assert(InventoryMovement::movementTypeToString(MovementType::RECEIPT) == "receipt");
    assert(InventoryMovement::stringToMovementType("shipment") == MovementType::SHIPMENT);
    
    // Тестируем isTransfer()
    assert(movement.isTransfer() == true);
    assert(movement.isReceipt() == false);
    
    std::cout << "  ✓ InventoryMovement model tests passed!" << std::endl;
}

void testAuditLogModel() {
    std::cout << "Testing AuditLog model..." << std::endl;
    
    AuditLog audit;
    audit.tableName = "products";
    audit.recordId = 1;
    audit.action = AuditAction::INSERT;
    audit.oldValues = "{}";
    audit.newValues = R"({"name": "Test Product", "price": 99.99})";
    audit.changedBy = 1;
    
    auto json = audit.toJson();
    assert(json.has("table_name"));
    assert(json.getValue<std::string>("table_name") == "products");
    assert(json.getValue<std::string>("action") == "INSERT");
    
    // Тестируем hasChanges()
    assert(audit.hasChanges() == true);
    
    // Тестируем преобразование действий
    assert(AuditLog::auditActionToString(AuditAction::UPDATE) == "UPDATE");
    assert(AuditLog::stringToAuditAction("DELETE") == AuditAction::DELETE);
    
    std::cout << "  ✓ AuditLog model tests passed!" << std::endl;
}

void testOrderPaymentModel() {
    std::cout << "Testing OrderPayment model..." << std::endl;
    
    OrderPayment payment;
    payment.orderId = 1;
    payment.paymentMethod = "credit_card";
    payment.paymentStatus = "pending";
    payment.amount = 199.99;
    payment.orderTotal = 199.99;
    
    auto json = payment.toJson();
    assert(json.has("payment_method"));
    assert(json.getValue<std::string>("payment_method") == "credit_card");
    assert(json.getValue<std::string>("payment_status") == "pending");
    
    // Тестируем isPending()
    assert(payment.isPending() == true);
    
    payment.paymentStatus = "paid";
    assert(payment.isPaid() == true);
    assert(payment.isFullPayment() == true);
    
    std::cout << "  ✓ OrderPayment model tests passed!" << std::endl;
}

// ================= Тесты для DatabaseConnection =================
void testDatabaseConnection() {
    std::cout << "Testing DatabaseConnection..." << std::endl;
    
    // Тестируем создание и перемещение
    {
        DatabaseConnection conn1;
        DatabaseConnection conn2 = std::move(conn1);
        DatabaseConnection conn3(std::move(conn2));
        
        std::cout << "  ✓ DatabaseConnection move operations work" << std::endl;
    }
    
    // Тестируем ConnectionConfig
    ConnectionConfig config;
    config.host = "localhost";
    config.port = 5432;
    config.database = "warehouse_db";
    config.user = "test_user";
    config.password = "test_password";
    config.connectionTimeout = 30;
    config.sslMode = "disable";
    
    assert(config.host == "localhost");
    assert(config.port == 5432);
    assert(config.database == "warehouse_db");
    
    std::cout << "  ✓ ConnectionConfig works correctly" << std::endl;
    
    // Примечание: Не тестируем реальное подключение, так как это потребует запущенной БД
    // Это можно сделать в интеграционных тестах
    
    std::cout << "  ✓ DatabaseConnection basic tests passed!" << std::endl;
}

// ================= Тесты для ConnectionPool =================
void testConnectionPool() {
    std::cout << "Testing ConnectionPool..." << std::endl;
    
    // Тестируем синглтон
    auto& pool1 = ConnectionPool::getInstance();
    
    // Это должен быть один и тот же объект
    // (не можем напрямую сравнить адреса, но можем проверить поведение)
    
    // Тестируем PoolConfig
    PoolConfig poolConfig;
    poolConfig.minConnections = 5;
    poolConfig.maxConnections = 20;
    poolConfig.poolSize = 10;
    poolConfig.connectionLifetime = std::chrono::seconds(300);
    poolConfig.connectionTimeout = std::chrono::seconds(30);
    poolConfig.validationInterval = std::chrono::seconds(60);
    
    assert(poolConfig.minConnections == 5);
    assert(poolConfig.maxConnections == 20);
    assert(poolConfig.poolSize == 10);
    
    std::cout << "  ✓ PoolConfig works correctly" << std::endl;
    
    // Тестируем, что пул не инициализирован по умолчанию
    assert(pool1.isInitialized() == false);
    
    std::cout << "  ✓ ConnectionPool correctly reports not initialized" << std::endl;
    
    // Примечание: Не тестируем инициализацию и получение соединений,
    // так как это потребует реальной БД
    
    std::cout << "  ✓ ConnectionPool basic tests passed!" << std::endl;
}

// ================= Главная функция =================
int main() {
    std::cout << "=========================================" << std::endl;
    std::cout << "  Database Layer Unit Tests" << std::endl;
    std::cout << "=========================================" << std::endl;
    
    try {
        // Запускаем все тесты
        testDatabaseException();
        std::cout << std::endl;
        
        testDatabaseConnection();
        std::cout << std::endl;
        
        testConnectionPool();
        std::cout << std::endl;
        
        // Тестируем все модели
        testUserModel();
        std::cout << std::endl;
        
        testProductModel();
        std::cout << std::endl;
        
        testCategoryModel();
        std::cout << std::endl;
        
        testSupplierModel();
        std::cout << std::endl;
        
        testProductBatchModel();
        std::cout << std::endl;
        
        testWarehouseCellModel();
        std::cout << std::endl;
        
        testCustomerOrderModel();
        std::cout << std::endl;
        
        testOrderItemModel();
        std::cout << std::endl;
        
        testShipmentModel();
        std::cout << std::endl;
        
        testInventoryMovementModel();
        std::cout << std::endl;
        
        testAuditLogModel();
        std::cout << std::endl;
        
        testOrderPaymentModel();
        
        std::cout << "\n=========================================" << std::endl;
        std::cout << "  ✅ All tests passed successfully!" << std::endl;
        std::cout << "=========================================" << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "\n❌ Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "\n❌ Test failed with unknown exception" << std::endl;
        return 1;
    }
}