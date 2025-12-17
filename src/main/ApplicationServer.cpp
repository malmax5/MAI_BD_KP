#include "ApplicationServer.hpp"
#include <Poco/Net/HTTPServerParams.h>
#include <Poco/Net/HTTPServer.h>
#include <Poco/Net/ServerSocket.h>
#include <Poco/Util/Option.h>
#include <Poco/Util/HelpFormatter.h>
#include <Poco/Logger.h>
#include <Poco/Message.h>
#include <Poco/ConsoleChannel.h>
#include <Poco/FileChannel.h>
#include <Poco/PatternFormatter.h>
#include <Poco/FormattingChannel.h>
#include <Poco/AutoPtr.h>
#include <Poco/DateTimeFormatter.h>
#include <Poco/DateTimeFormat.h>
#include <algorithm>
#include <cctype>
#include <iostream>
#include <csignal>
#include <thread>
#include <chrono>

#include "../config/ConfigManager.hpp"
#include "../config/ApplicationConfig.hpp"
#include "../database/ConnectionPool.hpp"
#include "../routes/ApiRoutes.hpp"
#include "../controllers/AuthController.hpp"
#include "../controllers/UserController.hpp"
#include "../controllers/ProductController.hpp"

namespace warehouse_backend
{

ApplicationServer* ApplicationServer::instance_ = nullptr;

ApplicationServer::ApplicationServer()
    : running_(false)
    , initialized_(false)
    , shutdownRequested_(false)
    , serverPort_(8080)
    , serverHost_("0.0.0.0")
    , maxThreads_(100)
    , maxQueued_(64)
    , serverTimeout_(30)
    , helpRequested_(false)
    , versionRequested_(false)
    , daemonMode_(false)
{
    instance_ = this;
}

ApplicationServer::~ApplicationServer()
{
    stopServer();
    instance_ = nullptr;
}

ApplicationServer& ApplicationServer::instance()
{
    if (!instance_)
    {
        throw std::runtime_error("ApplicationServer instance not created");
    }
    return *instance_;
}

void ApplicationServer::initialize(Poco::Util::Application& app)
{
    ServerApplication::initialize(app);
    
    try
    {
        setupSignalHandlers();
        setupLogging();
        setupDatabase();
        setupServer();
        
        initialized_ = true;
        logger().information("Application initialized successfully");
    }
    catch (const std::exception& e)
    {
        handleException(e);
        throw;
    }
}

void ApplicationServer::uninitialize()
{
    if (initialized_)
    {
        logger().information("Shutting down application...");
        
        stopServer();
        
        try
        {
            database::ConnectionPool::getInstance().shutdown();
            logger().information("Database connection pool shut down");
        }
        catch (const std::exception& e)
        {
            logger().error("Failed to shutdown database connection pool: " + std::string(e.what()));
        }
        
        initialized_ = false;
        logger().information("Application uninitialized");
    }
    
    ServerApplication::uninitialize();
}

void ApplicationServer::defineOptions(Poco::Util::OptionSet& options)
{
    ServerApplication::defineOptions(options);
    
    options.addOption(
        Poco::Util::Option("help", "h", "Display help information")
            .required(false)
            .repeatable(false)
            .callback(Poco::Util::OptionCallback<ApplicationServer>(this, &ApplicationServer::handleOption))
    );
    
    options.addOption(
        Poco::Util::Option("version", "v", "Display version information")
            .required(false)
            .repeatable(false)
            .callback(Poco::Util::OptionCallback<ApplicationServer>(this, &ApplicationServer::handleOption))
    );
    
    options.addOption(
        Poco::Util::Option("config", "c", "Configuration file path")
            .required(false)
            .repeatable(false)
            .argument("path")
            .callback(Poco::Util::OptionCallback<ApplicationServer>(this, &ApplicationServer::handleOption))
    );
    
    options.addOption(
        Poco::Util::Option("port", "p", "Server port (default: 8080)")
            .required(false)
            .repeatable(false)
            .argument("port")
            .callback(Poco::Util::OptionCallback<ApplicationServer>(this, &ApplicationServer::handleOption))
    );
    
    options.addOption(
        Poco::Util::Option("host", "H", "Server host (default: 0.0.0.0)")
            .required(false)
            .repeatable(false)
            .argument("host")
            .callback(Poco::Util::OptionCallback<ApplicationServer>(this, &ApplicationServer::handleOption))
    );
}

void ApplicationServer::handleOption(const std::string& name, const std::string& value)
{
    if (name == "help")
    {
        helpRequested_ = true;
        displayHelp();
        stopOptionsProcessing();
    }
    else if (name == "version")
    {
        versionRequested_ = true;
        std::cout << "Warehouse Management System Backend v1.0.0" << std::endl;
        stopOptionsProcessing();
    }
    else if (name == "config")
    {
        configPath_ = value;
    }
    else if (name == "port")
    {
        serverPort_ = std::stoi(value);
    }
    else if (name == "host")
    {
        serverHost_ = value;
    }
    else if (name == "daemon")
    {
        daemonMode_ = true;
    }
}

void ApplicationServer::displayHelp()
{
    Poco::Util::HelpFormatter helpFormatter(options());
    helpFormatter.setCommand(commandName());
    helpFormatter.setUsage("OPTIONS");
    helpFormatter.setHeader("Warehouse Management System Backend Server");
    helpFormatter.format(std::cout);
}

int ApplicationServer::main(const std::vector<std::string>& args)
{
    if (helpRequested_ || versionRequested_)
    {
        return Application::EXIT_OK;
    }
    
    try
    {
        logStartupInfo();
        startServer();
        
        if (!daemonMode_)
        {
            waitForTermination();
        }
        else
        {
            while (running_ && !shutdownRequested_)
            {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }
        
        logShutdownInfo();
        return Application::EXIT_OK;
    }
    catch (const std::exception& e)
    {
        handleException(e);
        return Application::EXIT_SOFTWARE;
    }
}

void ApplicationServer::setupSignalHandlers()
{
    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);
    std::signal(SIGHUP, handleSignal);
}

void ApplicationServer::handleSignal(int signal)
{
    if (instance_)
    {
        instance_->logger().information("Received signal: " + std::to_string(signal));
        
        switch (signal)
        {
            case SIGINT:
            case SIGTERM:
                instance_->shutdownRequested_ = true;
                instance_->stopServer();
                break;
            case SIGHUP:
                instance_->logger().information("Configuration reload requested");
                break;
        }
    }
}

namespace {
    Poco::Message::Priority stringToLogLevel(const std::string& level)
    {
        std::string levelLower = level;
        std::transform(levelLower.begin(), levelLower.end(), levelLower.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        
        if (levelLower == "fatal")
            return Poco::Message::PRIO_FATAL;
        else if (levelLower == "critical")
            return Poco::Message::PRIO_CRITICAL;
        else if (levelLower == "error")
            return Poco::Message::PRIO_ERROR;
        else if (levelLower == "warning" || levelLower == "warn")
            return Poco::Message::PRIO_WARNING;
        else if (levelLower == "notice")
            return Poco::Message::PRIO_NOTICE;
        else if (levelLower == "info" || levelLower == "information")
            return Poco::Message::PRIO_INFORMATION;
        else if (levelLower == "debug")
            return Poco::Message::PRIO_DEBUG;
        else if (levelLower == "trace")
            return Poco::Message::PRIO_TRACE;
        else
            return Poco::Message::PRIO_INFORMATION;
    }
}

void ApplicationServer::setupLogging()
{
    try
    {
        config::ConfigManager& configManager = config::ConfigManager::getInstance();
        
        if (!configManager.loadConfig())
        {
            logger().warning("Failed to load configuration, using defaults");
        }
        
        const config::LoggingConfig& loggingConfig = configManager.getLoggingConfig();
        
        Poco::AutoPtr<Poco::PatternFormatter> patternFormatter(new Poco::PatternFormatter());
        patternFormatter->setProperty("pattern", loggingConfig.console.pattern);
        
        Poco::AutoPtr<Poco::FormattingChannel> formattingChannel(new Poco::FormattingChannel(patternFormatter));
        
        if (loggingConfig.console.enabled)
        {
            Poco::AutoPtr<Poco::ConsoleChannel> consoleChannel(new Poco::ConsoleChannel());
            formattingChannel->setChannel(consoleChannel);
        }
        else if (loggingConfig.file.enabled)
        {
            Poco::AutoPtr<Poco::FileChannel> fileChannel(new Poco::FileChannel());
            fileChannel->setProperty("path", loggingConfig.file.path);
            fileChannel->setProperty("rotation", loggingConfig.file.rotation);
            fileChannel->setProperty("archive", "timestamp");
            formattingChannel->setChannel(fileChannel);
        }
        
        Poco::Logger::root().setChannel(formattingChannel);
        Poco::Logger::root().setLevel(stringToLogLevel(loggingConfig.level));
        
        logger().information("Logging setup completed");
    }
    catch (const std::exception& e)
    {
        std::cerr << "Failed to setup logging: " << e.what() << std::endl;
        throw;
    }
}

void ApplicationServer::setupDatabase()
{
    try
    {
        config::ConfigManager& configManager = config::ConfigManager::getInstance();
        const config::DatabaseConfig& dbConfig = configManager.getDatabaseConfig();
        
        database::ConnectionConfig connConfig;
        connConfig.host = dbConfig.host;
        connConfig.port = dbConfig.port;
        connConfig.database = dbConfig.database;
        connConfig.user = dbConfig.user;
        connConfig.password = dbConfig.password;
        connConfig.connectionTimeout = dbConfig.connectionTimeout;
        connConfig.sslMode = dbConfig.sslMode;
        
        database::PoolConfig poolConfig;
        poolConfig.minConnections = dbConfig.minConnections;
        poolConfig.maxConnections = dbConfig.maxConnections;
        poolConfig.poolSize = dbConfig.poolSize;
        poolConfig.connectionLifetime = std::chrono::seconds(dbConfig.connectionLifetime);
        
        database::ConnectionPool::getInstance().initialize(connConfig, poolConfig);
        
        logger().information("Database connection pool initialized");
        logger().information("Database: " + dbConfig.host + ":" + std::to_string(dbConfig.port) + "/" + dbConfig.database);
    }
    catch (const std::exception& e)
    {
        logger().error("Failed to setup database: " + std::string(e.what()));
        throw;
    }
}

void ApplicationServer::setupServer()
{
    try
    {
        config::ConfigManager& configManager = config::ConfigManager::getInstance();
        const config::ServerConfig& serverConfig = configManager.getServerConfig();
        
        serverPort_ = serverConfig.port;
        serverHost_ = serverConfig.host;
        maxThreads_ = serverConfig.maxThreads;
        maxQueued_ = serverConfig.maxQueued;
        serverTimeout_ = serverConfig.timeout;
        
        logger().information("Server configuration loaded");
    }
    catch (const std::exception& e)
    {
        logger().warning("Using default server configuration: " + std::string(e.what()));
    }
}

void ApplicationServer::setupControllers()
{
    logger().information("Controllers setup completed (dynamic initialization)");
}

void ApplicationServer::setupRoutes()
{
    logger().information("Routes setup completed");
}

void ApplicationServer::setupMiddleware()
{
    logger().information("Middleware setup completed");
}

void ApplicationServer::startServer()
{
    if (running_)
    {
        logger().warning("Server is already running");
        return;
    }
    
    try
    {
        Poco::Net::ServerSocket serverSocket(Poco::Net::SocketAddress(serverHost_, serverPort_));
        serverSocket.setReuseAddress(true);
        serverSocket.setReusePort(true);
        
        Poco::Net::HTTPServerParams::Ptr params = new Poco::Net::HTTPServerParams();
        params->setMaxThreads(maxThreads_);
        params->setMaxQueued(maxQueued_);
        params->setTimeout(serverTimeout_);
        
        Poco::Net::HTTPRequestHandlerFactory::Ptr factory = new routes::ApiRoutes();
        
        server_ = std::make_unique<Poco::Net::HTTPServer>(factory, serverSocket, params);
        
        server_->start();
        running_ = true;
        
        logger().information("Server started on " + serverHost_ + ":" + std::to_string(serverPort_));
        logger().information("Max threads: " + std::to_string(maxThreads_));
        logger().information("API base path: " + routes::ApiRoutes::FULL_API_BASE_PATH);
    }
    catch (const std::exception& e)
    {
        handleCriticalError("Failed to start server: " + std::string(e.what()));
        throw;
    }
}

void ApplicationServer::stopServer()
{
    if (running_ && server_)
    {
        try
        {
            logger().information("Stopping server...");
            server_->stop();
            running_ = false;
            logger().information("Server stopped");
        }
        catch (const std::exception& e)
        {
            logger().error("Error stopping server: " + std::string(e.what()));
        }
    }
}

void ApplicationServer::waitForTermination()
{
    logger().information("Press Ctrl+C to stop the server");
    
    waitForTerminationRequest();
    
    logger().information("Termination request received");
}

void ApplicationServer::logStartupInfo()
{
    Poco::DateTime now;
    std::string timestamp = Poco::DateTimeFormatter::format(now, Poco::DateTimeFormat::SORTABLE_FORMAT);
    
    logger().information("========================================");
    logger().information("Warehouse Management System Backend");
    logger().information("Version: 1.0.0");
    logger().information("Startup time: " + timestamp);
    logger().information("Server: " + serverHost_ + ":" + std::to_string(serverPort_));
    logger().information("========================================");
}

void ApplicationServer::logShutdownInfo()
{
    Poco::DateTime now;
    std::string timestamp = Poco::DateTimeFormatter::format(now, Poco::DateTimeFormat::SORTABLE_FORMAT);
    
    logger().information("========================================");
    logger().information("Shutdown time: " + timestamp);
    logger().information("Server stopped");
    logger().information("========================================");
}

void ApplicationServer::handleException(const std::exception& e)
{
    logger().error("Unhandled exception: " + std::string(e.what()));
}

void ApplicationServer::handleCriticalError(const std::string& error)
{
    logger().fatal("Critical error: " + error);
    std::cerr << "CRITICAL ERROR: " << error << std::endl;
}

} // namespace warehouse_backend
