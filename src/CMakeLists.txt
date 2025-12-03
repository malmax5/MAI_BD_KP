add_executable(warehouse_backend
    main/main.cpp
)

target_include_directories(warehouse_backend
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/main
        ${CMAKE_CURRENT_BINARY_DIR}/../include
        ${CMAKE_BINARY_DIR}
        ${CMAKE_BINARY_DIR}/config
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(warehouse_backend
    PRIVATE
        Poco::Foundation 
        Poco::Util 
        Poco::Net 
        Poco::JSON
        ${PostgreSQL_LIBRARIES}
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
