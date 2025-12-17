#include "BaseController.hpp"
#include "../utils/JsonUtils.hpp"
#include "../utils/Validator.hpp"
#include <Poco/URI.h>
#include <Poco/Net/HTTPRequestHandler.h>
#include <Poco/Net/HTTPServerRequest.h>
#include <Poco/Net/HTTPServerResponse.h>
#include <Poco/StreamCopier.h>
#include <Poco/DateTimeFormatter.h>
#include <Poco/DateTimeFormat.h>
#include <Poco/NumberParser.h>
#include <Poco/StringTokenizer.h>
#include <chrono>
#include <mutex>
#include <map>

namespace warehouse_backend::controllers
{

std::map<std::string, BaseController::RateLimitEntry> BaseController::rateLimitStore;
std::mutex BaseController::rateLimitMutex;

BaseController::BaseController()
{

}

void BaseController::handleRequest(Poco::Net::HTTPServerRequest& request, 
                                           Poco::Net::HTTPServerResponse& response)
{
    sendNotFoundResponse(response, "Endpoint not found");
}

void BaseController::sendJsonResponse(Poco::Net::HTTPServerResponse& response, 
                                     Poco::JSON::Object::Ptr json, 
                                     Poco::Net::HTTPResponse::HTTPStatus status)
{
    response.setStatus(status);
    response.setContentType("application/json");
    response.setChunkedTransferEncoding(true);
    
    std::ostream& ostr = response.send();
    if (json)
    {
        json->stringify(ostr);
    }
    else
    {
        Poco::JSON::Object empty;
        empty.stringify(ostr);
    }
}

void BaseController::sendSuccessResponse(Poco::Net::HTTPServerResponse& response, 
                                       const std::string& message, 
                                       Poco::JSON::Object::Ptr data)
{
    Poco::JSON::Object::Ptr responseJson = new Poco::JSON::Object;
    responseJson->set("success", true);
    responseJson->set("timestamp", getCurrentTimestamp());
    
    if (!message.empty())
    {
        responseJson->set("message", message);
    }
    
    if (data)
    {
        responseJson->set("data", data);
    }

    sendJsonResponse(response, responseJson, Poco::Net::HTTPResponse::HTTP_OK);
}

void BaseController::sendErrorResponse(Poco::Net::HTTPServerResponse& response, 
                                      const std::string& message, 
                                      Poco::Net::HTTPResponse::HTTPStatus status,
                                      int errorCode)
{
    Poco::JSON::Object::Ptr responseJson = new Poco::JSON::Object;
    responseJson->set("success", false);
    responseJson->set("timestamp", getCurrentTimestamp());
    responseJson->set("message", message);
    
    if (errorCode != 0)
    {
        responseJson->set("errorCode", errorCode);
    }
    
    sendJsonResponse(response, responseJson, status);
}

void BaseController::sendNotFoundResponse(Poco::Net::HTTPServerResponse& response, 
                                        const std::string& resource)
{
    std::string message = resource.empty() ? "Resource not found" : 
                         resource + " not found";
    sendErrorResponse(response, message, Poco::Net::HTTPResponse::HTTP_NOT_FOUND);
}

void BaseController::sendUnauthorizedResponse(Poco::Net::HTTPServerResponse& response, 
                                            const std::string& message)
{
    sendErrorResponse(response, message, Poco::Net::HTTPResponse::HTTP_UNAUTHORIZED);
}

void BaseController::sendForbiddenResponse(Poco::Net::HTTPServerResponse& response, 
                                         const std::string& message)
{
    sendErrorResponse(response, message, Poco::Net::HTTPResponse::HTTP_FORBIDDEN);
}

void BaseController::sendValidationErrorResponse(Poco::Net::HTTPServerResponse& response, 
                                               const std::vector<std::string>& errors)
{
    Poco::JSON::Object::Ptr responseJson = new Poco::JSON::Object;
    responseJson->set("success", false);
    responseJson->set("timestamp", getCurrentTimestamp());
    responseJson->set("message", "Validation failed");
    
    Poco::JSON::Array::Ptr errorsArray = new Poco::JSON::Array;
    for (const auto& error : errors)
    {
        errorsArray->add(error);
    }
    responseJson->set("errors", errorsArray);
    
    sendJsonResponse(response, responseJson, Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
}

Poco::JSON::Object::Ptr BaseController::parseJsonBody(Poco::Net::HTTPServerRequest& request)
{
    try
    {
        std::istream& stream = request.stream();
        std::string body;
        Poco::StreamCopier::copyToString(stream, body);
        
        if (body.empty())
        {
            return nullptr;
        }
        
        Poco::JSON::Parser parser;
        Poco::Dynamic::Var result = parser.parse(body);
        return result.extract<Poco::JSON::Object::Ptr>();
    }
    catch (const Poco::Exception& e)
    {
        throw Poco::JSON::JSONException("Failed to parse JSON body: " + e.message());
    }
    catch (const std::exception& e)
    {
        throw Poco::JSON::JSONException("Failed to parse JSON body: " + std::string(e.what()));
    }
}

Poco::Dynamic::Var BaseController::parseJsonBodyToVar(Poco::Net::HTTPServerRequest& request)
{
    try
    {
        std::istream& stream = request.stream();
        std::string body;
        Poco::StreamCopier::copyToString(stream, body);
        
        if (body.empty())
        {
            return Poco::Dynamic::Var();
        }
        
        Poco::JSON::Parser parser;
        return parser.parse(body);
    }
    catch (const Poco::Exception& e)
    {
        throw Poco::JSON::JSONException("Failed to parse JSON body: " + e.message());
    }
}

std::string BaseController::getPathParameter(const std::string& uri, const std::string& basePath, int paramIndex)
{
    try
    {
        std::string path = uri;
        size_t queryPos = path.find('?');
        if (queryPos != std::string::npos)
        {
            path = path.substr(0, queryPos);
        }
        
        if (path.find(basePath) == 0)
        {
            path = path.substr(basePath.length());
        }
        
        Poco::StringTokenizer tokenizer(path, "/", Poco::StringTokenizer::TOK_TRIM);
        
        if (paramIndex >= 0 && paramIndex < (int)tokenizer.count())
        {
            return tokenizer[paramIndex];
        }
    }
    catch (...)
    {
    }
    
    return "";
}

std::string BaseController::getQueryParameter(const std::string& uri, const std::string& paramName)
{
    try
    {
        Poco::URI pocoUri(uri);
        Poco::URI::QueryParameters params = pocoUri.getQueryParameters();
        
        for (const auto& param : params)
        {
            if (param.first == paramName)
            {
                return param.second;
            }
        }
    }
    catch (...)
    {
    }
    
    return "";
}

std::vector<std::string> BaseController::getQueryParameters(const std::string& uri, const std::string& paramName)
{
    std::vector<std::string> values;
    
    try
    {
        Poco::URI pocoUri(uri);
        Poco::URI::QueryParameters params = pocoUri.getQueryParameters();
        
        for (const auto& param : params)
        {
            if (param.first == paramName)
            {
                values.push_back(param.second);
            }
        }
    }
    catch (...)
    {
    }
    
    return values;
}

bool BaseController::getPaginationParameters(Poco::Net::HTTPServerRequest& request, 
                                            int& page, int& pageSize, int defaultPageSize)
{
    std::string pageStr = getQueryParameter(request.getURI(), "page");
    std::string pageSizeStr = getQueryParameter(request.getURI(), "pageSize");
    
    page = 1;
    pageSize = defaultPageSize;
    
    bool valid = true;
    
    if (!pageStr.empty())
    {
        try
        {
            page = Poco::NumberParser::parse(pageStr);
            if (page < 1) page = 1;
        }
        catch (...)
        {
            valid = false;
        }
    }
    
    if (!pageSizeStr.empty())
    {
        try
        {
            pageSize = Poco::NumberParser::parse(pageSizeStr);
            if (pageSize < 1) pageSize = 1;
            if (pageSize > 100) pageSize = 100;
        }
        catch (...)
        {
            valid = false;
        }
    }
    
    return valid;
}

std::string BaseController::getSortParameter(Poco::Net::HTTPServerRequest& request, 
                                            const std::string& defaultSort)
{
    std::string sort = getQueryParameter(request.getURI(), "sort");
    return sort.empty() ? defaultSort : sort;
}

std::string BaseController::getSortDirection(Poco::Net::HTTPServerRequest& request, 
                                            const std::string& defaultDirection)
{
    std::string direction = getQueryParameter(request.getURI(), "direction");
    if (direction.empty())
    {
        direction = getQueryParameter(request.getURI(), "order");
    }
    
    std::string lowerDirection;
    std::transform(direction.begin(), direction.end(), 
                   std::back_inserter(lowerDirection), ::tolower);
    
    if (lowerDirection == "asc" || lowerDirection == "desc")
    {
        return lowerDirection;
    }
    
    return defaultDirection;
}

std::map<std::string, std::string> BaseController::getFilterParameters(Poco::Net::HTTPServerRequest& request)
{
    std::map<std::string, std::string> filters;
    
    try
    {
        Poco::URI pocoUri(request.getURI());
        Poco::URI::QueryParameters params = pocoUri.getQueryParameters();
        
        for (const auto& param : params)
        {
            if (param.first == "page" || param.first == "pageSize" || 
                param.first == "sort" || param.first == "direction" || 
                param.first == "order")
            {
                continue;
            }
            
            filters[param.first] = param.second;
        }
    }
    catch (...)
    {
    }
    
    return filters;
}

std::string BaseController::getHeader(Poco::Net::HTTPServerRequest& request, 
                                     const std::string& headerName, 
                                     const std::string& defaultValue)
{
    try
    {
        if (request.has(headerName))
        {
            return request[headerName];
        }
    }
    catch (...)
    {
    }
    
    return defaultValue;
}

std::string BaseController::getAuthorizationHeader(Poco::Net::HTTPServerRequest& request)
{
    return getHeader(request, "Authorization");
}

std::string BaseController::extractBearerToken(const std::string& authHeader)
{
    return extractTokenFromHeader(authHeader);
}

std::string BaseController::getClientIpAddress(Poco::Net::HTTPServerRequest& request)
{
    std::string ip = getHeader(request, "X-Forwarded-For");
    
    if (ip.empty())
    {
        ip = getHeader(request, "X-Real-IP");
    }
    
    if (ip.empty())
    {
        ip = request.clientAddress().host().toString();
    }
    
    size_t commaPos = ip.find(',');
    if (commaPos != std::string::npos)
    {
        ip = ip.substr(0, commaPos);
    }
    
    return ip;
}

std::string BaseController::getUserAgent(Poco::Net::HTTPServerRequest& request)
{
    return getHeader(request, "User-Agent", "Unknown");
}

bool BaseController::validateRequiredFields(const Poco::JSON::Object::Ptr& json, 
                                           const std::vector<std::string>& requiredFields,
                                           std::vector<std::string>& missingFields)
{
    missingFields.clear();
    
    if (!json)
    {
        missingFields = requiredFields;
        return false;
    }
    
    for (const auto& field : requiredFields)
    {
        if (!json->has(field) || json->get(field).isEmpty())
        {
            missingFields.push_back(field);
        }
    }
    
    return missingFields.empty();
}

bool BaseController::validateFieldType(const Poco::Dynamic::Var& field, 
                                      const std::string& expectedType,
                                      std::string& error)
{
    error.clear();
    
    try
    {
        if (expectedType == "string")
        {
            field.convert<std::string>();
        }
        else if (expectedType == "int")
        {
            field.convert<int>();
        }
        else if (expectedType == "double")
        {
            field.convert<double>();
        }
        else if (expectedType == "bool")
        {
            field.convert<bool>();
        }
        else if (expectedType == "array")
        {
            field.extract<Poco::JSON::Array::Ptr>();
        }
        else if (expectedType == "object")
        {
            field.extract<Poco::JSON::Object::Ptr>();
        }
        else
        {
            error = "Unknown expected type: " + expectedType;
            return false;
        }
    }
    catch (...)
    {
        error = "Field is not of type " + expectedType;
        return false;
    }
    
    return true;
}

void BaseController::logRequest(Poco::Net::HTTPServerRequest& request, 
                               const std::string& method, 
                               const std::string& endpoint)
{
    std::string ip = getClientIpAddress(request);
    std::string userAgent = getUserAgent(request);
    
    std::cout << "[" << getCurrentTimestamp() << "] "
              << ip << " " << method << " " << endpoint
              << " UA: " << userAgent << std::endl;
}

void BaseController::logResponse(Poco::Net::HTTPServerRequest& request, 
                                Poco::Net::HTTPServerResponse& response, 
                                const std::string& method, 
                                const std::string& endpoint,
                                long long durationMs)
{
    std::string ip = getClientIpAddress(request);
    
    std::cout << "[" << getCurrentTimestamp() << "] "
              << ip << " " << method << " " << endpoint
              << " " << response.getStatus() << " " << durationMs << "ms" << std::endl;
}

bool BaseController::checkRateLimit(const std::string& clientId, 
                                   const std::string& endpoint, 
                                   int maxRequests, 
                                   int timeWindowSeconds)
{
    std::lock_guard<std::mutex> lock(rateLimitMutex);
    
    auto now = std::chrono::steady_clock::now();
    std::string key = clientId + ":" + endpoint;
    
    auto it = rateLimitStore.find(key);
    if (it == rateLimitStore.end())
    {
        RateLimitEntry entry;
        entry.requestCount = 1;
        entry.windowStart = now;
        rateLimitStore[key] = entry;
        return true;
    }
    
    auto& entry = it->second;
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - entry.windowStart);
    
    if (elapsed.count() >= timeWindowSeconds)
    {
        entry.requestCount = 1;
        entry.windowStart = now;
        return true;
    }
    else if (entry.requestCount < maxRequests)
    {
        entry.requestCount++;
        return true;
    }
    
    return false;
}

void BaseController::setCorsHeaders(Poco::Net::HTTPServerResponse& response)
{
    response.set("Access-Control-Allow-Origin", "*");
    response.set("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    response.set("Access-Control-Allow-Headers", "Content-Type, Authorization, X-Requested-With");
    response.set("Access-Control-Max-Age", "86400");
}

std::string BaseController::generateRequestId()
{
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    
    return "req_" + std::to_string(millis);
}

std::string BaseController::getCurrentTimestamp()
{
    Poco::DateTime now;
    return Poco::DateTimeFormatter::format(now, Poco::DateTimeFormat::ISO8601_FORMAT);
}

bool BaseController::validateRequest(Poco::Net::HTTPServerRequest& request, 
                                    Poco::Net::HTTPServerResponse& response,
                                    std::string& errorMessage)
{
    if (request.getMethod() == "POST" || request.getMethod() == "PUT")
    {
        if (request.getContentType().find("application/json") == std::string::npos)
        {
            errorMessage = "Content-Type must be application/json";
            return false;
        }
    }
    
    return true;
}

bool BaseController::authorizeRequest(Poco::Net::HTTPServerRequest& request, 
                                     Poco::Net::HTTPServerResponse& response,
                                     std::string& errorMessage)
{
    return true;
}

std::string BaseController::extractTokenFromHeader(const std::string& headerValue)
{
    if (headerValue.empty())
    {
        return "";
    }
    
    std::string prefix = "Bearer ";
    if (headerValue.find(prefix) == 0)
    {
        return headerValue.substr(prefix.length());
    }
    
    return headerValue;
}

std::string BaseController::sanitizeInput(const std::string& input)
{
    std::string sanitized = input;
    
    sanitized.erase(std::remove(sanitized.begin(), sanitized.end(), '\0'), sanitized.end());
    
    size_t start = sanitized.find_first_not_of(" \t\n\r");
    size_t end = sanitized.find_last_not_of(" \t\n\r");
    
    if (start == std::string::npos || end == std::string::npos)
    {
        return "";
    }
    
    return sanitized.substr(start, end - start + 1);
}

} // namespace controllers
