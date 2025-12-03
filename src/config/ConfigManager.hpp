#pragma once

#include "ApplicationConfig.hpp"
#include <string>
#include <memory>
#include <mutex>
#include <Poco/JSON/Object.h>
#include <Poco/File.h>
#include <Poco/Timestamp.h>

namespace warehouse_backend::config
{

class ConfigManager
{
public:
    ConfigManager();
    explicit ConfigManager(const std::string& configPath);
    ~ConfigManager() = default;
    
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;
    
    static ConfigManager& getInstance();
    static ConfigManager& getInstance(const std::string& configPath);
    
    bool loadConfig();
    bool loadConfig(const std::string& configPath);
    
    bool saveConfig();
    bool saveConfig(const std::string& configPath);
    
    void createDefaultConfig();
    
    bool isConfigFileUpdated();
    
    bool reloadIfNeeded();
    
    const ApplicationConfig& getConfig() const;
    ApplicationConfig& getConfig();
    
    std::string getConfigPath() const;
    Poco::Timestamp getLastModified() const;
    bool isConfigLoaded() const;
    
    std::string getEnvironment() const;
    bool isDevelopment() const;
    bool isProduction() const;
    bool isStaging() const;
    
    const DatabaseConfig& getDatabaseConfig() const;
    const ServerConfig& getServerConfig() const;
    const SecurityConfig& getSecurityConfig() const;
    const LoggingConfig& getLoggingConfig() const;
    const InventoryConfig& getInventoryConfig() const;
    const OrdersConfig& getOrdersConfig() const;
    const ApiConfig& getApiConfig() const;
    
    void updateConfig(const ApplicationConfig& newConfig);
    
    bool validateConfig() const;
    
    bool createBackup(const std::string& backupPath = "");
    
    bool restoreFromBackup(const std::string& backupPath);
    
private:
    void initialize();
    bool parseConfigFile();
    bool writeConfigFile(const ApplicationConfig& config, const std::string& path);
    bool copyFile(const std::string& source, const std::string& destination);
    
    static std::unique_ptr<ConfigManager> instance;
    static std::mutex instanceMutex;
    
    std::string configPath;
    ApplicationConfig config;
    Poco::Timestamp lastModified;
    bool configLoaded;
    std::mutex configMutex;
};

} // namespace config
