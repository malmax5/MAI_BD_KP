#include "JsonUtils.hpp"
#include <Poco/JSON/Parser.h>
#include <Poco/JSON/Stringifier.h>
#include <Poco/JSON/Query.h>
#include <Poco/StreamCopier.h>
#include <Poco/DateTimeFormat.h>
#include <Poco/DateTimeParser.h>
#include <Poco/NumberParser.h>
#include <Poco/NumberFormatter.h>
#include <Poco/UTF8String.h>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace warehouse_backend::utils
{

std::string JsonUtils::objectToString(const Poco::JSON::Object& obj, bool pretty)
{
    std::ostringstream oss;
    if (pretty)
    {
        obj.stringify(oss, 4);
    }
    else
    {
        obj.stringify(oss, 0);
    }

    return oss.str();
}

Poco::JSON::Object::Ptr JsonUtils::stringToObject(const std::string& jsonStr)
{
    Poco::JSON::Parser parser;
    Poco::Dynamic::Var result = parser.parse(jsonStr);

    return result.extract<Poco::JSON::Object::Ptr>();
}

std::string JsonUtils::arrayToString(const Poco::JSON::Array& arr, bool pretty)
{
    std::ostringstream oss;

    if (pretty)
    {
        arr.stringify(oss, 4);
    }
    else
    {
        arr.stringify(oss, 0);
    }

    return oss.str();
}

Poco::JSON::Array::Ptr JsonUtils::stringToArray(const std::string& jsonStr)
{
    Poco::JSON::Parser parser;
    Poco::Dynamic::Var result = parser.parse(jsonStr);

    return result.extract<Poco::JSON::Array::Ptr>();
}

template<>
std::string JsonUtils::getValue<std::string>(const Poco::JSON::Object& obj, 
                                               const std::string& key, 
                                               const std::string& defaultValue)
{
    if (obj.has(key))
    {
        try
        {
            return obj.getValue<std::string>(key);
        }
        catch (...)
        {
            return defaultValue;
        }
    }

    return defaultValue;
}

template<>
int JsonUtils::getValue<int>(const Poco::JSON::Object& obj, 
                              const std::string& key, 
                              const int& defaultValue)
{
    if (obj.has(key))
    {
        try
        {
            return obj.getValue<int>(key);
        }
        catch (...)
        {
            return defaultValue;
        }
    }
    
    return defaultValue;
}

template<>
double JsonUtils::getValue<double>(const Poco::JSON::Object& obj, 
                                    const std::string& key, 
                                    const double& defaultValue)
{
    if (obj.has(key))
    {
        try
        {
            return obj.getValue<int>(key);
        }
        catch (...)
        {
            return defaultValue;
        }
    }
    
    return defaultValue;
}

template<>
bool JsonUtils::getValue<bool>(const Poco::JSON::Object& obj, 
                                const std::string& key, 
                                const bool& defaultValue)
{
    if (obj.has(key))
    {
        try
        {
            return obj.getValue<int>(key);
        }
        catch (...)
        {
            return defaultValue;
        }
    }
    
    return defaultValue;
}

std::string JsonUtils::getString(const Poco::JSON::Object& obj, const std::string& key, 
                                  const std::string& defaultValue)
{
    return getValue<std::string>(obj, key, defaultValue);
}

int JsonUtils::getInt(const Poco::JSON::Object& obj, const std::string& key, int defaultValue)
{
    return getValue<int>(obj, key, defaultValue);
}

double JsonUtils::getDouble(const Poco::JSON::Object& obj, const std::string& key, 
                             double defaultValue)
{
    return getValue<double>(obj, key, defaultValue);
}

bool JsonUtils::getBool(const Poco::JSON::Object& obj, const std::string& key, 
                         bool defaultValue)
{
    return getValue<bool>(obj, key, defaultValue);
}

Poco::JSON::Object::Ptr JsonUtils::getObject(const Poco::JSON::Object& obj, 
                                              const std::string& key)
{
    if (obj.has(key))
    {
        try
        {
            return obj.getObject(key);
        }
    
    catch (...) {
            return nullptr;
        }
    }

    return nullptr;
}

Poco::JSON::Array::Ptr JsonUtils::getArray(const Poco::JSON::Object& obj, 
                                            const std::string& key)
{
    if (obj.has(key))
    {
        try
        {
            return obj.getArray(key);
        }
        catch (...)
        {
            return nullptr;
        }
    }

    return nullptr;
}

bool JsonUtils::hasKey(const Poco::JSON::Object& obj, const std::string& key)
{
    return obj.has(key);
}

std::string JsonUtils::dateTimeToISOString(const Poco::DateTime& dt)
{
    return Poco::DateTimeFormatter::format(dt, Poco::DateTimeFormat::ISO8601_FORMAT);
}

std::string JsonUtils::timestampToISOString(const Poco::Timestamp& ts)
{
    Poco::DateTime dt(ts);

    return dateTimeToISOString(dt);
}

Poco::DateTime JsonUtils::isoStringToDateTime(const std::string& isoStr)
{
    int timeZoneDifferential = 0;
    Poco::DateTime dt;
    Poco::DateTimeParser::parse(Poco::DateTimeFormat::ISO8601_FORMAT, isoStr, dt, timeZoneDifferential);

    return dt;
}

Poco::Timestamp JsonUtils::isoStringToTimestamp(const std::string& isoStr)
{
    Poco::DateTime dt = isoStringToDateTime(isoStr);

    return Poco::Timestamp::fromUtcTime(dt.utcTime());
}

Poco::JSON::Object JsonUtils::createSuccessResponse(const std::string& message, 
                                                      Poco::JSON::Object::Ptr data)
{
    Poco::JSON::Object response;
    response.set("success", true);
    response.set("timestamp", timestampToISOString(Poco::Timestamp()));
    
    if (!message.empty())
    {
        response.set("message", message);
    }
    
    if (data)
    {
        response.set("data", data);
    }
    
    return response;
}

Poco::JSON::Object JsonUtils::createErrorResponse(const std::string& message, 
                                                   int errorCode,
                                                   Poco::JSON::Object::Ptr details)
{
    Poco::JSON::Object response;
    response.set("success", false);
    response.set("timestamp", timestampToISOString(Poco::Timestamp()));
    response.set("message", message);
    response.set("errorCode", errorCode);
    
    if (details)
    {
        response.set("details", details);
    }
    
    return response;
}

bool JsonUtils::validateSchema(const Poco::JSON::Object& obj, 
                                const std::map<std::string, std::string>& schema)
{
    for (const auto& [key, type] : schema)
    {
        if (!obj.has(key))
        {
            return false;
        }
        
        try {
            if (type == "string")
            {
                obj.getValue<std::string>(key);
            }
            else if (type == "int")
            {
                obj.getValue<int>(key);
            }
            else if (type == "double")
            {
                obj.getValue<double>(key);
            }
            else if (type == "bool")
            {
                obj.getValue<bool>(key);
            }
            else if (type == "object")
            {
                obj.getObject(key);
            }
            else if (type == "array")
            {
                obj.getArray(key);
            }
        }
        catch (...)
        {
            return false;
        }
    }

    return true;
}

Poco::JSON::Object JsonUtils::mapToJson(const std::map<std::string, std::string>& map)
{
    Poco::JSON::Object jsonObj;

    for (const auto& [key, value] : map)
    {
        jsonObj.set(key, value);
    }

    return jsonObj;
}

std::map<std::string, std::string> JsonUtils::jsonToMap(const Poco::JSON::Object& obj)
{
    std::map<std::string, std::string> result;

    for (const auto& key : obj.getNames())
    {
        try
        {
            result[key] = obj.getValue<std::string>(key);
        }
        catch (...)
        {
            
        }
    }

    return result;
}

std::string JsonUtils::prettyPrint(const std::string& jsonStr)
{
    try
    {
        Poco::JSON::Parser parser;
        Poco::Dynamic::Var result = parser.parse(jsonStr);
        std::ostringstream oss;

        if (result.type() == typeid(Poco::JSON::Object::Ptr))
        {
            Poco::JSON::Object::Ptr obj = result.extract<Poco::JSON::Object::Ptr>();
            obj->stringify(oss, 4);
        }
        else if (result.type() == typeid(Poco::JSON::Array::Ptr))
        {
            Poco::JSON::Array::Ptr arr = result.extract<Poco::JSON::Array::Ptr>();
            arr->stringify(oss, 4);
        }
        else
        {
            return jsonStr;
        }

        return oss.str();
    }
    catch (...)
    {
        return jsonStr;
    }
}

std::string JsonUtils::escapeJsonString(const std::string& str)
{
    std::ostringstream oss;
    for (char c : str)
    {
        switch (c)
        {
            case '"': oss << "\\\""; break;
            case '\\': oss << "\\\\"; break;
            case '\b': oss << "\\b"; break;
            case '\f': oss << "\\f"; break;
            case '\n': oss << "\\n"; break;
            case '\r': oss << "\\r"; break;
            case '\t': oss << "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20 || static_cast<unsigned char>(c) == 0x7f)
                {
                    oss << "\\u" << std::hex << std::setw(4) << std::setfill('0') 
                        << static_cast<int>(c);
                }
                else
                {
                    oss << c;
                }
        }
    }
    
    return oss.str();
}

} // namespace utils