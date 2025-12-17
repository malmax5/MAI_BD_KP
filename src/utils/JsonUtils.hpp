#pragma once

#include <string>
#include <vector>
#include <map>
#include <Poco/JSON/Object.h>
#include <Poco/JSON/Array.h>
#include <Poco/Dynamic/Var.h>
#include <Poco/DateTime.h>
#include <Poco/Timestamp.h>

namespace warehouse_backend::utils
{

class JsonUtils
{
public:
    static std::string objectToString(const Poco::JSON::Object& obj, bool pretty = false);
    
    static Poco::JSON::Object::Ptr stringToObject(const std::string& jsonStr);
    
    static std::string arrayToString(const Poco::JSON::Array& arr, bool pretty = false);
    
    static Poco::JSON::Array::Ptr stringToArray(const std::string& jsonStr);
    
    template<typename T>
    static T getValue(const Poco::JSON::Object& obj, const std::string& key, const T& defaultValue = T());
    
    static std::string getString(const Poco::JSON::Object& obj, const std::string& key, 
                                  const std::string& defaultValue = "");
    
    static int getInt(const Poco::JSON::Object& obj, const std::string& key, int defaultValue = 0);
    
    static double getDouble(const Poco::JSON::Object& obj, const std::string& key, 
                             double defaultValue = 0.0);
    
    static bool getBool(const Poco::JSON::Object& obj, const std::string& key, 
                         bool defaultValue = false);
    
    static Poco::JSON::Object::Ptr getObject(const Poco::JSON::Object& obj, 
                                              const std::string& key);
    
    static Poco::JSON::Array::Ptr getArray(const Poco::JSON::Object& obj, 
                                            const std::string& key);
    
    static bool hasKey(const Poco::JSON::Object& obj, const std::string& key);
    
    static std::string dateTimeToISOString(const Poco::DateTime& dt);
    
    static std::string timestampToISOString(const Poco::Timestamp& ts);
    
    static Poco::DateTime isoStringToDateTime(const std::string& isoStr);
    
    static Poco::Timestamp isoStringToTimestamp(const std::string& isoStr);
    
    template<typename T>
    static std::string serialize(const T& obj);
    
    template<typename T>
    static T deserialize(const std::string& jsonStr);
    
    static Poco::JSON::Object createSuccessResponse(const std::string& message = "", 
                                                     Poco::JSON::Object::Ptr data = nullptr);
    
    static Poco::JSON::Object createErrorResponse(const std::string& message, 
                                                   int errorCode = 0,
                                                   Poco::JSON::Object::Ptr details = nullptr);
    
    static bool validateSchema(const Poco::JSON::Object& obj, 
                               const std::map<std::string, std::string>& schema);
    
    static Poco::JSON::Object mapToJson(const std::map<std::string, std::string>& map);
    
    static std::map<std::string, std::string> jsonToMap(const Poco::JSON::Object& obj);
    
    static std::string prettyPrint(const std::string& jsonStr);
    
    static std::string escapeJsonString(const std::string& str);
    
    template<typename T>
    static Poco::JSON::Array createJsonArray(const std::vector<T>& items);
};

} // namespace utils
