#pragma once

#include "BaseRepository.hpp"
#include "../models/OrderPayment.hpp"
#include <memory>
#include <string>
#include <vector>
#include <Poco/JSON/Object.h>
#include <Poco/JSON/Array.h>

namespace warehouse_backend::database::repositories
{

class OrderPaymentRepository : public BaseRepository<models::OrderPayment>
{
public:
    OrderPaymentRepository();
    ~OrderPaymentRepository() override = default;
    
    std::unique_ptr<models::OrderPayment> findById(long long id) override;
    std::vector<std::unique_ptr<models::OrderPayment>> findAll() override;
    std::vector<std::unique_ptr<models::OrderPayment>> findPaginated(int page, int pageSize) override;
    long long create(const models::OrderPayment& payment) override;
    bool update(long long id, const models::OrderPayment& payment) override;
    bool remove(long long id) override;
    bool softDelete(long long id) override;
    int count() override;
    
    Poco::JSON::Array findAllAsJson() override;
    Poco::JSON::Object findByIdAsJson(long long id) override;
    
    std::vector<std::unique_ptr<models::OrderPayment>> findByField(const std::string& fieldName, 
                                                                   const std::string& fieldValue) override;
    std::vector<std::unique_ptr<models::OrderPayment>> search(const std::string& query, 
                                                              const std::vector<std::string>& fields) override;
    
    std::unique_ptr<models::OrderPayment> findByOrderId(long long orderId);
    std::vector<std::unique_ptr<models::OrderPayment>> findByPaymentMethod(const std::string& paymentMethod);
    std::vector<std::unique_ptr<models::OrderPayment>> findByPaymentStatus(const std::string& paymentStatus);
    std::vector<std::unique_ptr<models::OrderPayment>> findByTransactionId(const std::string& transactionId);
    std::vector<std::unique_ptr<models::OrderPayment>> findByDateRange(const std::string& startDate, const std::string& endDate);
    std::vector<std::unique_ptr<models::OrderPayment>> findByAmountRange(double minAmount, double maxAmount);
    
    std::vector<std::unique_ptr<models::OrderPayment>> findPendingPayments();
    std::vector<std::unique_ptr<models::OrderPayment>> findPaidPayments();
    std::vector<std::unique_ptr<models::OrderPayment>> findFailedPayments();
    std::vector<std::unique_ptr<models::OrderPayment>> findRefundedPayments();
    std::vector<std::unique_ptr<models::OrderPayment>> findOverduePayments(int daysThreshold = 7);
    
    bool updatePaymentStatus(long long id, const std::string& newStatus);
    bool updateTransactionInfo(long long id, const std::string& transactionId, const std::string& paymentDate);
    bool updateAmount(long long id, double newAmount);
    bool updatePaymentMethod(long long id, const std::string& newMethod);
    
    bool markAsPaid(long long id, const std::string& transactionId = "");
    bool markAsFailed(long long id, const std::string& transactionId = "");
    bool markAsRefunded(long long id, const std::string& transactionId = "");
    bool markAsPending(long long id);
    
    int countByPaymentMethod(const std::string& paymentMethod);
    int countByPaymentStatus(const std::string& paymentStatus);
    int countByOrder(long long orderId);
    
    double getTotalPaymentsAmount(const std::string& startDate, const std::string& endDate);
    double getAveragePaymentAmount();
    double getTotalRevenueByPaymentMethod(const std::string& paymentMethod, const std::string& startDate, const std::string& endDate);
    
    Poco::JSON::Array getPaymentStatistics();
    Poco::JSON::Array getPaymentReport(const std::string& startDate, const std::string& endDate);
    Poco::JSON::Array getPaymentMethodAnalysis();
    Poco::JSON::Array getCustomerPaymentHistory(long long customerOrderId);
    
    bool transactionIdExists(const std::string& transactionId);
    bool hasOrderPayment(long long orderId);
    
    std::vector<std::pair<long long, std::string>> getOrderPaymentStatuses();
    
private:
    models::OrderPayment mapRowToPayment(Poco::Data::Row& row) const;
    void enrichOrderPaymentWithOrderDetails(models::OrderPayment& payment);
    
    static const std::string TABLE_NAME;
    static const std::vector<std::string> SEARCH_FIELDS;
};

} // namespace database::repositories
