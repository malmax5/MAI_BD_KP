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
    // Преобразование Poco::JSON::Object в строку
    static std::string objectToString(const Poco::JSON::Object& obj, bool pretty = false);
    
    // Преобразование строки в Poco::JSON::Object
    static Poco::JSON::Object::Ptr stringToObject(const std::string& jsonStr);
    
    // Преобразование Poco::JSON::Array в строку
    static std::string arrayToString(const Poco::JSON::Array& arr, bool pretty = false);
    
    // Преобразование строки в Poco::JSON::Array
    static Poco::JSON::Array::Ptr stringToArray(const std::string& jsonStr);
    
    // Получение значения из объекта с проверкой типа
    template<typename T>
    static T getValue(const Poco::JSON::Object& obj, const std::string& key, const T& defaultValue = T());
    
    // Получение значения из объекта как строки
    static std::string getString(const Poco::JSON::Object& obj, const std::string& key, 
                                  const std::string& defaultValue = "");
    
    // Получение значения из объекта как int
    static int getInt(const Poco::JSON::Object& obj, const std::string& key, int defaultValue = 0);
    
    // Получение значения из объекта как double
    static double getDouble(const Poco::JSON::Object& obj, const std::string& key, 
                             double defaultValue = 0.0);
    
    // Получение значения из объекта как bool
    static bool getBool(const Poco::JSON::Object& obj, const std::string& key, 
                         bool defaultValue = false);
    
    // Получение вложенного объекта
    static Poco::JSON::Object::Ptr getObject(const Poco::JSON::Object& obj, 
                                              const std::string& key);
    
    // Получение вложенного массива
    static Poco::JSON::Array::Ptr getArray(const Poco::JSON::Object& obj, 
                                            const std::string& key);
    
    // Проверка существования ключа
    static bool hasKey(const Poco::JSON::Object& obj, const std::string& key);
    
    // Преобразование DateTime в строку ISO 8601
    static std::string dateTimeToISOString(const Poco::DateTime& dt);
    
    // Преобразование Timestamp в строку ISO 8601
    static std::string timestampToISOString(const Poco::Timestamp& ts);
    
    // Преобразование строки ISO 8601 в DateTime
    static Poco::DateTime isoStringToDateTime(const std::string& isoStr);
    
    // Преобразование строки ISO 8601 в Timestamp
    static Poco::Timestamp isoStringToTimestamp(const std::string& isoStr);
    
    // Сериализация объекта в JSON строку
    template<typename T>
    static std::string serialize(const T& obj);
    
    // Десериализация JSON строки в объект
    template<typename T>
    static T deserialize(const std::string& jsonStr);
    
    // Создание успешного JSON ответа
    static Poco::JSON::Object createSuccessResponse(const std::string& message = "", 
                                                     Poco::JSON::Object::Ptr data = nullptr);
    
    // Создание JSON ответа об ошибке
    static Poco::JSON::Object createErrorResponse(const std::string& message, 
                                                   int errorCode = 0,
                                                   Poco::JSON::Object::Ptr details = nullptr);
    
    // Валидация JSON схемы
    static bool validateSchema(const Poco::JSON::Object& obj, 
                               const std::map<std::string, std::string>& schema);
    
    // Преобразование map в JSON объект
    static Poco::JSON::Object mapToJson(const std::map<std::string, std::string>& map);
    
    // Преобразование JSON объекта в map
    static std::map<std::string, std::string> jsonToMap(const Poco::JSON::Object& obj);
    
    // Форматирование JSON с отступами
    static std::string prettyPrint(const std::string& jsonStr);
    
    // Экранирование специальных символов в JSON
    static std::string escapeJsonString(const std::string& str);
    
    // Создание JSON из списка объектов
    template<typename T>
    static Poco::JSON::Array createJsonArray(const std::vector<T>& items);
};

} // namespace utils
