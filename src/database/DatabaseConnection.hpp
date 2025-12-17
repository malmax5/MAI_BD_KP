#pragma once

#include "DatabaseException.hpp"
#include <Poco/Data/Session.h>
#include <Poco/Data/PostgreSQL/Connector.h>
#include <Poco/Data/PostgreSQL/PostgreSQLException.h>
#include <Poco/Data/Transaction.h>
#include <memory>
#include <string>
#include <mutex>
#include <atomic>
#include <functional>

namespace warehouse_backend::database
{

struct ConnectionConfig
{
    std::string host;
    int port;
    std::string database;
    std::string user;
    std::string password;
    int connectionTimeout;
    std::string sslMode;
};

class DatabaseConnection
{
public:
    DatabaseConnection();
    ~DatabaseConnection();
    
    DatabaseConnection(const DatabaseConnection&) = delete;
    DatabaseConnection& operator=(const DatabaseConnection&) = delete;
    
    DatabaseConnection(DatabaseConnection&& other) noexcept;
    DatabaseConnection& operator=(DatabaseConnection&& other) noexcept;
    
    void connect(const ConnectionConfig& config);
    void disconnect();
    bool isConnected() const;
    
    Poco::Data::Session& getSession();
    const Poco::Data::Session& getSession() const;
    
    void beginTransaction();
    void commitTransaction();
    void rollbackTransaction();
    bool isTransactionActive() const;
    
    void ping();
    bool isValid() const;
    
    std::string getConnectionInfo() const;
    const ConnectionConfig& getConfig() const;
    
    void setAutoCommit(bool autoCommit);
    bool getAutoCommit() const;
    
    void execute(const std::string& sql);
    
    template<typename T>
    T executeScalar(const std::string& sql);
    
    template<typename T>
    std::vector<T> executeQuery(const std::string& sql);
    
    template<typename... Args>
    void bindExecute(const std::string& sql, Args&&... args);
    
    template<typename T, typename... Args>
    T bindExecuteScalar(const std::string& sql, Args&&... args);
    
    template<typename T, typename... Args>
    std::vector<T> bindExecuteQuery(const std::string& sql, Args&&... args);
    
    template<typename T>
    class StatementExecutor
    {
    public:
        StatementExecutor(DatabaseConnection& connection, const std::string& sql)
            : statement(connection.getSession() << sql, Poco::Data::Keywords::into(result))
        {

        }
        
        template<typename... Args>
        StatementExecutor& bind(Args&&... args)
        {
            statement.bind(std::forward<Args>(args)...);
            return *this;
        }
        
        T execute()
        {
            statement.execute();
            return result;
        }
        
    private:
        Poco::Data::Statement statement;
        T result;
    };
    
    template<typename T>
    class QueryExecutor
    {
    public:
        QueryExecutor(DatabaseConnection& connection, const std::string& sql)
            : statement(connection.getSession() << sql, Poco::Data::Keywords::into(results))
        {
            
        }
        
        template<typename... Args>
        QueryExecutor& bind(Args&&... args)
        {
            statement.bind(std::forward<Args>(args)...);
            return *this;
        }
        
        std::vector<T> execute()
        {
            statement.execute();
            return results;
        }
        
    private:
        Poco::Data::Statement statement;
        std::vector<T> results;
    };
    
private:
    void registerPostgreSQLConnector();
    void unregisterPostgreSQLConnector();
    std::string buildConnectionString(const ConnectionConfig& config) const;
    
    std::unique_ptr<Poco::Data::Session> session;
    std::unique_ptr<Poco::Data::Transaction> transaction;
    ConnectionConfig config;
    bool connected;
    bool postgresConnectorRegistered;
    mutable std::mutex mutex;
};

} // namespace database