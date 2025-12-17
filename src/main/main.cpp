#include "ApplicationServer.hpp"
#include <Poco/Net/HTTPServer.h>
#include <Poco/Net/ServerSocket.h>
#include <iostream>
#include <cstdlib>

#ifdef POCO_NETSSL_FOUND
#include <Poco/Net/SSLManager.h>
#include <Poco/Net/Context.h>
#endif

int main(int argc, char** argv)
{
    try
    {
        warehouse_backend::ApplicationServer app;
        
        Poco::Net::initializeNetwork();
        
        #ifdef POCO_NETSSL_FOUND
        try 
        {
            
        }
        catch (const Poco::Exception& e)
        {
            std::cerr << "Warning: SSL initialization failed: " << e.displayText() << std::endl;
        }
        #endif
        
        int exitCode = app.run(argc, argv);
        
        #ifdef POCO_NETSSL_FOUND
        try 
        {
        }
        catch (const Poco::Exception& e)
        {
            std::cerr << "Warning: SSL cleanup failed: " << e.displayText() << std::endl;
        }
        #endif
        
        Poco::Net::uninitializeNetwork();
        
        return exitCode;
    }
    catch (const Poco::Exception& e)
    {
        std::cerr << "POCO Exception: " << e.displayText() << std::endl;
        return EXIT_FAILURE;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Standard Exception: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    catch (...)
    {
        std::cerr << "Unknown exception occurred" << std::endl;
        return EXIT_FAILURE;
    }
}
