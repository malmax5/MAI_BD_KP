#include "ApplicationConfig.hpp"
#include "ConfigManager.hpp"
#include "../utils/JsonUtils.hpp"
#include <Poco/JSON/Parser.h>
#include <Poco/JSON/Stringifier.h>
#include <Poco/Exception.h>
#include <sstream>

namespace warehouse_backend::config
{

using namespace warehouse_backend::utils;

Poco::JSON::Object DatabaseConfig::toJson() const
{
    Poco::JSON::Object json;
    json.set("host", host);
    json.set("port", port);
    json.set("database", database);
    json.set("user", user);
    json.set("password", password);
    json.set("connection_timeout", connectionTimeout);
    json.set("pool_size", poolSize);
    json.set("max_connections", maxConnections);
    json.set("min_connections", minConnections);
    json.set("connection_lifetime", connectionLifetime);
    json.set("ssl_mode", sslMode);
    return json;
}

DatabaseConfig DatabaseConfig::fromJson(const Poco::JSON::Object& json)
{
    DatabaseConfig config;
    config.host = JsonUtils::getString(json, "host", "localhost");
    config.port = JsonUtils::getInt(json, "port", 5432);
    config.database = JsonUtils::getString(json, "database", "warehouse_db");
    config.user = JsonUtils::getString(json, "user", "warehouse_user");
    config.password = JsonUtils::getString(json, "password", "");
    config.connectionTimeout = JsonUtils::getInt(json, "connection_timeout", 30);
    config.poolSize = JsonUtils::getInt(json, "pool_size", 10);
    config.maxConnections = JsonUtils::getInt(json, "max_connections", 20);
    config.minConnections = JsonUtils::getInt(json, "min_connections", 5);
    config.connectionLifetime = JsonUtils::getInt(json, "connection_lifetime", 300);
    config.sslMode = JsonUtils::getString(json, "ssl_mode", "disable");
    return config;
}

Poco::JSON::Object ServerConfig::toJson() const
{
    Poco::JSON::Object json;
    json.set("port", port);
    json.set("host", host);
    json.set("max_threads", maxThreads);
    json.set("max_queued", maxQueued);
    json.set("timeout", timeout);
    json.set("keep_alive", keepAlive);
    json.set("keep_alive_timeout", keepAliveTimeout);
    json.set("max_request_size", maxRequestSize);
    json.set("compression", compression);
    
    Poco::JSON::Array origins;
    for (const auto& origin : corsAllowedOrigins)
    {
        origins.add(origin);
    }
    json.set("cors_allowed_origins", origins);
    
    return json;
}

ServerConfig ServerConfig::fromJson(const Poco::JSON::Object& json)
{
    ServerConfig config;
    config.port = JsonUtils::getInt(json, "port", 8080);
    config.host = JsonUtils::getString(json, "host", "0.0.0.0");
    config.maxThreads = JsonUtils::getInt(json, "max_threads", 16);
    config.maxQueued = JsonUtils::getInt(json, "max_queued", 100);
    config.timeout = JsonUtils::getInt(json, "timeout", 30);
    config.keepAlive = JsonUtils::getBool(json, "keep_alive", true);
    config.keepAliveTimeout = JsonUtils::getInt(json, "keep_alive_timeout", 10);
    config.maxRequestSize = JsonUtils::getInt(json, "max_request_size", 10485760);
    config.compression = JsonUtils::getBool(json, "compression", true);
    
    auto originsArray = JsonUtils::getArray(json, "cors_allowed_origins");
    if (originsArray)
    {
        for (size_t i = 0; i < originsArray->size(); ++i)
        {
            config.corsAllowedOrigins.push_back(originsArray->getElement<std::string>(i));
        }
    }
    
    return config;
}

Poco::JSON::Object SecurityConfig::toJson() const
{
    Poco::JSON::Object json;
    json.set("jwt_secret", jwtSecret);
    json.set("token_expiry_hours", tokenExpiryHours);
    json.set("refresh_token_expiry_days", refreshTokenExpiryDays);
    json.set("bcrypt_cost", bcryptCost);
    json.set("password_min_length", passwordMinLength);
    json.set("password_require_uppercase", passwordRequireUppercase);
    json.set("password_require_lowercase", passwordRequireLowercase);
    json.set("password_require_digits", passwordRequireDigits);
    json.set("password_require_special", passwordRequireSpecial);
    json.set("rate_limit_per_minute", rateLimitPerMinute);
    json.set("max_login_attempts", maxLoginAttempts);
    json.set("lockout_duration_minutes", lockoutDurationMinutes);
    return json;
}

SecurityConfig SecurityConfig::fromJson(const Poco::JSON::Object& json)
{
    SecurityConfig config;
    config.jwtSecret = JsonUtils::getString(json, "jwt_secret", "default_secret_key_change_in_production");
    config.tokenExpiryHours = JsonUtils::getInt(json, "token_expiry_hours", 24);
    config.refreshTokenExpiryDays = JsonUtils::getInt(json, "refresh_token_expiry_days", 7);
    config.bcryptCost = JsonUtils::getInt(json, "bcrypt_cost", 12);
    config.passwordMinLength = JsonUtils::getInt(json, "password_min_length", 8);
    config.passwordRequireUppercase = JsonUtils::getBool(json, "password_require_uppercase", true);
    config.passwordRequireLowercase = JsonUtils::getBool(json, "password_require_lowercase", true);
    config.passwordRequireDigits = JsonUtils::getBool(json, "password_require_digits", true);
    config.passwordRequireSpecial = JsonUtils::getBool(json, "password_require_special", true);
    config.rateLimitPerMinute = JsonUtils::getInt(json, "rate_limit_per_minute", 60);
    config.maxLoginAttempts = JsonUtils::getInt(json, "max_login_attempts", 5);
    config.lockoutDurationMinutes = JsonUtils::getInt(json, "lockout_duration_minutes", 15);
    return config;
}

Poco::JSON::Object LoggingConsoleConfig::toJson() const
{
    Poco::JSON::Object json;
    json.set("enabled", enabled);
    json.set("pattern", pattern);
    return json;
}

LoggingConsoleConfig LoggingConsoleConfig::fromJson(const Poco::JSON::Object& json)
{
    LoggingConsoleConfig config;
    config.enabled = JsonUtils::getBool(json, "enabled", true);
    config.pattern = JsonUtils::getString(json, "pattern", "%Y-%m-%d %H:%M:%S [%p] %t");
    return config;
}

Poco::JSON::Object LoggingFileConfig::toJson() const
{
    Poco::JSON::Object json;
    json.set("enabled", enabled);
    json.set("path", path);
    json.set("rotation", rotation);
    json.set("max_size_mb", maxSizeMB);
    json.set("max_files", maxFiles);
    json.set("compress", compress);
    return json;
}

LoggingFileConfig LoggingFileConfig::fromJson(const Poco::JSON::Object& json)
{
    LoggingFileConfig config;
    config.enabled = JsonUtils::getBool(json, "enabled", true);
    config.path = JsonUtils::getString(json, "path", "logs/warehouse.log");
    config.rotation = JsonUtils::getString(json, "rotation", "daily");
    config.maxSizeMB = JsonUtils::getInt(json, "max_size_mb", 100);
    config.maxFiles = JsonUtils::getInt(json, "max_files", 10);
    config.compress = JsonUtils::getBool(json, "compress", true);
    return config;
}

Poco::JSON::Object LoggingDatabaseConfig::toJson() const
{
    Poco::JSON::Object json;
    json.set("enabled", enabled);
    json.set("table", table);
    return json;
}

LoggingDatabaseConfig LoggingDatabaseConfig::fromJson(const Poco::JSON::Object& json)
{
    LoggingDatabaseConfig config;
    config.enabled = JsonUtils::getBool(json, "enabled", false);
    config.table = JsonUtils::getString(json, "table", "system_logs");
    return config;
}

Poco::JSON::Object LoggingConfig::toJson() const
{
    Poco::JSON::Object json;
    json.set("level", level);
    json.set("console", console.toJson());
    json.set("file", file.toJson());
    json.set("database", database.toJson());
    return json;
}

LoggingConfig LoggingConfig::fromJson(const Poco::JSON::Object& json)
{
    LoggingConfig config;
    config.level = JsonUtils::getString(json, "level", "info");
    
    auto consoleJson = JsonUtils::getObject(json, "console");
    if (consoleJson)
    {
        config.console = LoggingConsoleConfig::fromJson(*consoleJson);
    }
    
    auto fileJson = JsonUtils::getObject(json, "file");
    if (fileJson)
    {
        config.file = LoggingFileConfig::fromJson(*fileJson);
    }
    
    auto dbJson = JsonUtils::getObject(json, "database");
    if (dbJson)
    {
        config.database = LoggingDatabaseConfig::fromJson(*dbJson);
    }
    
    return config;
}

Poco::JSON::Object InventoryConfig::toJson() const
{
    Poco::JSON::Object json;
    json.set("default_page_size", defaultPageSize);
    json.set("max_page_size", maxPageSize);
    json.set("auto_reorder_enabled", autoReorderEnabled);
    json.set("reorder_threshold_percent", reorderThresholdPercent);
    json.set("expiration_warning_days", expirationWarningDays);
    json.set("batch_tracking_enabled", batchTrackingEnabled);
    json.set("serial_number_tracking", serialNumberTracking);
    return json;
}

InventoryConfig InventoryConfig::fromJson(const Poco::JSON::Object& json)
{
    InventoryConfig config;
    config.defaultPageSize = JsonUtils::getInt(json, "default_page_size", 50);
    config.maxPageSize = JsonUtils::getInt(json, "max_page_size", 1000);
    config.autoReorderEnabled = JsonUtils::getBool(json, "auto_reorder_enabled", true);
    config.reorderThresholdPercent = JsonUtils::getInt(json, "reorder_threshold_percent", 20);
    config.expirationWarningDays = JsonUtils::getInt(json, "expiration_warning_days", 30);
    config.batchTrackingEnabled = JsonUtils::getBool(json, "batch_tracking_enabled", true);
    config.serialNumberTracking = JsonUtils::getBool(json, "serial_number_tracking", false);
    return config;
}

Poco::JSON::Object OrdersConfig::toJson() const
{
    Poco::JSON::Object json;
    json.set("auto_assign_picker", autoAssignPicker);
    json.set("default_priority", defaultPriority);
    json.set("max_items_per_order", maxItemsPerOrder);
    json.set("allow_backorders", allowBackorders);
    json.set("default_shipping_method", defaultShippingMethod);
    return json;
}

OrdersConfig OrdersConfig::fromJson(const Poco::JSON::Object& json)
{
    OrdersConfig config;
    config.autoAssignPicker = JsonUtils::getBool(json, "auto_assign_picker", true);
    config.defaultPriority = JsonUtils::getString(json, "default_priority", "normal");
    config.maxItemsPerOrder = JsonUtils::getInt(json, "max_items_per_order", 100);
    config.allowBackorders = JsonUtils::getBool(json, "allow_backorders", false);
    config.defaultShippingMethod = JsonUtils::getString(json, "default_shipping_method", "standard");
    return config;
}

Poco::JSON::Object RedisConfig::toJson() const
{
    Poco::JSON::Object json;
    json.set("host", host);
    json.set("port", port);
    json.set("password", password);
    json.set("database", database);
    return json;
}

RedisConfig RedisConfig::fromJson(const Poco::JSON::Object& json)
{
    RedisConfig config;
    config.host = JsonUtils::getString(json, "host", "localhost");
    config.port = JsonUtils::getInt(json, "port", 6379);
    config.password = JsonUtils::getString(json, "password", "");
    config.database = JsonUtils::getInt(json, "database", 0);
    return config;
}

Poco::JSON::Object CacheConfig::toJson() const
{
    Poco::JSON::Object json;
    json.set("enabled", enabled);
    json.set("type", type);
    json.set("ttl_seconds", ttlSeconds);
    json.set("max_size_mb", maxSizeMB);
    json.set("redis", redis.toJson());
    return json;
}

CacheConfig CacheConfig::fromJson(const Poco::JSON::Object& json)
{
    CacheConfig config;
    config.enabled = JsonUtils::getBool(json, "enabled", true);
    config.type = JsonUtils::getString(json, "type", "memory");
    config.ttlSeconds = JsonUtils::getInt(json, "ttl_seconds", 300);
    config.maxSizeMB = JsonUtils::getInt(json, "max_size_mb", 100);
    
    auto redisJson = JsonUtils::getObject(json, "redis");
    if (redisJson)
    {
        config.redis = RedisConfig::fromJson(*redisJson);
    }
    
    return config;
}

Poco::JSON::Object AuditConfig::toJson() const
{
    Poco::JSON::Object json;
    json.set("enabled", enabled);
    json.set("log_user_actions", logUserActions);
    json.set("log_data_changes", logDataChanges);
    json.set("retention_days", retentionDays);
    
    Poco::JSON::Array fields;
    for (const auto& field : sensitiveFields)
    {
        fields.add(field);
    }
    json.set("sensitive_fields", fields);
    
    return json;
}

AuditConfig AuditConfig::fromJson(const Poco::JSON::Object& json)
{
    AuditConfig config;
    config.enabled = JsonUtils::getBool(json, "enabled", true);
    config.logUserActions = JsonUtils::getBool(json, "log_user_actions", true);
    config.logDataChanges = JsonUtils::getBool(json, "log_data_changes", true);
    config.retentionDays = JsonUtils::getInt(json, "retention_days", 365);
    
    auto fieldsArray = JsonUtils::getArray(json, "sensitive_fields");
    if (fieldsArray)
    {
        for (size_t i = 0; i < fieldsArray->size(); ++i)
        {
            config.sensitiveFields.push_back(fieldsArray->getElement<std::string>(i));
        }
    }
    
    return config;
}

Poco::JSON::Object MonitoringConfig::toJson() const
{
    Poco::JSON::Object json;
    json.set("enabled", enabled);
    json.set("port", port);
    json.set("metrics_enabled", metricsEnabled);
    json.set("health_check_enabled", healthCheckEnabled);
    json.set("prometheus_endpoint", prometheusEndpoint);
    return json;
}

MonitoringConfig MonitoringConfig::fromJson(const Poco::JSON::Object& json)
{
    MonitoringConfig config;
    config.enabled = JsonUtils::getBool(json, "enabled", true);
    config.port = JsonUtils::getInt(json, "port", 9090);
    config.metricsEnabled = JsonUtils::getBool(json, "metrics_enabled", true);
    config.healthCheckEnabled = JsonUtils::getBool(json, "health_check_enabled", true);
    config.prometheusEndpoint = JsonUtils::getString(json, "prometheus_endpoint", "/metrics");
    return config;
}

Poco::JSON::Object EmailConfig::toJson() const
{
    Poco::JSON::Object json;
    json.set("enabled", enabled);
    json.set("smtp_server", smtpServer);
    json.set("smtp_port", smtpPort);
    json.set("username", username);
    json.set("password", password);
    json.set("from_address", fromAddress);
    json.set("use_tls", useTls);
    return json;
}

EmailConfig EmailConfig::fromJson(const Poco::JSON::Object& json)
{
    EmailConfig config;
    config.enabled = JsonUtils::getBool(json, "enabled", false);
    config.smtpServer = JsonUtils::getString(json, "smtp_server", "smtp.gmail.com");
    config.smtpPort = JsonUtils::getInt(json, "smtp_port", 587);
    config.username = JsonUtils::getString(json, "username", "");
    config.password = JsonUtils::getString(json, "password", "");
    config.fromAddress = JsonUtils::getString(json, "from_address", "noreply@warehouse.com");
    config.useTls = JsonUtils::getBool(json, "use_tls", true);
    return config;
}

Poco::JSON::Object SmsConfig::toJson() const
{
    Poco::JSON::Object json;
    json.set("enabled", enabled);
    json.set("provider", provider);
    json.set("account_sid", accountSid);
    json.set("auth_token", authToken);
    json.set("from_number", fromNumber);
    return json;
}

SmsConfig SmsConfig::fromJson(const Poco::JSON::Object& json)
{
    SmsConfig config;
    config.enabled = JsonUtils::getBool(json, "enabled", false);
    config.provider = JsonUtils::getString(json, "provider", "twilio");
    config.accountSid = JsonUtils::getString(json, "account_sid", "");
    config.authToken = JsonUtils::getString(json, "auth_token", "");
    config.fromNumber = JsonUtils::getString(json, "from_number", "");
    return config;
}

Poco::JSON::Object NotificationsConfig::toJson() const
{
    Poco::JSON::Object json;
    json.set("email", email.toJson());
    json.set("sms", sms.toJson());
    return json;
}

NotificationsConfig NotificationsConfig::fromJson(const Poco::JSON::Object& json)
{
    NotificationsConfig config;
    
    auto emailJson = JsonUtils::getObject(json, "email");
    if (emailJson)
    {
        config.email = EmailConfig::fromJson(*emailJson);
    }
    
    auto smsJson = JsonUtils::getObject(json, "sms");
    if (smsJson)
    {
        config.sms = SmsConfig::fromJson(*smsJson);
    }
    
    return config;
}

Poco::JSON::Object ApiConfig::toJson() const
{
    Poco::JSON::Object json;
    json.set("version", version);
    json.set("base_path", basePath);
    json.set("swagger_enabled", swaggerEnabled);
    json.set("swagger_path", swaggerPath);
    json.set("rate_limiting_enabled", rateLimitingEnabled);
    json.set("request_timeout_seconds", requestTimeoutSeconds);
    return json;
}

ApiConfig ApiConfig::fromJson(const Poco::JSON::Object& json)
{
    ApiConfig config;
    config.version = JsonUtils::getString(json, "version", "v1");
    config.basePath = JsonUtils::getString(json, "base_path", "/api/v1");
    config.swaggerEnabled = JsonUtils::getBool(json, "swagger_enabled", true);
    config.swaggerPath = JsonUtils::getString(json, "swagger_path", "/api/docs");
    config.rateLimitingEnabled = JsonUtils::getBool(json, "rate_limiting_enabled", true);
    config.requestTimeoutSeconds = JsonUtils::getInt(json, "request_timeout_seconds", 30);
    return config;
}

Poco::JSON::Object ApplicationInfo::toJson() const
{
    Poco::JSON::Object json;
    json.set("name", name);
    json.set("version", version);
    json.set("environment", environment);
    return json;
}

ApplicationInfo ApplicationInfo::fromJson(const Poco::JSON::Object& json)
{
    ApplicationInfo info;
    info.name = JsonUtils::getString(json, "name", "Warehouse Management System");
    info.version = JsonUtils::getString(json, "version", "1.0.0");
    info.environment = JsonUtils::getString(json, "environment", "development");
    return info;
}

ApplicationConfig::ApplicationConfig(const Poco::JSON::Object& json)
{
    initializeFromJson(json);
}

Poco::JSON::Object ApplicationConfig::toJson() const
{
    Poco::JSON::Object json;
    json.set("application", application.toJson());
    json.set("database", database.toJson());
    json.set("server", server.toJson());
    json.set("security", security.toJson());
    json.set("logging", logging.toJson());
    json.set("inventory", inventory.toJson());
    json.set("orders", orders.toJson());
    json.set("cache", cache.toJson());
    json.set("audit", audit.toJson());
    json.set("monitoring", monitoring.toJson());
    json.set("notifications", notifications.toJson());
    json.set("api", api.toJson());
    return json;
}

std::string ApplicationConfig::toString(bool pretty) const
{
    std::ostringstream oss;
    auto json = toJson();
    if (pretty)
    {
        json.stringify(oss, 4);
    }
    else
    {
        json.stringify(oss, 0);
    }
    return oss.str();
}

ApplicationConfig ApplicationConfig::createDefault()
{
    ApplicationConfig config;
    
    config.application.name = "Warehouse Management System";
    config.application.version = "1.0.0";
    config.application.environment = "development";
    
    config.database.host = "localhost";
    config.database.port = 5432;
    config.database.database = "warehouse_db";
    config.database.user = "warehouse_user";
    config.database.password = "warehouse_password";
    config.database.connectionTimeout = 30;
    config.database.poolSize = 10;
    config.database.maxConnections = 20;
    config.database.minConnections = 5;
    config.database.connectionLifetime = 300;
    config.database.sslMode = "disable";
    
    config.server.port = 8080;
    config.server.host = "0.0.0.0";
    config.server.maxThreads = 16;
    config.server.maxQueued = 100;
    config.server.timeout = 30;
    config.server.keepAlive = true;
    config.server.keepAliveTimeout = 10;
    config.server.maxRequestSize = 10485760;
    config.server.compression = true;
    config.server.corsAllowedOrigins = {"http://localhost:3000", "http://localhost:8081"};
    
    config.security.jwtSecret = "SecretKeyAofdamfpKMF{fm131231312";
    config.security.tokenExpiryHours = 24;
    config.security.refreshTokenExpiryDays = 7;
    config.security.bcryptCost = 12;
    config.security.passwordMinLength = 8;
    config.security.passwordRequireUppercase = true;
    config.security.passwordRequireLowercase = true;
    config.security.passwordRequireDigits = true;
    config.security.passwordRequireSpecial = true;
    config.security.rateLimitPerMinute = 60;
    config.security.maxLoginAttempts = 5;
    config.security.lockoutDurationMinutes = 15;
    
    config.logging.level = "info";
    config.logging.console.enabled = true;
    config.logging.console.pattern = "%Y-%m-%d %H:%M:%S [%p] %t";
    config.logging.file.enabled = true;
    config.logging.file.path = "logs/warehouse.log";
    config.logging.file.rotation = "daily";
    config.logging.file.maxSizeMB = 100;
    config.logging.file.maxFiles = 10;
    config.logging.file.compress = true;
    config.logging.database.enabled = false;
    config.logging.database.table = "system_logs";
    
    config.inventory.defaultPageSize = 50;
    config.inventory.maxPageSize = 1000;
    config.inventory.autoReorderEnabled = true;
    config.inventory.reorderThresholdPercent = 20;
    config.inventory.expirationWarningDays = 30;
    config.inventory.batchTrackingEnabled = true;
    config.inventory.serialNumberTracking = false;
    
    config.orders.autoAssignPicker = true;
    config.orders.defaultPriority = "normal";
    config.orders.maxItemsPerOrder = 100;
    config.orders.allowBackorders = false;
    config.orders.defaultShippingMethod = "standard";
    
    config.cache.enabled = true;
    config.cache.type = "memory";
    config.cache.ttlSeconds = 300;
    config.cache.maxSizeMB = 100;
    config.cache.redis.host = "localhost";
    config.cache.redis.port = 6379;
    config.cache.redis.password = "";
    config.cache.redis.database = 0;
    
    config.audit.enabled = true;
    config.audit.logUserActions = true;
    config.audit.logDataChanges = true;
    config.audit.retentionDays = 365;
    config.audit.sensitiveFields = {"password", "password_hash", "token", "secret"};
    
    config.monitoring.enabled = true;
    config.monitoring.port = 9090;
    config.monitoring.metricsEnabled = true;
    config.monitoring.healthCheckEnabled = true;
    config.monitoring.prometheusEndpoint = "/metrics";
    
    config.notifications.email.enabled = false;
    config.notifications.email.smtpServer = "smtp.gmail.com";
    config.notifications.email.smtpPort = 587;
    config.notifications.email.username = "";
    config.notifications.email.password = "";
    config.notifications.email.fromAddress = "noreply@warehouse.com";
    config.notifications.email.useTls = true;
    
    config.notifications.sms.enabled = false;
    config.notifications.sms.provider = "twilio";
    config.notifications.sms.accountSid = "";
    config.notifications.sms.authToken = "";
    config.notifications.sms.fromNumber = "";
    
    config.api.version = "v1";
    config.api.basePath = "/api/v1";
    config.api.swaggerEnabled = true;
    config.api.swaggerPath = "/api/docs";
    config.api.rateLimitingEnabled = true;
    config.api.requestTimeoutSeconds = 30;
    
    return config;
}

bool ApplicationConfig::isValid() const
{
    if (database.host.empty() || database.port <= 0 || database.database.empty())
    {
        return false;
    }
    
    if (server.port <= 0 || server.port > 65535)
    {
        return false;
    }
    
    if (security.jwtSecret.empty())
    {
        return false;
    }
    
    if (security.passwordMinLength < 4)
    {
        return false;
    }
    
    if (server.port == monitoring.port)
    {
        return false;
    }
    
    return true;
}

void ApplicationConfig::initializeFromJson(const Poco::JSON::Object& json)
{
    auto appJson = JsonUtils::getObject(json, "application");
    if (appJson)
    {
        application = ApplicationInfo::fromJson(*appJson);
    }
    
    auto dbJson = JsonUtils::getObject(json, "database");
    if (dbJson)
    {
        database = DatabaseConfig::fromJson(*dbJson);
    }
    
    auto serverJson = JsonUtils::getObject(json, "server");
    if (serverJson)
    {
        server = ServerConfig::fromJson(*serverJson);
    }
    
    auto securityJson = JsonUtils::getObject(json, "security");
    if (securityJson)
    {
        security = SecurityConfig::fromJson(*securityJson);
    }
    
    auto loggingJson = JsonUtils::getObject(json, "logging");
    if (loggingJson)
    {
        logging = LoggingConfig::fromJson(*loggingJson);
    }
    
    auto inventoryJson = JsonUtils::getObject(json, "inventory");
    if (inventoryJson)
    {
        inventory = InventoryConfig::fromJson(*inventoryJson);
    }
    
    auto ordersJson = JsonUtils::getObject(json, "orders");
    if (ordersJson)
    {
        orders = OrdersConfig::fromJson(*ordersJson);
    }
    
    auto cacheJson = JsonUtils::getObject(json, "cache");
    if (cacheJson)
    {
        cache = CacheConfig::fromJson(*cacheJson);
    }
    
    auto auditJson = JsonUtils::getObject(json, "audit");
    if (auditJson)
    {
        audit = AuditConfig::fromJson(*auditJson);
    }
    
    auto monitoringJson = JsonUtils::getObject(json, "monitoring");
    if (monitoringJson)
    {
        monitoring = MonitoringConfig::fromJson(*monitoringJson);
    }
    
    auto notificationsJson = JsonUtils::getObject(json, "notifications");
    if (notificationsJson)
    {
        notifications = NotificationsConfig::fromJson(*notificationsJson);
    }
    
    auto apiJson = JsonUtils::getObject(json, "api");
    if (apiJson)
    {
        api = ApiConfig::fromJson(*apiJson);
    }
}

} // namespace config
