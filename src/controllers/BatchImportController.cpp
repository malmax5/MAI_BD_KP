#include "BatchImportController.hpp"
#include "../database/models/WarehouseCell.hpp"
#include "../utils/DateUtils.hpp"
#include "../utils/Validator.hpp"
#include <Poco/Net/HTMLForm.h>
#include <Poco/StreamCopier.h>
#include <Poco/File.h>
#include <Poco/Path.h>
#include <Poco/FileStream.h>
#include <Poco/StringTokenizer.h>
#include <Poco/Net/PartHandler.h>
#include <Poco/UUIDGenerator.h>
#include <Poco/UUID.h>
#include <Poco/DateTimeFormatter.h>
#include <Poco/DateTimeParser.h>
#include <Poco/NumberParser.h>
#include <Poco/NumberFormatter.h>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <algorithm>
#include <cctype>

namespace warehouse_backend::controllers
{

std::map<std::string, BatchImportController::BatchJob> BatchImportController::batchJobs;
std::mutex BatchImportController::batchJobsMutex;

BatchImportController::BatchImportController()
    : userService(std::make_unique<services::UserService>()),
      productService(std::make_unique<services::ProductService>()),
      categoryService(std::make_unique<services::CategoryService>()),
      supplierService(std::make_unique<services::SupplierService>()),
      warehouseCellService(std::make_unique<services::WarehouseCellService>())
{
}

void BatchImportController::handleRequest(Poco::Net::HTTPServerRequest& request,
                                        Poco::Net::HTTPServerResponse& response)
{
    setCorsHeaders(response);
    
    if (request.getMethod() == "OPTIONS")
    {
        response.setStatus(Poco::Net::HTTPResponse::HTTP_OK);
        response.send();
        return;
    }
    
    std::string path = request.getURI();
    std::string method = request.getMethod();
    
    try
    {
        logRequest(request, method, path);
        auto startTime = std::chrono::high_resolution_clock::now();
        
        if (path == "/api/v1/users/batch/upload" && method == "POST")
        {
            handleBatchUserUpload(request, response);
        }
        else if (path == "/api/v1/users/batch/template" && method == "GET")
        {
            handleTemplateUserDownload(response);
        }
        else if (path == "/api/v1/products/batch/upload" && method == "POST")
        {
            handleBatchProductUpload(request, response);
        }
        else if (path == "/api/v1/products/batch/template" && method == "GET")
        {
            handleTemplateProductDownload(response);
        }
        else if (path == "/api/v1/categories/batch/upload" && method == "POST")
        {
            handleBatchCategoryUpload(request, response);
        }
        else if (path == "/api/v1/categories/batch/template" && method == "GET")
        {
            handleTemplateCategoryDownload(response);
        }
        else if (path == "/api/v1/warehouse-cells/batch/upload" && method == "POST")
        {
            handleBatchWarehouseCellUpload(request, response);
        }
        else if (path == "/api/v1/warehouse-cells/batch/template" && method == "GET")
        {
            handleTemplateWarehouseCellDownload(response);
        }
        else if (path == "/api/v1/suppliers/batch/upload" && method == "POST")
        {
            handleBatchSupplierUpload(request, response);
        }
        else if (path == "/api/v1/suppliers/batch/template" && method == "GET")
        {
            handleTemplateSupplierDownload(response);
        }
        else
        {
            sendNotFoundResponse(response, "Batch endpoint not found");
        }
        
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        logResponse(request, response, method, path, duration.count());
    }
    catch (const Poco::Exception& e)
    {
        sendErrorResponse(response, "Error processing batch request: " + e.displayText());
    }
    catch (const std::exception& e)
    {
        sendErrorResponse(response, "Error processing batch request: " + std::string(e.what()));
    }
}

void BatchImportController::handleBatchUserUpload(Poco::Net::HTTPServerRequest& request,
                                           Poco::Net::HTTPServerResponse& response)
{
    std::string authHeader = getAuthorizationHeader(request);
    std::string token = extractBearerToken(authHeader);
    
    if (token.empty())
    {
        sendUnauthorizedResponse(response, "Authorization token required");
        return;
    }
    
    long long uploadedBy = 1;
    
    std::string ipAddress = getClientIpAddress(request);
    std::string userAgent = getUserAgent(request);
    
    CSVPartHandler partHandler;
    Poco::Net::HTMLForm form(request, request.stream(), partHandler);
    
    std::string tempFile = partHandler.getTempFile();
    if (tempFile.empty())
    {
        sendErrorResponse(response, "No CSV file uploaded");
        return;
    }
    
    std::string batchId = getCurrentBatchId();
    BatchJob job;
    job.batchId = batchId;
    job.filename = partHandler.getFilename();
    job.startedAt = Poco::DateTimeFormatter::format(utils::DateUtils::currentTimestamp(), Poco::DateTimeFormat::ISO8601_FORMAT);
    job.uploadedBy = uploadedBy;
    
    {
        std::lock_guard<std::mutex> lock(batchJobsMutex);
        batchJobs[batchId] = job;
    }
    
    Poco::JSON::Array usersArray;
    try
    {
        usersArray = parseCSVFile(tempFile, uploadedBy, ipAddress, userAgent);
    }
    catch (const std::exception& e)
    {
        Poco::File(tempFile).remove();
        
        {
            std::lock_guard<std::mutex> lock(batchJobsMutex);
            batchJobs[batchId].completedAt = Poco::DateTimeFormatter::format(utils::DateUtils::currentTimestamp(), Poco::DateTimeFormat::ISO8601_FORMAT);
        }
        
        sendErrorResponse(response, "Failed to parse CSV file: " + std::string(e.what()));
        return;
    }
    
    Poco::File(tempFile).remove();
    
    Poco::JSON::Array importResults = userService->importUsers(usersArray, uploadedBy, ipAddress, userAgent);
    
    int successCount = 0;
    int failureCount = 0;
    for (size_t i = 0; i < importResults.size() - 1; ++i)
    {
        auto result = importResults.get(i).extract<Poco::JSON::Object>();
        if (result.has("success"))
        {
            successCount++;
        }
        else
        {
            failureCount++;
        }
    }
    
    {
        std::lock_guard<std::mutex> lock(batchJobsMutex);
        batchJobs[batchId].totalRows = static_cast<int>(importResults.size() - 1);
        batchJobs[batchId].processedRows = static_cast<int>(importResults.size() - 1);
        batchJobs[batchId].successCount = successCount;
        batchJobs[batchId].failureCount = failureCount;
        batchJobs[batchId].completedAt = Poco::DateTimeFormatter::format(utils::DateUtils::currentTimestamp(), Poco::DateTimeFormat::ISO8601_FORMAT);
        batchJobs[batchId].results = importResults;
    }
    
    Poco::JSON::Object::Ptr responseJson = new Poco::JSON::Object;
    responseJson->set("success", true);
    responseJson->set("message", "Batch upload completed");
    responseJson->set("batchId", batchId);
    responseJson->set("filename", partHandler.getFilename());
    responseJson->set("totalRows", static_cast<int>(importResults.size() - 1));
    responseJson->set("successCount", successCount);
    responseJson->set("failureCount", failureCount);
    responseJson->set("results", importResults);
    
    sendJsonResponse(response, responseJson);
}

void BatchImportController::handleTemplateUserDownload(Poco::Net::HTTPServerResponse& response)
{
    std::string csvTemplate = "username,password_hash,full_name,email,role,phone_number,is_active\n"
                             "john.doe,$2a$10$abc123...,John Doe,john@example.com,worker,+1234567890,true\n"
                             "jane.smith,$2a$10$def456...,Jane Smith,jane@example.com,manager,+0987654321,true\n"
                             "bob.wilson,$2a$10$ghi789...,Bob Wilson,bob@example.com,auditor,,true\n\n"
                             "Notes:\n"
                             "1. password_hash: BCrypt hash of password\n"
                             "2. role: admin, manager, worker, or auditor\n"
                             "3. phone_number: optional, international format\n"
                             "4. is_active: true or false\n"
                             "5. Do not include id, created_at, last_login fields\n"
                             "6. Keep header row as shown";
    
    response.setStatus(Poco::Net::HTTPResponse::HTTP_OK);
    response.setContentType("text/csv");
    response.set("Content-Disposition", "attachment; filename=user_template.csv");
    response.setChunkedTransferEncoding(true);
    
    std::ostream& ostr = response.send();
    ostr << csvTemplate;
}

void BatchImportController::handleBatchProductUpload(Poco::Net::HTTPServerRequest& request,
                                                   Poco::Net::HTTPServerResponse& response)
{
    std::string authHeader = getAuthorizationHeader(request);
    std::string token = extractBearerToken(authHeader);
    
    if (token.empty())
    {
        sendUnauthorizedResponse(response, "Authorization token required");
        return;
    }
    
    long long uploadedBy = 1;
    
    std::string ipAddress = getClientIpAddress(request);
    std::string userAgent = getUserAgent(request);
    
    CSVPartHandler partHandler;
    Poco::Net::HTMLForm form;
    form.load(request, request.stream(), partHandler);
    
    std::string tempFile = partHandler.getTempFile();
    if (tempFile.empty())
    {
        sendErrorResponse(response, "No CSV file uploaded");
        return;
    }
    
    std::string batchId = getCurrentBatchId();
    BatchJob job;
    job.batchId = batchId;
    job.entityType = "products";
    job.filename = partHandler.getFilename();
    job.startedAt = Poco::DateTimeFormatter::format(utils::DateUtils::currentTimestamp(), 
                                                    Poco::DateTimeFormat::ISO8601_FORMAT);
    job.uploadedBy = uploadedBy;
    job.status = "processing";
    
    {
        std::lock_guard<std::mutex> lock(batchJobsMutex);
        batchJobs[batchId] = job;
    }
    
    Poco::JSON::Array productsArray;
    try
    {
        productsArray = parseProductCSVFile(tempFile, uploadedBy, ipAddress, userAgent);
    }
    catch (const std::exception& e)
    {
        Poco::File(tempFile).remove();
        
        {
            std::lock_guard<std::mutex> lock(batchJobsMutex);
            batchJobs[batchId].completedAt = Poco::DateTimeFormatter::format(
                utils::DateUtils::currentTimestamp(), 
                Poco::DateTimeFormat::ISO8601_FORMAT);
            batchJobs[batchId].status = "failed";
        }
        
        sendErrorResponse(response, "Failed to parse CSV file: " + std::string(e.what()));
        return;
    }
    
    Poco::File(tempFile).remove();
    
    Poco::JSON::Array importResults;
    int successCount = 0;
    int failureCount = 0;
    
    for (size_t i = 0; i < productsArray.size(); ++i)
    {
        auto productObj = productsArray.get(i).extract<Poco::JSON::Object>();
        
        if (productObj.has("error"))
        {
            Poco::JSON::Object errorResult;
            errorResult.set("row", static_cast<int>(i + 1));
            errorResult.set("success", false);
            errorResult.set("error", productObj.getValue<std::string>("error"));
            importResults.add(errorResult);
            failureCount++;
            continue;
        }
        
        try
        {
            database::models::Product productModel(productObj);
            
            if (!productModel.validate())
            {
                throw std::runtime_error("Product validation failed");
            }
            
            auto result = productService->createProduct(productModel, uploadedBy);
            
            if (result.success)
            {
                Poco::JSON::Object successResult;
                successResult.set("row", static_cast<int>(i + 1));
                successResult.set("success", true);
                successResult.set("productId", result.productId);
                successResult.set("sku", productModel.sku);
                successResult.set("name", productModel.name);
                successResult.set("message", result.message);
                importResults.add(successResult);
                successCount++;
            }
            else
            {
                Poco::JSON::Object errorResult;
                errorResult.set("row", static_cast<int>(i + 1));
                errorResult.set("success", false);
                errorResult.set("error", result.message);
                errorResult.set("sku", productModel.sku);
                importResults.add(errorResult);
                failureCount++;
            }
        }
        catch (const std::exception& e)
        {
            Poco::JSON::Object errorResult;
            errorResult.set("row", static_cast<int>(i + 1));
            errorResult.set("success", false);
            errorResult.set("error", "Failed to create product: " + std::string(e.what()));
            if (productObj.has("sku"))
                errorResult.set("sku", productObj.getValue<std::string>("sku"));
            importResults.add(errorResult);
            failureCount++;
        }
    }
    
    updateBatchJobStatus(batchId, static_cast<int>(productsArray.size()),
                        static_cast<int>(productsArray.size()), 
                        successCount, failureCount, importResults);
    
    Poco::JSON::Object::Ptr responseJson = new Poco::JSON::Object;
    responseJson->set("success", true);
    responseJson->set("message", "Product batch upload completed");
    responseJson->set("batchId", batchId);
    responseJson->set("filename", partHandler.getFilename());
    responseJson->set("totalRows", static_cast<int>(productsArray.size()));
    responseJson->set("successCount", successCount);
    responseJson->set("failureCount", failureCount);
    responseJson->set("results", importResults);
    
    sendJsonResponse(response, responseJson);
}

void BatchImportController::handleTemplateProductDownload(Poco::Net::HTTPServerResponse& response)
{
    std::string csvTemplate = "sku,name,description,category_id,supplier_id,unit_price,weight,dimensions,min_stock_level,max_stock_level,is_active\n"
                             "SKU-001,Product 1,Description for product 1,1,1,19.99,1.5,10x20x30 cm,10,100,true\n"
                             "SKU-002,Product 2,Description for product 2,2,2,29.99,0.5,,,500,true\n"
                             "SKU-003,Product 3,,3,3,199.99,15.0,50x30x80 cm,5,50,true\n\n"
                             "Notes:\n"
                             "1. sku: unique product identifier (required)\n"
                             "2. name: product name (required)\n"
                             "3. description: optional product description\n"
                             "4. category_id: existing category ID (required)\n"
                             "5. supplier_id: existing supplier ID (required)\n"
                             "6. unit_price: unit price (required, decimal)\n"
                             "7. weight: weight in kg (optional, decimal)\n"
                             "8. dimensions: optional dimensions string\n"
                             "9. min_stock_level: minimum stock level (optional, integer)\n"
                             "10. max_stock_level: maximum stock level (optional, integer)\n"
                             "11. is_active: true or false (default: true)\n"
                             "12. Use empty value for NULL\n"
                             "13. Category and supplier must exist in database before import";
    
    response.setStatus(Poco::Net::HTTPResponse::HTTP_OK);
    response.setContentType("text/csv");
    response.set("Content-Disposition", "attachment; filename=product_template.csv");
    response.setChunkedTransferEncoding(true);
    
    std::ostream& ostr = response.send();
    ostr << csvTemplate;
}

Poco::JSON::Array BatchImportController::parseCSVFile(const std::string& filePath, long long uploadedBy,
                                                   const std::string& ipAddress, const std::string& userAgent)
{
    Poco::JSON::Array usersArray;
    
    std::ifstream file(filePath);
    if (!file.is_open())
    {
        throw std::runtime_error("Could not open CSV file");
    }
    
    std::string line;
    int lineNumber = 0;
    std::vector<std::string> headers;
    
    while (std::getline(file, line))
    {
        lineNumber++;
        
        if (line.empty() || line.find_first_not_of(" \t\n\r") == std::string::npos)
        {
            continue;
        }
        
        std::vector<std::string> row;
        std::stringstream ss(line);
        std::string cell;
        
        while (std::getline(ss, cell, ','))
        {
            size_t start = cell.find_first_not_of(" \t");
            size_t end = cell.find_last_not_of(" \t");
            if (start != std::string::npos && end != std::string::npos)
            {
                cell = cell.substr(start, end - start + 1);
            }
            row.push_back(cell);
        }
        
        if (lineNumber == 1)
        {
            headers = row;
            
            std::vector<std::string> requiredHeaders = {"username", "password_hash", "full_name", "email", "role"};
            for (const auto& required : requiredHeaders)
            {
                if (std::find(headers.begin(), headers.end(), required) == headers.end())
                {
                    throw std::runtime_error("Missing required column: " + required);
                }
            }
        }
        else
        {
            Poco::JSON::Object userData = processCSVRow(row, lineNumber);
            
            std::string validationError;
            if (validateCSVRow(userData, validationError))
            {
                usersArray.add(userData);
            }
            else
            {
                Poco::JSON::Object error;
                error.set("row", lineNumber);
                error.set("error", "Validation failed: " + validationError);
                error.set("data", userData);
                usersArray.add(error);
            }
        }
    }
    
    file.close();
    
    if (usersArray.size() == 0)
    {
        throw std::runtime_error("No valid user data found in CSV");
    }
    
    return usersArray;
}

Poco::JSON::Array BatchImportController::parseProductCSVFile(const std::string& filePath, 
                                                           long long uploadedBy,
                                                           const std::string& ipAddress, 
                                                           const std::string& userAgent)
{
    Poco::JSON::Array productsArray;
    
    std::ifstream file(filePath);
    if (!file.is_open())
    {
        throw std::runtime_error("Could not open CSV file");
    }
    
    std::string line;
    int lineNumber = 0;
    std::vector<std::string> headers;
    
    while (std::getline(file, line))
    {
        lineNumber++;
        
        if (line.empty() || line.find_first_not_of(" \t\n\r") == std::string::npos || 
            line[0] == '#' || line.substr(0, 3) == "Notes:")
        {
            continue;
        }
        
        std::vector<std::string> row;
        std::stringstream ss(line);
        std::string cell;
        bool inQuotes = false;
        
        while (std::getline(ss, cell, ','))
        {
            if (!cell.empty() && cell[0] == '"' && cell[cell.size()-1] != '"')
            {
                inQuotes = true;
                std::string fullCell = cell;
                while (inQuotes && std::getline(ss, cell, ','))
                {
                    fullCell += "," + cell;
                    if (!cell.empty() && cell[cell.size()-1] == '"')
                    {
                        inQuotes = false;
                        cell = fullCell;
                    }
                }
            }
            
            if (!cell.empty() && cell[0] == '"' && cell[cell.size()-1] == '"')
            {
                cell = cell.substr(1, cell.size() - 2);
            }
            
            size_t start = cell.find_first_not_of(" \t");
            size_t end = cell.find_last_not_of(" \t");
            if (start != std::string::npos && end != std::string::npos)
            {
                cell = cell.substr(start, end - start + 1);
            }
            else if (cell == "\\N")
            {
                cell = "";
            }
            
            row.push_back(cell);
        }
        
        if (lineNumber == 1)
        {
            headers = row;
            
            std::vector<std::string> requiredHeaders = {"sku", "name", "category_id", "supplier_id", "unit_price"};
            for (const auto& required : requiredHeaders)
            {
                auto it = std::find(headers.begin(), headers.end(), required);
                if (it == headers.end())
                {
                    throw std::runtime_error("Missing required column: " + required);
                }
            }
        }
        else
        {
            try
            {
                Poco::JSON::Object productData = processProductCSVRow(row, lineNumber);
                
                std::string validationError;
                if (validateProductCSVRow(productData, validationError))
                {
                    productsArray.add(productData);
                }
                else
                {
                    Poco::JSON::Object error;
                    error.set("row", lineNumber);
                    error.set("error", "Validation failed: " + validationError);
                    if (!row.empty() && headers.size() > 0 && row[0] != "")
                        error.set("sku", row[0]);
                    productsArray.add(error);
                }
            }
            catch (const std::exception& e)
            {
                Poco::JSON::Object error;
                error.set("row", lineNumber);
                error.set("error", "Processing error: " + std::string(e.what()));
                if (!row.empty() && headers.size() > 0 && row[0] != "")
                    error.set("sku", row[0]);
                productsArray.add(error);
            }
        }
    }
    
    file.close();
    
    if (productsArray.size() == 0)
    {
        throw std::runtime_error("No valid product data found in CSV");
    }
    
    return productsArray;
}


Poco::JSON::Object BatchImportController::processCSVRow(const std::vector<std::string>& row, int rowNumber)
{
    Poco::JSON::Object userData;
    
    
    if (row.size() >= 1) userData.set("username", row[0]);
    if (row.size() >= 2) userData.set("password_hash", row[1]);
    if (row.size() >= 3) userData.set("full_name", row[2]);
    if (row.size() >= 4) userData.set("email", row[3]);
    if (row.size() >= 5) userData.set("role", row[4]);
    if (row.size() >= 6 && !row[5].empty()) userData.set("phone_number", row[5]);
    if (row.size() >= 7)
    {
        std::string isActive = row[6];
        std::transform(isActive.begin(), isActive.end(), isActive.begin(), ::tolower);
        userData.set("is_active", (isActive == "true" || isActive == "1" || isActive == "yes"));
    }
    else
    {
        userData.set("is_active", true);
    }
    
    return userData;
}

Poco::JSON::Object BatchImportController::processProductCSVRow(const std::vector<std::string>& row, 
                                                              int rowNumber)
{
    Poco::JSON::Object productData;
    
    productData.set("sku", "");
    productData.set("name", "");
    productData.set("description", "");
    productData.set("category_id", 0);
    productData.set("supplier_id", 0);
    productData.set("unit_price", 0.0);
    productData.set("weight", 0.0);
    productData.set("dimensions", "");
    productData.set("min_stock_level", 0);
    productData.set("max_stock_level", 1000);
    productData.set("is_active", true);
    
    if (row.size() >= 1) productData.set("sku", row[0]);
    if (row.size() >= 2) productData.set("name", row[1]);
    if (row.size() >= 3 && !row[2].empty()) productData.set("description", row[2]);
    
    if (row.size() >= 4 && !row[3].empty())
    {
        try
        {
            int64_t categoryId = Poco::NumberParser::parse64(row[3]);
            productData.set("category_id", categoryId);
        }
        catch (const Poco::Exception&)
        {
            throw std::runtime_error("Invalid category ID: " + row[3]);
        }
    }
    
    if (row.size() >= 5 && !row[4].empty())
    {
        try
        {
            int64_t supplierId = Poco::NumberParser::parse64(row[4]);
            productData.set("supplier_id", supplierId);
        }
        catch (const Poco::Exception&)
        {
            throw std::runtime_error("Invalid supplier ID: " + row[4]);
        }
    }
    
    try
    {
        if (row.size() >= 6 && !row[5].empty())
        {
            double unitPrice = Poco::NumberParser::parseFloat(row[5]);
            productData.set("unit_price", unitPrice);
        }
        
        if (row.size() >= 7 && !row[6].empty())
        {
            double weight = Poco::NumberParser::parseFloat(row[6]);
            productData.set("weight", weight);
        }
        
        if (row.size() >= 8 && !row[7].empty())
        {
            productData.set("dimensions", row[7]);
        }
        
        if (row.size() >= 9 && !row[8].empty())
        {
            int minStock = Poco::NumberParser::parse(row[8]);
            productData.set("min_stock_level", minStock);
        }
        
        if (row.size() >= 10 && !row[9].empty())
        {
            int maxStock = Poco::NumberParser::parse(row[9]);
            productData.set("max_stock_level", maxStock);
        }
        
        if (row.size() >= 11 && !row[10].empty())
        {
            std::string isActive = row[10];
            std::transform(isActive.begin(), isActive.end(), isActive.begin(), ::tolower);
            productData.set("is_active", (isActive == "true" || isActive == "1" || isActive == "yes" || isActive == "y"));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Error parsing numeric value: " + e.displayText());
    }
    
    return productData;
}

bool BatchImportController::validateCSVRow(const Poco::JSON::Object& userData, std::string& error)
{
    if (!userData.has("username") || userData.getValue<std::string>("username").empty())
    {
        error = "Username is required";
        return false;
    }
    
    if (!userData.has("password_hash") || userData.getValue<std::string>("password_hash").empty())
    {
        error = "Password hash is required";
        return false;
    }
    
    if (!userData.has("full_name") || userData.getValue<std::string>("full_name").empty())
    {
        error = "Full name is required";
        return false;
    }
    
    if (!userData.has("email") || userData.getValue<std::string>("email").empty())
    {
        error = "Email is required";
        return false;
    }
    
    if (!utils::Validator::isValidEmail(userData.getValue<std::string>("email")))
    {
        error = "Invalid email format";
        return false;
    }
    
    if (!userData.has("role") || userData.getValue<std::string>("role").empty())
    {
        error = "Role is required";
        return false;
    }
    
    std::string role = userData.getValue<std::string>("role");
    std::vector<std::string> validRoles = {"admin", "manager", "worker", "auditor"};
    if (std::find(validRoles.begin(), validRoles.end(), role) == validRoles.end())
    {
        error = "Invalid role: " + role;
        return false;
    }
    
    return true;
}

bool BatchImportController::validateProductCSVRow(const Poco::JSON::Object& productData, 
                                                 std::string& error)
{
    if (!productData.has("sku") || productData.getValue<std::string>("sku").empty())
    {
        error = "SKU is required";
        return false;
    }
    
    if (!productData.has("name") || productData.getValue<std::string>("name").empty())
    {
        error = "Product name is required";
        return false;
    }
    
    if (!productData.has("category_id") || productData.getValue<int64_t>("category_id") <= 0)
    {
        error = "Valid category ID is required";
        return false;
    }
    
    if (!productData.has("supplier_id") || productData.getValue<int64_t>("supplier_id") <= 0)
    {
        error = "Valid supplier ID is required";
        return false;
    }
    
    if (!productData.has("unit_price") || productData.getValue<double>("unit_price") <= 0)
    {
        error = "Unit price must be greater than 0";
        return false;
    }
    
    std::string sku = productData.getValue<std::string>("sku");
    if (sku.length() > 50)
    {
        error = "SKU cannot exceed 50 characters";
        return false;
    }
    
    std::string name = productData.getValue<std::string>("name");
    if (name.length() > 200)
    {
        error = "Product name cannot exceed 200 characters";
        return false;
    }
    
    int minStock = productData.has("min_stock_level") ? productData.getValue<int>("min_stock_level") : 0;
    int maxStock = productData.has("max_stock_level") ? productData.getValue<int>("max_stock_level") : 1000;
    
    if (minStock < 0)
    {
        error = "Minimum stock level cannot be negative";
        return false;
    }
    
    if (maxStock < minStock)
    {
        error = "Maximum stock level cannot be less than minimum stock level";
        return false;
    }
    
    return true;
}

void BatchImportController::handleBatchCategoryUpload(Poco::Net::HTTPServerRequest& request,
                                                    Poco::Net::HTTPServerResponse& response)
{
    std::string authHeader = getAuthorizationHeader(request);
    std::string token = extractBearerToken(authHeader);
    
    if (token.empty())
    {
        sendUnauthorizedResponse(response, "Authorization token required");
        return;
    }
    
    long long uploadedBy = 1;
    std::string ipAddress = getClientIpAddress(request);
    std::string userAgent = getUserAgent(request);
    
    CSVPartHandler partHandler;
    Poco::Net::HTMLForm form;
    form.load(request, request.stream(), partHandler);
    
    std::string tempFile = partHandler.getTempFile();
    if (tempFile.empty())
    {
        sendErrorResponse(response, "No CSV file uploaded");
        return;
    }
    
    std::string batchId = getCurrentBatchId();
    BatchJob job;
    job.batchId = batchId;
    job.entityType = "categories";
    job.filename = partHandler.getFilename();
    job.startedAt = Poco::DateTimeFormatter::format(utils::DateUtils::currentTimestamp(), 
                                                    Poco::DateTimeFormat::ISO8601_FORMAT);
    job.uploadedBy = uploadedBy;
    job.status = "processing";
    
    {
        std::lock_guard<std::mutex> lock(batchJobsMutex);
        batchJobs[batchId] = job;
    }
    
    Poco::JSON::Array categoriesArray;
    try
    {
        categoriesArray = parseCategoryCSVFile(tempFile, uploadedBy, ipAddress, userAgent);
    }
    catch (const std::exception& e)
    {
        Poco::File(tempFile).remove();
        
        {
            std::lock_guard<std::mutex> lock(batchJobsMutex);
            batchJobs[batchId].completedAt = Poco::DateTimeFormatter::format(
                utils::DateUtils::currentTimestamp(), 
                Poco::DateTimeFormat::ISO8601_FORMAT);
            batchJobs[batchId].status = "failed";
        }
        
        sendErrorResponse(response, "Failed to parse CSV file: " + std::string(e.what()));
        return;
    }
    
    Poco::File(tempFile).remove();
    
    Poco::JSON::Array importResults = categoryService->importCategories(categoriesArray, uploadedBy);
    
    int successCount = 0;
    int failureCount = 0;
    for (size_t i = 0; i < importResults.size(); ++i)
    {
        auto result = importResults.get(i).extract<Poco::JSON::Object>();
        if (result.has("success") && result.getValue<bool>("success"))
        {
            successCount++;
        }
        else
        {
            failureCount++;
        }
    }
    
    updateBatchJobStatus(batchId, static_cast<int>(categoriesArray.size()),
                        static_cast<int>(categoriesArray.size()),
                        successCount, failureCount, importResults);
    
    Poco::JSON::Object::Ptr responseJson = new Poco::JSON::Object;
    responseJson->set("success", true);
    responseJson->set("message", "Category batch upload completed");
    responseJson->set("batchId", batchId);
    responseJson->set("filename", partHandler.getFilename());
    responseJson->set("totalRows", static_cast<int>(categoriesArray.size()));
    responseJson->set("successCount", successCount);
    responseJson->set("failureCount", failureCount);
    responseJson->set("results", importResults);
    
    sendJsonResponse(response, responseJson);
}

void BatchImportController::handleTemplateCategoryDownload(Poco::Net::HTTPServerResponse& response)
{
    std::string csvTemplate = "name,description,parent_id,sort_order\n"
                             "Electronics,All electronic devices,0,1\n"
                             "Computers,Desktop and laptop computers,1,1\n"
                             "Laptops,Portable computers,2,1\n"
                             "Accessories,Computer accessories,1,2\n\n"
                             "Notes:\n"
                             "1. name: Category name (required)\n"
                             "2. description: Optional description\n"
                             "3. parent_id: Parent category ID (0 for root)\n"
                             "4. sort_order: Display order\n"
                             "5. Parent category must exist if parent_id > 0";
    
    response.setStatus(Poco::Net::HTTPResponse::HTTP_OK);
    response.setContentType("text/csv");
    response.set("Content-Disposition", "attachment; filename=category_template.csv");
    response.setChunkedTransferEncoding(true);
    
    std::ostream& ostr = response.send();
    ostr << csvTemplate;
}

void BatchImportController::handleBatchWarehouseCellUpload(Poco::Net::HTTPServerRequest& request,
                                                          Poco::Net::HTTPServerResponse& response)
{
    std::string authHeader = getAuthorizationHeader(request);
    std::string token = extractBearerToken(authHeader);
    
    if (token.empty())
    {
        sendUnauthorizedResponse(response, "Authorization token required");
        return;
    }
    
    long long uploadedBy = 1;
    std::string ipAddress = getClientIpAddress(request);
    std::string userAgent = getUserAgent(request);
    
    CSVPartHandler partHandler;
    Poco::Net::HTMLForm form;
    form.load(request, request.stream(), partHandler);
    
    std::string tempFile = partHandler.getTempFile();
    if (tempFile.empty())
    {
        sendErrorResponse(response, "No CSV file uploaded");
        return;
    }
    
    std::string batchId = getCurrentBatchId();
    BatchJob job;
    job.batchId = batchId;
    job.entityType = "warehouse_cells";
    job.filename = partHandler.getFilename();
    job.startedAt = Poco::DateTimeFormatter::format(utils::DateUtils::currentTimestamp(), 
                                                    Poco::DateTimeFormat::ISO8601_FORMAT);
    job.uploadedBy = uploadedBy;
    job.status = "processing";
    
    {
        std::lock_guard<std::mutex> lock(batchJobsMutex);
        batchJobs[batchId] = job;
    }
    
    Poco::JSON::Array cellsArray;
    try
    {
        cellsArray = parseWarehouseCellCSVFile(tempFile, uploadedBy, ipAddress, userAgent);
    }
    catch (const std::exception& e)
    {
        Poco::File(tempFile).remove();
        
        {
            std::lock_guard<std::mutex> lock(batchJobsMutex);
            batchJobs[batchId].completedAt = Poco::DateTimeFormatter::format(
                utils::DateUtils::currentTimestamp(), 
                Poco::DateTimeFormat::ISO8601_FORMAT);
            batchJobs[batchId].status = "failed";
        }
        
        sendErrorResponse(response, "Failed to parse CSV file: " + std::string(e.what()));
        return;
    }
    
    Poco::File(tempFile).remove();
    
    Poco::JSON::Array importResults;
    int successCount = 0;
    int failureCount = 0;
    
    for (size_t i = 0; i < cellsArray.size(); ++i)
    {
        auto cellObj = cellsArray.get(i).extract<Poco::JSON::Object>();
        
        if (cellObj.has("error"))
        {
            Poco::JSON::Object errorResult;
            errorResult.set("row", static_cast<int>(i + 1));
            errorResult.set("success", false);
            errorResult.set("error", cellObj.getValue<std::string>("error"));
            importResults.add(errorResult);
            failureCount++;
            continue;
        }
        
        try
        {
            database::models::WarehouseCell cellModel(cellObj);
            
            if (!cellModel.validate())
            {
                throw std::runtime_error("Warehouse cell validation failed");
            }
            
            auto result = warehouseCellService->createCell(cellModel, uploadedBy);
            
            if (result.success)
            {
                Poco::JSON::Object successResult;
                successResult.set("row", static_cast<int>(i + 1));
                successResult.set("success", true);
                successResult.set("cellId", result.cellId);
                successResult.set("cellCode", cellModel.cellCode);
                successResult.set("zone", cellModel.zone);
                successResult.set("message", result.message);
                importResults.add(successResult);
                successCount++;
            }
            else
            {
                Poco::JSON::Object errorResult;
                errorResult.set("row", static_cast<int>(i + 1));
                errorResult.set("success", false);
                errorResult.set("error", result.message);
                errorResult.set("cellCode", cellModel.cellCode);
                importResults.add(errorResult);
                failureCount++;
            }
        }
        catch (const std::exception& e)
        {
            Poco::JSON::Object errorResult;
            errorResult.set("row", static_cast<int>(i + 1));
            errorResult.set("success", false);
            errorResult.set("error", "Failed to create warehouse cell: " + std::string(e.what()));
            if (cellObj.has("cell_code"))
                errorResult.set("cellCode", cellObj.getValue<std::string>("cell_code"));
            importResults.add(errorResult);
            failureCount++;
        }
    }
    
    updateBatchJobStatus(batchId, static_cast<int>(cellsArray.size()),
                        static_cast<int>(cellsArray.size()),
                        successCount, failureCount, importResults);
    
    Poco::JSON::Object::Ptr responseJson = new Poco::JSON::Object;
    responseJson->set("success", true);
    responseJson->set("message", "Warehouse cell batch upload completed");
    responseJson->set("batchId", batchId);
    responseJson->set("filename", partHandler.getFilename());
    responseJson->set("totalRows", static_cast<int>(cellsArray.size()));
    responseJson->set("successCount", successCount);
    responseJson->set("failureCount", failureCount);
    responseJson->set("results", importResults);
    
    sendJsonResponse(response, responseJson);
}

void BatchImportController::handleTemplateWarehouseCellDownload(Poco::Net::HTTPServerResponse& response)
{
    std::string csvTemplate = "cell_code,zone,rack,shelf,position,max_volume,max_weight,temperature_zone\n"
                             "A-01-01,Zone A,Rack 01,Shelf 01,Position 01,10.0,1000.0,normal\n"
                             "B-02-03,Zone B,Rack 02,Shelf 03,Position 02,5.0,500.0,cool\n"
                             "C-03-02,Zone C,Rack 03,Shelf 02,Position 01,2.0,200.0,frozen\n\n"
                             "Notes:\n"
                             "1. cell_code: Unique cell identifier (required)\n"
                             "2. zone: Warehouse zone\n"
                             "3. rack: Rack identifier\n"
                             "4. shelf: Shelf level\n"
                             "5. position: Position on shelf\n"
                             "6. max_volume: Maximum volume capacity in m³ (required)\n"
                             "7. max_weight: Maximum weight capacity in kg (required)\n"
                             "8. temperature_zone: normal, cool, or frozen (default: normal)";
    
    response.setStatus(Poco::Net::HTTPResponse::HTTP_OK);
    response.setContentType("text/csv");
    response.set("Content-Disposition", "attachment; filename=warehouse_cell_template.csv");
    response.setChunkedTransferEncoding(true);
    
    std::ostream& ostr = response.send();
    ostr << csvTemplate;
}

void BatchImportController::handleBatchSupplierUpload(Poco::Net::HTTPServerRequest& request,
                                                     Poco::Net::HTTPServerResponse& response)
{
    std::string authHeader = getAuthorizationHeader(request);
    std::string token = extractBearerToken(authHeader);
    
    if (token.empty())
    {
        sendUnauthorizedResponse(response, "Authorization token required");
        return;
    }
    
    long long uploadedBy = 1;
    std::string ipAddress = getClientIpAddress(request);
    std::string userAgent = getUserAgent(request);
    
    CSVPartHandler partHandler;
    Poco::Net::HTMLForm form;
    form.load(request, request.stream(), partHandler);
    
    std::string tempFile = partHandler.getTempFile();
    if (tempFile.empty())
    {
        sendErrorResponse(response, "No CSV file uploaded");
        return;
    }
    
    std::string batchId = getCurrentBatchId();
    BatchJob job;
    job.batchId = batchId;
    job.entityType = "suppliers";
    job.filename = partHandler.getFilename();
    job.startedAt = Poco::DateTimeFormatter::format(utils::DateUtils::currentTimestamp(), 
                                                    Poco::DateTimeFormat::ISO8601_FORMAT);
    job.uploadedBy = uploadedBy;
    job.status = "processing";
    
    {
        std::lock_guard<std::mutex> lock(batchJobsMutex);
        batchJobs[batchId] = job;
    }
    
    Poco::JSON::Array suppliersArray;
    try
    {
        suppliersArray = parseSupplierCSVFile(tempFile, uploadedBy, ipAddress, userAgent);
    }
    catch (const std::exception& e)
    {
        Poco::File(tempFile).remove();
        
        {
            std::lock_guard<std::mutex> lock(batchJobsMutex);
            batchJobs[batchId].completedAt = Poco::DateTimeFormatter::format(
                utils::DateUtils::currentTimestamp(), 
                Poco::DateTimeFormat::ISO8601_FORMAT);
            batchJobs[batchId].status = "failed";
        }
        
        sendErrorResponse(response, "Failed to parse CSV file: " + std::string(e.what()));
        return;
    }
    
    Poco::File(tempFile).remove();
    
    Poco::JSON::Array importResults;
    int successCount = 0;
    int failureCount = 0;
    
    for (size_t i = 0; i < suppliersArray.size(); ++i)
    {
        auto supplierObj = suppliersArray.get(i).extract<Poco::JSON::Object>();
        
        if (supplierObj.has("error"))
        {
            Poco::JSON::Object errorResult;
            errorResult.set("row", static_cast<int>(i + 1));
            errorResult.set("success", false);
            errorResult.set("error", supplierObj.getValue<std::string>("error"));
            importResults.add(errorResult);
            failureCount++;
            continue;
        }
        
        try
        {
            database::models::Supplier supplierModel(supplierObj);
            
            if (!supplierModel.validate())
            {
                throw std::runtime_error("Supplier validation failed");
            }
            
            auto result = supplierService->createSupplier(supplierModel, uploadedBy);
            
            if (result.success)
            {
                Poco::JSON::Object successResult;
                successResult.set("row", static_cast<int>(i + 1));
                successResult.set("success", true);
                successResult.set("supplierId", result.supplierId);
                successResult.set("name", supplierModel.name);
                successResult.set("taxId", supplierModel.taxId);
                successResult.set("message", result.message);
                importResults.add(successResult);
                successCount++;
            }
            else
            {
                Poco::JSON::Object errorResult;
                errorResult.set("row", static_cast<int>(i + 1));
                errorResult.set("success", false);
                errorResult.set("error", result.message);
                errorResult.set("name", supplierModel.name);
                importResults.add(errorResult);
                failureCount++;
            }
        }
        catch (const std::exception& e)
        {
            Poco::JSON::Object errorResult;
            errorResult.set("row", static_cast<int>(i + 1));
            errorResult.set("success", false);
            errorResult.set("error", "Failed to create supplier: " + std::string(e.what()));
            if (supplierObj.has("name"))
                errorResult.set("name", supplierObj.getValue<std::string>("name"));
            importResults.add(errorResult);
            failureCount++;
        }
    }
    
    updateBatchJobStatus(batchId, static_cast<int>(suppliersArray.size()),
                        static_cast<int>(suppliersArray.size()),
                        successCount, failureCount, importResults);
    
    Poco::JSON::Object::Ptr responseJson = new Poco::JSON::Object;
    responseJson->set("success", true);
    responseJson->set("message", "Supplier batch upload completed");
    responseJson->set("batchId", batchId);
    responseJson->set("filename", partHandler.getFilename());
    responseJson->set("totalRows", static_cast<int>(suppliersArray.size()));
    responseJson->set("successCount", successCount);
    responseJson->set("failureCount", failureCount);
    responseJson->set("results", importResults);
    
    sendJsonResponse(response, responseJson);
}

void BatchImportController::handleTemplateSupplierDownload(Poco::Net::HTTPServerResponse& response)
{
    std::string csvTemplate = "name,contact_person,email,phone,address,tax_id,payment_terms,rating,is_active\n"
                             "Tech Supplies Inc.,John Smith,john@techsupplies.com,+1234567890,123 Tech St,TAX12345,Net 30,4.5,true\n"
                             "Global Electronics,Alice Johnson,alice@globalelectronics.com,,456 Global Ave,TAX67890,Net 15,4.2,true\n"
                             "Quality Parts Ltd.,,sales@qualityparts.com,+9876543210,789 Quality Blvd,TAX11223,,3.8,true\n\n"
                             "Notes:\n"
                             "1. name: Supplier name (required)\n"
                             "2. contact_person: Optional contact person\n"
                             "3. email: Optional email address\n"
                             "4. phone: Optional phone number\n"
                             "5. address: Optional address\n"
                             "6. tax_id: Tax identification number\n"
                             "7. payment_terms: Optional payment terms\n"
                             "8. rating: Rating 0.0-5.0 (default: 0.0)\n"
                             "9. is_active: true or false (default: true)";
    
    response.setStatus(Poco::Net::HTTPResponse::HTTP_OK);
    response.setContentType("text/csv");
    response.set("Content-Disposition", "attachment; filename=supplier_template.csv");
    response.setChunkedTransferEncoding(true);
    
    std::ostream& ostr = response.send();
    ostr << csvTemplate;
}

Poco::JSON::Array BatchImportController::parseCategoryCSVFile(const std::string& filePath,
                                                            long long uploadedBy,
                                                            const std::string& ipAddress,
                                                            const std::string& userAgent)
{
    Poco::JSON::Array categoriesArray;
    
    std::ifstream file(filePath);
    if (!file.is_open())
    {
        throw std::runtime_error("Could not open CSV file");
    }
    
    std::string line;
    int lineNumber = 0;
    std::vector<std::string> headers;
    
    while (std::getline(file, line))
    {
        lineNumber++;
        
        if (line.empty() || line.find_first_not_of(" \t\n\r") == std::string::npos ||
            line[0] == '#' || line.substr(0, 3) == "Notes:")
        {
            continue;
        }
        
        std::vector<std::string> row;
        std::stringstream ss(line);
        std::string cell;
        bool inQuotes = false;
        
        while (std::getline(ss, cell, ','))
        {
            if (!cell.empty() && cell[0] == '"' && cell[cell.size()-1] != '"')
            {
                inQuotes = true;
                std::string fullCell = cell;
                while (inQuotes && std::getline(ss, cell, ','))
                {
                    fullCell += "," + cell;
                    if (!cell.empty() && cell[cell.size()-1] == '"')
                    {
                        inQuotes = false;
                        cell = fullCell;
                    }
                }
            }
            
            if (!cell.empty() && cell[0] == '"' && cell[cell.size()-1] == '"')
            {
                cell = cell.substr(1, cell.size() - 2);
            }
            
            size_t start = cell.find_first_not_of(" \t");
            size_t end = cell.find_last_not_of(" \t");
            if (start != std::string::npos && end != std::string::npos)
            {
                cell = cell.substr(start, end - start + 1);
            }
            else if (cell == "\\N")
            {
                cell = "";
            }
            
            row.push_back(cell);
        }
        
        if (lineNumber == 1)
        {
            headers = row;
            
            std::vector<std::string> requiredHeaders = {"name"};
            for (const auto& required : requiredHeaders)
            {
                auto it = std::find(headers.begin(), headers.end(), required);
                if (it == headers.end())
                {
                    throw std::runtime_error("Missing required column: " + required);
                }
            }
        }
        else
        {
            try
            {
                Poco::JSON::Object categoryData = processCategoryCSVRow(row, lineNumber);
                
                std::string validationError;
                if (validateCategoryCSVRow(categoryData, validationError))
                {
                    categoriesArray.add(categoryData);
                }
                else
                {
                    Poco::JSON::Object error;
                    error.set("row", lineNumber);
                    error.set("error", "Validation failed: " + validationError);
                    if (!row.empty() && headers.size() > 0 && row[0] != "")
                        error.set("name", row[0]);
                    categoriesArray.add(error);
                }
            }
            catch (const std::exception& e)
            {
                Poco::JSON::Object error;
                error.set("row", lineNumber);
                error.set("error", "Processing error: " + std::string(e.what()));
                if (!row.empty() && headers.size() > 0 && row[0] != "")
                    error.set("name", row[0]);
                categoriesArray.add(error);
            }
        }
    }
    
    file.close();
    
    if (categoriesArray.size() == 0)
    {
        throw std::runtime_error("No valid category data found in CSV");
    }
    
    return categoriesArray;
}

Poco::JSON::Array BatchImportController::parseWarehouseCellCSVFile(const std::string& filePath,
                                                                 long long uploadedBy,
                                                                 const std::string& ipAddress,
                                                                 const std::string& userAgent)
{
    Poco::JSON::Array cellsArray;
    
    std::ifstream file(filePath);
    if (!file.is_open())
    {
        throw std::runtime_error("Could not open CSV file");
    }
    
    std::string line;
    int lineNumber = 0;
    std::vector<std::string> headers;
    
    while (std::getline(file, line))
    {
        lineNumber++;
        
        if (line.empty() || line.find_first_not_of(" \t\n\r") == std::string::npos ||
            line[0] == '#' || line.substr(0, 3) == "Notes:")
        {
            continue;
        }
        
        std::vector<std::string> row;
        std::stringstream ss(line);
        std::string cell;
        bool inQuotes = false;
        
        while (std::getline(ss, cell, ','))
        {
            if (!cell.empty() && cell[0] == '"' && cell[cell.size()-1] != '"')
            {
                inQuotes = true;
                std::string fullCell = cell;
                while (inQuotes && std::getline(ss, cell, ','))
                {
                    fullCell += "," + cell;
                    if (!cell.empty() && cell[cell.size()-1] == '"')
                    {
                        inQuotes = false;
                        cell = fullCell;
                    }
                }
            }
            
            if (!cell.empty() && cell[0] == '"' && cell[cell.size()-1] == '"')
            {
                cell = cell.substr(1, cell.size() - 2);
            }
            
            size_t start = cell.find_first_not_of(" \t");
            size_t end = cell.find_last_not_of(" \t");
            if (start != std::string::npos && end != std::string::npos)
            {
                cell = cell.substr(start, end - start + 1);
            }
            else if (cell == "\\N")
            {
                cell = "";
            }
            
            row.push_back(cell);
        }
        
        if (lineNumber == 1)
        {
            headers = row;
            
            std::vector<std::string> requiredHeaders = {"cell_code", "max_volume", "max_weight"};
            for (const auto& required : requiredHeaders)
            {
                auto it = std::find(headers.begin(), headers.end(), required);
                if (it == headers.end())
                {
                    throw std::runtime_error("Missing required column: " + required);
                }
            }
        }
        else
        {
            try
            {
                Poco::JSON::Object cellData = processWarehouseCellCSVRow(row, lineNumber);
                
                std::string validationError;
                if (validateWarehouseCellCSVRow(cellData, validationError))
                {
                    cellsArray.add(cellData);
                }
                else
                {
                    Poco::JSON::Object error;
                    error.set("row", lineNumber);
                    error.set("error", "Validation failed: " + validationError);
                    if (!row.empty() && headers.size() > 0 && row[0] != "")
                        error.set("cell_code", row[0]);
                    cellsArray.add(error);
                }
            }
            catch (const std::exception& e)
            {
                Poco::JSON::Object error;
                error.set("row", lineNumber);
                error.set("error", "Processing error: " + std::string(e.what()));
                if (!row.empty() && headers.size() > 0 && row[0] != "")
                    error.set("cell_code", row[0]);
                cellsArray.add(error);
            }
        }
    }
    
    file.close();
    
    if (cellsArray.size() == 0)
    {
        throw std::runtime_error("No valid warehouse cell data found in CSV");
    }
    
    return cellsArray;
}

Poco::JSON::Array BatchImportController::parseSupplierCSVFile(const std::string& filePath,
                                                            long long uploadedBy,
                                                            const std::string& ipAddress,
                                                            const std::string& userAgent)
{
    Poco::JSON::Array suppliersArray;
    
    std::ifstream file(filePath);
    if (!file.is_open())
    {
        throw std::runtime_error("Could not open CSV file");
    }
    
    std::string line;
    int lineNumber = 0;
    std::vector<std::string> headers;
    
    while (std::getline(file, line))
    {
        lineNumber++;
        
        if (line.empty() || line.find_first_not_of(" \t\n\r") == std::string::npos ||
            line[0] == '#' || line.substr(0, 3) == "Notes:")
        {
            continue;
        }
        
        std::vector<std::string> row;
        std::stringstream ss(line);
        std::string cell;
        bool inQuotes = false;
        
        while (std::getline(ss, cell, ','))
        {
            if (!cell.empty() && cell[0] == '"' && cell[cell.size()-1] != '"')
            {
                inQuotes = true;
                std::string fullCell = cell;
                while (inQuotes && std::getline(ss, cell, ','))
                {
                    fullCell += "," + cell;
                    if (!cell.empty() && cell[cell.size()-1] == '"')
                    {
                        inQuotes = false;
                        cell = fullCell;
                    }
                }
            }
            
            if (!cell.empty() && cell[0] == '"' && cell[cell.size()-1] == '"')
            {
                cell = cell.substr(1, cell.size() - 2);
            }
            
            size_t start = cell.find_first_not_of(" \t");
            size_t end = cell.find_last_not_of(" \t");
            if (start != std::string::npos && end != std::string::npos)
            {
                cell = cell.substr(start, end - start + 1);
            }
            else if (cell == "\\N")
            {
                cell = "";
            }
            
            row.push_back(cell);
        }
        
        if (lineNumber == 1)
        {
            headers = row;
            
            std::vector<std::string> requiredHeaders = {"name"};
            for (const auto& required : requiredHeaders)
            {
                auto it = std::find(headers.begin(), headers.end(), required);
                if (it == headers.end())
                {
                    throw std::runtime_error("Missing required column: " + required);
                }
            }
        }
        else
        {
            try
            {
                Poco::JSON::Object supplierData = processSupplierCSVRow(row, lineNumber);
                
                std::string validationError;
                if (validateSupplierCSVRow(supplierData, validationError))
                {
                    suppliersArray.add(supplierData);
                }
                else
                {
                    Poco::JSON::Object error;
                    error.set("row", lineNumber);
                    error.set("error", "Validation failed: " + validationError);
                    if (!row.empty() && headers.size() > 0 && row[0] != "")
                        error.set("name", row[0]);
                    suppliersArray.add(error);
                }
            }
            catch (const std::exception& e)
            {
                Poco::JSON::Object error;
                error.set("row", lineNumber);
                error.set("error", "Processing error: " + std::string(e.what()));
                if (!row.empty() && headers.size() > 0 && row[0] != "")
                    error.set("name", row[0]);
                suppliersArray.add(error);
            }
        }
    }
    
    file.close();
    
    if (suppliersArray.size() == 0)
    {
        throw std::runtime_error("No valid supplier data found in CSV");
    }
    
    return suppliersArray;
}

Poco::JSON::Object BatchImportController::processCategoryCSVRow(const std::vector<std::string>& row,
                                                              int rowNumber)
{
    Poco::JSON::Object categoryData;
    
    categoryData.set("name", "");
    categoryData.set("description", "");
    categoryData.set("parent_id", 0);
    categoryData.set("sort_order", 0);
    
    if (row.size() >= 1) categoryData.set("name", row[0]);
    if (row.size() >= 2 && !row[1].empty()) categoryData.set("description", row[1]);
    
    try
    {
        if (row.size() >= 3 && !row[2].empty())
        {
            int64_t parentId = Poco::NumberParser::parse64(row[2]);
            categoryData.set("parent_id", parentId);
        }
        
        if (row.size() >= 4 && !row[3].empty())
        {
            int sortOrder = Poco::NumberParser::parse(row[3]);
            categoryData.set("sort_order", sortOrder);
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Error parsing numeric value: " + e.displayText());
    }
    
    return categoryData;
}

Poco::JSON::Object BatchImportController::processWarehouseCellCSVRow(const std::vector<std::string>& row,
                                                                   int rowNumber)
{
    Poco::JSON::Object cellData;
    
    cellData.set("cell_code", "");
    cellData.set("zone", "");
    cellData.set("rack", "");
    cellData.set("shelf", "");
    cellData.set("position", "");
    cellData.set("max_volume", 0.0);
    cellData.set("max_weight", 0.0);
    cellData.set("temperature_zone", "normal");
    
    if (row.size() >= 1) cellData.set("cell_code", row[0]);
    if (row.size() >= 2 && !row[1].empty()) cellData.set("zone", row[1]);
    if (row.size() >= 3 && !row[2].empty()) cellData.set("rack", row[2]);
    if (row.size() >= 4 && !row[3].empty()) cellData.set("shelf", row[3]);
    if (row.size() >= 5 && !row[4].empty()) cellData.set("position", row[4]);
    
    try
    {
        if (row.size() >= 6 && !row[5].empty())
        {
            double maxVolume = Poco::NumberParser::parseFloat(row[5]);
            cellData.set("max_volume", maxVolume);
        }
        
        if (row.size() >= 7 && !row[6].empty())
        {
            double maxWeight = Poco::NumberParser::parseFloat(row[6]);
            cellData.set("max_weight", maxWeight);
        }
        
        if (row.size() >= 8 && !row[7].empty())
        {
            cellData.set("temperature_zone", row[7]);
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Error parsing numeric value: " + e.displayText());
    }
    
    return cellData;
}

Poco::JSON::Object BatchImportController::processSupplierCSVRow(const std::vector<std::string>& row,
                                                              int rowNumber)
{
    Poco::JSON::Object supplierData;
    
    supplierData.set("name", "");
    supplierData.set("contact_person", "");
    supplierData.set("email", "");
    supplierData.set("phone", "");
    supplierData.set("address", "");
    supplierData.set("tax_id", "");
    supplierData.set("payment_terms", "");
    supplierData.set("rating", 0.0);
    supplierData.set("is_active", true);
    
    if (row.size() >= 1) supplierData.set("name", row[0]);
    if (row.size() >= 2 && !row[1].empty()) supplierData.set("contact_person", row[1]);
    if (row.size() >= 3 && !row[2].empty()) supplierData.set("email", row[2]);
    if (row.size() >= 4 && !row[4].empty()) supplierData.set("phone", row[3]);
    if (row.size() >= 5 && !row[4].empty()) supplierData.set("address", row[4]);
    if (row.size() >= 6 && !row[5].empty()) supplierData.set("tax_id", row[5]);
    if (row.size() >= 7 && !row[6].empty()) supplierData.set("payment_terms", row[6]);
    
    try
    {
        if (row.size() >= 8 && !row[7].empty())
        {
            double rating = Poco::NumberParser::parseFloat(row[7]);
            supplierData.set("rating", rating);
        }
        
        if (row.size() >= 9 && !row[8].empty())
        {
            std::string isActive = row[8];
            std::transform(isActive.begin(), isActive.end(), isActive.begin(), ::tolower);
            supplierData.set("is_active", (isActive == "true" || isActive == "1" || isActive == "yes" || isActive == "y"));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Error parsing numeric value: " + e.displayText());
    }
    
    return supplierData;
}

bool BatchImportController::validateCategoryCSVRow(const Poco::JSON::Object& categoryData,
                                                 std::string& error)
{
    if (!categoryData.has("name") || categoryData.getValue<std::string>("name").empty())
    {
        error = "Category name is required";
        return false;
    }
    
    std::string name = categoryData.getValue<std::string>("name");
    if (name.length() > 100)
    {
        error = "Category name cannot exceed 100 characters";
        return false;
    }
    
    return true;
}

bool BatchImportController::validateWarehouseCellCSVRow(const Poco::JSON::Object& cellData,
                                                       std::string& error)
{
    if (!cellData.has("cell_code") || cellData.getValue<std::string>("cell_code").empty())
    {
        error = "Cell code is required";
        return false;
    }
    
    if (!cellData.has("max_volume") || cellData.getValue<double>("max_volume") <= 0)
    {
        error = "Maximum volume must be greater than 0";
        return false;
    }
    
    if (!cellData.has("max_weight") || cellData.getValue<double>("max_weight") <= 0)
    {
        error = "Maximum weight must be greater than 0";
        return false;
    }
    
    std::string cellCode = cellData.getValue<std::string>("cell_code");
    if (cellCode.length() > 50)
    {
        error = "Cell code cannot exceed 50 characters";
        return false;
    }
    
    return true;
}

bool BatchImportController::validateSupplierCSVRow(const Poco::JSON::Object& supplierData,
                                                  std::string& error)
{
    if (!supplierData.has("name") || supplierData.getValue<std::string>("name").empty())
    {
        error = "Supplier name is required";
        return false;
    }
    
    std::string name = supplierData.getValue<std::string>("name");
    if (name.length() > 150)
    {
        error = "Supplier name cannot exceed 150 characters";
        return false;
    }
    
    if (supplierData.has("email") && !supplierData.getValue<std::string>("email").empty())
    {
        if (!utils::Validator::isValidEmail(supplierData.getValue<std::string>("email")))
        {
            error = "Invalid email format";
            return false;
        }
    }
    
    if (supplierData.has("rating"))
    {
        double rating = supplierData.getValue<double>("rating");
        if (rating < 0.0 || rating > 5.0)
        {
            error = "Rating must be between 0.0 and 5.0";
            return false;
        }
    }
    
    return true;
}

std::string BatchImportController::getCurrentBatchId()
{
    Poco::UUIDGenerator& generator = Poco::UUIDGenerator::defaultGenerator();
    Poco::UUID uuid = generator.create();
    return "batch_" + uuid.toString();
}

void BatchImportController::updateBatchJobStatus(const std::string& batchId, int totalRows, 
                                                int processedRows, int successCount, int failureCount,
                                                const Poco::JSON::Array& results)
{
    std::lock_guard<std::mutex> lock(batchJobsMutex);
    
    if (batchJobs.find(batchId) != batchJobs.end())
    {
        batchJobs[batchId].totalRows = totalRows;
        batchJobs[batchId].processedRows = processedRows;
        batchJobs[batchId].successCount = successCount;
        batchJobs[batchId].failureCount = failureCount;
        batchJobs[batchId].completedAt = Poco::DateTimeFormatter::format(
            utils::DateUtils::currentTimestamp(), 
            Poco::DateTimeFormat::ISO8601_FORMAT);
        batchJobs[batchId].results = results;
        batchJobs[batchId].status = "completed";
    }
}

} // namespace controllers
