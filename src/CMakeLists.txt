add_executable(warehouse_backend
    main/test_main.cpp

    # config
    config/ApplicationConfig.cpp
    config/ConfigManager.cpp

    # utils
    utils/DateUtils.cpp
    utils/JsonUtils.cpp
    utils/Validator.cpp

    # database
    database/DatabaseConnection.cpp
    database/ConnectionPool.cpp
    #database/models
    database/models/User.cpp
    database/models/Supplier.cpp
    database/models/Category.cpp
    database/models/Product.cpp
    database/models/WarehouseCell.cpp
    database/models/ProductBatch.cpp
    database/models/CustomerOrder.cpp
    database/models/OrderItem.cpp
    database/models/Shipment.cpp
    database/models/InventoryMovement.cpp
    database/models/AuditLog.cpp
    database/models/OrderPayment.cpp
    #database/repositories
    database/repositories/BaseRepository.cpp
    database/repositories/UserRepository.cpp
    database/repositories/SupplierRepository.cpp
    database/repositories/CategoryRepository.cpp
    database/repositories/ProductRepository.cpp
    database/repositories/WarehouseCellRepository.cpp
    database/repositories/ProductBatchRepository.cpp
    database/repositories/CustomerOrderRepository.cpp
    database/repositories/OrderItemRepository.cpp
    database/repositories/ShipmentRepository.cpp
    database/repositories/InventoryMovementRepository.cpp
    database/repositories/AuditLogRepository.cpp
    database/repositories/OrderPaymentRepository.cpp

    #services
    services/AuthService.cpp
    services/UserService.cpp
)

target_include_directories(warehouse_backend
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/main
        ${CMAKE_CURRENT_SOURCE_DIR}/config
        ${CMAKE_CURRENT_SOURCE_DIR}/database
        ${CMAKE_CURRENT_SOURCE_DIR}/database/models
        ${CMAKE_CURRENT_SOURCE_DIR}/database/repositories
        ${CMAKE_CURRENT_SOURCE_DIR}/utils
        ${CMAKE_CURRENT_SOURCE_DIR}/services
        ${CMAKE_CURRENT_BINARY_DIR}/../include
        ${CMAKE_BINARY_DIR}
        ${CMAKE_BINARY_DIR}/config
        ${LIBSODIUM_INCLUDE_DIRS}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(warehouse_backend
    PRIVATE
        Poco::Foundation 
        Poco::Util 
        Poco::Net 
        Poco::JSON
        Poco::Data
        Poco::DataPostgreSQL
        Poco::JWT
        ${PostgreSQL_LIBRARIES}
        ${LIBSODIUM_LIBRARIES}
        pthread
        dl
)

add_custom_command(TARGET warehouse_backend POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/config
        $<TARGET_FILE_DIR:warehouse_backend>/../config
    COMMENT "Copying configuration files..."
)

add_custom_command(TARGET warehouse_backend POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:warehouse_backend>/../logs
    COMMENT "Creating logs directory..."
)
