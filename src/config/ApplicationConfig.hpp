#pragma once

#include <string>
#include <vector>
#include <memory>
#include <Poco/JSON/Object.h>

namespace warehouse_backend::config
{

struct DatabaseConfig
{
    std::string host;
    int port;
    std::string database;
    std::string user;
    std::string password;
    int connectionTimeout;
    int poolSize;
    int maxConnections;
    int minConnections;
    int connectionLifetime;
    std::string sslMode;
    
    Poco::JSON::Object toJson() const;
    static DatabaseConfig fromJson(const Poco::JSON::Object& json);
};

struct ServerConfig
{
    int port;
    std::string host;
    int maxThreads;
    int maxQueued;
    int timeout;
    bool keepAlive;
    int keepAliveTimeout;
    int maxRequestSize;
    bool compression;
    std::vector<std::string> corsAllowedOrigins;
    
    Poco::JSON::Object toJson() const;
    static ServerConfig fromJson(const Poco::JSON::Object& json);
};

struct SecurityConfig
{
    std::string jwtSecret;
    int tokenExpiryHours;
    int refreshTokenExpiryDays;
    int bcryptCost;
    int passwordMinLength;
    bool passwordRequireUppercase;
    bool passwordRequireLowercase;
    bool passwordRequireDigits;
    bool passwordRequireSpecial;
    int rateLimitPerMinute;
    int maxLoginAttempts;
    int lockoutDurationMinutes;
    
    Poco::JSON::Object toJson() const;
    static SecurityConfig fromJson(const Poco::JSON::Object& json);
};

struct LoggingConsoleConfig
{
    bool enabled;
    std::string pattern;
    
    Poco::JSON::Object toJson() const;
    static LoggingConsoleConfig fromJson(const Poco::JSON::Object& json);
};

struct LoggingFileConfig
{
    bool enabled;
    std::string path;
    std::string rotation;
    int maxSizeMB;
    int maxFiles;
    bool compress;
    
    Poco::JSON::Object toJson() const;
    static LoggingFileConfig fromJson(const Poco::JSON::Object& json);
};

struct LoggingDatabaseConfig
{
    bool enabled;
    std::string table;
    
    Poco::JSON::Object toJson() const;
    static LoggingDatabaseConfig fromJson(const Poco::JSON::Object& json);
};

struct LoggingConfig
{
    std::string level;
    LoggingConsoleConfig console;
    LoggingFileConfig file;
    LoggingDatabaseConfig database;
    
    Poco::JSON::Object toJson() const;
    static LoggingConfig fromJson(const Poco::JSON::Object& json);
};

struct InventoryConfig
{
    int defaultPageSize;
    int maxPageSize;
    bool autoReorderEnabled;
    int reorderThresholdPercent;
    int expirationWarningDays;
    bool batchTrackingEnabled;
    bool serialNumberTracking;
    
    Poco::JSON::Object toJson() const;
    static InventoryConfig fromJson(const Poco::JSON::Object& json);
};

struct OrdersConfig
{
    bool autoAssignPicker;
    std::string defaultPriority;
    int maxItemsPerOrder;
    bool allowBackorders;
    std::string defaultShippingMethod;
    
    Poco::JSON::Object toJson() const;
    static OrdersConfig fromJson(const Poco::JSON::Object& json);
};

struct RedisConfig
{
    std::string host;
    int port;
    std::string password;
    int database;
    
    Poco::JSON::Object toJson() const;
    static RedisConfig fromJson(const Poco::JSON::Object& json);
};

struct CacheConfig
{
    bool enabled;
    std::string type;
    int ttlSeconds;
    int maxSizeMB;
    RedisConfig redis;
    
    Poco::JSON::Object toJson() const;
    static CacheConfig fromJson(const Poco::JSON::Object& json);
};

struct AuditConfig
{
    bool enabled;
    bool logUserActions;
    bool logDataChanges;
    int retentionDays;
    std::vector<std::string> sensitiveFields;
    
    Poco::JSON::Object toJson() const;
    static AuditConfig fromJson(const Poco::JSON::Object& json);
};

struct MonitoringConfig
{
    bool enabled;
    int port;
    bool metricsEnabled;
    bool healthCheckEnabled;
    std::string prometheusEndpoint;
    
    Poco::JSON::Object toJson() const;
    static MonitoringConfig fromJson(const Poco::JSON::Object& json);
};

struct EmailConfig
{
    bool enabled;
    std::string smtpServer;
    int smtpPort;
    std::string username;
    std::string password;
    std::string fromAddress;
    bool useTls;
    
    Poco::JSON::Object toJson() const;
    static EmailConfig fromJson(const Poco::JSON::Object& json);
};

struct SmsConfig
{
    bool enabled;
    std::string provider;
    std::string accountSid;
    std::string authToken;
    std::string fromNumber;
    
    Poco::JSON::Object toJson() const;
    static SmsConfig fromJson(const Poco::JSON::Object& json);
};

struct NotificationsConfig
{
    EmailConfig email;
    SmsConfig sms;
    
    Poco::JSON::Object toJson() const;
    static NotificationsConfig fromJson(const Poco::JSON::Object& json);
};

struct ApiConfig
{
    std::string version;
    std::string basePath;
    bool swaggerEnabled;
    std::string swaggerPath;
    bool rateLimitingEnabled;
    int requestTimeoutSeconds;
    
    Poco::JSON::Object toJson() const;
    static ApiConfig fromJson(const Poco::JSON::Object& json);
};

struct ApplicationInfo
{
    std::string name;
    std::string version;
    std::string environment;
    
    Poco::JSON::Object toJson() const;
    static ApplicationInfo fromJson(const Poco::JSON::Object& json);
};

class ApplicationConfig
{
public:
    ApplicationInfo application;
    DatabaseConfig database;
    ServerConfig server;
    SecurityConfig security;
    LoggingConfig logging;
    InventoryConfig inventory;
    OrdersConfig orders;
    CacheConfig cache;
    AuditConfig audit;
    MonitoringConfig monitoring;
    NotificationsConfig notifications;
    ApiConfig api;
    
    ApplicationConfig() = default;
    explicit ApplicationConfig(const Poco::JSON::Object& json);
    
    Poco::JSON::Object toJson() const;
    std::string toString(bool pretty = false) const;
    
    static ApplicationConfig createDefault();
    
    bool isValid() const;
    
private:
    void initializeFromJson(const Poco::JSON::Object& json);
};

} // namespace config
