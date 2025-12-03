#include "Supplier.hpp"
#include "../../utils/JsonUtils.hpp"
#include "../../utils/Validator.hpp"
#include <Poco/JSON/Parser.h>
#include <Poco/JSON/Stringifier.h>
#include <sstream>
#include <iomanip>

namespace warehouse_backend::database::models
{

using namespace warehouse_backend::utils;

Supplier::Supplier()
    : id(0),
      rating(0.0),
      isActive(true)
{

}

Supplier::Supplier(const Poco::JSON::Object& json)
{
    name = JsonUtils::getString(json, "name", "");
    contactPerson = JsonUtils::getString(json, "contact_person", "");
    email = JsonUtils::getString(json, "email", "");
    phone = JsonUtils::getString(json, "phone", "");
    address = JsonUtils::getString(json, "address", "");
    taxId = JsonUtils::getString(json, "tax_id", "");
    paymentTerms = JsonUtils::getString(json, "payment_terms", "");
    rating = JsonUtils::getDouble(json, "rating", 0.0);
    createdAt = JsonUtils::getString(json, "created_at", "");
    isActive = JsonUtils::getBool(json, "is_active", true);
    
    if (json.has("id"))
    {
        id = JsonUtils::getInt(json, "id", 0);
    }
}

Poco::JSON::Object Supplier::toJson() const
{
    Poco::JSON::Object json;
    
    if (id > 0)
    {
        json.set("id", id);
    }
    
    json.set("name", name);
    
    if (!contactPerson.empty())
    {
        json.set("contact_person", contactPerson);
    }
    
    if (!email.empty())
    {
        json.set("email", email);
    }
    
    if (!phone.empty())
    {
        json.set("phone", phone);
    }
    
    if (!address.empty())
    {
        json.set("address", address);
    }
    
    if (!taxId.empty())
    {
        json.set("tax_id", taxId);
    }
    
    if (!paymentTerms.empty())
    {
        json.set("payment_terms", paymentTerms);
    }
    
    json.set("rating", rating);
    json.set("created_at", createdAt);
    json.set("is_active", isActive);
    
    return json;
}

Supplier Supplier::fromJson(const Poco::JSON::Object& json)
{
    return Supplier(json);
}

bool Supplier::validate() const
{
    if (name.empty() || name.length() > 150)
    {
        return false;
    }
    
    if (!email.empty() && !Validator::isValidEmail(email))
    {
        return false;
    }
    
    if (!taxId.empty() && taxId.length() > 20)
    {
        return false;
    }
    
    if (rating < MIN_RATING || rating > MAX_RATING)
    {
        return false;
    }
    
    return true;
}

} // namespace database::models
