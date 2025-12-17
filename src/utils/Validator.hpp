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
    
    static bool isValidEmail(const std::string& email);
    
    static bool isValidPhone(const std::string& phone);
    
    static bool isValidUrl(const std::string& url);
    
    static bool isValidIpAddress(const std::string& ip);
    
    static bool isValidDate(const std::string& date);
    
    static bool isValidDateTime(const std::string& datetime);
    
    template<typename T>
    static bool isInRange(T value, T min, T max);
    
    static bool isValidLength(const std::string& str, size_t min, size_t max);
    
    static bool isAlpha(const std::string& str);
    
    static bool isAlphanumeric(const std::string& str);
    
    static bool isNumeric(const std::string& str);
    
    static bool isValidUuid(const std::string& uuid);
    
    static bool isValidCreditCard(const std::string& cardNumber);
    
    static bool isValidPostalCode(const std::string& postalCode, const std::string& country = "RU");
    
    static bool isValidInn(const std::string& inn);
    
    static bool isValidKpp(const std::string& kpp);
    
    static bool isValidOgrn(const std::string& ogrn);
    
    static bool isValidSnils(const std::string& snils);
    
    static bool isValidPassword(const std::string& password, 
                                 int minLength = 8,
                                 bool requireUpper = true,
                                 bool requireLower = true,
                                 bool requireDigit = true,
                                 bool requireSpecial = true);
    
    static ValidationResult validateJsonSchema(const Poco::JSON::Object& obj,
                                                const std::map<std::string, std::string>& schema);
    
    static ValidationResult validateUser(const std::string& username,
                                          const std::string& email,
                                          const std::string& password,
                                          const std::string& role);
    
    static ValidationResult validateProduct(const std::string& sku,
                                             const std::string& name,
                                             double price,
                                             int stockLevel);
    
    static ValidationResult validateOrder(const std::string& orderNumber,
                                           const std::string& customerName,
                                           double totalAmount);
    
    static ValidationResult validateProductBatch(const std::string& batchNumber,
                                                  int quantity,
                                                  double unitCost);
    
    static ValidationResult validateWarehouseCell(const std::string& cellCode,
                                                   double maxVolume,
                                                   double maxWeight);
    
    static std::string sanitizeString(const std::string& str);
    
    static std::string sanitizeHtml(const std::string& html);
    
    static std::string sanitizeSql(const std::string& sql);
    
    static std::string sanitizeFilename(const std::string& filename);
    
    static bool hasXss(const std::string& input);
    
    static bool hasSqlInjection(const std::string& input);
    
    static std::string generateSecureToken(size_t length = 32);
    
    static std::string hashString(const std::string& input);
    
    static bool compareHashes(const std::string& hash1, const std::string& hash2);
    
private:
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
    
    static int calculateLuhnChecksum(const std::string& number);
    static bool isValidRussianPostalCode(const std::string& postalCode);
    static bool isValidUsPostalCode(const std::string& postalCode);
    static bool isValidEuPostalCode(const std::string& postalCode);
};

} // namespace utils
