#pragma once

#include "BaseController.hpp"
#include "../services/UserService.hpp"
#include "../services/ProductService.hpp"
#include "../services/CategoryService.hpp"
#include "../services/SupplierService.hpp"
#include "../services/WarehouseCellService.hpp"
#include <Poco/Net/HTMLForm.h>
#include <Poco/Net/PartHandler.h>
#include <Poco/Net/MessageHeader.h>
#include <Poco/StreamCopier.h>
#include <memory>
#include <sstream>
#include <fstream>

namespace warehouse_backend::controllers
{

class CSVPartHandler : public Poco::Net::PartHandler
{
public:
    CSVPartHandler() : _tempFile(""), _filename("") {}
    
    void handlePart(const Poco::Net::MessageHeader& header, 
                    std::istream& stream) override
    {
        std::string disp;
        if (header.has("Content-Disposition"))
        {
            std::string cd = header.get("Content-Disposition");
            Poco::Net::NameValueCollection params;
            Poco::Net::MessageHeader::splitParameters(cd.begin(), cd.end(), params);
            disp = params.get("name", "");
            _filename = params.get("filename", "");
        }
        
        if (_filename.empty() || _filename.find(".csv") == std::string::npos)
        {
            throw Poco::Exception("Invalid or missing CSV file");
        }
        
        _tempFile = "temp_" + _filename;
        std::ofstream ofs(_tempFile, std::ios::binary);
        Poco::StreamCopier::copyStream(stream, ofs);
        ofs.close();
    }
    
    std::string getTempFile() const { return _tempFile; }
    std::string getFilename() const { return _filename; }
    
private:
    std::string _tempFile;
    std::string _filename;
};

class BatchImportController : public BaseController
{
public:
    BatchImportController();
    virtual ~BatchImportController() = default;
    
    void handleRequest(Poco::Net::HTTPServerRequest& request,
                       Poco::Net::HTTPServerResponse& response) override;
    
private:
    std::unique_ptr<services::UserService> userService;
    std::unique_ptr<services::ProductService> productService;
    std::unique_ptr<services::CategoryService> categoryService;
    std::unique_ptr<services::SupplierService> supplierService;
    std::unique_ptr<services::WarehouseCellService> warehouseCellService;
    
    void handleBatchUserUpload(Poco::Net::HTTPServerRequest& request,
                          Poco::Net::HTTPServerResponse& response);
    void handleTemplateUserDownload(Poco::Net::HTTPServerResponse& response);

    void handleBatchProductUpload(Poco::Net::HTTPServerRequest& request,
                                  Poco::Net::HTTPServerResponse& response);
    void handleTemplateProductDownload(Poco::Net::HTTPServerResponse& response);
    
    void handleBatchCategoryUpload(Poco::Net::HTTPServerRequest& request,
                                Poco::Net::HTTPServerResponse& response);
    void handleTemplateCategoryDownload(Poco::Net::HTTPServerResponse& response);

    void handleBatchWarehouseCellUpload(Poco::Net::HTTPServerRequest& request,
                                        Poco::Net::HTTPServerResponse& response);
    void handleTemplateWarehouseCellDownload(Poco::Net::HTTPServerResponse& response);

    void handleBatchSupplierUpload(Poco::Net::HTTPServerRequest& request,
                                Poco::Net::HTTPServerResponse& response);
    void handleTemplateSupplierDownload(Poco::Net::HTTPServerResponse& response);

    Poco::JSON::Array parseCSVFile(const std::string& filePath, long long uploadedBy,
                                  const std::string& ipAddress, const std::string& userAgent);

    Poco::JSON::Array parseProductCSVFile(const std::string& filePath, 
                                          long long uploadedBy,
                                          const std::string& ipAddress, 
                                          const std::string& userAgent);

    Poco::JSON::Array parseCategoryCSVFile(const std::string& filePath,
                                      long long uploadedBy,
                                      const std::string& ipAddress,
                                      const std::string& userAgent);

    Poco::JSON::Array parseWarehouseCellCSVFile(const std::string& filePath,
                                            long long uploadedBy,
                                            const std::string& ipAddress,
                                            const std::string& userAgent);

    Poco::JSON::Array parseSupplierCSVFile(const std::string& filePath,
                                        long long uploadedBy,
                                        const std::string& ipAddress,
                                        const std::string& userAgent);
    
    Poco::JSON::Object processCSVRow(const std::vector<std::string>& row, int rowNumber);

    Poco::JSON::Object processProductCSVRow(const std::vector<std::string>& row, 
                                            int rowNumber);

    Poco::JSON::Object processCategoryCSVRow(const std::vector<std::string>& row,
                                            int rowNumber);

    Poco::JSON::Object processWarehouseCellCSVRow(const std::vector<std::string>& row,
                                                int rowNumber);

    Poco::JSON::Object processSupplierCSVRow(const std::vector<std::string>& row,
                                            int rowNumber);
    
    bool validateCSVRow(const Poco::JSON::Object& userData, std::string& error);

    bool validateProductCSVRow(const Poco::JSON::Object& productData, 
                               std::string& error);
                        
    bool validateCategoryCSVRow(const Poco::JSON::Object& categoryData,
                                std::string& error);

    bool validateWarehouseCellCSVRow(const Poco::JSON::Object& cellData,
                                    std::string& error);

    bool validateSupplierCSVRow(const Poco::JSON::Object& supplierData,
                                std::string& error);

    void updateBatchJobStatus(const std::string& batchId, int totalRows, 
                             int processedRows, int successCount, int failureCount,
                             const Poco::JSON::Array& results);
    
    std::string getCurrentBatchId();
    
    struct BatchJob
    {
        std::string batchId;
        std::string entityType;
        std::string filename;
        int totalRows;
        int processedRows;
        int successCount;
        int failureCount;
        std::string startedAt;
        std::string completedAt;
        Poco::JSON::Array results;
        long long uploadedBy;
        std::string status;
    };
    
    static std::map<std::string, BatchJob> batchJobs;
    static std::mutex batchJobsMutex;
};

} // namespace controllers
