#pragma once

#include <Poco/Net/HTTPServer.h>
#include <Poco/Net/ServerSocket.h>
#include <Poco/Util/ServerApplication.h>
#include <Poco/Util/OptionSet.h>
#include <memory>
#include <string>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <mutex>

namespace warehouse_backend
{

class ApplicationServer : public Poco::Util::ServerApplication
{
public:
    ApplicationServer();
    virtual ~ApplicationServer();
    
protected:
    void initialize(Poco::Util::Application& app) override;
    void uninitialize() override;
    void defineOptions(Poco::Util::OptionSet& options) override;
    void handleOption(const std::string& name, const std::string& value) override;
    int main(const std::vector<std::string>& args) override;
    
    void setupSignalHandlers();
    static void handleSignal(int signal);
    
    void setupLogging();
    void setupDatabase();
    void setupServer();
    void setupControllers();
    void setupRoutes();
    void setupMiddleware();
    
    void startServer();
    void stopServer();
    void waitForTermination();
    
    void logStartupInfo();
    void logShutdownInfo();
    
    void handleException(const std::exception& e);
    void handleCriticalError(const std::string& error);
    
    bool isRunning() const { return running_; }
    bool isInitialized() const { return initialized_; }
    
    static ApplicationServer& instance();

    void displayHelp();
    
private:
    std::unique_ptr<Poco::Net::HTTPServer> server_;
    std::unique_ptr<Poco::Net::ServerSocket> serverSocket_;
    
    std::atomic<bool> running_;
    std::atomic<bool> initialized_;
    std::atomic<bool> shutdownRequested_;
    
    int serverPort_;
    std::string serverHost_;
    int maxThreads_;
    int maxQueued_;
    int serverTimeout_;
    
    std::string configPath_;
    bool helpRequested_;
    bool versionRequested_;
    bool daemonMode_;
    
    std::thread serverThread_;
    std::mutex serverMutex_;
    std::condition_variable serverCondition_;
    
    static ApplicationServer* instance_;
};

} // namespace warehouse_backend