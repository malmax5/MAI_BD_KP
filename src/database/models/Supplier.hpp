#pragma once

#include <string>
#include <Poco/JSON/Object.h>
#include <Poco/Nullable.h>

namespace warehouse_backend::database::models
{

class Supplier
{
public:
    Poco::Int64 id;
    std::string name;
    Poco::Nullable<std::string> contactPerson;
    Poco::Nullable<std::string> email;
    Poco::Nullable<std::string> phone;
    Poco::Nullable<std::string> address;
    std::string taxId;
    Poco::Nullable<std::string> paymentTerms;
    double rating;
    Poco::Nullable<std::string> createdAt;
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
