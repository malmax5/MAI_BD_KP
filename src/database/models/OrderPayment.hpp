#pragma once

#include <string>
#include <Poco/JSON/Object.h>

namespace warehouse_backend::database::models
{

class OrderPayment
{
public:
    long long id;
    long long orderId;
    std::string paymentMethod;
    std::string paymentStatus;
    double amount;
    std::string transactionId;
    std::string paymentDate;
    std::string createdAt;

    std::string orderNumber;
    std::string customerName;
    double orderTotal;

    OrderPayment();
    explicit OrderPayment(const Poco::JSON::Object& json);

    Poco::JSON::Object toJson() const;
    static OrderPayment fromJson(const Poco::JSON::Object& json);

    bool validate() const;
    
    bool isPaid() const;
    bool isPending() const;
    bool isFailed() const;
    bool isRefunded() const;
    bool isFullPayment() const;
    double getRemainingAmount(double orderTotal) const;
    
    static constexpr int MAX_PAYMENT_METHOD = 50;
    static constexpr int MAX_PAYMENT_STATUS = 20;
    static constexpr int MAX_TRANSACTION_ID = 100;
    static constexpr double MIN_AMOUNT = 0.0;
};

} // namespace database::models