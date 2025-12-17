#pragma once

#include <memory>
#include <string>
#include <vector>
#include <map>
#include <Poco/JSON/Object.h>
#include <Poco/JSON/Array.h>
#include "../database/repositories/OrderPaymentRepository.hpp"
#include "../database/repositories/CustomerOrderRepository.hpp"
#include "../database/models/OrderPayment.hpp"
#include "../database/models/CustomerOrder.hpp"
#include "../services/AuthService.hpp"

namespace warehouse_backend::services
{

struct PaymentStatistics
{
    int totalPayments;
    double totalAmount;
    double averagePayment;
    int pendingPayments;
    int paidPayments;
    int failedPayments;
    int refundedPayments;
    int overduePayments;
    
    Poco::JSON::Object toJson() const;
};

struct PaymentReportData
{
    std::string paymentDate;
    std::string paymentMethod;
    int totalPayments;
    double totalAmount;
    double paidAmount;
    double pendingAmount;
    double failedAmount;
    double refundedAmount;
    
    Poco::JSON::Object toJson() const;
};

struct PaymentMethodAnalysis
{
    std::string paymentMethod;
    int totalTransactions;
    double totalVolume;
    double avgTransactionValue;
    int successfulTransactions;
    int failedTransactions;
    int pendingTransactions;
    double successRate;
    
    Poco::JSON::Object toJson() const;
};

class OrderPaymentService
{
public:
    OrderPaymentService();
    ~OrderPaymentService() = default;
    
    std::unique_ptr<database::models::OrderPayment> getPaymentById(long long paymentId);
    std::vector<std::unique_ptr<database::models::OrderPayment>> getAllPayments();
    std::vector<std::unique_ptr<database::models::OrderPayment>> getPaginatedPayments(int page, int pageSize);
    long long createPayment(const database::models::OrderPayment& payment, long long userId);
    bool updatePayment(long long paymentId, const database::models::OrderPayment& payment, long long userId);
    bool deletePayment(long long paymentId, long long userId);
    
    bool markPaymentAsPaid(long long paymentId, const std::string& transactionId, long long userId);
    bool markPaymentAsFailed(long long paymentId, const std::string& transactionId, long long userId);
    bool markPaymentAsRefunded(long long paymentId, const std::string& transactionId, long long userId);
    bool markPaymentAsPending(long long paymentId, long long userId);
    bool updatePaymentStatus(long long paymentId, const std::string& newStatus, 
                           const std::string& transactionId, const std::string& paymentDate,
                           long long userId);
    
    std::vector<std::unique_ptr<database::models::OrderPayment>> searchPayments(
        const std::string& query, const std::vector<std::string>& fields);
    std::vector<std::unique_ptr<database::models::OrderPayment>> findPaymentsByField(
        const std::string& fieldName, const std::string& fieldValue);
    
    std::unique_ptr<database::models::OrderPayment> getPaymentByOrderId(long long orderId);
    std::vector<std::unique_ptr<database::models::OrderPayment>> getPaymentsByPaymentMethod(
        const std::string& paymentMethod);
    std::vector<std::unique_ptr<database::models::OrderPayment>> getPaymentsByStatus(
        const std::string& status);
    std::vector<std::unique_ptr<database::models::OrderPayment>> getPaymentsByDateRange(
        const std::string& startDate, const std::string& endDate);
    std::vector<std::unique_ptr<database::models::OrderPayment>> getPaymentsByAmountRange(
        double minAmount, double maxAmount);
    
    std::vector<std::unique_ptr<database::models::OrderPayment>> getPendingPayments();
    std::vector<std::unique_ptr<database::models::OrderPayment>> getPaidPayments();
    std::vector<std::unique_ptr<database::models::OrderPayment>> getFailedPayments();
    std::vector<std::unique_ptr<database::models::OrderPayment>> getRefundedPayments();
    std::vector<std::unique_ptr<database::models::OrderPayment>> getOverduePayments(int daysThreshold = 7);
    
    PaymentStatistics getPaymentStatistics();
    Poco::JSON::Array getPaymentReport(const std::string& startDate, const std::string& endDate);
    Poco::JSON::Array getPaymentMethodAnalysis();
    Poco::JSON::Array getCustomerPaymentHistory(long long orderId);
    
    bool validatePayment(const database::models::OrderPayment& payment);
    bool canProcessPayment(const database::models::OrderPayment& payment, std::string& errorMessage);
    bool processPayment(long long paymentId, long long userId);
    bool refundPayment(long long paymentId, const std::string& reason, long long userId);
    
    bool validatePaymentCreation(const database::models::OrderPayment& payment, std::vector<std::string>& errors);
    bool validatePaymentUpdate(const database::models::OrderPayment& payment, std::vector<std::string>& errors);
    
    bool canUserViewPayment(long long userId, database::models::UserRole userRole, long long paymentId);
    bool canUserCreatePayment(long long userId, database::models::UserRole userRole, 
                            const database::models::OrderPayment& payment);
    bool canUserUpdatePayment(long long userId, database::models::UserRole userRole, long long paymentId);
    bool canUserDeletePayment(long long userId, database::models::UserRole userRole, long long paymentId);
    bool canUserUpdatePaymentStatus(long long userId, database::models::UserRole userRole, long long paymentId);
    
    bool validateOrderForPayment(long long orderId, std::string& errorMessage);
    double calculateOrderRemainingBalance(long long orderId);
    bool isOrderFullyPaid(long long orderId);
    
    int countPayments();
    int countPaymentsByStatus(const std::string& status);
    int countPaymentsByMethod(const std::string& method);
    double getTotalPaymentsAmount(const std::string& startDate, const std::string& endDate);
    double getAveragePaymentAmount();
    bool transactionIdExists(const std::string& transactionId);
    bool hasOrderPayment(long long orderId);
    
    void logPaymentEvent(long long userId, const std::string& action, long long paymentId,
                        const std::string& ipAddress, const std::string& userAgent,
                        bool success, const std::string& details = "");
    
private:
    std::unique_ptr<database::repositories::OrderPaymentRepository> paymentRepository;
    std::unique_ptr<database::repositories::CustomerOrderRepository> orderRepository;
    std::unique_ptr<AuthService> authService;
    
    bool validatePaymentAmount(double amount, double orderTotal, std::string& errorMessage);
    bool validatePaymentMethod(const std::string& paymentMethod, std::string& errorMessage);
    bool validatePaymentStatus(const std::string& status, std::string& errorMessage);
    
    std::string generateTransactionId();
    void enrichPaymentWithOrderDetails(database::models::OrderPayment& payment);
    
    bool canRefundPayment(const database::models::OrderPayment& payment, std::string& errorMessage);
    bool canMarkAsPaid(const database::models::OrderPayment& payment, std::string& errorMessage);
    bool canMarkAsFailed(const database::models::OrderPayment& payment, std::string& errorMessage);
    
    void notifyPaymentCreated(const database::models::OrderPayment& payment);
    void notifyPaymentStatusChanged(const database::models::OrderPayment& payment, 
                                  const std::string& oldStatus, const std::string& newStatus);
    void notifyPaymentFailed(const database::models::OrderPayment& payment, const std::string& reason);
    void notifyPaymentRefunded(const database::models::OrderPayment& payment, const std::string& reason);
    
    void createAuditRecord(long long userId, const std::string& action, 
                          long long paymentId, const std::string& details);
};

} // namespace services
