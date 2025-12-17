#pragma once

#include "BaseController.hpp"
#include "../services/OrderService.hpp"
#include "../services/AuthService.hpp"
#include "../services/OrderPaymentService.hpp"

#include <memory>

namespace warehouse_backend::controllers
{

class OrderPaymentController : public BaseController
{
public:
    OrderPaymentController();
    virtual ~OrderPaymentController() = default;
    
    void handleRequest(Poco::Net::HTTPServerRequest& request, 
                      Poco::Net::HTTPServerResponse& response) override;
    
    void handleGetPayments(Poco::Net::HTTPServerRequest& request, 
                          Poco::Net::HTTPServerResponse& response);
    
    void handleGetPaymentById(Poco::Net::HTTPServerRequest& request, 
                             Poco::Net::HTTPServerResponse& response);
    
    void handleGetPaymentByOrderId(Poco::Net::HTTPServerRequest& request, 
                                  Poco::Net::HTTPServerResponse& response);
    
    void handleCreatePayment(Poco::Net::HTTPServerRequest& request, 
                           Poco::Net::HTTPServerResponse& response);
    
    void handleUpdatePayment(Poco::Net::HTTPServerRequest& request, 
                           Poco::Net::HTTPServerResponse& response);
    
    void handleDeletePayment(Poco::Net::HTTPServerRequest& request, 
                           Poco::Net::HTTPServerResponse& response);
    
    void handleUpdatePaymentStatus(Poco::Net::HTTPServerRequest& request, 
                                 Poco::Net::HTTPServerResponse& response);
    
    void handleSearchPayments(Poco::Net::HTTPServerRequest& request, 
                            Poco::Net::HTTPServerResponse& response);
    
    void handleGetPaymentStatistics(Poco::Net::HTTPServerRequest& request, 
                                  Poco::Net::HTTPServerResponse& response);
    
    void handleGetPaymentReport(Poco::Net::HTTPServerRequest& request, 
                              Poco::Net::HTTPServerResponse& response);
    
    void handleGetPendingPayments(Poco::Net::HTTPServerRequest& request, 
                                Poco::Net::HTTPServerResponse& response);
    
    void handleGetPaidPayments(Poco::Net::HTTPServerRequest& request, 
                              Poco::Net::HTTPServerResponse& response);
    
    void handleGetFailedPayments(Poco::Net::HTTPServerRequest& request, 
                                Poco::Net::HTTPServerResponse& response);
    
    void handleGetRefundedPayments(Poco::Net::HTTPServerRequest& request, 
                                  Poco::Net::HTTPServerResponse& response);
    
    void handleGetOverduePayments(Poco::Net::HTTPServerRequest& request, 
                                 Poco::Net::HTTPServerResponse& response);
    
    void handleMarkAsPaid(Poco::Net::HTTPServerRequest& request, 
                         Poco::Net::HTTPServerResponse& response);
    
    void handleMarkAsFailed(Poco::Net::HTTPServerRequest& request, 
                           Poco::Net::HTTPServerResponse& response);
    
    void handleMarkAsRefunded(Poco::Net::HTTPServerRequest& request, 
                             Poco::Net::HTTPServerResponse& response);
    
    void handleMarkAsPending(Poco::Net::HTTPServerRequest& request, 
                            Poco::Net::HTTPServerResponse& response);
    
    void handleProcessPayment(Poco::Net::HTTPServerRequest& request, 
                              Poco::Net::HTTPServerResponse& response);
    
    void handleRefundPayment(Poco::Net::HTTPServerRequest& request, 
                             Poco::Net::HTTPServerResponse& response);
    
    bool validateRequest(Poco::Net::HTTPServerRequest& request, 
                        Poco::Net::HTTPServerResponse& response,
                        std::string& errorMessage) override;
    
    bool authorizeRequest(Poco::Net::HTTPServerRequest& request, 
                         Poco::Net::HTTPServerResponse& response,
                         std::string& errorMessage) override;
    
    bool validatePaymentAccess(Poco::Net::HTTPServerRequest& request, 
                              long long paymentId,
                              std::string& errorMessage);
    
    long long getCurrentUserId(Poco::Net::HTTPServerRequest& request);
    database::models::UserRole getCurrentUserRole(Poco::Net::HTTPServerRequest& request);
    
private:
    std::unique_ptr<services::OrderService> orderService;
    std::unique_ptr<services::AuthService> authService;
    std::unique_ptr<services::OrderPaymentService> orderPaymentService;
    
    bool validateCreatePaymentData(const Poco::JSON::Object::Ptr& json, 
                                  std::vector<std::string>& errors);
    
    bool validateUpdatePaymentData(const Poco::JSON::Object::Ptr& json, 
                                  std::vector<std::string>& errors);
    
    bool validateUpdatePaymentStatusData(const Poco::JSON::Object::Ptr& json, 
                                        std::vector<std::string>& errors);
    
    bool validateSearchParameters(const std::map<std::string, std::string>& filters,
                                 std::vector<std::string>& errors);
    
    void logPaymentEvent(long long userId, 
                        const std::string& action,
                        long long paymentId,
                        const std::string& ipAddress,
                        const std::string& userAgent,
                        bool success,
                        const std::string& details = "");
    
    std::unique_ptr<database::models::OrderPayment> extractPaymentFromJson(const Poco::JSON::Object::Ptr& json);
    
    bool canCreatePayment(Poco::Net::HTTPServerRequest& request, 
                         const database::models::OrderPayment& paymentData,
                         std::string& errorMessage);
    
    bool canUpdatePayment(Poco::Net::HTTPServerRequest& request, 
                         long long paymentId,
                         const database::models::OrderPayment& paymentData,
                         std::string& errorMessage);
    
    bool canDeletePayment(Poco::Net::HTTPServerRequest& request, 
                         long long paymentId,
                         std::string& errorMessage);
    
    bool canUpdatePaymentStatus(Poco::Net::HTTPServerRequest& request, 
                               long long paymentId,
                               const std::string& newStatus,
                               std::string& errorMessage);
    
    bool canViewPayment(Poco::Net::HTTPServerRequest& request, 
                       long long paymentId,
                       std::string& errorMessage);
    
    Poco::JSON::Object buildPaginationResponse(int page, int pageSize, int totalItems, 
                                              const Poco::JSON::Array& data);
};

} // namespace controllers
