#pragma once

#include <string>
#include <vector>
#include <regex>
#include <map>
#include <Poco/JSON/Object.h>
#include <Poco/RegularExpression.h>

namespace warehouse_backend::utils
{

class Validator
{
public:
    // Результат валидации
    struct ValidationResult
    {
        bool isValid;
        std::vector<std::string> errors;
        std::map<std::string, std::string> fieldErrors;
        
        ValidationResult()
            : isValid(true)
        {

        }
        
        void addError(const std::string& error)
        {
            isValid = false;
            errors.push_back(error);
        }
        
        void addFieldError(const std::string& field, const std::string& error)
        {
            isValid = false;
            fieldErrors[field] = error;
        }
        
        std::string toString() const;
    };
    
    // Валидация email
    static bool isValidEmail(const std::string& email);
    
    // Валидация телефона
    static bool isValidPhone(const std::string& phone);
    
    // Валидация URL
    static bool isValidUrl(const std::string& url);
    
    // Валидация IP адреса
    static bool isValidIpAddress(const std::string& ip);
    
    // Валидация даты в формате YYYY-MM-DD
    static bool isValidDate(const std::string& date);
    
    // Валидация даты-времени в формате ISO 8601
    static bool isValidDateTime(const std::string& datetime);
    
    // Валидация числового диапазона
    template<typename T>
    static bool isInRange(T value, T min, T max);
    
    // Валидация длины строки
    static bool isValidLength(const std::string& str, size_t min, size_t max);
    
    // Валидация, что строка содержит только буквы
    static bool isAlpha(const std::string& str);
    
    // Валидация, что строка содержит только буквы и цифры
    static bool isAlphanumeric(const std::string& str);
    
    // Валидация, что строка содержит только цифры
    static bool isNumeric(const std::string& str);
    
    // Валидация UUID
    static bool isValidUuid(const std::string& uuid);
    
    // Валидация кредитной карты
    static bool isValidCreditCard(const std::string& cardNumber);
    
    // Валидация почтового индекса
    static bool isValidPostalCode(const std::string& postalCode, const std::string& country = "RU");
    
    // Валидация ИНН
    static bool isValidInn(const std::string& inn);
    
    // Валидация КПП
    static bool isValidKpp(const std::string& kpp);
    
    // Валидация ОГРН
    static bool isValidOgrn(const std::string& ogrn);
    
    // Валидация СНИЛС
    static bool isValidSnils(const std::string& snils);
    
    // Валидация пароля
    static bool isValidPassword(const std::string& password, 
                                 int minLength = 8,
                                 bool requireUpper = true,
                                 bool requireLower = true,
                                 bool requireDigit = true,
                                 bool requireSpecial = true);
    
    // Валидация JSON объекта по схеме
    static ValidationResult validateJsonSchema(const Poco::JSON::Object& obj,
                                                const std::map<std::string, std::string>& schema);
    
    // Валидация пользователя на основе ролей
    static ValidationResult validateUser(const std::string& username,
                                          const std::string& email,
                                          const std::string& password,
                                          const std::string& role);
    
    // Валидация продукта
    static ValidationResult validateProduct(const std::string& sku,
                                             const std::string& name,
                                             double price,
                                             int stockLevel);
    
    // Валидация заказа
    static ValidationResult validateOrder(const std::string& orderNumber,
                                           const std::string& customerName,
                                           double totalAmount);
    
    // Валидация партии товара
    static ValidationResult validateProductBatch(const std::string& batchNumber,
                                                  int quantity,
                                                  double unitCost);
    
    // Валидация складской ячейки
    static ValidationResult validateWarehouseCell(const std::string& cellCode,
                                                   double maxVolume,
                                                   double maxWeight);
    
    // Очистка и нормализация строки
    static std::string sanitizeString(const std::string& str);
    
    // Очистка HTML
    static std::string sanitizeHtml(const std::string& html);
    
    // Очистка SQL инъекций
    static std::string sanitizeSql(const std::string& sql);
    
    // Преобразование к безопасному имени файла
    static std::string sanitizeFilename(const std::string& filename);
    
    // Проверка на XSS атаки
    static bool hasXss(const std::string& input);
    
    // Проверка на SQL инъекции
    static bool hasSqlInjection(const std::string& input);
    
    // Генерация безопасного случайного токена
    static std::string generateSecureToken(size_t length = 32);
    
    // Хеширование строки
    static std::string hashString(const std::string& input);
    
    // Сравнение хешей
    static bool compareHashes(const std::string& hash1, const std::string& hash2);
    
private:
    // Регулярные выражения
    static const std::regex EMAIL_REGEX;
    static const std::regex PHONE_REGEX;
    static const std::regex URL_REGEX;
    static const std::regex IP_REGEX;
    static const std::regex DATE_REGEX;
    static const std::regex UUID_REGEX;
    static const std::regex INN_REGEX;
    static const std::regex KPP_REGEX;
    static const std::regex OGRN_REGEX;
    static const std::regex SNILS_REGEX;
    
    // Вспомогательные методы
    static int calculateLuhnChecksum(const std::string& number);
    static bool isValidRussianPostalCode(const std::string& postalCode);
    static bool isValidUsPostalCode(const std::string& postalCode);
    static bool isValidEuPostalCode(const std::string& postalCode);
};

} // namespace utils
