#include <iostream>

#include "config/version.h"

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  " << PROJECT_NAME << std::endl;
    std::cout << "  Version: " << PROJECT_VERSION << std::endl;
    std::cout << "  " << PROJECT_DESCRIPTION << std::endl;
    std::cout << "  Build: " << BUILD_TYPE << std::endl;
    std::cout << "  Date: " << BUILD_DATE << " " << BUILD_TIME << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Hello World! Backend is running." << std::endl;
    std::cout << "Test successful!" << std::endl;
    return 0;
}