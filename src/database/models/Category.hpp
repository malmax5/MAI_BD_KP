#pragma once

#include <string>
#include <vector>
#include <memory>
#include <Poco/JSON/Object.h>
#include <Poco/Nullable.h>

namespace warehouse_backend::database::models
{

class Category
{
public:
    Poco::Int64 id;
    std::string name;
    Poco::Nullable<std::string> description;
    Poco::Nullable<Poco::Int64> parentId;
    Poco::Nullable<std::string> path;
    int sortOrder;
    Poco::Nullable<std::string> createdAt;

    Poco::Nullable<std::string> parentName;
    std::vector<std::shared_ptr<Category>> children;

    Category();
    explicit Category(const Poco::JSON::Object& json);

    Poco::JSON::Object toJson() const;
    static Category fromJson(const Poco::JSON::Object& json);

    Poco::JSON::Object toJsonWithChildren() const;
    
    bool validate() const;
    
    bool isRoot() const { return !parentId.isNull() && parentId.value() == 0; }
    void addChild(const std::shared_ptr<Category>& child);
    
    static constexpr int MAX_NAME_LENGTH = 100;
    static constexpr int MAX_DESCRIPTION_LENGTH = 1000;
};

} // namespace database::models
