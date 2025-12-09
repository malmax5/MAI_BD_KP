#include "OrderPaymentRepository.hpp"
#include "../models/OrderPayment.hpp"
#include "../models/CustomerOrder.hpp"
#include <Poco/Data/Session.h>
#include <Poco/Data/RecordSet.h>
#include <Poco/Data/Statement.h>
#include <Poco/Data/Row.h>
#include <Poco/DateTime.h>
#include <Poco/DateTimeFormat.h>
#include <Poco/DateTimeFormatter.h>
#include <Poco/DateTimeParser.h>
#include <Poco/Dynamic/Var.h>
#include "../../utils/DateUtils.hpp"
#include "../../utils/JsonUtils.hpp"

using namespace Poco::Data;
using namespace Poco::Data::Keywords;
using namespace Poco;

using namespace warehouse_backend::utils;

namespace warehouse_backend::database::repositories
{

const std::string OrderPaymentRepository::TABLE_NAME = "order_payments";
const std::vector<std::string> OrderPaymentRepository::SEARCH_FIELDS = {
    "payment_method", "payment_status", "transaction_id"
};

OrderPaymentRepository::OrderPaymentRepository() : BaseRepository<models::OrderPayment>()
{
}

std::unique_ptr<models::OrderPayment> OrderPaymentRepository::findById(long long id)
{
    auto connection = acquireConnection();
    
    try
    {
        Poco::Int64 pocoId = static_cast<Poco::Int64>(id);
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT op.id, op.order_id, op.payment_method, op.payment_status, "
                  "op.amount, op.transaction_id, op.payment_date, op.created_at, "
                  "co.order_number, co.customer_name, co.total_amount as order_total "
                  "FROM " << TABLE_NAME << " op "
                  "JOIN customer_orders co ON co.id = op.order_id "
                  "WHERE op.id = $1",
            Poco::Data::Keywords::use(pocoId),
            now;
        
        Poco::Data::RecordSet rs(select);
        
        if (rs.rowCount() > 0)
        {
            auto payment = std::make_unique<models::OrderPayment>();
            payment->id = rs.value("id", 0).convert<long long>();
            payment->orderId = rs.value("order_id", 0).convert<long long>();
            payment->paymentMethod = rs.value("payment_method").convert<std::string>();
            payment->paymentStatus = rs.value("payment_status").convert<std::string>();
            payment->amount = rs.value("amount", 0.0).convert<double>();
            payment->transactionId = rs.value("transaction_id").isEmpty() ? "" : rs.value("transaction_id").convert<std::string>();
            payment->paymentDate = rs.value("payment_date").isEmpty() ? "" : rs.value("payment_date").convert<std::string>();
            payment->createdAt = rs.value("created_at").convert<std::string>();
            payment->orderNumber = rs.value("order_number").convert<std::string>();
            payment->customerName = rs.value("customer_name").convert<std::string>();
            payment->orderTotal = rs.value("order_total", 0.0).convert<double>();
            
            return payment;
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findById: " + e.displayText());
    }
    
    return nullptr;
}

std::vector<std::unique_ptr<models::OrderPayment>> OrderPaymentRepository::findAll()
{
    std::vector<std::unique_ptr<models::OrderPayment>> payments;
    auto connection = acquireConnection();
    
    try
    {
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT op.id, op.order_id, op.payment_method, op.payment_status, "
                  "op.amount, op.transaction_id, op.payment_date, op.created_at, "
                  "co.order_number, co.customer_name, co.total_amount as order_total "
                  "FROM " << TABLE_NAME << " op "
                  "JOIN customer_orders co ON co.id = op.order_id "
                  "ORDER BY op.created_at DESC, op.id DESC",
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            auto payment = std::make_unique<models::OrderPayment>();
            payment->id = rs.value("id", 0).convert<long long>();
            payment->orderId = rs.value("order_id", 0).convert<long long>();
            payment->paymentMethod = rs.value("payment_method").convert<std::string>();
            payment->paymentStatus = rs.value("payment_status").convert<std::string>();
            payment->amount = rs.value("amount", 0.0).convert<double>();
            payment->transactionId = rs.value("transaction_id").isEmpty() ? "" : rs.value("transaction_id").convert<std::string>();
            payment->paymentDate = rs.value("payment_date").isEmpty() ? "" : rs.value("payment_date").convert<std::string>();
            payment->createdAt = rs.value("created_at").convert<std::string>();
            payment->orderNumber = rs.value("order_number").convert<std::string>();
            payment->customerName = rs.value("customer_name").convert<std::string>();
            payment->orderTotal = rs.value("order_total", 0.0).convert<double>();
            
            payments.push_back(std::move(payment));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findAll: " + e.displayText());
    }
    
    return payments;
}

std::vector<std::unique_ptr<models::OrderPayment>> OrderPaymentRepository::findPaginated(int page, int pageSize)
{
    std::vector<std::unique_ptr<models::OrderPayment>> payments;
    auto connection = acquireConnection();
    
    if (page < 1) page = 1;
    int offset = (page - 1) * pageSize;
    
    try
    {
        int usePageSize = pageSize;
        int useOffset = offset;
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT op.id, op.order_id, op.payment_method, op.payment_status, "
                  "op.amount, op.transaction_id, op.payment_date, op.created_at, "
                  "co.order_number, co.customer_name, co.total_amount as order_total "
                  "FROM " << TABLE_NAME << " op "
                  "JOIN customer_orders co ON co.id = op.order_id "
                  "ORDER BY op.created_at DESC, op.id DESC LIMIT $1 OFFSET $2",
            Poco::Data::Keywords::use(usePageSize),
            Poco::Data::Keywords::use(useOffset),
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            auto payment = std::make_unique<models::OrderPayment>();
            payment->id = rs.value("id", 0).convert<long long>();
            payment->orderId = rs.value("order_id", 0).convert<long long>();
            payment->paymentMethod = rs.value("payment_method").convert<std::string>();
            payment->paymentStatus = rs.value("payment_status").convert<std::string>();
            payment->amount = rs.value("amount", 0.0).convert<double>();
            payment->transactionId = rs.value("transaction_id").isEmpty() ? "" : rs.value("transaction_id").convert<std::string>();
            payment->paymentDate = rs.value("payment_date").isEmpty() ? "" : rs.value("payment_date").convert<std::string>();
            payment->createdAt = rs.value("created_at").convert<std::string>();
            payment->orderNumber = rs.value("order_number").convert<std::string>();
            payment->customerName = rs.value("customer_name").convert<std::string>();
            payment->orderTotal = rs.value("order_total", 0.0).convert<double>();
            
            payments.push_back(std::move(payment));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findPaginated: " + e.displayText());
    }
    
    return payments;
}

long long OrderPaymentRepository::create(const models::OrderPayment& payment)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        models::OrderPayment paymentCopy = payment;
        
        Poco::Data::Statement insert(connection->getSession());
        Poco::Int64 newId = 0;
        
        insert << "INSERT INTO " << TABLE_NAME << " "
                  "(order_id, payment_method, payment_status, amount, "
                  "transaction_id, payment_date) "
                  "VALUES ($1, $2, $3, $4, $5, $6) "
                  "RETURNING id",
            Poco::Data::Keywords::use(paymentCopy.orderId),
            Poco::Data::Keywords::use(paymentCopy.paymentMethod),
            Poco::Data::Keywords::use(paymentCopy.paymentStatus),
            Poco::Data::Keywords::use(paymentCopy.amount),
            Poco::Data::Keywords::use(paymentCopy.transactionId),
            Poco::Data::Keywords::use(paymentCopy.paymentDate),
            Poco::Data::Keywords::into(newId),
            now;
        
        commitTransaction(*connection);
        return static_cast<long long>(newId);
    }
    catch (const Poco::Exception& e)
    {
        rollbackTransaction(*connection);
        throw std::runtime_error("Database error in create: " + e.displayText());
    }
}

bool OrderPaymentRepository::update(long long id, const models::OrderPayment& payment)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        models::OrderPayment paymentCopy = payment;
        
        Poco::Int64 idCopy = id;
        
        Poco::Data::Statement update(connection->getSession());
        update << "UPDATE " << TABLE_NAME << " SET "
                  "payment_method = $1, payment_status = $2, amount = $3, "
                  "transaction_id = $4, payment_date = $5 "
                  "WHERE id = $6",
            Poco::Data::Keywords::use(paymentCopy.paymentMethod),
            Poco::Data::Keywords::use(paymentCopy.paymentStatus),
            Poco::Data::Keywords::use(paymentCopy.amount),
            Poco::Data::Keywords::use(paymentCopy.transactionId),
            Poco::Data::Keywords::use(paymentCopy.paymentDate),
            Poco::Data::Keywords::use(idCopy);
        
        int rowsAffected = update.execute();
        
        commitTransaction(*connection);
        return rowsAffected > 0;
    }
    catch (const Poco::Exception& e)
    {
        rollbackTransaction(*connection);
        throw std::runtime_error("Database error in update: " + e.displayText());
    }
}

bool OrderPaymentRepository::remove(long long id)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        long long idCopy = id;
        
        Poco::Data::Statement del(connection->getSession());
        del << "DELETE FROM " << TABLE_NAME << " WHERE id = $1",
            Poco::Data::Keywords::use(idCopy);
        
        int rowsAffected = del.execute();
        
        commitTransaction(*connection);
        return rowsAffected > 0;
    }
    catch (const Poco::Exception& e)
    {
        rollbackTransaction(*connection);
        throw std::runtime_error("Database error in remove: " + e.displayText());
    }
}

bool OrderPaymentRepository::softDelete(long long id)
{
    auto payment = findById(id);
    if (!payment)
    {
        return false;
    }
    
    return updatePaymentStatus(id, "cancelled");
}

int OrderPaymentRepository::count()
{
    auto connection = acquireConnection();
    
    try
    {
        Poco::Data::Statement countStmt(connection->getSession());
        countStmt << "SELECT COUNT(*) FROM " << TABLE_NAME,
            now;
        
        Poco::Data::RecordSet rs(countStmt);
        if (rs.rowCount() > 0)
        {
            return rs.value(0, 0).convert<int>();
        }
        
        return 0;
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in count: " + e.displayText());
    }
}

Poco::JSON::Array OrderPaymentRepository::findAllAsJson()
{
    auto payments = findAll();
    Poco::JSON::Array jsonArray;
    
    for (const auto& payment : payments)
    {
        if (payment)
        {
            jsonArray.add(payment->toJson());
        }
    }
    
    return jsonArray;
}

Poco::JSON::Object OrderPaymentRepository::findByIdAsJson(long long id)
{
    auto payment = findById(id);
    if (payment)
    {
        return payment->toJson();
    }
    
    return Poco::JSON::Object();
}

std::vector<std::unique_ptr<models::OrderPayment>> OrderPaymentRepository::findByField(
    const std::string& fieldName, const std::string& fieldValue)
{
    std::vector<std::unique_ptr<models::OrderPayment>> payments;
    auto connection = acquireConnection();
    
    try
    {
        std::string sql = "SELECT op.id, op.order_id, op.payment_method, op.payment_status, "
                          "op.amount, op.transaction_id, op.payment_date, op.created_at, "
                          "co.order_number, co.customer_name, co.total_amount as order_total "
                          "FROM " + TABLE_NAME + " op "
                          "JOIN customer_orders co ON co.id = op.order_id "
                          "WHERE op." + fieldName + " = $1 "
                          "ORDER BY op.created_at DESC, op.id DESC";
        
        std::string fieldValueCopy = fieldValue;
        Poco::Data::Statement select(connection->getSession());
        select << sql,
            Poco::Data::Keywords::use(fieldValueCopy),
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            auto payment = std::make_unique<models::OrderPayment>();
            payment->id = rs.value("id", 0).convert<long long>();
            payment->orderId = rs.value("order_id", 0).convert<long long>();
            payment->paymentMethod = rs.value("payment_method").convert<std::string>();
            payment->paymentStatus = rs.value("payment_status").convert<std::string>();
            payment->amount = rs.value("amount", 0.0).convert<double>();
            payment->transactionId = rs.value("transaction_id").isEmpty() ? "" : rs.value("transaction_id").convert<std::string>();
            payment->paymentDate = rs.value("payment_date").isEmpty() ? "" : rs.value("payment_date").convert<std::string>();
            payment->createdAt = rs.value("created_at").convert<std::string>();
            payment->orderNumber = rs.value("order_number").convert<std::string>();
            payment->customerName = rs.value("customer_name").convert<std::string>();
            payment->orderTotal = rs.value("order_total", 0.0).convert<double>();
            
            payments.push_back(std::move(payment));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findByField: " + e.displayText());
    }
    
    return payments;
}

std::vector<std::unique_ptr<models::OrderPayment>> OrderPaymentRepository::search(
    const std::string& query, const std::vector<std::string>& fields)
{
    std::vector<std::unique_ptr<models::OrderPayment>> payments;
    auto connection = acquireConnection();
    
    try
    {
        std::string searchCondition = buildSearchQuery(query, SEARCH_FIELDS);
        
        std::string sql = "SELECT op.id, op.order_id, op.payment_method, op.payment_status, "
                          "op.amount, op.transaction_id, op.payment_date, op.created_at, "
                          "co.order_number, co.customer_name, co.total_amount as order_total "
                          "FROM " + TABLE_NAME + " op "
                          "JOIN customer_orders co ON co.id = op.order_id "
                          "WHERE " + searchCondition + " "
                          "ORDER BY op.created_at DESC, op.id DESC";
        
        Poco::Data::Statement select(connection->getSession());
        select << sql,
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            auto payment = std::make_unique<models::OrderPayment>();
            payment->id = rs.value("id", 0).convert<long long>();
            payment->orderId = rs.value("order_id", 0).convert<long long>();
            payment->paymentMethod = rs.value("payment_method").convert<std::string>();
            payment->paymentStatus = rs.value("payment_status").convert<std::string>();
            payment->amount = rs.value("amount", 0.0).convert<double>();
            payment->transactionId = rs.value("transaction_id").isEmpty() ? "" : rs.value("transaction_id").convert<std::string>();
            payment->paymentDate = rs.value("payment_date").isEmpty() ? "" : rs.value("payment_date").convert<std::string>();
            payment->createdAt = rs.value("created_at").convert<std::string>();
            payment->orderNumber = rs.value("order_number").convert<std::string>();
            payment->customerName = rs.value("customer_name").convert<std::string>();
            payment->orderTotal = rs.value("order_total", 0.0).convert<double>();
            
            payments.push_back(std::move(payment));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in search: " + e.displayText());
    }
    
    return payments;
}

std::unique_ptr<models::OrderPayment> OrderPaymentRepository::findByOrderId(long long orderId)
{
    auto connection = acquireConnection();
    
    try
    {
        Poco::Int64 orderIdCopy = orderId;
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT op.id, op.order_id, op.payment_method, op.payment_status, "
                  "op.amount, op.transaction_id, op.payment_date, op.created_at, "
                  "co.order_number, co.customer_name, co.total_amount as order_total "
                  "FROM " << TABLE_NAME << " op "
                  "JOIN customer_orders co ON co.id = op.order_id "
                  "WHERE op.order_id = $1",
            Poco::Data::Keywords::use(orderIdCopy),
            now;
        
        Poco::Data::RecordSet rs(select);
        
        if (rs.rowCount() > 0)
        {
            auto payment = std::make_unique<models::OrderPayment>();
            payment->id = rs.value("id", 0).convert<long long>();
            payment->orderId = rs.value("order_id", 0).convert<long long>();
            payment->paymentMethod = rs.value("payment_method").convert<std::string>();
            payment->paymentStatus = rs.value("payment_status").convert<std::string>();
            payment->amount = rs.value("amount", 0.0).convert<double>();
            payment->transactionId = rs.value("transaction_id").isEmpty() ? "" : rs.value("transaction_id").convert<std::string>();
            payment->paymentDate = rs.value("payment_date").isEmpty() ? "" : rs.value("payment_date").convert<std::string>();
            payment->createdAt = rs.value("created_at").convert<std::string>();
            payment->orderNumber = rs.value("order_number").convert<std::string>();
            payment->customerName = rs.value("customer_name").convert<std::string>();
            payment->orderTotal = rs.value("order_total", 0.0).convert<double>();
            
            return payment;
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findByOrderId: " + e.displayText());
    }
    
    return nullptr;
}

std::vector<std::unique_ptr<models::OrderPayment>> OrderPaymentRepository::findByPaymentMethod(const std::string& paymentMethod)
{
    return findByField("payment_method", paymentMethod);
}

std::vector<std::unique_ptr<models::OrderPayment>> OrderPaymentRepository::findByPaymentStatus(const std::string& paymentStatus)
{
    return findByField("payment_status", paymentStatus);
}

std::vector<std::unique_ptr<models::OrderPayment>> OrderPaymentRepository::findByTransactionId(const std::string& transactionId)
{
    auto connection = acquireConnection();
    
    try
    {
        std::string transactionIdCopy = transactionId;
        
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT op.id, op.order_id, op.payment_method, op.payment_status, "
                  "op.amount, op.transaction_id, op.payment_date, op.created_at, "
                  "co.order_number, co.customer_name, co.total_amount as order_total "
                  "FROM " << TABLE_NAME << " op "
                  "JOIN customer_orders co ON co.id = op.order_id "
                  "WHERE op.transaction_id = $1 "
                  "ORDER BY op.created_at DESC, op.id DESC",
            Poco::Data::Keywords::use(transactionIdCopy),
            now;
        
        Poco::Data::RecordSet rs(select);
        
        std::vector<std::unique_ptr<models::OrderPayment>> payments;
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            auto payment = std::make_unique<models::OrderPayment>();
            payment->id = rs.value("id", 0).convert<long long>();
            payment->orderId = rs.value("order_id", 0).convert<long long>();
            payment->paymentMethod = rs.value("payment_method").convert<std::string>();
            payment->paymentStatus = rs.value("payment_status").convert<std::string>();
            payment->amount = rs.value("amount", 0.0).convert<double>();
            payment->transactionId = transactionId;
            payment->paymentDate = rs.value("payment_date").isEmpty() ? "" : rs.value("payment_date").convert<std::string>();
            payment->createdAt = rs.value("created_at").convert<std::string>();
            payment->orderNumber = rs.value("order_number").convert<std::string>();
            payment->customerName = rs.value("customer_name").convert<std::string>();
            payment->orderTotal = rs.value("order_total", 0.0).convert<double>();
            
            payments.push_back(std::move(payment));
        }
        
        return payments;
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findByTransactionId: " + e.displayText());
    }
}

std::vector<std::unique_ptr<models::OrderPayment>> OrderPaymentRepository::findByDateRange(const std::string& startDate, const std::string& endDate)
{
    std::vector<std::unique_ptr<models::OrderPayment>> payments;
    auto connection = acquireConnection();
    
    try
    {
        std::string startDateCopy = startDate;
        std::string endDateCopy = endDate;
        
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT op.id, op.order_id, op.payment_method, op.payment_status, "
                  "op.amount, op.transaction_id, op.payment_date, op.created_at, "
                  "co.order_number, co.customer_name, co.total_amount as order_total "
                  "FROM " << TABLE_NAME << " op "
                  "JOIN customer_orders co ON co.id = op.order_id "
                  "WHERE op.created_at >= $1 AND op.created_at <= $2 "
                  "ORDER BY op.created_at DESC, op.id DESC",
            Poco::Data::Keywords::use(startDateCopy),
            Poco::Data::Keywords::use(endDateCopy),
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            auto payment = std::make_unique<models::OrderPayment>();
            payment->id = rs.value("id", 0).convert<long long>();
            payment->orderId = rs.value("order_id", 0).convert<long long>();
            payment->paymentMethod = rs.value("payment_method").convert<std::string>();
            payment->paymentStatus = rs.value("payment_status").convert<std::string>();
            payment->amount = rs.value("amount", 0.0).convert<double>();
            payment->transactionId = rs.value("transaction_id").isEmpty() ? "" : rs.value("transaction_id").convert<std::string>();
            payment->paymentDate = rs.value("payment_date").isEmpty() ? "" : rs.value("payment_date").convert<std::string>();
            payment->createdAt = rs.value("created_at").convert<std::string>();
            payment->orderNumber = rs.value("order_number").convert<std::string>();
            payment->customerName = rs.value("customer_name").convert<std::string>();
            payment->orderTotal = rs.value("order_total", 0.0).convert<double>();
            
            payments.push_back(std::move(payment));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findByDateRange: " + e.displayText());
    }
    
    return payments;
}

std::vector<std::unique_ptr<models::OrderPayment>> OrderPaymentRepository::findByAmountRange(double minAmount, double maxAmount)
{
    std::vector<std::unique_ptr<models::OrderPayment>> payments;
    auto connection = acquireConnection();
    
    try
    {
        double minAmountCopy = minAmount;
        double maxAmountCopy = maxAmount;
        
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT op.id, op.order_id, op.payment_method, op.payment_status, "
                  "op.amount, op.transaction_id, op.payment_date, op.created_at, "
                  "co.order_number, co.customer_name, co.total_amount as order_total "
                  "FROM " << TABLE_NAME << " op "
                  "JOIN customer_orders co ON co.id = op.order_id "
                  "WHERE op.amount >= $1 AND op.amount <= $2 "
                  "ORDER BY op.amount DESC, op.created_at DESC",
            Poco::Data::Keywords::use(minAmountCopy),
            Poco::Data::Keywords::use(maxAmountCopy),
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            auto payment = std::make_unique<models::OrderPayment>();
            payment->id = rs.value("id", 0).convert<long long>();
            payment->orderId = rs.value("order_id", 0).convert<long long>();
            payment->paymentMethod = rs.value("payment_method").convert<std::string>();
            payment->paymentStatus = rs.value("payment_status").convert<std::string>();
            payment->amount = rs.value("amount", 0.0).convert<double>();
            payment->transactionId = rs.value("transaction_id").isEmpty() ? "" : rs.value("transaction_id").convert<std::string>();
            payment->paymentDate = rs.value("payment_date").isEmpty() ? "" : rs.value("payment_date").convert<std::string>();
            payment->createdAt = rs.value("created_at").convert<std::string>();
            payment->orderNumber = rs.value("order_number").convert<std::string>();
            payment->customerName = rs.value("customer_name").convert<std::string>();
            payment->orderTotal = rs.value("order_total", 0.0).convert<double>();
            
            payments.push_back(std::move(payment));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findByAmountRange: " + e.displayText());
    }
    
    return payments;
}

std::vector<std::unique_ptr<models::OrderPayment>> OrderPaymentRepository::findPendingPayments()
{
    return findByPaymentStatus("pending");
}

std::vector<std::unique_ptr<models::OrderPayment>> OrderPaymentRepository::findPaidPayments()
{
    return findByPaymentStatus("paid");
}

std::vector<std::unique_ptr<models::OrderPayment>> OrderPaymentRepository::findFailedPayments()
{
    return findByPaymentStatus("failed");
}

std::vector<std::unique_ptr<models::OrderPayment>> OrderPaymentRepository::findRefundedPayments()
{
    return findByPaymentStatus("refunded");
}

std::vector<std::unique_ptr<models::OrderPayment>> OrderPaymentRepository::findOverduePayments(int daysThreshold)
{
    std::vector<std::unique_ptr<models::OrderPayment>> payments;
    auto connection = acquireConnection();
    
    try
    {
        std::string cutoffDate = DateUtils::formatDate(DateUtils::addDays(DateUtils::now(), -daysThreshold));
        
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT op.id, op.order_id, op.payment_method, op.payment_status, "
                  "op.amount, op.transaction_id, op.payment_date, op.created_at, "
                  "co.order_number, co.customer_name, co.total_amount as order_total, "
                  "co.order_date "
                  "FROM " << TABLE_NAME << " op "
                  "JOIN customer_orders co ON co.id = op.order_id "
                  "WHERE op.payment_status = 'pending' "
                  "AND co.order_date < $1 "
                  "ORDER BY co.order_date ASC, op.amount DESC",
            Poco::Data::Keywords::use(cutoffDate),
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            auto payment = std::make_unique<models::OrderPayment>();
            payment->id = rs.value("id", 0).convert<long long>();
            payment->orderId = rs.value("order_id", 0).convert<long long>();
            payment->paymentMethod = rs.value("payment_method").convert<std::string>();
            payment->paymentStatus = "pending";
            payment->amount = rs.value("amount", 0.0).convert<double>();
            payment->transactionId = rs.value("transaction_id").isEmpty() ? "" : rs.value("transaction_id").convert<std::string>();
            payment->paymentDate = rs.value("payment_date").isEmpty() ? "" : rs.value("payment_date").convert<std::string>();
            payment->createdAt = rs.value("created_at").convert<std::string>();
            payment->orderNumber = rs.value("order_number").convert<std::string>();
            payment->customerName = rs.value("customer_name").convert<std::string>();
            payment->orderTotal = rs.value("order_total", 0.0).convert<double>();
            
            payments.push_back(std::move(payment));
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in findOverduePayments: " + e.displayText());
    }
    
    return payments;
}

bool OrderPaymentRepository::updatePaymentStatus(long long id, const std::string& newStatus)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        long long idCopy = id;
        std::string statusCopy = newStatus;
        
        Poco::Data::Statement update(connection->getSession());
        update << "UPDATE " << TABLE_NAME << " SET payment_status = $1 WHERE id = $2",
            Poco::Data::Keywords::use(statusCopy),
            Poco::Data::Keywords::use(idCopy);
        
        int rowsAffected = update.execute();
        
        commitTransaction(*connection);
        return rowsAffected > 0;
    }
    catch (const Poco::Exception& e)
    {
        rollbackTransaction(*connection);
        throw std::runtime_error("Database error in updatePaymentStatus: " + e.displayText());
    }
}

bool OrderPaymentRepository::updateTransactionInfo(long long id, const std::string& transactionId, const std::string& paymentDate)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        long long idCopy = id;
        Poco::Nullable<std::string> transactionIdCopy;
        if (!transactionId.empty())
            transactionIdCopy = transactionId;
        
        Poco::Nullable<std::string> paymentDateCopy;
        if (!paymentDate.empty())
            paymentDateCopy = paymentDate;
        
        Poco::Data::Statement update(connection->getSession());
        update << "UPDATE " << TABLE_NAME << " SET "
                  "transaction_id = $1, payment_date = $2 "
                  "WHERE id = $3",
            Poco::Data::Keywords::use(transactionIdCopy),
            Poco::Data::Keywords::use(paymentDateCopy),
            Poco::Data::Keywords::use(idCopy);
        
        int rowsAffected = update.execute();
        
        commitTransaction(*connection);
        return rowsAffected > 0;
    }
    catch (const Poco::Exception& e)
    {
        rollbackTransaction(*connection);
        throw std::runtime_error("Database error in updateTransactionInfo: " + e.displayText());
    }
}

bool OrderPaymentRepository::updateAmount(long long id, double newAmount)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        long long idCopy = id;
        double amountCopy = newAmount;
        
        Poco::Data::Statement update(connection->getSession());
        update << "UPDATE " << TABLE_NAME << " SET amount = $1 WHERE id = $2",
            Poco::Data::Keywords::use(amountCopy),
            Poco::Data::Keywords::use(idCopy);
        
        int rowsAffected = update.execute();
        
        commitTransaction(*connection);
        return rowsAffected > 0;
    }
    catch (const Poco::Exception& e)
    {
        rollbackTransaction(*connection);
        throw std::runtime_error("Database error in updateAmount: " + e.displayText());
    }
}

bool OrderPaymentRepository::updatePaymentMethod(long long id, const std::string& newMethod)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        long long idCopy = id;
        std::string methodCopy = newMethod;
        
        Poco::Data::Statement update(connection->getSession());
        update << "UPDATE " << TABLE_NAME << " SET payment_method = $1 WHERE id = $2",
            Poco::Data::Keywords::use(methodCopy),
            Poco::Data::Keywords::use(idCopy);
        
        int rowsAffected = update.execute();
        
        commitTransaction(*connection);
        return rowsAffected > 0;
    }
    catch (const Poco::Exception& e)
    {
        rollbackTransaction(*connection);
        throw std::runtime_error("Database error in updatePaymentMethod: " + e.displayText());
    }
}

bool OrderPaymentRepository::markAsPaid(long long id, const std::string& transactionId)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        long long idCopy = id;
        std::string statusCopy = "paid";
        std::string paymentDateCopy = DateUtils::formatDateTime(DateUtils::now());
        
        Poco::Nullable<std::string> transactionIdCopy;
        if (!transactionId.empty())
            transactionIdCopy = transactionId;
        
        Poco::Data::Statement update(connection->getSession());
        update << "UPDATE " << TABLE_NAME << " SET "
                  "payment_status = $1, payment_date = $2, "
                  "transaction_id = COALESCE($3, transaction_id) "
                  "WHERE id = $4",
            Poco::Data::Keywords::use(statusCopy),
            Poco::Data::Keywords::use(paymentDateCopy),
            Poco::Data::Keywords::use(transactionIdCopy),
            Poco::Data::Keywords::use(idCopy);
        
        int rowsAffected = update.execute();
        
        commitTransaction(*connection);
        return rowsAffected > 0;
    }
    catch (const Poco::Exception& e)
    {
        rollbackTransaction(*connection);
        throw std::runtime_error("Database error in markAsPaid: " + e.displayText());
    }
}

bool OrderPaymentRepository::markAsFailed(long long id, const std::string& transactionId)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        long long idCopy = id;
        std::string statusCopy = "failed";
        
        Poco::Nullable<std::string> transactionIdCopy;
        if (!transactionId.empty())
            transactionIdCopy = transactionId;
        
        Poco::Data::Statement update(connection->getSession());
        update << "UPDATE " << TABLE_NAME << " SET "
                  "payment_status = $1, "
                  "transaction_id = COALESCE($2, transaction_id) "
                  "WHERE id = $3",
            Poco::Data::Keywords::use(statusCopy),
            Poco::Data::Keywords::use(transactionIdCopy),
            Poco::Data::Keywords::use(idCopy);
        
        int rowsAffected = update.execute();
        
        commitTransaction(*connection);
        return rowsAffected > 0;
    }
    catch (const Poco::Exception& e)
    {
        rollbackTransaction(*connection);
        throw std::runtime_error("Database error in markAsFailed: " + e.displayText());
    }
}

bool OrderPaymentRepository::markAsRefunded(long long id, const std::string& transactionId)
{
    auto connection = acquireConnection();
    
    try
    {
        beginTransaction(*connection);
        
        long long idCopy = id;
        std::string statusCopy = "refunded";
        std::string refundDateCopy = DateUtils::formatDateTime(DateUtils::now());
        
        Poco::Nullable<std::string> transactionIdCopy;
        if (!transactionId.empty())
            transactionIdCopy = transactionId;
        
        Poco::Data::Statement update(connection->getSession());
        update << "UPDATE " << TABLE_NAME << " SET "
                  "payment_status = $1, payment_date = $2, "
                  "transaction_id = COALESCE($3, transaction_id) "
                  "WHERE id = $4",
            Poco::Data::Keywords::use(statusCopy),
            Poco::Data::Keywords::use(refundDateCopy),
            Poco::Data::Keywords::use(transactionIdCopy),
            Poco::Data::Keywords::use(idCopy);
        
        int rowsAffected = update.execute();
        
        commitTransaction(*connection);
        return rowsAffected > 0;
    }
    catch (const Poco::Exception& e)
    {
        rollbackTransaction(*connection);
        throw std::runtime_error("Database error in markAsRefunded: " + e.displayText());
    }
}

bool OrderPaymentRepository::markAsPending(long long id)
{
    return updatePaymentStatus(id, "pending");
}

int OrderPaymentRepository::countByPaymentMethod(const std::string& paymentMethod)
{
    auto connection = acquireConnection();
    
    try
    {
        std::string methodCopy = paymentMethod;
        
        Poco::Data::Statement countStmt(connection->getSession());
        countStmt << "SELECT COUNT(*) FROM " << TABLE_NAME << " WHERE payment_method = $1",
            Poco::Data::Keywords::use(methodCopy),
            now;
        
        Poco::Data::RecordSet rs(countStmt);
        if (rs.rowCount() > 0)
        {
            return rs.value(0, 0).convert<int>();
        }
        
        return 0;
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in countByPaymentMethod: " + e.displayText());
    }
}

int OrderPaymentRepository::countByPaymentStatus(const std::string& paymentStatus)
{
    auto connection = acquireConnection();
    
    try
    {
        std::string statusCopy = paymentStatus;
        
        Poco::Data::Statement countStmt(connection->getSession());
        countStmt << "SELECT COUNT(*) FROM " << TABLE_NAME << " WHERE payment_status = $1",
            Poco::Data::Keywords::use(statusCopy),
            now;
        
        Poco::Data::RecordSet rs(countStmt);
        if (rs.rowCount() > 0)
        {
            return rs.value(0, 0).convert<int>();
        }
        
        return 0;
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in countByPaymentStatus: " + e.displayText());
    }
}

int OrderPaymentRepository::countByOrder(long long orderId)
{
    auto connection = acquireConnection();
    
    try
    {
        Poco::Int64 orderIdCopy = orderId;
        
        Poco::Data::Statement countStmt(connection->getSession());
        countStmt << "SELECT COUNT(*) FROM " << TABLE_NAME << " WHERE order_id = $1",
            Poco::Data::Keywords::use(orderIdCopy),
            now;
        
        Poco::Data::RecordSet rs(countStmt);
        if (rs.rowCount() > 0)
        {
            return rs.value(0, 0).convert<int>();
        }
        
        return 0;
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in countByOrder: " + e.displayText());
    }
}

double OrderPaymentRepository::getTotalPaymentsAmount(const std::string& startDate, const std::string& endDate)
{
    auto connection = acquireConnection();
    
    try
    {
        std::string startDateCopy = startDate;
        std::string endDateCopy = endDate;
        
        Poco::Data::Statement sumStmt(connection->getSession());
        sumStmt << "SELECT COALESCE(SUM(amount), 0) FROM " << TABLE_NAME 
                << " WHERE created_at >= $1 AND created_at <= $2 "
                << " AND payment_status = 'paid'",
            Poco::Data::Keywords::use(startDateCopy),
            Poco::Data::Keywords::use(endDateCopy),
            now;
        
        Poco::Data::RecordSet rs(sumStmt);
        if (rs.rowCount() > 0)
        {
            return rs.value(0, 0.0).convert<double>();
        }
        
        return 0.0;
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in getTotalPaymentsAmount: " + e.displayText());
    }
}

double OrderPaymentRepository::getAveragePaymentAmount()
{
    auto connection = acquireConnection();
    
    try
    {
        Poco::Data::Statement avgStmt(connection->getSession());
        avgStmt << "SELECT COALESCE(AVG(amount), 0) FROM " << TABLE_NAME 
                << " WHERE payment_status = 'paid'",
            now;
        
        Poco::Data::RecordSet rs(avgStmt);
        if (rs.rowCount() > 0)
        {
            return rs.value(0, 0.0).convert<double>();
        }
        
        return 0.0;
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in getAveragePaymentAmount: " + e.displayText());
    }
}

double OrderPaymentRepository::getTotalRevenueByPaymentMethod(const std::string& paymentMethod, const std::string& startDate, const std::string& endDate)
{
    auto connection = acquireConnection();
    
    try
    {
        std::string methodCopy = paymentMethod;
        std::string startDateCopy = startDate;
        std::string endDateCopy = endDate;
        
        Poco::Data::Statement sumStmt(connection->getSession());
        sumStmt << "SELECT COALESCE(SUM(amount), 0) FROM " << TABLE_NAME 
                << " WHERE payment_method = $1 "
                << " AND created_at >= $2 AND created_at <= $3 "
                << " AND payment_status = 'paid'",
            Poco::Data::Keywords::use(methodCopy),
            Poco::Data::Keywords::use(startDateCopy),
            Poco::Data::Keywords::use(endDateCopy),
            now;
        
        Poco::Data::RecordSet rs(sumStmt);
        if (rs.rowCount() > 0)
        {
            return rs.value(0, 0.0).convert<double>();
        }
        
        return 0.0;
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in getTotalRevenueByPaymentMethod: " + e.displayText());
    }
}

Poco::JSON::Array OrderPaymentRepository::getPaymentStatistics()
{
    auto connection = acquireConnection();
    Poco::JSON::Array jsonArray;
    
    try
    {
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT "
                  "payment_method, "
                  "payment_status, "
                  "COUNT(*) as count, "
                  "SUM(amount) as total_amount, "
                  "AVG(amount)::DECIMAL(15,2) as avg_amount, "
                  "MIN(created_at) as first_payment, "
                  "MAX(created_at) as last_payment "
                  "FROM " << TABLE_NAME << " "
                  "GROUP BY payment_method, payment_status "
                  "ORDER BY payment_method, payment_status",
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            Poco::JSON::Object stats;
            stats.set("payment_method", rs.value("payment_method").convert<std::string>());
            stats.set("payment_status", rs.value("payment_status").convert<std::string>());
            stats.set("count", rs.value("count", 0).convert<int>());
            stats.set("total_amount", rs.value("total_amount", 0.0).convert<double>());
            stats.set("avg_amount", rs.value("avg_amount", 0.0).convert<double>());
            stats.set("first_payment", rs.value("first_payment").convert<std::string>());
            stats.set("last_payment", rs.value("last_payment").convert<std::string>());
            
            jsonArray.add(stats);
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in getPaymentStatistics: " + e.displayText());
    }
    
    return jsonArray;
}

Poco::JSON::Array OrderPaymentRepository::getPaymentReport(const std::string& startDate, const std::string& endDate)
{
    auto connection = acquireConnection();
    Poco::JSON::Array jsonArray;
    
    try
    {
        std::string startDateCopy = startDate;
        std::string endDateCopy = endDate;
        
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT "
                  "DATE(created_at) as payment_date, "
                  "payment_method, "
                  "COUNT(*) as total_payments, "
                  "SUM(amount) as total_amount, "
                  "SUM(CASE WHEN payment_status = 'paid' THEN amount ELSE 0 END) as paid_amount, "
                  "SUM(CASE WHEN payment_status = 'pending' THEN amount ELSE 0 END) as pending_amount, "
                  "SUM(CASE WHEN payment_status = 'failed' THEN amount ELSE 0 END) as failed_amount, "
                  "SUM(CASE WHEN payment_status = 'refunded' THEN amount ELSE 0 END) as refunded_amount "
                  "FROM " << TABLE_NAME << " "
                  "WHERE created_at >= $1 AND created_at <= $2 "
                  "GROUP BY DATE(created_at), payment_method "
                  "ORDER BY payment_date DESC, payment_method",
            Poco::Data::Keywords::use(startDateCopy),
            Poco::Data::Keywords::use(endDateCopy),
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            Poco::JSON::Object report;
            report.set("payment_date", rs.value("payment_date").convert<std::string>());
            report.set("payment_method", rs.value("payment_method").convert<std::string>());
            report.set("total_payments", rs.value("total_payments", 0).convert<int>());
            report.set("total_amount", rs.value("total_amount", 0.0).convert<double>());
            report.set("paid_amount", rs.value("paid_amount", 0.0).convert<double>());
            report.set("pending_amount", rs.value("pending_amount", 0.0).convert<double>());
            report.set("failed_amount", rs.value("failed_amount", 0.0).convert<double>());
            report.set("refunded_amount", rs.value("refunded_amount", 0.0).convert<double>());
            
            jsonArray.add(report);
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in getPaymentReport: " + e.displayText());
    }
    
    return jsonArray;
}

Poco::JSON::Array OrderPaymentRepository::getPaymentMethodAnalysis()
{
    auto connection = acquireConnection();
    Poco::JSON::Array jsonArray;
    
    try
    {
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT "
                  "payment_method, "
                  "COUNT(*) as total_transactions, "
                  "SUM(amount) as total_volume, "
                  "AVG(amount)::DECIMAL(15,2) as avg_transaction_value, "
                  "SUM(CASE WHEN payment_status = 'paid' THEN 1 ELSE 0 END) as successful_transactions, "
                  "SUM(CASE WHEN payment_status = 'failed' THEN 1 ELSE 0 END) as failed_transactions, "
                  "SUM(CASE WHEN payment_status = 'pending' THEN 1 ELSE 0 END) as pending_transactions, "
                  "(SUM(CASE WHEN payment_status = 'paid' THEN 1 ELSE 0 END)::DECIMAL / COUNT(*) * 100)::DECIMAL(5,2) as success_rate "
                  "FROM " << TABLE_NAME << " "
                  "GROUP BY payment_method "
                  "ORDER BY total_volume DESC",
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            Poco::JSON::Object analysis;
            analysis.set("payment_method", rs.value("payment_method").convert<std::string>());
            analysis.set("total_transactions", rs.value("total_transactions", 0).convert<int>());
            analysis.set("total_volume", rs.value("total_volume", 0.0).convert<double>());
            analysis.set("avg_transaction_value", rs.value("avg_transaction_value", 0.0).convert<double>());
            analysis.set("successful_transactions", rs.value("successful_transactions", 0).convert<int>());
            analysis.set("failed_transactions", rs.value("failed_transactions", 0).convert<int>());
            analysis.set("pending_transactions", rs.value("pending_transactions", 0).convert<int>());
            analysis.set("success_rate", rs.value("success_rate", 0.0).convert<double>());
            
            jsonArray.add(analysis);
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in getPaymentMethodAnalysis: " + e.displayText());
    }
    
    return jsonArray;
}

Poco::JSON::Array OrderPaymentRepository::getCustomerPaymentHistory(long long customerOrderId)
{
    auto connection = acquireConnection();
    Poco::JSON::Array jsonArray;
    
    try
    {
        Poco::Int64 orderIdCopy = customerOrderId;
        
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT "
                  "op.id, "
                  "op.payment_method, "
                  "op.payment_status, "
                  "op.amount, "
                  "op.transaction_id, "
                  "op.payment_date, "
                  "op.created_at, "
                  "co.order_number, "
                  "co.customer_name, "
                  "co.total_amount as order_total, "
                  "CASE "
                  "  WHEN op.amount >= co.total_amount THEN 'FULL' "
                  "  ELSE 'PARTIAL' "
                  "END as payment_type "
                  "FROM " << TABLE_NAME << " op "
                  "JOIN customer_orders co ON co.id = op.order_id "
                  "WHERE co.id = $1 "
                  "ORDER BY op.created_at DESC",
            Poco::Data::Keywords::use(orderIdCopy),
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            Poco::JSON::Object history;
            history.set("id", rs.value("id", 0).convert<long long>());
            history.set("payment_method", rs.value("payment_method").convert<std::string>());
            history.set("payment_status", rs.value("payment_status").convert<std::string>());
            history.set("amount", rs.value("amount", 0.0).convert<double>());
            history.set("transaction_id", rs.value("transaction_id").isEmpty() ? "" : rs.value("transaction_id").convert<std::string>());
            history.set("payment_date", rs.value("payment_date").isEmpty() ? "" : rs.value("payment_date").convert<std::string>());
            history.set("created_at", rs.value("created_at").convert<std::string>());
            history.set("order_number", rs.value("order_number").convert<std::string>());
            history.set("customer_name", rs.value("customer_name").convert<std::string>());
            history.set("order_total", rs.value("order_total", 0.0).convert<double>());
            history.set("payment_type", rs.value("payment_type").convert<std::string>());
            history.set("remaining_amount", rs.value("order_total", 0.0).convert<double>() - rs.value("amount", 0.0).convert<double>());
            
            jsonArray.add(history);
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in getCustomerPaymentHistory: " + e.displayText());
    }
    
    return jsonArray;
}

bool OrderPaymentRepository::transactionIdExists(const std::string& transactionId)
{
    auto connection = acquireConnection();
    
    try
    {
        std::string transactionIdCopy = transactionId;
        
        Poco::Data::Statement check(connection->getSession());
        check << "SELECT COUNT(*) FROM " << TABLE_NAME << " WHERE transaction_id = $1",
            Poco::Data::Keywords::use(transactionIdCopy),
            now;
        
        Poco::Data::RecordSet rs(check);
        if (rs.rowCount() > 0)
        {
            return rs.value(0, 0).convert<int>() > 0;
        }
        
        return false;
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in transactionIdExists: " + e.displayText());
    }
}

bool OrderPaymentRepository::hasOrderPayment(long long orderId)
{
    auto connection = acquireConnection();
    
    try
    {
        Poco::Int64 orderIdCopy = orderId;
        
        Poco::Data::Statement check(connection->getSession());
        check << "SELECT COUNT(*) FROM " << TABLE_NAME << " WHERE order_id = $1",
            Poco::Data::Keywords::use(orderIdCopy),
            now;
        
        Poco::Data::RecordSet rs(check);
        if (rs.rowCount() > 0)
        {
            return rs.value(0, 0).convert<int>() > 0;
        }
        
        return false;
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in hasOrderPayment: " + e.displayText());
    }
}

std::vector<std::pair<long long, std::string>> OrderPaymentRepository::getOrderPaymentStatuses()
{
    std::vector<std::pair<long long, std::string>> statuses;
    auto connection = acquireConnection();
    
    try
    {
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT co.id, op.payment_status "
                  "FROM customer_orders co "
                  "LEFT JOIN " << TABLE_NAME << " op ON op.order_id = co.id "
                  "ORDER BY co.id",
            now;
        
        Poco::Data::RecordSet rs(select);
        
        for (size_t i = 0; i < rs.rowCount(); ++i)
        {
            long long orderId = rs.value("id", 0).convert<long long>();
            std::string paymentStatus = rs.value("payment_status").isEmpty() ? "no_payment" : rs.value("payment_status").convert<std::string>();
            statuses.emplace_back(orderId, paymentStatus);
        }
    }
    catch (const Poco::Exception& e)
    {
        throw std::runtime_error("Database error in getOrderPaymentStatuses: " + e.displayText());
    }
    
    return statuses;
}

models::OrderPayment OrderPaymentRepository::mapRowToPayment(Poco::Data::Row& row) const
{
    models::OrderPayment payment;
    payment.id = row.get(0).convert<long long>();
    payment.orderId = row.get(1).convert<long long>();
    payment.paymentMethod = row.get(2).convert<std::string>();
    payment.paymentStatus = row.get(3).convert<std::string>();
    payment.amount = row.get(4).convert<double>();
    payment.transactionId = row.get(5).convert<std::string>();
    payment.paymentDate = row.get(6).convert<std::string>();
    payment.createdAt = row.get(7).convert<std::string>();
    
    return payment;
}

void OrderPaymentRepository::enrichOrderPaymentWithOrderDetails(models::OrderPayment& payment)
{
    auto connection = acquireConnection();
    
    try
    {
        Poco::Int64 orderId = payment.orderId;
        
        Poco::Data::Statement select(connection->getSession());
        select << "SELECT order_number, customer_name, total_amount "
                  "FROM customer_orders WHERE id = $1",
            Poco::Data::Keywords::use(orderId),
            now;
        
        Poco::Data::RecordSet rs(select);
        
        if (rs.rowCount() > 0)
        {
            payment.orderNumber = rs.value("order_number").convert<std::string>();
            payment.customerName = rs.value("customer_name").convert<std::string>();
            payment.orderTotal = rs.value("total_amount", 0.0).convert<double>();
        }
    }
    catch (const Poco::Exception& e)
    {
        payment.orderNumber = "";
        payment.customerName = "";
        payment.orderTotal = 0.0;
    }
}

} // namespace database::repositories
