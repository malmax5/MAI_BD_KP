#include "DatabaseConnection.hpp"
#include <Poco/Data/SessionFactory.h>
#include <Poco/Data/RecordSet.h>
#include <Poco/Data/Column.h>
#include <Poco/Format.h>
#include <Poco/String.h>
#include <sstream>
#include <iomanip>

namespace warehouse_backend::database
{

DatabaseConnection::DatabaseConnection()
    : connected(false), postgresConnectorRegistered(false)
{
    registerPostgreSQLConnector();
}

DatabaseConnection::~DatabaseConnection()
{
    try
    {
        disconnect();
        unregisterPostgreSQLConnector();
    }
    catch (...)
    {

    }
}

DatabaseConnection::DatabaseConnection(DatabaseConnection&& other) noexcept
    : session(std::move(other.session)),
      transaction(std::move(other.transaction)),
      config(std::move(other.config)),
      connected(other.connected),
      postgresConnectorRegistered(other.postgresConnectorRegistered)
{
    other.connected = false;
    other.postgresConnectorRegistered = false;
}

DatabaseConnection& DatabaseConnection::operator=(DatabaseConnection&& other) noexcept
{
    if (this != &other)
    {
        disconnect();
        unregisterPostgreSQLConnector();
        
        session = std::move(other.session);
        transaction = std::move(other.transaction);
        config = std::move(other.config);
        connected = other.connected;
        postgresConnectorRegistered = other.postgresConnectorRegistered;
        
        other.connected = false;
        other.postgresConnectorRegistered = false;
    }
    return *this;
}

void DatabaseConnection::registerPostgreSQLConnector()
{
    if (!postgresConnectorRegistered)
    {
        Poco::Data::PostgreSQL::Connector::registerConnector();
        postgresConnectorRegistered = true;
    }
}

void DatabaseConnection::unregisterPostgreSQLConnector()
{
    if (postgresConnectorRegistered)
    {
        try
        {
            Poco::Data::PostgreSQL::Connector::unregisterConnector();
        }
        catch (...)
        {

        }
        postgresConnectorRegistered = false;
    }
}

std::string DatabaseConnection::buildConnectionString(const ConnectionConfig& config) const
{
    std::ostringstream oss;
    
    oss << "host=" << config.host
        << " port=" << config.port
        << " dbname=" << config.database
        << " user=" << config.user
        << " password=" << config.password;
    
    if (!config.sslMode.empty())
    {
        oss << " sslmode=" << config.sslMode;
    }
    
    if (config.connectionTimeout > 0)
    {
        oss << " connect_timeout=" << config.connectionTimeout;
    }
    
    return oss.str();
}

void DatabaseConnection::connect(const ConnectionConfig& config)
{
    std::lock_guard<std::mutex> lock(mutex);
    
    if (connected)
    {
        disconnect();
    }
    
    this->config = config;
    
    try
    {
        std::string connectionString = buildConnectionString(config);
        session = std::make_unique<Poco::Data::Session>(
            Poco::Data::SessionFactory::instance().create(
                Poco::Data::PostgreSQL::Connector::KEY, connectionString));
        
        session->setFeature("autoCommit", true);
        
        *session << "SELECT 1", Poco::Data::Keywords::now;
        
        connected = true;
    }
    catch (const Poco::Data::PostgreSQL::PostgreSQLException& e)
    {
        throw ConnectionException(
            Poco::format("Failed to connect to database: %s", e.displayText()),
            e.displayText());
    }
    catch (const Poco::Exception& e)
    {
        throw ConnectionException(
            Poco::format("Failed to connect to database: %s", e.displayText()));
    }
    catch (const std::exception& e)
    {
        throw ConnectionException(
            Poco::format("Failed to connect to database: %s", std::string(e.what())));
    }
}

void DatabaseConnection::disconnect()
{
    std::lock_guard<std::mutex> lock(mutex);
    
    if (transaction)
    {
        try
        {
            transaction->rollback();
        }
        catch (...)
        {

        }
        transaction.reset();
    }
    
    if (session)
    {
        try
        {
            session->close();
        }
        catch (...)
        {

        }
        session.reset();
    }
    
    connected = false;
}

bool DatabaseConnection::isConnected() const
{
    return connected && session && session->isConnected();
}

Poco::Data::Session& DatabaseConnection::getSession()
{
    if (!isConnected())
    {
        throw ConnectionException("Database connection is not established");
    }
    return *session;
}

const Poco::Data::Session& DatabaseConnection::getSession() const
{
    if (!isConnected())
    {
        throw ConnectionException("Database connection is not established");
    }
    return *session;
}

void DatabaseConnection::beginTransaction()
{
    std::lock_guard<std::mutex> lock(mutex);
    
    if (!isConnected())
    {
        throw ConnectionException("Database connection is not established");
    }
    
    if (transaction)
    {
        throw TransactionException("Transaction is already active");
    }
    
    try
    {
        transaction = std::make_unique<Poco::Data::Transaction>(*session);
        session->setFeature("autoCommit", false);
    }
    catch (const Poco::Exception& e)
    {
        throw TransactionException(
            Poco::format("Failed to begin transaction: %s", e.displayText()));
    }
}

void DatabaseConnection::commitTransaction()
{
    std::lock_guard<std::mutex> lock(mutex);
    
    if (!transaction)
    {
        throw TransactionException("No active transaction to commit");
    }
    
    try
    {
        transaction->commit();
        transaction.reset();
        session->setFeature("autoCommit", true);
    }
    catch (const Poco::Exception& e)
    {
        throw TransactionException(
            Poco::format("Failed to commit transaction: %s", e.displayText()));
    }
}

void DatabaseConnection::rollbackTransaction()
{
    std::lock_guard<std::mutex> lock(mutex);
    
    if (!transaction)
    {
        throw TransactionException("No active transaction to rollback");
    }
    
    try
    {
        transaction->rollback();
        transaction.reset();
        session->setFeature("autoCommit", true);
    }
    catch (const Poco::Exception& e)
    {
        throw TransactionException(
            Poco::format("Failed to rollback transaction: %s", e.displayText()));
    }
}

bool DatabaseConnection::isTransactionActive() const
{
    return transaction != nullptr;
}

void DatabaseConnection::ping()
{
    std::lock_guard<std::mutex> lock(mutex);
    
    if (!isConnected())
    {
        throw ConnectionException("Database connection is not established");
    }
    
    try
    {
        *session << "SELECT 1", Poco::Data::Keywords::now;
    }
    catch (const Poco::Exception& e)
    {
        connected = false;
        throw ConnectionException(
            Poco::format("Database connection ping failed: %s", e.displayText()));
    }
}

bool DatabaseConnection::isValid() const
{
    try
    {
        const_cast<DatabaseConnection*>(this)->ping();
        return true;
    }
    catch (...)
    {
        return false;
    }
}

std::string DatabaseConnection::getConnectionInfo() const
{
    std::ostringstream oss;
    oss << "Host: " << config.host
        << ", Port: " << config.port
        << ", Database: " << config.database
        << ", User: " << config.user
        << ", Connected: " << (connected ? "Yes" : "No");
    return oss.str();
}

const ConnectionConfig& DatabaseConnection::getConfig() const
{
    return config;
}

void DatabaseConnection::setAutoCommit(bool autoCommit)
{
    std::lock_guard<std::mutex> lock(mutex);
    
    if (!isConnected())
    {
        throw ConnectionException("Database connection is not established");
    }
    
    try
    {
        session->setFeature("autoCommit", autoCommit);
    }
    catch (const Poco::Exception& e)
    {
        throw QueryException(
            Poco::format("Failed to set autoCommit: %s", e.displayText()));
    }
}

bool DatabaseConnection::getAutoCommit() const
{
    if (!isConnected())
    {
        throw ConnectionException("Database connection is not established");
    }
    
    try
    {
        return session->getFeature("autoCommit");
    }
    catch (...)
    {
        return true;
    }
}

void DatabaseConnection::execute(const std::string& sql)
{
    std::lock_guard<std::mutex> lock(mutex);
    
    if (!isConnected())
    {
        throw ConnectionException("Database connection is not established");
    }
    
    try
    {
        *session << sql, Poco::Data::Keywords::now;
    }
    catch (const Poco::Data::PostgreSQL::PostgreSQLException& e)
    {
        throw QueryException(
            Poco::format("Query execution failed: %s\nSQL: %s", e.displayText(), sql));
    }
    catch (const Poco::Exception& e)
    {
        throw QueryException(
            Poco::format("Query execution failed: %s\nSQL: %s", e.displayText(), sql));
    }
}

template<>
int DatabaseConnection::executeScalar<int>(const std::string& sql)
{
    std::lock_guard<std::mutex> lock(mutex);
    
    if (!isConnected())
    {
        throw ConnectionException("Database connection is not established");
    }
    
    try
    {
        int result;
        *session << sql, Poco::Data::Keywords::into(result), Poco::Data::Keywords::now;
        return result;
    }
    catch (const Poco::Data::PostgreSQL::PostgreSQLException& e)
    {
        throw QueryException(
            Poco::format("Scalar query execution failed: %s\nSQL: %s", e.displayText(), sql));
    }
    catch (const Poco::Exception& e)
    {
        throw QueryException(
            Poco::format("Scalar query execution failed: %s\nSQL: %s", e.displayText(), sql));
    }
}

template<>
std::string DatabaseConnection::executeScalar<std::string>(const std::string& sql)
{
    std::lock_guard<std::mutex> lock(mutex);
    
    if (!isConnected())
    {
        throw ConnectionException("Database connection is not established");
    }
    
    try
    {
        std::string result;
        *session << sql, Poco::Data::Keywords::into(result), Poco::Data::Keywords::now;
        return result;
    }
    catch (const Poco::Data::PostgreSQL::PostgreSQLException& e)
    {
        throw QueryException(
            Poco::format("Scalar query execution failed: %s\nSQL: %s", e.displayText(), sql));
    }
    catch (const Poco::Exception& e)
    {
        throw QueryException(
            Poco::format("Scalar query execution failed: %s\nSQL: %s", e.displayText(), sql));
    }
}

} // namespace database