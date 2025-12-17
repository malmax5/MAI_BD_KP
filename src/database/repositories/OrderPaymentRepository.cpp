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
            Poco::Data::Row row = rs.row(0);
            auto payment = std::make_unique<models::OrderPayment>(mapRowToPayment(row));
            
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
            Poco::Data::Row row = rs.row(i);
            auto payment = std::make_unique<models::OrderPayment>(mapRowToPayment(row));
            
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
            Poco::Data::Row row = rs.row(i);
            auto payment = std::make_unique<models::OrderPayment>(mapRowToPayment(row));
            
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
            Poco::Data::Row row = rs.row(i);
            auto payment = std::make_unique<models::OrderPayment>(mapRowToPayment(row));
            
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
            Poco::Data::Row row = rs.row(i);
            auto payment = std::make_unique<models::OrderPayment>(mapRowToPayment(row));
            
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
            Poco::Data::Row row = rs.row(0);
            auto payment = std::make_unique<models::OrderPayment>(mapRowToPayment(row));
            
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
            Poco::Data::Row row = rs.row(i);
            auto payment = std::make_unique<models::OrderPayment>(mapRowToPayment(row));
            
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
            Poco::Data::Row row = rs.row(i);
            auto payment = std::make_unique<models::OrderPayment>(mapRowToPayment(row));
            
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
            Poco::Data::Row row = rs.row(i);
            auto payment = std::make_unique<models::OrderPayment>(mapRowToPayment(row));
            
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
            Poco::Data::Row row = rs.row(i);
            auto payment = std::make_unique<models::OrderPayment>(mapRowToPayment(row));
            
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
            Poco::Data::Row row = rs.row(i);
            Poco::JSON::Object stats;

            stats.set("payment_method", row["payment_method"].isEmpty() ? "" : row["payment_method"].convert<std::string>());
            stats.set("payment_status", row["payment_status"].isEmpty() ? "" : row["payment_status"].convert<std::string>());
            stats.set("first_payment", row["first_payment"].isEmpty() ? "" : row["first_payment"].convert<std::string>());
            stats.set("last_payment", row["last_payment"].isEmpty() ? "" : row["last_payment"].convert<std::string>());

            stats.set("count", row["count"].isEmpty() ? 0 : row["count"].convert<int>());
            stats.set("total_amount", row["total_amount"].isEmpty() ? 0.0 : row["total_amount"].convert<double>());
            stats.set("avg_amount", row["avg_amount"].isEmpty() ? 0.0 : row["avg_amount"].convert<double>());

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
            Poco::Data::Row row = rs.row(i);
            Poco::JSON::Object report;
            report.set("payment_date", 
                row["payment_date"].isEmpty() ? "" : row["payment_date"].convert<std::string>());

            report.set("payment_method", 
                row["payment_method"].isEmpty() ? "" : row["payment_method"].convert<std::string>());

            report.set("total_payments", 
                row["total_payments"].isEmpty() ? 0 : row["total_payments"].convert<int>());

            report.set("total_amount", 
                row["total_amount"].isEmpty() ? 0.0 : row["total_amount"].convert<double>());

            report.set("paid_amount", 
                row["paid_amount"].isEmpty() ? 0.0 : row["paid_amount"].convert<double>());

            report.set("pending_amount", 
                row["pending_amount"].isEmpty() ? 0.0 : row["pending_amount"].convert<double>());

            report.set("failed_amount", 
                row["failed_amount"].isEmpty() ? 0.0 : row["failed_amount"].convert<double>());

            report.set("refunded_amount", 
                row["refunded_amount"].isEmpty() ? 0.0 : row["refunded_amount"].convert<double>());
            
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
            Poco::Data::Row row = rs.row(i);
            Poco::JSON::Object analysis;

            analysis.set("payment_method", 
                row["payment_method"].isEmpty() ? "" : row["payment_method"].convert<std::string>());

            analysis.set("total_transactions", 
                row["total_transactions"].isEmpty() ? 0 : row["total_transactions"].convert<int>());

            analysis.set("total_volume", 
                row["total_volume"].isEmpty() ? 0.0 : row["total_volume"].convert<double>());

            analysis.set("avg_transaction_value", 
                row["avg_transaction_value"].isEmpty() ? 0.0 : row["avg_transaction_value"].convert<double>());

            analysis.set("successful_transactions", 
                row["successful_transactions"].isEmpty() ? 0 : row["successful_transactions"].convert<int>());

            analysis.set("failed_transactions", 
                row["failed_transactions"].isEmpty() ? 0 : row["failed_transactions"].convert<int>());

            analysis.set("pending_transactions", 
                row["pending_transactions"].isEmpty() ? 0 : row["pending_transactions"].convert<int>());

            analysis.set("success_rate", 
                row["success_rate"].isEmpty() ? 0.0 : row["success_rate"].convert<double>());

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
            Poco::Data::Row row = rs.row(i);
            Poco::JSON::Object history;

            history.set("id", row["id"].isEmpty() ? 0LL : row["id"].convert<Poco::Int64>());

            history.set("payment_method", row["payment_method"].isEmpty() ? "" : row["payment_method"].convert<std::string>());
            history.set("payment_status", row["payment_status"].isEmpty() ? "" : row["payment_status"].convert<std::string>());
            history.set("transaction_id", row["transaction_id"].isEmpty() ? "" : row["transaction_id"].convert<std::string>());
            history.set("payment_date",    row["payment_date"].isEmpty()    ? "" : row["payment_date"].convert<std::string>());
            history.set("created_at",      row["created_at"].isEmpty()      ? "" : row["created_at"].convert<std::string>());
            history.set("order_number",    row["order_number"].isEmpty()    ? "" : row["order_number"].convert<std::string>());
            history.set("customer_name",   row["customer_name"].isEmpty()   ? "" : row["customer_name"].convert<std::string>());
            history.set("payment_type",    row["payment_type"].isEmpty()    ? "" : row["payment_type"].convert<std::string>());

            double amount = row["amount"].isEmpty() ? 0.0 : row["amount"].convert<double>();
            double orderTotal = row["order_total"].isEmpty() ? 0.0 : row["order_total"].convert<double>();

            history.set("amount", amount);
            history.set("order_total", orderTotal);
            history.set("remaining_amount", orderTotal - amount);

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
            Poco::Data::Row row = rs.row(i);
        
            Poco::Int64 orderId = row["id"].isEmpty() ? 0 : row["id"].convert<Poco::Int64>();
            std::string paymentStatus = row["payment_status"].isEmpty() ? "no_payment" : row["payment_status"].convert<std::string>();
        
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

    payment.id = row["id"].isEmpty() ? 0 : row["id"].convert<Poco::Int64>();

    payment.orderId = row["order_id"].isEmpty() ? 0 : row["order_id"].convert<Poco::Int64>();

    payment.paymentMethod = row["payment_method"].isEmpty() ? "" : row["payment_method"].convert<std::string>();

    payment.paymentStatus = row["payment_status"].isEmpty() ? "" : row["payment_status"].convert<std::string>();

    payment.amount = row["amount"].isEmpty() ? 0.0 : row["amount"].convert<double>();

    payment.createdAt = row["created_at"].isEmpty() ? "" : row["created_at"].convert<std::string>();

    payment.orderTotal = row["order_total"].isEmpty() ? 0.0 : row["order_total"].convert<double>();

    payment.orderNumber = row["order_number"].isEmpty() ? "" : row["order_number"].convert<std::string>();

    payment.customerName = row["customer_name"].isEmpty() ? "" : row["customer_name"].convert<std::string>();

    if (!row["transaction_id"].isEmpty())
        payment.transactionId = row["transaction_id"].convert<std::string>();

    if (!row["payment_date"].isEmpty())
        payment.paymentDate = row["payment_date"].convert<std::string>();

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
