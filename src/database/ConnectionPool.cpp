#include "ConnectionPool.hpp"
#include <Poco/Format.h>
#include <Poco/Timestamp.h>
#include <algorithm>
#include <iostream>

namespace warehouse_backend::database
{

ConnectionPool::ConnectionPool()
    : initialized(false),
      shutdownRequested(false),
      activeConnections(0),
      totalConnectionsCreated(0)
{

}

ConnectionPool::~ConnectionPool()
{
    shutdown();
}

ConnectionPool& ConnectionPool::getInstance()
{
    static ConnectionPool instance;
    return instance;
}

void ConnectionPool::initialize(const ConnectionConfig& connConfig, const PoolConfig& poolConfig)
{
    std::lock_guard<std::mutex> lock(poolMutex);
    
    if (initialized)
    {
        throw ConfigurationException("Connection pool is already initialized");
    }
    
    if (poolConfig.minConnections > poolConfig.maxConnections)
    {
        throw ConfigurationException("minConnections cannot be greater than maxConnections");
    }
    
    if (poolConfig.poolSize < poolConfig.minConnections)
    {
        throw ConfigurationException("poolSize cannot be less than minConnections");
    }
    
    if (poolConfig.poolSize > poolConfig.maxConnections)
    {
        throw ConfigurationException("poolSize cannot be greater than maxConnections");
    }
    
    this->connectionConfig = connConfig;
    this->poolConfig = poolConfig;
    
    try
    {
        createInitialConnections();
        
        shutdownRequested = false;
        maintenanceThreadHandle = std::thread(&ConnectionPool::maintenanceThread, this);
        
        initialized = true;
        
        std::cout << "Connection pool initialized with " 
                  << poolConfig.minConnections << " connections" << std::endl;
    }
    catch (const std::exception& e)
    {
        throw ConfigurationException(
            Poco::format("Failed to initialize connection pool: %s", std::string(e.what())));
    }
}

void ConnectionPool::shutdown()
{
    {
        std::lock_guard<std::mutex> lock(poolMutex);
        
        if (!initialized)
        {
            return;
        }
        
        shutdownRequested = true;
        initialized = false;
        
        connectionAvailable.notify_all();
        
        while (!availableIndices.empty())
        {
            availableIndices.pop();
        }
        
        for (auto& pooledConn : connections)
        {
            if (pooledConn.connection)
            {
                try
                {
                    pooledConn.connection->disconnect();
                }
                catch (...)
                {
                    
                }
            }
        }
        
        connections.clear();
        activeConnections = 0;
    }
    
    if (maintenanceThreadHandle.joinable())
    {
        maintenanceThreadHandle.join();
    }
    
    std::cout << "Connection pool shut down" << std::endl;
}

void ConnectionPool::createInitialConnections()
{
    for (size_t i = 0; i < poolConfig.minConnections; ++i)
    {
        try
        {
            auto conn = createNewConnection();
            connections.emplace_back(std::move(conn));
            availableIndices.push(connections.size() - 1);
            totalConnectionsCreated++;
        }
        catch (const std::exception& e)
        {
            std::cerr << "Failed to create initial connection " << i + 1 
                      << ": " << e.what() << std::endl;
            
            if (i == 0)
            {
                throw;
            }
        }
    }
    
    if (connections.empty())
    {
        throw ConnectionException("Failed to create any database connections");
    }
}

std::unique_ptr<DatabaseConnection> ConnectionPool::createNewConnection()
{
    auto connection = std::make_unique<DatabaseConnection>();
    connection->connect(connectionConfig);
    return connection;
}

std::shared_ptr<DatabaseConnection> ConnectionPool::acquireConnection()
{
    std::unique_lock<std::mutex> lock(poolMutex);
    
    if (!initialized)
    {
        throw ConfigurationException("Connection pool is not initialized");
    }
    
    auto waitStart = std::chrono::steady_clock::now();
    
    while (true)
    {
        if (!availableIndices.empty())
        {
            size_t index = availableIndices.front();
            availableIndices.pop();
            
            PooledConnection& pooledConn = connections[index];
            
            if (!pooledConn.inUse && pooledConn.connection)
            {
                if (pooledConn.isExpired(poolConfig.connectionLifetime))
                {
                    try
                    {
                        pooledConn.connection->disconnect();
                        pooledConn.connection = createNewConnection();
                        pooledConn.createdAt = std::chrono::steady_clock::now();
                        pooledConn.lastUsedAt = pooledConn.createdAt;
                    }
                    catch (...)
                    {
                        connections.erase(connections.begin() + index);
                        continue;
                    }
                }
                
                if (pooledConn.needsValidation(poolConfig.validationInterval))
                {
                    try
                    {
                        pooledConn.connection->ping();
                        pooledConn.lastUsedAt = std::chrono::steady_clock::now();
                    }
                    catch (...)
                    {
                        try
                        {
                            pooledConn.connection->disconnect();
                            pooledConn.connection = createNewConnection();
                            pooledConn.createdAt = std::chrono::steady_clock::now();
                            pooledConn.lastUsedAt = pooledConn.createdAt;
                        }
                        catch (...)
                        {
                            connections.erase(connections.begin() + index);
                            continue;
                        }
                    }
                }
                
                pooledConn.inUse = true;
                pooledConn.lastUsedAt = std::chrono::steady_clock::now();
                activeConnections++;
                
                auto deleter = [this, index](DatabaseConnection* conn) {
                    this->releaseConnection(index);
                };
                
                return std::shared_ptr<DatabaseConnection>(
                    pooledConn.connection.get(), 
                    deleter
                );
            }
        }
        
        if (connections.size() < poolConfig.maxConnections)
        {
            try
            {
                auto newConn = createNewConnection();
                size_t index = connections.size();
                
                connections.emplace_back(std::move(newConn));
                PooledConnection& pooledConn = connections.back();
                
                pooledConn.inUse = true;
                pooledConn.createdAt = std::chrono::steady_clock::now();
                pooledConn.lastUsedAt = pooledConn.createdAt;
                activeConnections++;
                totalConnectionsCreated++;
                
                auto deleter = [this, index](DatabaseConnection* conn) {
                    this->releaseConnection(index);
                };
                
                return std::shared_ptr<DatabaseConnection>(
                    pooledConn.connection.get(),
                    deleter
                );
            }
            catch (const std::exception& e)
            {
                throw PoolExhaustedException(
                    Poco::format("Failed to create new connection: %s", std::string(e.what())));
            }
        }
        
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - waitStart);
        
        if (elapsed >= poolConfig.connectionTimeout)
        {
            throw ConnectionTimeoutException(
                Poco::format("Timeout waiting for database connection after %lld seconds",
                            poolConfig.connectionTimeout.count()));
        }
        
        connectionAvailable.wait_for(lock, std::chrono::seconds(1));
    }
}

void ConnectionPool::releaseConnection(size_t index)
{
    std::lock_guard<std::mutex> lock(poolMutex);
    
    if (!initialized || index >= connections.size())
    {
        return;
    }
    
    auto& pooledConn = connections[index];
    
    if (!pooledConn.inUse)
    {
        return;
    }

    if (pooledConn.connection && pooledConn.connection->isTransactionActive())
    {
        try
        {
            pooledConn.connection->rollbackTransaction();
        }
        catch (const std::exception& e)
        {
            std::cerr << "Warning: Failed to handle transaction on connection release: " 
                      << e.what() << std::endl;
        }
    }
    
    pooledConn.inUse = false;
    pooledConn.lastUsedAt = std::chrono::steady_clock::now();
    activeConnections--;
    
    availableIndices.push(index);
    connectionAvailable.notify_one();
}

size_t ConnectionPool::getActiveConnections() const
{
    return activeConnections.load();
}

size_t ConnectionPool::getIdleConnections() const
{
    std::lock_guard<std::mutex> lock(poolMutex);
    return availableIndices.size();
}

size_t ConnectionPool::getTotalConnections() const
{
    std::lock_guard<std::mutex> lock(poolMutex);
    return connections.size();
}

bool ConnectionPool::isInitialized() const
{
    return initialized.load();
}

const PoolConfig& ConnectionPool::getPoolConfig() const
{
    return poolConfig;
}

const ConnectionConfig& ConnectionPool::getConnectionConfig() const
{
    return connectionConfig;
}

void ConnectionPool::validateConnections()
{
    std::lock_guard<std::mutex> lock(poolMutex);
    
    if (!initialized)
    {
        return;
    }
    
    for (size_t i = 0; i < connections.size(); ++i)
    {
        auto& pooledConn = connections[i];
        
        if (pooledConn.inUse)
        {
            continue;
        }
        
        if (!pooledConn.connection)
        {
            connections.erase(connections.begin() + i);
            i--;
            continue;
        }
        
        if (pooledConn.isExpired(poolConfig.connectionLifetime))
        {
            try
            {
                pooledConn.connection->disconnect();
            }
            catch (...)
            {

            }
            
            connections.erase(connections.begin() + i);
            i--;
            continue;
        }
        
        if (pooledConn.needsValidation(poolConfig.validationInterval))
        {
            try
            {
                pooledConn.connection->ping();
                pooledConn.lastUsedAt = std::chrono::steady_clock::now();
            }
            catch (...)
            {
                try
                {
                    pooledConn.connection->disconnect();
                }
                catch (...)
                {

                }
                
                connections.erase(connections.begin() + i);
                i--;
            }
        }
    }
    
    while (connections.size() < poolConfig.minConnections)
    {
        try
        {
            auto conn = createNewConnection();
            connections.emplace_back(std::move(conn));
            availableIndices.push(connections.size() - 1);
            totalConnectionsCreated++;
        }
        catch (const std::exception& e)
        {
            std::cerr << "Failed to create replacement connection: " 
                      << e.what() << std::endl;
            break;
        }
    }
    
    std::queue<size_t> newQueue;
    for (size_t i = 0; i < connections.size(); ++i)
    {
        if (!connections[i].inUse && connections[i].connection)
        {
            newQueue.push(i);
        }
    }
    availableIndices = std::move(newQueue);
}

void ConnectionPool::maintenanceThread()
{
    while (!shutdownRequested)
    {
        std::this_thread::sleep_for(MAINTENANCE_INTERVAL);
        
        try
        {
            validateConnections();
        }
        catch (const std::exception& e)
        {
            std::cerr << "Error in connection pool maintenance: " 
                      << e.what() << std::endl;
        }
    }
}

} // namespace database
