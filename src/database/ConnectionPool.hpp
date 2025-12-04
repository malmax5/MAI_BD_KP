#pragma once

#include "DatabaseConnection.hpp"
#include "DatabaseException.hpp"
#include <memory>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <thread>
#include <chrono>

namespace warehouse_backend::database
{

struct PoolConfig
{
    size_t minConnections;
    size_t maxConnections;
    size_t poolSize;
    std::chrono::seconds connectionLifetime;
    std::chrono::seconds connectionTimeout;
    std::chrono::seconds validationInterval;
    
    PoolConfig()
        : minConnections(5),
          maxConnections(20),
          poolSize(10),
          connectionLifetime(300),
          connectionTimeout(30),
          validationInterval(60)
    {
        
    }
};

class ConnectionPool
{
public:
    static ConnectionPool& getInstance();
    
    void initialize(const ConnectionConfig& connConfig, const PoolConfig& poolConfig);
    void shutdown();
    
    std::shared_ptr<DatabaseConnection> acquireConnection();
    
    size_t getActiveConnections() const;
    size_t getIdleConnections() const;
    size_t getTotalConnections() const;
    
    bool isInitialized() const;
    const PoolConfig& getPoolConfig() const;
    const ConnectionConfig& getConnectionConfig() const;
    
    void validateConnections();
    
private:
    ConnectionPool();
    ~ConnectionPool();
    
    ConnectionPool(const ConnectionPool&) = delete;
    ConnectionPool& operator=(const ConnectionPool&) = delete;
    
    void createInitialConnections();
    std::unique_ptr<DatabaseConnection> createNewConnection();
    void releaseConnection(size_t index);
    void maintenanceThread();
    
    struct PooledConnection
    {
        std::unique_ptr<DatabaseConnection> connection;
        std::chrono::steady_clock::time_point createdAt;
        std::chrono::steady_clock::time_point lastUsedAt;
        bool inUse;
        
        PooledConnection(std::unique_ptr<DatabaseConnection> conn)
            : connection(std::move(conn)),
              createdAt(std::chrono::steady_clock::now()),
              lastUsedAt(std::chrono::steady_clock::now()),
              inUse(false)
        {
        }
        
        bool isExpired(const std::chrono::seconds& lifetime) const
        {
            auto now = std::chrono::steady_clock::now();
            return (now - createdAt) > lifetime;
        }
        
        bool needsValidation(const std::chrono::seconds& validationInterval) const
        {
            auto now = std::chrono::steady_clock::now();
            return (now - lastUsedAt) > validationInterval;
        }
    };
    
    ConnectionConfig connectionConfig;
    PoolConfig poolConfig;
    
    std::vector<PooledConnection> connections;
    std::queue<size_t> availableIndices;
    
    mutable std::mutex poolMutex;
    std::condition_variable connectionAvailable;
    
    std::atomic<bool> initialized;
    std::atomic<bool> shutdownRequested;
    std::thread maintenanceThreadHandle;
    
    std::atomic<size_t> activeConnections;
    std::atomic<size_t> totalConnectionsCreated;
    
    static constexpr std::chrono::milliseconds MAINTENANCE_INTERVAL{5000};
};

} // namespace database
