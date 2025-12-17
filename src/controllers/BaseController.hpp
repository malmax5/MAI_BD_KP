#pragma once

#include <Poco/Net/HTTPRequestHandler.h>
#include <Poco/Net/HTTPServerRequest.h>
#include <Poco/Net/HTTPServerResponse.h>
#include <Poco/JSON/Object.h>
#include <Poco/JSON/Parser.h>
#include <Poco/Net/HTMLForm.h>
#include <string>
#include <memory>
#include <map>
#include <mutex>

namespace warehouse_backend::controllers
{

class BaseController : public Poco::Net::HTTPRequestHandler
{
public:
    BaseController();
    virtual ~BaseController() = default;

protected:
    void sendJsonResponse(Poco::Net::HTTPServerResponse& response, 
                         Poco::JSON::Object::Ptr json, 
                         Poco::Net::HTTPResponse::HTTPStatus status = Poco::Net::HTTPResponse::HTTP_OK);
    
    void sendSuccessResponse(Poco::Net::HTTPServerResponse& response, 
                           const std::string& message = "", 
                           Poco::JSON::Object::Ptr data = nullptr);
    
    void sendErrorResponse(Poco::Net::HTTPServerResponse& response, 
                          const std::string& message, 
                          Poco::Net::HTTPResponse::HTTPStatus status = Poco::Net::HTTPResponse::HTTP_BAD_REQUEST,
                          int errorCode = 0);
    
    void sendNotFoundResponse(Poco::Net::HTTPServerResponse& response, 
                            const std::string& resource = "");
    
    void sendUnauthorizedResponse(Poco::Net::HTTPServerResponse& response, 
                                const std::string& message = "Authentication required");
    
    void sendForbiddenResponse(Poco::Net::HTTPServerResponse& response, 
                             const std::string& message = "Access forbidden");
    
    void sendValidationErrorResponse(Poco::Net::HTTPServerResponse& response, 
                                   const std::vector<std::string>& errors);
    
    Poco::JSON::Object::Ptr parseJsonBody(Poco::Net::HTTPServerRequest& request);
    Poco::Dynamic::Var parseJsonBodyToVar(Poco::Net::HTTPServerRequest& request);
    
    std::string getPathParameter(const std::string& uri, const std::string& basePath, int paramIndex);
    std::string getQueryParameter(const std::string& uri, const std::string& paramName);
    std::vector<std::string> getQueryParameters(const std::string& uri, const std::string& paramName);
    
    bool getPaginationParameters(Poco::Net::HTTPServerRequest& request, 
                                int& page, int& pageSize, int defaultPageSize = 20);
    
    std::string getSortParameter(Poco::Net::HTTPServerRequest& request, 
                                const std::string& defaultSort = "");
    std::string getSortDirection(Poco::Net::HTTPServerRequest& request, 
                                const std::string& defaultDirection = "asc");
    
    std::map<std::string, std::string> getFilterParameters(Poco::Net::HTTPServerRequest& request);
    
    std::string getHeader(Poco::Net::HTTPServerRequest& request, 
                         const std::string& headerName, 
                         const std::string& defaultValue = "");
    
    std::string getAuthorizationHeader(Poco::Net::HTTPServerRequest& request);
    std::string extractBearerToken(const std::string& authHeader);
    
    std::string getClientIpAddress(Poco::Net::HTTPServerRequest& request);
    std::string getUserAgent(Poco::Net::HTTPServerRequest& request);
    
    bool validateRequiredFields(const Poco::JSON::Object::Ptr& json, 
                               const std::vector<std::string>& requiredFields,
                               std::vector<std::string>& missingFields);
    
    bool validateFieldType(const Poco::Dynamic::Var& field, 
                          const std::string& expectedType,
                          std::string& error);
    
    void logRequest(Poco::Net::HTTPServerRequest& request, 
                   const std::string& method, 
                   const std::string& endpoint);
    
    void logResponse(Poco::Net::HTTPServerRequest& request, 
                    Poco::Net::HTTPServerResponse& response, 
                    const std::string& method, 
                    const std::string& endpoint,
                    long long durationMs);
    
    bool checkRateLimit(const std::string& clientId, 
                       const std::string& endpoint, 
                       int maxRequests, 
                       int timeWindowSeconds);
    
    void setCorsHeaders(Poco::Net::HTTPServerResponse& response);
    
    std::string generateRequestId();
    std::string getCurrentTimestamp();
    
    virtual void handleRequest(Poco::Net::HTTPServerRequest& request, 
                              Poco::Net::HTTPServerResponse& response);
    
    virtual bool validateRequest(Poco::Net::HTTPServerRequest& request, 
                                Poco::Net::HTTPServerResponse& response,
                                std::string& errorMessage);
    
    virtual bool authorizeRequest(Poco::Net::HTTPServerRequest& request, 
                                 Poco::Net::HTTPServerResponse& response,
                                 std::string& errorMessage);
    
private:
    std::string extractTokenFromHeader(const std::string& headerValue);
    std::string sanitizeInput(const std::string& input);
    
    struct RateLimitEntry
    {
        int requestCount;
        std::chrono::steady_clock::time_point windowStart;
    };
    
    static std::map<std::string, RateLimitEntry> rateLimitStore;
    static std::mutex rateLimitMutex;
};

} // namespace controllers
