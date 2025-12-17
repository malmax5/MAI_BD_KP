#pragma once

#include <Poco/Net/HTTPRequestHandlerFactory.h>
#include <Poco/Net/HTTPRequestHandler.h>
#include <Poco/Net/HTTPServerRequest.h>
#include <Poco/URI.h>
#include <memory>
#include <string>
#include <map>
#include <functional>

namespace warehouse_backend
{

class RequestHandlerFactory : public Poco::Net::HTTPRequestHandlerFactory
{
public:
    RequestHandlerFactory();
    virtual ~RequestHandlerFactory() = default;
    
    Poco::Net::HTTPRequestHandler* createRequestHandler(const Poco::Net::HTTPServerRequest& request) override;
    
    void registerRoute(const std::string& method, const std::string& path, 
                       Poco::Net::HTTPRequestHandler* handler);
    void registerRoute(const std::string& method, const std::string& path,
                       std::function<Poco::Net::HTTPRequestHandler*()> handlerFactory);
    
    void addPreMiddleware(std::function<void(const Poco::Net::HTTPServerRequest&)> middleware);
    void addPostMiddleware(std::function<void(const Poco::Net::HTTPServerRequest&, Poco::Net::HTTPServerResponse&)> middleware);
    
    void setErrorHandler(std::function<Poco::Net::HTTPRequestHandler*(int statusCode)> errorHandler);
    
    static bool matchRoute(const std::string& routePattern, const std::string& requestPath, 
                          std::map<std::string, std::string>& params);
    
    static std::string normalizePath(const std::string& path);
    static std::vector<std::string> splitPath(const std::string& path);
    
private:
    struct RouteInfo
    {
        std::string method;
        std::string pattern;
        std::function<Poco::Net::HTTPRequestHandler*()> handlerFactory;
        bool requiresAuth;
        std::vector<std::string> requiredPermissions;
    };
    
    struct RouteMatch
    {
        RouteInfo route;
        std::map<std::string, std::string> params;
        bool matched;
        
        RouteMatch() : matched(false) {}
    };
    
    RouteMatch findRoute(const std::string& method, const std::string& path) const;
    bool checkPermissions(const RouteInfo& route, const Poco::Net::HTTPServerRequest& request) const;
    
    std::vector<RouteInfo> routes_;
    std::vector<std::function<void(const Poco::Net::HTTPServerRequest&)>> preMiddlewares_;
    std::vector<std::function<void(const Poco::Net::HTTPServerRequest&, Poco::Net::HTTPServerResponse&)>> postMiddlewares_;
    std::function<Poco::Net::HTTPRequestHandler*(int statusCode)> errorHandler_;
    
    static const std::string PATTERN_ID;
    static const std::string PATTERN_UUID;
    static const std::string PATTERN_SKU;
    static const std::string PATTERN_EMAIL;
    
    static std::string buildPattern(const std::string& base, const std::string& paramPattern = PATTERN_ID);
};

} // namespace warehouse_backend
