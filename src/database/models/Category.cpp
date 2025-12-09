#include "Category.hpp"
#include "../../utils/JsonUtils.hpp"
#include "../../utils/Validator.hpp"
#include <Poco/JSON/Parser.h>
#include <Poco/JSON/Stringifier.h>
#include <Poco/JSON/Array.h>
#include <sstream>

namespace warehouse_backend::database::models
{

using namespace warehouse_backend::utils;

Category::Category()
    : id(0),
      sortOrder(0)
{

}

Category::Category(const Poco::JSON::Object& json)
{
    name = JsonUtils::getString(json, "name", "");
    description = JsonUtils::getString(json, "description", "");
    parentId = JsonUtils::getInt(json, "parent_id", 0);
    path = JsonUtils::getString(json, "path", "");
    sortOrder = JsonUtils::getInt(json, "sort_order", 0);
    createdAt = JsonUtils::getString(json, "created_at", "");
    
    if (json.has("id"))
    {
        id = JsonUtils::getInt(json, "id", 0);
    }
    
    if (json.has("parent_name"))
    {
        parentName = JsonUtils::getString(json, "parent_name", "");
    }
}

Poco::JSON::Object Category::toJson() const
{
    Poco::JSON::Object json;
    
    if (id > 0)
    {
        json.set("id", id);
    }
    
    json.set("name", name);
    
    if (!description.isNull())
    {
        json.set("description", description);
    }
    
    if (parentId.value() > 0)
    {
        json.set("parent_id", parentId);
    }
    
    if (!path.isNull())
    {
        json.set("path", path);
    }
    
    json.set("sort_order", sortOrder);
    json.set("created_at", createdAt);
    
    if (!parentName.isNull())
    {
        json.set("parent_name", parentName);
    }
    
    return json;
}

Category Category::fromJson(const Poco::JSON::Object& json)
{
    return Category(json);
}

Poco::JSON::Object Category::toJsonWithChildren() const
{
    Poco::JSON::Object json = toJson();
    
    if (!children.empty())
    {
        Poco::JSON::Array childrenArray;
        for (const auto& child : children)
        {
            if (child)
            {
                childrenArray.add(child->toJson());
            }
        }
        json.set("children", childrenArray);
    }
    
    return json;
}

bool Category::validate() const
{
    if (name.empty() || name.length() > MAX_NAME_LENGTH)
    {
        return false;
    }
    
    if (description.value().length() > MAX_DESCRIPTION_LENGTH)
    {
        return false;
    }
    
    return true;
}

void Category::addChild(const std::shared_ptr<Category>& child)
{
    if (child)
    {
        children.push_back(child);
    }
}

} // namespace database::models
