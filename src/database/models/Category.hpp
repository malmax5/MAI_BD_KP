#pragma once

#include <string>
#include <vector>
#include <memory>
#include <Poco/JSON/Object.h>

namespace warehouse_backend::database::models
{

class Category
{
public:
    long long id;
    std::string name;
    std::string description;
    long long parentId;
    std::string path;
    int sortOrder;
    std::string createdAt;

    std::string parentName;
    std::vector<std::shared_ptr<Category>> children;

    Category();
    explicit Category(const Poco::JSON::Object& json);

    Poco::JSON::Object toJson() const;
    static Category fromJson(const Poco::JSON::Object& json);

    Poco::JSON::Object toJsonWithChildren() const;
    
    bool validate() const;
    
    bool isRoot() const { return parentId == 0; }
    void addChild(const std::shared_ptr<Category>& child);
    
    static constexpr int MAX_NAME_LENGTH = 100;
    static constexpr int MAX_DESCRIPTION_LENGTH = 1000;
};

} // namespace database::models