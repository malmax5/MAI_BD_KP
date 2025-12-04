#include "ConfigManager.hpp"
#include "../utils/JsonUtils.hpp"
#include "../utils/Validator.hpp"
#include <Poco/JSON/Parser.h>
#include <Poco/JSON/Stringifier.h>
#include <Poco/StreamCopier.h>
#include <Poco/FileStream.h>
#include <Poco/Path.h>
#include <Poco/File.h>
#include <Poco/Exception.h>
#include <Poco/Format.h>
#include <Poco/DateTimeFormatter.h>
#include <Poco/LocalDateTime.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <filesystem>

namespace warehouse_backend::config
{

using namespace warehouse_backend::utils;

std::unique_ptr<ConfigManager> ConfigManager::instance = nullptr;
std::mutex ConfigManager::instanceMutex;

ConfigManager::ConfigManager()
    : configPath("../config/config.json")
    , configLoaded(false)
{
    initialize();
}

ConfigManager::ConfigManager(const std::string& configPath)
    : configPath(configPath)
    , configLoaded(false)
{
    initialize();
}

ConfigManager& ConfigManager::getInstance()
{
    std::lock_guard<std::mutex> lock(instanceMutex);
    if (!instance)
    {
        instance = std::make_unique<ConfigManager>();
    }
    return *instance;
}

ConfigManager& ConfigManager::getInstance(const std::string& configPath)
{
    std::lock_guard<std::mutex> lock(instanceMutex);
    if (!instance)
    {
        instance = std::make_unique<ConfigManager>(configPath);
    }
    else if (instance->configPath != configPath)
    {
        instance->loadConfig(configPath);
    }
    return *instance;
}

void ConfigManager::initialize()
{
    Poco::Path configDirPath(configPath);
    configDirPath.makeParent();
    
    try
    {
        Poco::File configDir(configDirPath);
        if (!configDir.exists())
        {
            configDir.createDirectories();
        }
        
        if (!loadConfig())
        {
            createDefaultConfig();
        }
    }
    catch (const Poco::Exception& e)
    {
        std::cerr << "Failed to initialize config directory: " << e.displayText() << std::endl;
        createDefaultConfig();
    }
}

bool ConfigManager::loadConfig()
{
    return loadConfig(configPath);
}

bool ConfigManager::loadConfig(const std::string& path)
{
    std::lock_guard<std::mutex> lock(configMutex);
    
    try
    {
        Poco::File configFile(path);
        
        if (!configFile.exists())
        {
            std::cerr << "Config file not found: " << path << std::endl;
            configLoaded = false;
            return false;
        }
        
        if (!configFile.canRead())
        {
            std::cerr << "Cannot read config file: " << path << std::endl;
            configLoaded = false;
            return false;
        }
        
        configPath = path;
        lastModified = configFile.getLastModified();
        
        Poco::FileInputStream fis(path);
        std::stringstream ss;
        Poco::StreamCopier::copyStream(fis, ss);
        
        Poco::JSON::Parser parser;
        Poco::Dynamic::Var result = parser.parse(ss.str());
        
        if (result.type() == typeid(Poco::JSON::Object::Ptr))
        {
            Poco::JSON::Object::Ptr jsonObject = result.extract<Poco::JSON::Object::Ptr>();
            config = ApplicationConfig(*jsonObject);
            
            if (validateConfig())
            {
                configLoaded = true;
                std::cout << "Configuration loaded successfully from: " << path << std::endl;
                return true;
            }
            else
            {
                std::cerr << "Configuration validation failed" << std::endl;
                configLoaded = false;
                return false;
            }
        }
        else
        {
            std::cerr << "Invalid JSON format in config file" << std::endl;
            configLoaded = false;
            return false;
        }
    }
    catch (const Poco::Exception& e)
    {
        std::cerr << "Failed to load config file: " << e.displayText() << std::endl;
        configLoaded = false;
        return false;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Failed to load config file: " << e.what() << std::endl;
        configLoaded = false;
        return false;
    }
}

bool ConfigManager::saveConfig()
{
    return saveConfig(configPath);
}

bool ConfigManager::saveConfig(const std::string& path)
{
    std::lock_guard<std::mutex> lock(configMutex);
    
    try
    {
        if (!validateConfig())
        {
            std::cerr << "Cannot save invalid configuration" << std::endl;
            return false;
        }
        
        return writeConfigFile(config, path);
    }
    catch (const Poco::Exception& e)
    {
        std::cerr << "Failed to save config file: " << e.displayText() << std::endl;
        return false;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Failed to save config file: " << e.what() << std::endl;
        return false;
    }
}

void ConfigManager::createDefaultConfig()
{
    std::lock_guard<std::mutex> lock(configMutex);
    
    std::cout << "Creating default configuration..." << std::endl;
    
    config = ApplicationConfig::createDefault();
    configLoaded = true;
    
    if (saveConfig())
    {
        std::cout << "Default configuration created and saved to: " << configPath << std::endl;
    }
    else
    {
        std::cerr << "Failed to save default configuration" << std::endl;
    }
}

bool ConfigManager::isConfigFileUpdated()
{
    try
    {
        Poco::File configFile(configPath);
        if (!configFile.exists())
        {
            return false;
        }
        
        Poco::Timestamp currentModified = configFile.getLastModified();
        return currentModified > lastModified;
    }
    catch (...)
    {
        return false;
    }
}

bool ConfigManager::reloadIfNeeded()
{
    if (isConfigFileUpdated())
    {
        std::cout << "Configuration file has been modified, reloading..." << std::endl;
        return loadConfig();
    }
    return true;
}

const ApplicationConfig& ConfigManager::getConfig() const
{
    return config;
}

ApplicationConfig& ConfigManager::getConfig()
{
    return config;
}

std::string ConfigManager::getConfigPath() const
{
    return configPath;
}

Poco::Timestamp ConfigManager::getLastModified() const
{
    return lastModified;
}

bool ConfigManager::isConfigLoaded() const
{
    return configLoaded;
}

std::string ConfigManager::getEnvironment() const
{
    return config.application.environment;
}

bool ConfigManager::isDevelopment() const
{
    return config.application.environment == "development";
}

bool ConfigManager::isProduction() const
{
    return config.application.environment == "production";
}

bool ConfigManager::isStaging() const
{
    return config.application.environment == "staging";
}

const DatabaseConfig& ConfigManager::getDatabaseConfig() const
{
    return config.database;
}

const ServerConfig& ConfigManager::getServerConfig() const
{
    return config.server;
}

const SecurityConfig& ConfigManager::getSecurityConfig() const
{
    return config.security;
}

const LoggingConfig& ConfigManager::getLoggingConfig() const
{
    return config.logging;
}

const InventoryConfig& ConfigManager::getInventoryConfig() const
{
    return config.inventory;
}

const OrdersConfig& ConfigManager::getOrdersConfig() const
{
    return config.orders;
}

const ApiConfig& ConfigManager::getApiConfig() const
{
    return config.api;
}

void ConfigManager::updateConfig(const ApplicationConfig& newConfig)
{
    std::lock_guard<std::mutex> lock(configMutex);
    config = newConfig;
    
    if (validateConfig())
    {
        std::cout << "Configuration updated successfully" << std::endl;
    }
    else
    {
        std::cerr << "Updated configuration is invalid" << std::endl;
    }
}

bool ConfigManager::validateConfig() const
{
    if (!config.isValid())
    {
        return false;
    }
    
    Validator validator;
    
    if (config.notifications.email.enabled)
    {
        if (!validator.isValidEmail(config.notifications.email.fromAddress))
        {
            std::cerr << "Invalid email address in notifications configuration" << std::endl;
            return false;
        }
        
        if (config.notifications.email.smtpPort <= 0 || config.notifications.email.smtpPort > 65535)
        {
            std::cerr << "Invalid SMTP port in notifications configuration" << std::endl;
            return false;
        }
    }
    
    for (const auto& origin : config.server.corsAllowedOrigins)
    {
        if (origin.empty())
        {
            std::cerr << "Empty CORS origin is not allowed" << std::endl;
            return false;
        }
    }
    
    if (config.audit.enabled && config.audit.sensitiveFields.empty())
    {
        std::cerr << "Audit is enabled but no sensitive fields specified" << std::endl;
        return false;
    }
    
    return true;
}

bool ConfigManager::createBackup(const std::string& backupPath)
{
    try
    {
        std::string backupFilePath;
        
        if (backupPath.empty())
        {
            Poco::LocalDateTime now;
            std::string timestamp = Poco::DateTimeFormatter::format(now, "%Y%m%d_%H%M%S");
            
            Poco::Path configDir(configPath);
            configDir.makeParent();
            
            backupFilePath = configDir.toString() + "config_backup_" + timestamp + ".json";
        }
        else
        {
            backupFilePath = backupPath;
        }
        
        return copyFile(configPath, backupFilePath);
    }
    catch (const Poco::Exception& e)
    {
        std::cerr << "Failed to create backup: " << e.displayText() << std::endl;
        return false;
    }
}

bool ConfigManager::restoreFromBackup(const std::string& backupPath)
{
    try
    {
        std::string currentBackup = configPath + ".restore_backup";
        if (!copyFile(configPath, currentBackup))
        {
            std::cerr << "Failed to create backup of current config before restore" << std::endl;
            return false;
        }
        
        if (copyFile(backupPath, configPath))
        {
            return loadConfig();
        }
        else
        {
            copyFile(currentBackup, configPath);
            return false;
        }
    }
    catch (const Poco::Exception& e)
    {
        std::cerr << "Failed to restore from backup: " << e.displayText() << std::endl;
        return false;
    }
}

bool ConfigManager::writeConfigFile(const ApplicationConfig& config, const std::string& path)
{
    try
    {
        Poco::Path filePath(path);
        Poco::File dir(filePath.parent());
        
        if (!dir.exists())
        {
            dir.createDirectories();
        }
        
        Poco::JSON::Object jsonObject = config.toJson();
        
        Poco::FileOutputStream fos(path);
        jsonObject.stringify(fos, 4);
        fos.close();
        
        Poco::File configFile(path);
        lastModified = configFile.getLastModified();
        
        return true;
    }
    catch (const Poco::Exception& e)
    {
        std::cerr << "Failed to write config file: " << e.displayText() << std::endl;
        return false;
    }
}

bool ConfigManager::copyFile(const std::string& source, const std::string& destination)
{
    try
    {
        Poco::File sourceFile(source);
        if (!sourceFile.exists())
        {
            std::cerr << "Source file does not exist: " << source << std::endl;
            return false;
        }
        
        Poco::Path destPath(destination);
        Poco::File destDir(destPath.parent());
        
        if (!destDir.exists())
        {
            destDir.createDirectories();
        }
        
        sourceFile.copyTo(destination);
        
        return true;
    }
    catch (const Poco::Exception& e)
    {
        std::cerr << "Failed to copy file: " << e.displayText() << std::endl;
        return false;
    }
}

} // namespace config
