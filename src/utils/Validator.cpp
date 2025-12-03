#include "Validator.hpp"
#include <Poco/JSON/Parser.h>
#include <Poco/RegularExpression.h>
#include <Poco/SHA2Engine.h>
#include <Poco/HMACEngine.h>
#include <Poco/RandomStream.h>
#include <Poco/Base64Encoder.h>
#include <Poco/Base64Decoder.h>
#include <Poco/HexBinaryEncoder.h>
#include <Poco/HexBinaryDecoder.h>
#include <Poco/DigestStream.h>
#include <Poco/Crypto/RSAKey.h>
#include <Poco/Crypto/Cipher.h>
#include <Poco/Crypto/CipherFactory.h>
#include <algorithm>
#include <cctype>
#include <sstream>
#include <iomanip>
#include <random>

namespace warehouse_backend::utils
{

// Инициализация регулярных выражений
const std::regex Validator::EMAIL_REGEX(R"(^[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}$)");
const std::regex Validator::PHONE_REGEX(R"(^\+?[1-9]\d{1,14}$)");
const std::regex Validator::URL_REGEX(R"(^(https?|ftp)://[^\s/$.?#].[^\s]*$)");
const std::regex Validator::IP_REGEX(R"(^(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.){3}(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$)");
const std::regex Validator::DATE_REGEX(R"(^\d{4}-\d{2}-\d{2}$)");
const std::regex Validator::UUID_REGEX(R"(^[0-9a-f]{8}-[0-9a-f]{4}-[1-5][0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$)");
const std::regex Validator::INN_REGEX(R"(^\d{10}|\d{12}$)");
const std::regex Validator::KPP_REGEX(R"(^\d{9}$)");
const std::regex Validator::OGRN_REGEX(R"(^\d{13}$)");
const std::regex Validator::SNILS_REGEX(R"(^\d{11}$)");

std::string Validator::ValidationResult::toString() const
{
    std::ostringstream oss;

    if (isValid)
    {
        oss << "Validation passed";
    }
    else
    {
        oss << "Validation failed:\n";
        for (const auto& error : errors)
        {
            oss << "  - " << error << "\n";
        }

        for (const auto& [field, error] : fieldErrors)
        {
            oss << "  - Field '" << field << "': " << error << "\n";
        }
    }

    return oss.str();
}

bool Validator::isValidEmail(const std::string& email)
{
    return std::regex_match(email, EMAIL_REGEX);
}

bool Validator::isValidPhone(const std::string& phone)
{
    return std::regex_match(phone, PHONE_REGEX);
}

bool Validator::isValidUrl(const std::string& url)
{
    return std::regex_match(url, URL_REGEX);
}

bool Validator::isValidIpAddress(const std::string& ip)
{
    return std::regex_match(ip, IP_REGEX);
}

bool Validator::isValidDate(const std::string& date)
{
    if (!std::regex_match(date, DATE_REGEX))
    {
        return false;
    }
    
    try
    {
        int year = std::stoi(date.substr(0, 4));
        int month = std::stoi(date.substr(5, 2));
        int day = std::stoi(date.substr(8, 2));
        
        if (month < 1 || month > 12) return false;
        if (day < 1 || day > 31) return false;
        
        if (month == 2)
        {
            bool isLeap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);

            if (day > (isLeap ? 29 : 28)) return false;
        }
        else if (month == 4 || month == 6 || month == 9 || month == 11)
        {
            if (day > 30) return false;
        }
        
        return true;
    }
    catch (...)
    {
        return false;
    }
}

bool Validator::isValidDateTime(const std::string& datetime)
{
    try
    {
        Poco::DateTime dt;
        int tzd;

        return Poco::DateTimeParser::tryParse(Poco::DateTimeFormat::ISO8601_FORMAT, datetime, dt, tzd);
    }
    catch (...)
    {
        return false;
    }
}

bool Validator::isValidLength(const std::string& str, size_t min, size_t max)
{
    return str.length() >= min && str.length() <= max;
}

bool Validator::isAlpha(const std::string& str)
{
    return !str.empty() && std::all_of(str.begin(), str.end(), 
        [](unsigned char c) { return std::isalpha(c); });
}

bool Validator::isAlphanumeric(const std::string& str)
{
    return !str.empty() && std::all_of(str.begin(), str.end(), 
        [](unsigned char c) { return std::isalnum(c); });
}

bool Validator::isNumeric(const std::string& str)
{
    return !str.empty() && std::all_of(str.begin(), str.end(), 
        [](unsigned char c) { return std::isdigit(c); });
}

bool Validator::isValidUuid(const std::string& uuid)
{
    return std::regex_match(uuid, UUID_REGEX);
}

bool Validator::isValidCreditCard(const std::string& cardNumber)
{
    std::string cleaned;
    for (char c : cardNumber) {
        if (std::isdigit(c)) {
            cleaned += c;
        }
    }
    
    if (cleaned.length() < 13 || cleaned.length() > 19) {
        return false;
    }
    
    return calculateLuhnChecksum(cleaned) == 0;
}

bool Validator::isValidPostalCode(const std::string& postalCode, const std::string& country)
{
    if (country == "RU")
    {
        return isValidRussianPostalCode(postalCode);
    }
    else if (country == "US")
    {
        return isValidUsPostalCode(postalCode);
    }
    else if (country == "EU")
    {
        return isValidEuPostalCode(postalCode);
    }
    
    return isValidLength(postalCode, 3, 10) && isAlphanumeric(postalCode);
}

bool Validator::isValidInn(const std::string& inn)
{
    if (!std::regex_match(inn, INN_REGEX))
    {
        return false;
    }
    
    return inn.length() == 10 || inn.length() == 12;
}

bool Validator::isValidKpp(const std::string& kpp)
{
    return std::regex_match(kpp, KPP_REGEX);
}

bool Validator::isValidOgrn(const std::string& ogrn)
{
    if (!std::regex_match(ogrn, OGRN_REGEX))
    {
        return false;
    }
    
    try
    {
        long long number = std::stoll(ogrn.substr(0, 12));
        int checksum = std::stoi(ogrn.substr(12, 1));

        return (number % 11) % 10 == checksum;
    }
    catch (...)
    {
        return false;
    }
}

bool Validator::isValidSnils(const std::string& snils)
{
    if (!std::regex_match(snils, SNILS_REGEX))
    {
        return false;
    }
    
    return true;
}

bool Validator::isValidPassword(const std::string& password, 
                                 int minLength,
                                 bool requireUpper,
                                 bool requireLower,
                                 bool requireDigit,
                                 bool requireSpecial)
{
    if (password.length() < static_cast<size_t>(minLength))
    {
        return false;
    }
    
    bool hasUpper = false;
    bool hasLower = false;
    bool hasDigit = false;
    bool hasSpecial = false;
    
    for (char c : password)
    {
        if (std::isupper(c)) hasUpper = true;
        if (std::islower(c)) hasLower = true;
        if (std::isdigit(c)) hasDigit = true;
        if (!std::isalnum(c) && !std::isspace(c)) hasSpecial = true;
    }
    
    if (requireUpper && !hasUpper) return false;
    if (requireLower && !hasLower) return false;
    if (requireDigit && !hasDigit) return false;
    if (requireSpecial && !hasSpecial) return false;
    
    return true;
}

Validator::ValidationResult Validator::validateJsonSchema(const Poco::JSON::Object& obj,
                                                           const std::map<std::string, std::string>& schema)
{
    ValidationResult result;
    
    for (const auto& [field, type] : schema)
    {
        if (!obj.has(field))
        {
            result.addFieldError(field, "Field is required");
            continue;
        }

        try
        {
            if (type == "string")
            {
                obj.getValue<std::string>(field);
            }
            else if (type == "int")
            {
                obj.getValue<int>(field);
            }
            else if (type == "double")
            {
                obj.getValue<double>(field);
            }
            else if (type == "bool")
            {
                obj.getValue<bool>(field);
            }
            else if (type == "object")
            {
                obj.getObject(field);
            }
            else if (type == "array")
            {
                obj.getArray(field);
            }
            else if (type == "email")
            {
                std::string email = obj.getValue<std::string>(field);

                if (!isValidEmail(email))
                {
                    result.addFieldError(field, "Invalid email format");
                }
            }
            else if (type == "phone")
            {
                std::string phone = obj.getValue<std::string>(field);

                if (!isValidPhone(phone))
                {
                    result.addFieldError(field, "Invalid phone format");
                }
            }
            else if (type == "date")
            {
                std::string date = obj.getValue<std::string>(field);

                if (!isValidDate(date))
                {
                    result.addFieldError(field, "Invalid date format (expected YYYY-MM-DD)");
                }
            }
        }
        catch (const Poco::Exception& e)
        {
            result.addFieldError(field, "Invalid type, expected " + type);
        }
        catch (...)
        {
            result.addFieldError(field, "Validation error");
        }
    }
    
    return result;
}

Validator::ValidationResult Validator::validateUser(const std::string& username,
                                                     const std::string& email,
                                                     const std::string& password,
                                                     const std::string& role)
{
    ValidationResult result;
    
    if (!isValidLength(username, 3, 50))
    {
        result.addFieldError("username", "Username must be between 3 and 50 characters");
    }
    
    if (!isValidEmail(email))
    {
        result.addFieldError("email", "Invalid email address");
    }
    
    if (!isValidPassword(password, 8))
    {
        result.addFieldError("password", "Password must be at least 8 characters and contain upper/lower case letters, digits and special characters");
    }
    
    static const std::vector<std::string> validRoles = {"admin", "manager", "worker", "auditor"};
    if (std::find(validRoles.begin(), validRoles.end(), role) == validRoles.end())
    {
        result.addFieldError("role", "Invalid role. Must be one of: admin, manager, worker, auditor");
    }
    
    return result;
}

Validator::ValidationResult Validator::validateProduct(const std::string& sku,
                                                        const std::string& name,
                                                        double price,
                                                        int stockLevel)
{
    ValidationResult result;
    
    if (!isValidLength(sku, 1, 50))
    {
        result.addFieldError("sku", "SKU must be between 1 and 50 characters");
    }
    
    if (!isValidLength(name, 1, 200))
    {
        result.addFieldError("name", "Product name must be between 1 and 200 characters");
    }
    
    if (price < 0)
    {
        result.addFieldError("price", "Price cannot be negative");
    }
    
    if (stockLevel < 0)
    {
        result.addFieldError("stockLevel", "Stock level cannot be negative");
    }
    
    return result;
}

Validator::ValidationResult Validator::validateOrder(const std::string& orderNumber,
                                                      const std::string& customerName,
                                                      double totalAmount)
{
    ValidationResult result;
    
    if (!isValidLength(orderNumber, 1, 50))
    {
        result.addFieldError("orderNumber", "Order number must be between 1 and 50 characters");
    }
    
    if (!isValidLength(customerName, 1, 150))
    {
        result.addFieldError("customerName", "Customer name must be between 1 and 150 characters");
    }
    
    if (totalAmount < 0)
    {
        result.addFieldError("totalAmount", "Total amount cannot be negative");
    }
    
    return result;
}

Validator::ValidationResult Validator::validateProductBatch(const std::string& batchNumber,
                                                             int quantity,
                                                             double unitCost)
{
    ValidationResult result;
    
    if (!isValidLength(batchNumber, 1, 100))
    {
        result.addFieldError("batchNumber", "Batch number must be between 1 and 100 characters");
    }
    
    if (quantity < 0)
    {
        result.addFieldError("quantity", "Quantity cannot be negative");
    }
    
    if (unitCost < 0)
    {
        result.addFieldError("unitCost", "Unit cost cannot be negative");
    }
    
    return result;
}

Validator::ValidationResult Validator::validateWarehouseCell(const std::string& cellCode,
                                                              double maxVolume,
                                                              double maxWeight)
{
    ValidationResult result;
    
    if (!isValidLength(cellCode, 1, 50))
    {
        result.addFieldError("cellCode", "Cell code must be between 1 and 50 characters");
    }
    
    if (maxVolume <= 0)
    {
        result.addFieldError("maxVolume", "Maximum volume must be greater than 0");
    }
    
    if (maxWeight <= 0)
    {
        result.addFieldError("maxWeight", "Maximum weight must be greater than 0");
    }
    
    return result;
}

std::string Validator::sanitizeString(const std::string& str)
{
    std::string result = str;
    
    result.erase(0, result.find_first_not_of(" \t\n\r\f\v"));
    result.erase(result.find_last_not_of(" \t\n\r\f\v") + 1);
    
    result = std::regex_replace(result, std::regex("\\s+"), " ");
    
    return result;
}

std::string Validator::sanitizeHtml(const std::string& html)
{
    std::string result = html;
    
    static const std::vector<std::string> allowedTags = {
        "p", "br", "b", "i", "u", "strong", "em", "ul", "ol", "li",
        "h1", "h2", "h3", "h4", "h5", "h6", "div", "span"
    };
    
    std::regex tagRegex("<(\\/?)(\\w+)[^>]*>");
    
    std::sregex_iterator it(result.begin(), result.end(), tagRegex);
    std::sregex_iterator end;
    
    while (it != end)
    {
        std::string tagName = (*it)[2];
        if (std::find(allowedTags.begin(), allowedTags.end(), tagName) == allowedTags.end())
        {
            result = std::regex_replace(result, std::regex("<\\/?" + tagName + "[^>]*>"), "");
            it = std::sregex_iterator(result.begin(), result.end(), tagRegex);
        }
        else
        {
            ++it;
        }
    }
    
    result = std::regex_replace(result, std::regex("&"), "&amp;");
    result = std::regex_replace(result, std::regex("<"), "&lt;");
    result = std::regex_replace(result, std::regex(">"), "&gt;");
    result = std::regex_replace(result, std::regex("\""), "&quot;");
    result = std::regex_replace(result, std::regex("'"), "&#x27;");
    
    return result;
}

std::string Validator::sanitizeSql(const std::string& sql)
{
    std::string result = sql;
    
    static const std::vector<std::string> dangerousKeywords = {
        "DROP", "DELETE", "INSERT", "UPDATE", "ALTER", "CREATE", "EXEC", "UNION", "SELECT"
    };
    
    for (const auto& keyword : dangerousKeywords)
    {
        std::regex regex("\\b" + keyword + "\\b", std::regex_constants::icase);
        result = std::regex_replace(result, regex, "");
    }
    
    result = std::regex_replace(result, std::regex("'"), "''");
    
    return result;
}

std::string Validator::sanitizeFilename(const std::string& filename)
{
    std::string result = filename;
    
    std::regex dangerousChars("[\\\\/:*?\"<>|]");
    result = std::regex_replace(result, dangerousChars, "_");
    
    result = std::regex_replace(result, std::regex("^\\.+|\\.+$"), "");
    
    if (result.length() > 255)
    {
        result = result.substr(0, 255);
    }
    
    return result;
}

bool Validator::hasXss(const std::string& input)
{
    static const std::vector<std::string> xssPatterns = {
        "<script", "javascript:", "onload=", "onerror=", "onclick=",
        "eval\\(", "alert\\(", "document\\.cookie", "window\\.location"
    };
    
    std::string lowerInput = input;
    std::transform(lowerInput.begin(), lowerInput.end(), lowerInput.begin(), ::tolower);
    
    for (const auto& pattern : xssPatterns)
    {
        if (std::regex_search(lowerInput, std::regex(pattern)))
        {
            return true;
        }
    }
    
    return false;
}

bool Validator::hasSqlInjection(const std::string& input)
{
    static const std::vector<std::string> sqlPatterns = {
        "--", ";--", ";", "/*", "*/", "@@", "@", 
        "char\\(", "nchar\\(", "varchar\\(", "nvarchar\\(",
        "exec\\(", "execute\\(", "select.*from", "insert.*into",
        "update.*set", "delete.*from", "drop\\s", "create\\s"
    };
    
    std::string lowerInput = input;
    std::transform(lowerInput.begin(), lowerInput.end(), lowerInput.begin(), ::tolower);
    
    for (const auto& pattern : sqlPatterns)
    {
        if (std::regex_search(lowerInput, std::regex(pattern, std::regex_constants::icase)))
        {
            return true;
        }
    }
    
    return false;
}

std::string Validator::generateSecureToken(size_t length)
{
    static const std::string chars =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
    
    static std::random_device rd;
    static std::mt19937 generator(rd());
    static std::uniform_int_distribution<> distribution(0, chars.size() - 1);
    
    std::string token;
    token.reserve(length);
    
    for (size_t i = 0; i < length; ++i)
    {
        token += chars[distribution(generator)];
    }
    
    return token;
}

std::string Validator::hashString(const std::string& input)
{
    Poco::SHA2Engine sha2(Poco::SHA2Engine::SHA_256);
    sha2.update(input);
    const Poco::DigestEngine::Digest& digest = sha2.digest();
    
    std::ostringstream oss;
    Poco::HexBinaryEncoder encoder(oss);
    encoder.write(reinterpret_cast<const char*>(&digest[0]), digest.size());
    encoder.close();
    
    return oss.str();
}

bool Validator::compareHashes(const std::string& hash1, const std::string& hash2)
{
    if (hash1.length() != hash2.length())
    {
        return false;
    }
    
    unsigned char result = 0;
    for (size_t i = 0; i < hash1.length(); ++i)
    {
        result |= hash1[i] ^ hash2[i];
    }
    
    return result == 0;
}

int Validator::calculateLuhnChecksum(const std::string& number)
{
    int sum = 0;
    bool alternate = false;
    
    for (int i = static_cast<int>(number.length()) - 1;  i >=0; i--)
    {
        int n = number[i] - '0';
        if (alternate)
        {
            n *= 2;

            if (n > 9)
            {
                n = (n % 10) + 1;
            }
        }

        sum += n;
        alternate = !alternate;
    }
    
    return (sum % 10);
}

bool Validator::isValidRussianPostalCode(const std::string& postalCode)
{
    if (postalCode.length() != 6)
    {
        return false;
    }
    
    return isNumeric(postalCode);
}

bool Validator::isValidUsPostalCode(const std::string& postalCode)
{
    std::regex usZipRegex(R"(^\d{5}(-\d{4})?$)");

    return std::regex_match(postalCode, usZipRegex);
}

bool Validator::isValidEuPostalCode(const std::string& postalCode)
{
    return isValidLength(postalCode, 4, 5) && isNumeric(postalCode);
}

} // namespace utils