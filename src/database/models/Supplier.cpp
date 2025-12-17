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
    taxId = JsonUtils::getString(json, "tax_id", "");
    rating = JsonUtils::getDouble(json, "rating", 0.0);
    isActive = JsonUtils::getBool(json, "is_active", true);

    if (json.has("contact_person") && !json.isNull("contact_person"))
        contactPerson = JsonUtils::getString(json, "contact_person", "");

    if (json.has("email") && !json.isNull("email"))
        email = JsonUtils::getString(json, "email", "");

    if (json.has("phone") && !json.isNull("phone"))
        phone = JsonUtils::getString(json, "phone", "");

    if (json.has("address") && !json.isNull("address"))
        address = JsonUtils::getString(json, "address", "");

    if (json.has("payment_terms") && !json.isNull("payment_terms"))
        paymentTerms = JsonUtils::getString(json, "payment_terms", "");

    if (json.has("created_at") && !json.isNull("created_at"))
        createdAt = JsonUtils::getString(json, "created_at", "");

    if (json.has("id") && !json.isNull("id"))
        id = static_cast<Poco::Int64>(JsonUtils::getInt(json, "id", 0));
    else
        id = 0;
}

Poco::JSON::Object Supplier::toJson() const
{
    Poco::JSON::Object json;
    
    if (id > 0)
    {
        json.set("id", id);
    }
    
    json.set("name", name);
    
    if (!contactPerson.isNull())
    {
        json.set("contact_person", contactPerson.value());
    }
    
    if (!email.isNull())
    {
        json.set("email", email.value());
    }
    
    if (!phone.isNull())
    {
        json.set("phone", phone.value());
    }
    
    if (!address.isNull())
    {
        json.set("address", address.value());
    }
    
    if (!taxId.empty())
    {
        json.set("tax_id", taxId);
    }
    
    if (!paymentTerms.isNull())
    {
        json.set("payment_terms", paymentTerms.value());
    }
    
    json.set("rating", rating);

    if (!createdAt.isNull())
    {
        json.set("created_at", createdAt.value());
    }

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
    
    if (!email.isNull() && !Validator::isValidEmail(email))
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
