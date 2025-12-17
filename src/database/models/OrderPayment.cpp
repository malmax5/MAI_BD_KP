#include "OrderPayment.hpp"
#include "../../utils/JsonUtils.hpp"
#include "../../utils/Validator.hpp"
#include "../../utils/DateUtils.hpp"
#include <Poco/JSON/Parser.h>
#include <Poco/JSON/Stringifier.h>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace warehouse_backend::database::models
{

using namespace warehouse_backend::utils;

OrderPayment::OrderPayment()
    : id(0),
      orderId(0),
      amount(0.0),
      orderTotal(0.0)
{
    
}

OrderPayment::OrderPayment(const Poco::JSON::Object& json)
{
    orderId = static_cast<Poco::Int64>(JsonUtils::getInt(json, "order_id", 0));
    paymentMethod = JsonUtils::getString(json, "payment_method", "");
    paymentStatus = JsonUtils::getString(json, "payment_status", "pending");
    amount = JsonUtils::getDouble(json, "amount", 0.0);
    createdAt = JsonUtils::getString(json, "created_at", "");
    orderTotal = JsonUtils::getDouble(json, "order_total", 0.0);

    orderNumber = JsonUtils::getString(json, "order_number", "");
    customerName = JsonUtils::getString(json, "customer_name", "");

    if (json.has("transaction_id") && !json.isNull("transaction_id"))
        transactionId = JsonUtils::getString(json, "transaction_id", "");

    if (json.has("payment_date") && !json.isNull("payment_date"))
        paymentDate = JsonUtils::getString(json, "payment_date", "");

    if (json.has("id") && !json.isNull("id"))
        id = static_cast<Poco::Int64>(JsonUtils::getInt(json, "id", 0));
    else
        id = 0;
}

Poco::JSON::Object OrderPayment::toJson() const
{
    Poco::JSON::Object json;
    
    if (id > 0)
    {
        json.set("id", id);
    }
    
    json.set("order_id", orderId);
    json.set("payment_method", paymentMethod);
    json.set("payment_status", paymentStatus);
    json.set("amount", amount);
    
    if (!transactionId.isNull())
    {
        json.set("transaction_id", transactionId.value());
    }
    
    if (!paymentDate.isNull())
    {
        json.set("payment_date", paymentDate.value());
    }
    
    json.set("created_at", createdAt);
    
    if (!orderNumber.empty())
    {
        json.set("order_number", orderNumber);
    }
    
    if (!customerName.empty())
    {
        json.set("customer_name", customerName);
    }
    
    json.set("order_total", orderTotal);
    
    json.set("is_paid", isPaid());
    json.set("is_pending", isPending());
    json.set("is_failed", isFailed());
    json.set("is_refunded", isRefunded());
    json.set("is_full_payment", isFullPayment());
    json.set("remaining_amount", getRemainingAmount(orderTotal));
    
    return json;
}

OrderPayment OrderPayment::fromJson(const Poco::JSON::Object& json)
{
    return OrderPayment(json);
}

bool OrderPayment::validate() const
{
    if (orderId <= 0)
    {
        return false;
    }
    
    if (paymentMethod.empty() || paymentMethod.length() > MAX_PAYMENT_METHOD)
    {
        return false;
    }
    
    if (paymentStatus.length() > MAX_PAYMENT_STATUS)
    {
        return false;
    }
    
    if (amount < MIN_AMOUNT)
    {
        return false;
    }
    
    if (!transactionId.isNull() && transactionId.value().length() > MAX_TRANSACTION_ID)
    {
        return false;
    }
    
    if (!paymentDate.isNull() && !Validator::isValidDateTime(paymentDate.value()))
    {
        return false;
    }
    
    if (!createdAt.empty() && !Validator::isValidDateTime(createdAt))
    {
        return false;
    }
    
    return true;
}

bool OrderPayment::isPaid() const
{
    return paymentStatus == "paid" || paymentStatus == "completed" || paymentStatus == "succeeded";
}

bool OrderPayment::isPending() const
{
    return paymentStatus == "pending" || paymentStatus == "processing" || paymentStatus == "authorized";
}

bool OrderPayment::isFailed() const
{
    return paymentStatus == "failed" || paymentStatus == "declined" || paymentStatus == "cancelled";
}

bool OrderPayment::isRefunded() const
{
    return paymentStatus == "refunded" || paymentStatus == "reversed";
}

bool OrderPayment::isFullPayment() const
{
    return isPaid() && amount >= orderTotal;
}

double OrderPayment::getRemainingAmount(double orderTotal) const
{
    if (!isPaid())
    {
        return orderTotal;
    }
    
    double remaining = orderTotal - amount;
    return remaining > 0.0 ? remaining : 0.0;
}

} // namespace database::models
