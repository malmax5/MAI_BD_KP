#pragma once

#include <string>
#include <stdexcept>
#include <Poco/Exception.h>

namespace warehouse_backend::database
{

class DatabaseException : public std::runtime_error
{
public:
    enum class ErrorCode
    {
        CONNECTION_FAILED,
        QUERY_FAILED,
        TRANSACTION_FAILED,
        CONNECTION_POOL_EXHAUSTED,
        CONNECTION_TIMEOUT,
        INVALID_CONFIGURATION,
        UNKNOWN_ERROR
    };

    DatabaseException(const std::string& message, ErrorCode code = ErrorCode::UNKNOWN_ERROR)
        : std::runtime_error(message), errorCode(code)
    {

    }

    DatabaseException(const std::string& message, ErrorCode code, const std::string& sqlState)
        : std::runtime_error(message), errorCode(code), sqlState(sqlState)
    {

    }

    ErrorCode getErrorCode() const { return errorCode; }
    const std::string& getSqlState() const { return sqlState; }
    
    static std::string errorCodeToString(ErrorCode code)
    {
        switch (code)
        {
            case ErrorCode::CONNECTION_FAILED:
                return "CONNECTION_FAILED";
            case ErrorCode::QUERY_FAILED:
                return "QUERY_FAILED";
            case ErrorCode::TRANSACTION_FAILED:
                return "TRANSACTION_FAILED";
            case ErrorCode::CONNECTION_POOL_EXHAUSTED:
                return "CONNECTION_POOL_EXHAUSTED";
            case ErrorCode::CONNECTION_TIMEOUT:
                return "CONNECTION_TIMEOUT";
            case ErrorCode::INVALID_CONFIGURATION:
                return "INVALID_CONFIGURATION";
            case ErrorCode::UNKNOWN_ERROR:
                return "UNKNOWN_ERROR";
            default:
                return "UNKNOWN";
        }
    }

private:
    ErrorCode errorCode;
    std::string sqlState;
};

class ConnectionException : public DatabaseException
{
public:
    ConnectionException(const std::string& message)
        : DatabaseException(message, ErrorCode::CONNECTION_FAILED)
    {

    }

    ConnectionException(const std::string& message, const std::string& sqlState)
        : DatabaseException(message, ErrorCode::CONNECTION_FAILED, sqlState)
    {

    }
};

class QueryException : public DatabaseException
{
public:
    QueryException(const std::string& message)
        : DatabaseException(message, ErrorCode::QUERY_FAILED)
    {

    }

    QueryException(const std::string& message, const std::string& sqlState)
        : DatabaseException(message, ErrorCode::QUERY_FAILED, sqlState)
    {

    }
};

class TransactionException : public DatabaseException
{
public:
    TransactionException(const std::string& message)
        : DatabaseException(message, ErrorCode::TRANSACTION_FAILED)
    {

    }
};

class PoolExhaustedException : public DatabaseException
{
public:
    PoolExhaustedException(const std::string& message)
        : DatabaseException(message, ErrorCode::CONNECTION_POOL_EXHAUSTED)
    {

    }
};

class ConnectionTimeoutException : public DatabaseException
{
public:
    ConnectionTimeoutException(const std::string& message)
        : DatabaseException(message, ErrorCode::CONNECTION_TIMEOUT)
    {

    }
};

class ConfigurationException : public DatabaseException
{
public:
    ConfigurationException(const std::string& message)
        : DatabaseException(message, ErrorCode::INVALID_CONFIGURATION)
    {
        
    }
};

} // namespace database