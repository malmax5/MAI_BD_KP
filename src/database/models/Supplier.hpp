#pragma once

#include <string>
#include <Poco/JSON/Object.h>

namespace warehouse_backend::database::models
{

class Supplier
{
public:
    long long id;
    std::string name;
    std::string contactPerson;
    std::string email;
    std::string phone;
    std::string address;
    std::string taxId;
    std::string paymentTerms;
    double rating;
    std::string createdAt;
    bool isActive;

    Supplier();
    explicit Supplier(const Poco::JSON::Object& json);

    Poco::JSON::Object toJson() const;
    static Supplier fromJson(const Poco::JSON::Object& json);

    bool validate() const;

    static constexpr double MIN_RATING = 0.0;
    static constexpr double MAX_RATING = 5.0;
};

} // namespace database::models