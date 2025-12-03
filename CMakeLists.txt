cmake_minimum_required(VERSION 3.15)
project(WarehouseBackend 
    VERSION 1.0.0 
    DESCRIPTION "Warehouse Management System Backend"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror -pedantic -g -O0")
    add_definitions(-DDEBUG=1)
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O2")
    add_definitions(-DNDEBUG=1)
endif()

find_package(Poco REQUIRED COMPONENTS 
    Foundation 
    Util 
    Net 
    JSON
)

if(Poco_VERSION VERSION_LESS 1.10.0)
    message(WARNING "POCO version ${Poco_VERSION} is older than recommended 1.10.0")
endif()

find_package(PostgreSQL REQUIRED)

add_subdirectory(src)

option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

install(TARGETS warehouse_backend
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(DIRECTORY config/ DESTINATION etc/warehouse
    FILES_MATCHING PATTERN "*.json.example"
)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/config/version.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/config/version.h"
)
