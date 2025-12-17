#include "RequestHandlerFactory.hpp"
#include "controllers/BaseController.hpp"
#include "controllers/AuthController.hpp"
#include "controllers/UserController.hpp"
#include <Poco/Net/HTTPServerRequest.h>
#include <Poco/URI.h>
#include <Poco/StringTokenizer.h>
#include <Poco/RegularExpression.h>
#include <Poco/NumberParser.h>
#include <memory>
#include <sstream>

namespace warehouse_backend
{

const std::string RequestHandlerFactory::PATTERN_ID = "[0-9]+";
const std::string RequestHandlerFactory::PATTERN_UUID = "[a-fA-F0-9]{8}-[a-fA-F0-9]{4}-[a-fA-F0-9]{4}-[a-fA-F0-9]{4}-[a-fA-F0-9]{12}";
const std::string RequestHandlerFactory::PATTERN_SKU = "[A-Za-z0-9\\-]+";
const std::string RequestHandlerFactory::PATTERN_EMAIL = "[^/@]+@[^/@]+\\.[^/@]+";

RequestHandlerFactory::RequestHandlerFactory()
{
    errorHandler_ = [](int statusCode) -> Poco::Net::HTTPRequestHandler* {
        return new controllers::BaseController();
    };
}

Poco::Net::HTTPRequestHandler* RequestHandlerFactory::createRequestHandler(const Poco::Net::HTTPServerRequest& request)
{
    try
    {
        Poco::URI uri(request.getURI());
        std::string path = normalizePath(uri.getPath());
        std::string method = request.getMethod();
        
        for (const auto& middleware : preMiddlewares_)
        {
            middleware(request);
        }
        
        RouteMatch match = findRoute(method, path);
        
        if (match.matched)
        {
            if (match.route.requiresAuth && !checkPermissions(match.route, request))
            {
                return errorHandler_(403);
            }
            
            Poco::Net::HTTPRequestHandler* handler = match.route.handlerFactory();
            
            if (auto baseController = dynamic_cast<controllers::BaseController*>(handler))
            {
            }
            
            return handler;
        }
        
        return errorHandler_(404);
    }
    catch (const std::exception& e)
    {
        return errorHandler_(500);
    }
}

void RequestHandlerFactory::registerRoute(const std::string& method, const std::string& path, 
                                         Poco::Net::HTTPRequestHandler* handler)
{
    registerRoute(method, path, [handler]() { return handler; });
}

void RequestHandlerFactory::registerRoute(const std::string& method, const std::string& path,
                                         std::function<Poco::Net::HTTPRequestHandler*()> handlerFactory)
{
    RouteInfo route;
    route.method = method;
    route.pattern = normalizePath(path);
    route.handlerFactory = handlerFactory;
    route.requiresAuth = false;
    
    routes_.push_back(route);
}

void RequestHandlerFactory::addPreMiddleware(std::function<void(const Poco::Net::HTTPServerRequest&)> middleware)
{
    preMiddlewares_.push_back(middleware);
}

void RequestHandlerFactory::addPostMiddleware(std::function<void(const Poco::Net::HTTPServerRequest&, Poco::Net::HTTPServerResponse&)> middleware)
{
    postMiddlewares_.push_back(middleware);
}

void RequestHandlerFactory::setErrorHandler(std::function<Poco::Net::HTTPRequestHandler*(int statusCode)> errorHandler)
{
    errorHandler_ = errorHandler;
}

bool RequestHandlerFactory::matchRoute(const std::string& routePattern, const std::string& requestPath, 
                                      std::map<std::string, std::string>& params)
{
    std::vector<std::string> patternSegments = splitPath(routePattern);
    std::vector<std::string> pathSegments = splitPath(requestPath);
    
    if (patternSegments.size() != pathSegments.size())
    {
        return false;
    }
    
    for (size_t i = 0; i < patternSegments.size(); ++i)
    {
        const std::string& patternSeg = patternSegments[i];
        const std::string& pathSeg = pathSegments[i];
        
        if (patternSeg.length() > 2 && 
            patternSeg[0] == '{' && 
            patternSeg[patternSeg.length() - 1] == '}')
        {
            std::string paramName = patternSeg.substr(1, patternSeg.length() - 2);
            
            size_t colonPos = paramName.find(':');
            if (colonPos != std::string::npos)
            {
                std::string paramType = paramName.substr(colonPos + 1);
                paramName = paramName.substr(0, colonPos);
                
                if (paramType == "int" || paramType == "id")
                {
                    try
                    {
                        Poco::Int64 value;
                        if (Poco::NumberParser::tryParse64(pathSeg, value))
                        {
                            params[paramName] = pathSeg;
                        }
                        else
                        {
                            return false;
                        }
                    }
                    catch (...)
                    {
                        return false;
                    }
                }
                else if (paramType == "uuid")
                {
                    Poco::RegularExpression uuidRegex("^[a-fA-F0-9]{8}-[a-fA-F0-9]{4}-[a-fA-F0-9]{4}-[a-fA-F0-9]{4}-[a-fA-F0-9]{12}$");
                    if (uuidRegex.match(pathSeg))
                    {
                        params[paramName] = pathSeg;
                    }
                    else
                    {
                        return false;
                    }
                }
                else if (paramType == "string")
                {
                    if (!pathSeg.empty())
                    {
                        params[paramName] = pathSeg;
                    }
                    else
                    {
                        return false;
                    }
                }
                else
                {
                    params[paramName] = pathSeg;
                }
            }
            else
            {
                if (!pathSeg.empty())
                {
                    params[paramName] = pathSeg;
                }
                else
                {
                    return false;
                }
            }
        }
        else if (patternSeg != pathSeg)
        {
            return false;
        }
    }
    
    return true;
}

std::string RequestHandlerFactory::normalizePath(const std::string& path)
{
    if (path.empty() || path == "/")
    {
        return "/";
    }
    
    std::string normalized = path;
    if (normalized[0] != '/')
    {
        normalized = "/" + normalized;
    }
    
    if (normalized.length() > 1 && normalized[normalized.length() - 1] == '/')
    {
        normalized = normalized.substr(0, normalized.length() - 1);
    }
    
    Poco::RegularExpression slashRegex("/{2,}");
    slashRegex.subst(normalized, "/", Poco::RegularExpression::RE_GLOBAL);
    
    return normalized;
}

std::vector<std::string> RequestHandlerFactory::splitPath(const std::string& path)
{
    std::vector<std::string> parts;
    
    if (path.empty() || path == "/")
    {
        return parts;
    }
    
    std::string normalized = normalizePath(path);
    
    size_t start = (normalized[0] == '/') ? 1 : 0;
    size_t end = normalized.find('/', start);
    
    while (end != std::string::npos)
    {
        std::string segment = normalized.substr(start, end - start);
        if (!segment.empty())
        {
            parts.push_back(segment);
        }
        start = end + 1;
        end = normalized.find('/', start);
    }
    
    if (start < normalized.length())
    {
        std::string segment = normalized.substr(start);
        if (!segment.empty())
        {
            parts.push_back(segment);
        }
    }
    
    return parts;
}

RequestHandlerFactory::RouteMatch RequestHandlerFactory::findRoute(const std::string& method, const std::string& path) const
{
    RouteMatch match;
    
    for (const auto& route : routes_)
    {
        if (route.method == method || route.method == "ANY")
        {
            std::map<std::string, std::string> params;
            if (matchRoute(route.pattern, path, params))
            {
                match.route = route;
                match.params = params;
                match.matched = true;
                return match;
            }
        }
    }
    
    return match;
}

bool RequestHandlerFactory::checkPermissions(const RouteInfo& route, const Poco::Net::HTTPServerRequest& request) const
{
    if (!route.requiresAuth)
    {
        return true;
    }
    
    std::string authHeader = request.get("Authorization", "");
    
    if (authHeader.empty())
    {
        return false;
    }
    
    if (authHeader.find("Bearer ") == 0)
    {
        std::string token = authHeader.substr(7);
        
        if (!token.empty())
        {
            if (route.requiredPermissions.empty())
            {
                return true;
            }
            
            return true;
        }
    }
    
    return false;
}

std::string RequestHandlerFactory::buildPattern(const std::string& base, const std::string& paramPattern)
{
    std::string pattern = normalizePath(base);
    
    size_t pos = pattern.find("{id}");
    if (pos != std::string::npos)
    {
        std::string replacement = "{id:" + paramPattern + "}";
        pattern.replace(pos, 4, replacement);
    }
    
    return pattern;
}

} // namespace warehouse_backend
