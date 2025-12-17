#include "OrderPaymentService.hpp"
#include "../../utils/DateUtils.hpp"
#include "../../utils/Validator.hpp"
#include <Poco/DateTimeFormatter.h>
#include <Poco/DateTimeFormat.h>
#include <Poco/DateTimeParser.h>
#include <Poco/Random.h>
#include <Poco/UUIDGenerator.h>
#include <Poco/UUID.h>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace warehouse_backend::services
{

using namespace warehouse_backend::utils;
using namespace warehouse_backend::database;

OrderPaymentService::OrderPaymentService()
    : paymentRepository(std::make_unique<repositories::OrderPaymentRepository>()),
      orderRepository(std::make_unique<repositories::CustomerOrderRepository>()),
      authService(std::make_unique<AuthService>())
{
}

Poco::JSON::Object PaymentStatistics::toJson() const
{
    Poco::JSON::Object json;
    json.set("total_payments", totalPayments);
    json.set("total_amount", totalAmount);
    json.set("average_payment", averagePayment);
    json.set("pending_payments", pendingPayments);
    json.set("paid_payments", paidPayments);
    json.set("failed_payments", failedPayments);
    json.set("refunded_payments", refundedPayments);
    json.set("overdue_payments", overduePayments);
    return json;
}

Poco::JSON::Object PaymentReportData::toJson() const
{
    Poco::JSON::Object json;
    json.set("payment_date", paymentDate);
    json.set("payment_method", paymentMethod);
    json.set("total_payments", totalPayments);
    json.set("total_amount", totalAmount);
    json.set("paid_amount", paidAmount);
    json.set("pending_amount", pendingAmount);
    json.set("failed_amount", failedAmount);
    json.set("refunded_amount", refundedAmount);
    return json;
}

Poco::JSON::Object PaymentMethodAnalysis::toJson() const
{
    Poco::JSON::Object json;
    json.set("payment_method", paymentMethod);
    json.set("total_transactions", totalTransactions);
    json.set("total_volume", totalVolume);
    json.set("avg_transaction_value", avgTransactionValue);
    json.set("successful_transactions", successfulTransactions);
    json.set("failed_transactions", failedTransactions);
    json.set("pending_transactions", pendingTransactions);
    json.set("success_rate", successRate);
    return json;
}

std::unique_ptr<models::OrderPayment> OrderPaymentService::getPaymentById(long long paymentId)
{
    try
    {
        return paymentRepository->findById(paymentId);
    }
    catch (const std::exception& e)
    {
        throw std::runtime_error("Failed to get payment by ID: " + std::string(e.what()));
    }
}

std::vector<std::unique_ptr<models::OrderPayment>> OrderPaymentService::getAllPayments()
{
    try
    {
        return paymentRepository->findAll();
    }
    catch (const std::exception& e)
    {
        throw std::runtime_error("Failed to get all payments: " + std::string(e.what()));
    }
}

std::vector<std::unique_ptr<models::OrderPayment>> OrderPaymentService::getPaginatedPayments(
    int page, int pageSize)
{
    try
    {
        if (page < 1) page = 1;
        if (pageSize < 1) pageSize = 20;
        if (pageSize > 100) pageSize = 100;
        
        return paymentRepository->findPaginated(page, pageSize);
    }
    catch (const std::exception& e)
    {
        throw std::runtime_error("Failed to get paginated payments: " + std::string(e.what()));
    }
}

long long OrderPaymentService::createPayment(const models::OrderPayment& payment, long long userId)
{
    try
    {
        if (!validatePayment(payment))
        {
            throw std::runtime_error("Invalid payment data");
        }
        
        std::string orderError;
        if (!validateOrderForPayment(payment.orderId, orderError))
        {
            throw std::runtime_error(orderError);
        }
        
        models::OrderPayment paymentCopy = payment;
        if (paymentCopy.transactionId.isNull())
        {
            paymentCopy.transactionId = generateTransactionId();
        }
        
        paymentCopy.createdAt = DateUtils::formatDateTime(DateUtils::now());
        
        long long paymentId = paymentRepository->create(paymentCopy);
        
        logPaymentEvent(userId, "CREATE_PAYMENT", paymentId, 
                       "system", "OrderPaymentService", true, 
                       "Payment created successfully");
        
        createAuditRecord(userId, "CREATE_PAYMENT", paymentId, 
                         "Payment created for order " + std::to_string(payment.orderId));
        
        notifyPaymentCreated(paymentCopy);
        
        return paymentId;
    }
    catch (const std::exception& e)
    {
        logPaymentEvent(userId, "CREATE_PAYMENT", 0, 
                       "system", "OrderPaymentService", false, 
                       "Failed to create payment: " + std::string(e.what()));
        throw std::runtime_error("Failed to create payment: " + std::string(e.what()));
    }
}

bool OrderPaymentService::updatePayment(long long paymentId, const models::OrderPayment& payment, long long userId)
{
    try
    {
        auto existingPayment = paymentRepository->findById(paymentId);
        if (!existingPayment)
        {
            throw std::runtime_error("Payment not found");
        }
        
        if (!validatePayment(payment))
        {
            throw std::runtime_error("Invalid payment data");
        }
        
        bool success = paymentRepository->update(paymentId, payment);
        
        if (success)
        {
            logPaymentEvent(userId, "UPDATE_PAYMENT", paymentId, 
                           "system", "OrderPaymentService", true, 
                           "Payment updated successfully");
            
            createAuditRecord(userId, "UPDATE_PAYMENT", paymentId, 
                             "Payment details updated");
            
            if (existingPayment->paymentStatus != payment.paymentStatus)
            {
                notifyPaymentStatusChanged(payment, existingPayment->paymentStatus, payment.paymentStatus);
            }
        }
        
        return success;
    }
    catch (const std::exception& e)
    {
        logPaymentEvent(userId, "UPDATE_PAYMENT", paymentId, 
                       "system", "OrderPaymentService", false, 
                       "Failed to update payment: " + std::string(e.what()));
        throw std::runtime_error("Failed to update payment: " + std::string(e.what()));
    }
}

bool OrderPaymentService::deletePayment(long long paymentId, long long userId)
{
    try
    {
        auto payment = paymentRepository->findById(paymentId);
        if (!payment)
        {
            throw std::runtime_error("Payment not found");
        }
        
        if (payment->isPaid() || payment->isRefunded())
        {
            throw std::runtime_error("Cannot delete paid or refunded payments");
        }
        
        bool success = paymentRepository->remove(paymentId);
        
        if (success)
        {
            logPaymentEvent(userId, "DELETE_PAYMENT", paymentId, 
                           "system", "OrderPaymentService", true, 
                           "Payment deleted successfully");
            
            createAuditRecord(userId, "DELETE_PAYMENT", paymentId, 
                             "Payment deleted from system");
        }
        
        return success;
    }
    catch (const std::exception& e)
    {
        logPaymentEvent(userId, "DELETE_PAYMENT", paymentId, 
                       "system", "OrderPaymentService", false, 
                       "Failed to delete payment: " + std::string(e.what()));
        throw std::runtime_error("Failed to delete payment: " + std::string(e.what()));
    }
}

bool OrderPaymentService::markPaymentAsPaid(long long paymentId, const std::string& transactionId, long long userId)
{
    try
    {
        auto payment = paymentRepository->findById(paymentId);
        if (!payment)
        {
            throw std::runtime_error("Payment not found");
        }
        
        std::string errorMessage;
        if (!canMarkAsPaid(*payment, errorMessage))
        {
            throw std::runtime_error(errorMessage);
        }
        
        bool success = paymentRepository->markAsPaid(paymentId, transactionId);
        
        if (success)
        {
            logPaymentEvent(userId, "MARK_AS_PAID", paymentId, 
                           "system", "OrderPaymentService", true, 
                           "Payment marked as paid");
            
            createAuditRecord(userId, "MARK_AS_PAID", paymentId, 
                             "Payment marked as paid with transaction ID: " + transactionId);
            
            auto updatedPayment = paymentRepository->findById(paymentId);
            if (updatedPayment)
            {
                notifyPaymentStatusChanged(*updatedPayment, payment->paymentStatus, "paid");
            }
        }
        
        return success;
    }
    catch (const std::exception& e)
    {
        logPaymentEvent(userId, "MARK_AS_PAID", paymentId, 
                       "system", "OrderPaymentService", false, 
                       "Failed to mark payment as paid: " + std::string(e.what()));
        throw std::runtime_error("Failed to mark payment as paid: " + std::string(e.what()));
    }
}

bool OrderPaymentService::markPaymentAsFailed(long long paymentId, const std::string& transactionId, long long userId)
{
    try
    {
        auto payment = paymentRepository->findById(paymentId);
        if (!payment)
        {
            throw std::runtime_error("Payment not found");
        }
        
        std::string errorMessage;
        if (!canMarkAsFailed(*payment, errorMessage))
        {
            throw std::runtime_error(errorMessage);
        }
        
        bool success = paymentRepository->markAsFailed(paymentId, transactionId);
        
        if (success)
        {
            logPaymentEvent(userId, "MARK_AS_FAILED", paymentId, 
                           "system", "OrderPaymentService", true, 
                           "Payment marked as failed");
            
            createAuditRecord(userId, "MARK_AS_FAILED", paymentId, 
                             "Payment marked as failed with transaction ID: " + transactionId);
            
            notifyPaymentFailed(*payment, "Payment marked as failed by user");
        }
        
        return success;
    }
    catch (const std::exception& e)
    {
        logPaymentEvent(userId, "MARK_AS_FAILED", paymentId, 
                       "system", "OrderPaymentService", false, 
                       "Failed to mark payment as failed: " + std::string(e.what()));
        throw std::runtime_error("Failed to mark payment as failed: " + std::string(e.what()));
    }
}

bool OrderPaymentService::markPaymentAsRefunded(long long paymentId, const std::string& transactionId, long long userId)
{
    try
    {
        auto payment = paymentRepository->findById(paymentId);
        if (!payment)
        {
            throw std::runtime_error("Payment not found");
        }
        
        std::string errorMessage;
        if (!canRefundPayment(*payment, errorMessage))
        {
            throw std::runtime_error(errorMessage);
        }
        
        bool success = paymentRepository->markAsRefunded(paymentId, transactionId);
        
        if (success)
        {
            logPaymentEvent(userId, "MARK_AS_REFUNDED", paymentId, 
                           "system", "OrderPaymentService", true, 
                           "Payment marked as refunded");
            
            createAuditRecord(userId, "MARK_AS_REFUNDED", paymentId, 
                             "Payment refunded with transaction ID: " + transactionId);
            
            auto updatedPayment = paymentRepository->findById(paymentId);
            if (updatedPayment)
            {
                notifyPaymentStatusChanged(*updatedPayment, payment->paymentStatus, "refunded");
                notifyPaymentRefunded(*updatedPayment, "Payment refunded by user");
            }
        }
        
        return success;
    }
    catch (const std::exception& e)
    {
        logPaymentEvent(userId, "MARK_AS_REFUNDED", paymentId, 
                       "system", "OrderPaymentService", false, 
                       "Failed to mark payment as refunded: " + std::string(e.what()));
        throw std::runtime_error("Failed to mark payment as refunded: " + std::string(e.what()));
    }
}

bool OrderPaymentService::markPaymentAsPending(long long paymentId, long long userId)
{
    try
    {
        auto payment = paymentRepository->findById(paymentId);
        if (!payment)
        {
            throw std::runtime_error("Payment not found");
        }
        
        if (payment->isPaid() || payment->isRefunded())
        {
            throw std::runtime_error("Cannot set paid or refunded payments to pending");
        }
        
        bool success = paymentRepository->markAsPending(paymentId);
        
        if (success)
        {
            logPaymentEvent(userId, "MARK_AS_PENDING", paymentId, 
                           "system", "OrderPaymentService", true, 
                           "Payment marked as pending");
            
            createAuditRecord(userId, "MARK_AS_PENDING", paymentId, 
                             "Payment status changed to pending");
            
            auto updatedPayment = paymentRepository->findById(paymentId);
            if (updatedPayment)
            {
                notifyPaymentStatusChanged(*updatedPayment, payment->paymentStatus, "pending");
            }
        }
        
        return success;
    }
    catch (const std::exception& e)
    {
        logPaymentEvent(userId, "MARK_AS_PENDING", paymentId, 
                       "system", "OrderPaymentService", false, 
                       "Failed to mark payment as pending: " + std::string(e.what()));
        throw std::runtime_error("Failed to mark payment as pending: " + std::string(e.what()));
    }
}

bool OrderPaymentService::updatePaymentStatus(long long paymentId, const std::string& newStatus, 
                                            const std::string& transactionId, const std::string& paymentDate,
                                            long long userId)
{
    try
    {
        auto payment = paymentRepository->findById(paymentId);
        if (!payment)
        {
            throw std::runtime_error("Payment not found");
        }
        
        std::string oldStatus = payment->paymentStatus;
        
        bool success = false;
        if (!transactionId.empty() || !paymentDate.empty())
        {
            success = paymentRepository->updateTransactionInfo(paymentId, transactionId, paymentDate);
        }
        
        if (!success || newStatus != oldStatus)
        {
            success = paymentRepository->updatePaymentStatus(paymentId, newStatus);
        }
        
        if (success)
        {
            logPaymentEvent(userId, "UPDATE_STATUS", paymentId, 
                           "system", "OrderPaymentService", true, 
                           "Payment status updated from " + oldStatus + " to " + newStatus);
            
            createAuditRecord(userId, "UPDATE_STATUS", paymentId, 
                             "Status changed from " + oldStatus + " to " + newStatus);
            
            auto updatedPayment = paymentRepository->findById(paymentId);
            if (updatedPayment)
            {
                notifyPaymentStatusChanged(*updatedPayment, oldStatus, newStatus);
            }
        }
        
        return success;
    }
    catch (const std::exception& e)
    {
        logPaymentEvent(userId, "UPDATE_STATUS", paymentId, 
                       "system", "OrderPaymentService", false, 
                       "Failed to update payment status: " + std::string(e.what()));
        throw std::runtime_error("Failed to update payment status: " + std::string(e.what()));
    }
}

std::vector<std::unique_ptr<models::OrderPayment>> OrderPaymentService::searchPayments(
    const std::string& query, const std::vector<std::string>& fields)
{
    try
    {
        return paymentRepository->search(query, fields);
    }
    catch (const std::exception& e)
    {
        throw std::runtime_error("Failed to search payments: " + std::string(e.what()));
    }
}

std::vector<std::unique_ptr<models::OrderPayment>> OrderPaymentService::findPaymentsByField(
    const std::string& fieldName, const std::string& fieldValue)
{
    try
    {
        return paymentRepository->findByField(fieldName, fieldValue);
    }
    catch (const std::exception& e)
    {
        throw std::runtime_error("Failed to find payments by field: " + std::string(e.what()));
    }
}

std::unique_ptr<models::OrderPayment> OrderPaymentService::getPaymentByOrderId(long long orderId)
{
    try
    {
        return paymentRepository->findByOrderId(orderId);
    }
    catch (const std::exception& e)
    {
        throw std::runtime_error("Failed to get payment by order ID: " + std::string(e.what()));
    }
}

std::vector<std::unique_ptr<models::OrderPayment>> OrderPaymentService::getPaymentsByPaymentMethod(
    const std::string& paymentMethod)
{
    try
    {
        return paymentRepository->findByPaymentMethod(paymentMethod);
    }
    catch (const std::exception& e)
    {
        throw std::runtime_error("Failed to get payments by payment method: " + std::string(e.what()));
    }
}

std::vector<std::unique_ptr<models::OrderPayment>> OrderPaymentService::getPaymentsByStatus(
    const std::string& status)
{
    try
    {
        return paymentRepository->findByPaymentStatus(status);
    }
    catch (const std::exception& e)
    {
        throw std::runtime_error("Failed to get payments by status: " + std::string(e.what()));
    }
}

std::vector<std::unique_ptr<models::OrderPayment>> OrderPaymentService::getPaymentsByDateRange(
    const std::string& startDate, const std::string& endDate)
{
    try
    {
        return paymentRepository->findByDateRange(startDate, endDate);
    }
    catch (const std::exception& e)
    {
        throw std::runtime_error("Failed to get payments by date range: " + std::string(e.what()));
    }
}

std::vector<std::unique_ptr<models::OrderPayment>> OrderPaymentService::getPaymentsByAmountRange(
    double minAmount, double maxAmount)
{
    try
    {
        return paymentRepository->findByAmountRange(minAmount, maxAmount);
    }
    catch (const std::exception& e)
    {
        throw std::runtime_error("Failed to get payments by amount range: " + std::string(e.what()));
    }
}

std::vector<std::unique_ptr<models::OrderPayment>> OrderPaymentService::getPendingPayments()
{
    try
    {
        return paymentRepository->findPendingPayments();
    }
    catch (const std::exception& e)
    {
        throw std::runtime_error("Failed to get pending payments: " + std::string(e.what()));
    }
}

std::vector<std::unique_ptr<models::OrderPayment>> OrderPaymentService::getPaidPayments()
{
    try
    {
        return paymentRepository->findPaidPayments();
    }
    catch (const std::exception& e)
    {
        throw std::runtime_error("Failed to get paid payments: " + std::string(e.what()));
    }
}

std::vector<std::unique_ptr<models::OrderPayment>> OrderPaymentService::getFailedPayments()
{
    try
    {
        return paymentRepository->findFailedPayments();
    }
    catch (const std::exception& e)
    {
        throw std::runtime_error("Failed to get failed payments: " + std::string(e.what()));
    }
}

std::vector<std::unique_ptr<models::OrderPayment>> OrderPaymentService::getRefundedPayments()
{
    try
    {
        return paymentRepository->findRefundedPayments();
    }
    catch (const std::exception& e)
    {
        throw std::runtime_error("Failed to get refunded payments: " + std::string(e.what()));
    }
}

std::vector<std::unique_ptr<models::OrderPayment>> OrderPaymentService::getOverduePayments(int daysThreshold)
{
    try
    {
        return paymentRepository->findOverduePayments(daysThreshold);
    }
    catch (const std::exception& e)
    {
        throw std::runtime_error("Failed to get overdue payments: " + std::string(e.what()));
    }
}

PaymentStatistics OrderPaymentService::getPaymentStatistics()
{
    PaymentStatistics stats;
    
    try
    {
        stats.totalPayments = paymentRepository->count();
        stats.pendingPayments = paymentRepository->countByPaymentStatus("pending");
        stats.paidPayments = paymentRepository->countByPaymentStatus("paid");
        stats.failedPayments = paymentRepository->countByPaymentStatus("failed");
        stats.refundedPayments = paymentRepository->countByPaymentStatus("refunded");
        
        std::string today = DateUtils::formatDate(DateUtils::now());
        std::string thirtyDaysAgo = DateUtils::formatDate(DateUtils::addDays(DateUtils::now(), -30));
        stats.totalAmount = paymentRepository->getTotalPaymentsAmount(thirtyDaysAgo, today);
        
        if (stats.paidPayments > 0)
        {
            stats.averagePayment = paymentRepository->getAveragePaymentAmount();
        }
        else
        {
            stats.averagePayment = 0.0;
        }
        
        auto overduePayments = paymentRepository->findOverduePayments(7);
        stats.overduePayments = static_cast<int>(overduePayments.size());
    }
    catch (const std::exception& e)
    {
        throw std::runtime_error("Failed to get payment statistics: " + std::string(e.what()));
    }
    
    return stats;
}

Poco::JSON::Array OrderPaymentService::getPaymentReport(const std::string& startDate, const std::string& endDate)
{
    try
    {
        return paymentRepository->getPaymentReport(startDate, endDate);
    }
    catch (const std::exception& e)
    {
        throw std::runtime_error("Failed to get payment report: " + std::string(e.what()));
    }
}

Poco::JSON::Array OrderPaymentService::getPaymentMethodAnalysis()
{
    try
    {
        return paymentRepository->getPaymentMethodAnalysis();
    }
    catch (const std::exception& e)
    {
        throw std::runtime_error("Failed to get payment method analysis: " + std::string(e.what()));
    }
}

Poco::JSON::Array OrderPaymentService::getCustomerPaymentHistory(long long orderId)
{
    try
    {
        return paymentRepository->getCustomerPaymentHistory(orderId);
    }
    catch (const std::exception& e)
    {
        throw std::runtime_error("Failed to get customer payment history: " + std::string(e.what()));
    }
}

bool OrderPaymentService::validatePayment(const database::models::OrderPayment& payment)
{
    if (!payment.validate())
    {
        return false;
    }
    
    std::vector<std::string> errors;
    if (!validatePaymentCreation(payment, errors))
    {
        return false;
    }
    
    return true;
}

bool OrderPaymentService::canProcessPayment(const database::models::OrderPayment& payment, std::string& errorMessage)
{
    if (payment.isPaid() || payment.isRefunded())
    {
        errorMessage = "Payment has already been processed";
        return false;
    }
    
    if (!payment.isPending())
    {
        errorMessage = "Only pending payments can be processed";
        return false;
    }
    
    std::string orderError;
    if (!validateOrderForPayment(payment.orderId, orderError))
    {
        errorMessage = orderError;
        return false;
    }
    
    return true;
}

bool OrderPaymentService::processPayment(long long paymentId, long long userId)
{
    try
    {
        auto payment = paymentRepository->findById(paymentId);
        if (!payment)
        {
            throw std::runtime_error("Payment not found");
        }
        
        std::string errorMessage;
        if (!canProcessPayment(*payment, errorMessage))
        {
            throw std::runtime_error(errorMessage);
        }
        
        bool success = markPaymentAsPaid(paymentId, generateTransactionId(), userId);
        
        if (success)
        {
            logPaymentEvent(userId, "PROCESS_PAYMENT", paymentId, 
                           "system", "OrderPaymentService", true, 
                           "Payment processed successfully");
            
            createAuditRecord(userId, "PROCESS_PAYMENT", paymentId, 
                             "Payment processed through service");
        }
        
        return success;
    }
    catch (const std::exception& e)
    {
        logPaymentEvent(userId, "PROCESS_PAYMENT", paymentId, 
                       "system", "OrderPaymentService", false, 
                       "Failed to process payment: " + std::string(e.what()));
        throw std::runtime_error("Failed to process payment: " + std::string(e.what()));
    }
}

bool OrderPaymentService::refundPayment(long long paymentId, const std::string& reason, long long userId)
{
    try
    {
        auto payment = paymentRepository->findById(paymentId);
        if (!payment)
        {
            throw std::runtime_error("Payment not found");
        }
        
        if (!payment->isPaid())
        {
            throw std::runtime_error("Only paid payments can be refunded");
        }
        
        bool success = markPaymentAsRefunded(paymentId, generateTransactionId(), userId);
        
        if (success)
        {
            logPaymentEvent(userId, "REFUND_PAYMENT", paymentId, 
                           "system", "OrderPaymentService", true, 
                           "Payment refunded: " + reason);
            
            createAuditRecord(userId, "REFUND_PAYMENT", paymentId, 
                             "Payment refunded. Reason: " + reason);
        }
        
        return success;
    }
    catch (const std::exception& e)
    {
        logPaymentEvent(userId, "REFUND_PAYMENT", paymentId, 
                       "system", "OrderPaymentService", false, 
                       "Failed to refund payment: " + std::string(e.what()));
        throw std::runtime_error("Failed to refund payment: " + std::string(e.what()));
    }
}

bool OrderPaymentService::validatePaymentCreation(const database::models::OrderPayment& payment, std::vector<std::string>& errors)
{
    errors.clear();
    
    if (payment.orderId <= 0)
    {
        errors.push_back("Order ID is required");
    }
    
    if (payment.paymentMethod.empty())
    {
        errors.push_back("Payment method is required");
    }
    
    if (payment.amount <= 0)
    {
        errors.push_back("Payment amount must be greater than 0");
    }
    
    std::string methodError;
    if (!validatePaymentMethod(payment.paymentMethod, methodError))
    {
        errors.push_back(methodError);
    }
    
    auto order = orderRepository->findById(payment.orderId);
    if (!order)
    {
        errors.push_back("Order not found");
    }
    else
    {
        std::string amountError;
        if (!validatePaymentAmount(payment.amount, order->totalAmount, amountError))
        {
            errors.push_back(amountError);
        }
        
        if (order->isComplete())
        {
            errors.push_back("Cannot add payment to completed or cancelled order");
        }
    }
    
    return errors.empty();
}

bool OrderPaymentService::validatePaymentUpdate(const database::models::OrderPayment& payment, std::vector<std::string>& errors)
{
    errors.clear();
    
    bool hasChanges = !payment.paymentMethod.empty() || payment.amount > 0;
    if (!hasChanges)
    {
        errors.push_back("No fields to update");
    }
    
    if (!payment.paymentMethod.empty())
    {
        std::string methodError;
        if (!validatePaymentMethod(payment.paymentMethod, methodError))
        {
            errors.push_back(methodError);
        }
    }
    
    if (payment.amount > 0)
    {
        if (payment.amount <= 0)
        {
            errors.push_back("Payment amount must be greater than 0");
        }
    }
    
    return errors.empty();
}

bool OrderPaymentService::canUserViewPayment(long long userId, database::models::UserRole userRole, long long paymentId)
{
    if (userRole == database::models::UserRole::ADMIN || 
        userRole == database::models::UserRole::MANAGER || 
        userRole == database::models::UserRole::AUDITOR)
    {
        return true;
    }
    
    auto payment = paymentRepository->findById(paymentId);
    if (payment)
    {
        auto order = orderRepository->findById(payment->orderId);
        if (order && order->createdBy == userId)
        {
            return true;
        }
    }
    
    return false;
}

bool OrderPaymentService::canUserCreatePayment(long long userId, database::models::UserRole userRole, 
                                             const database::models::OrderPayment& payment)
{
    if (userRole == database::models::UserRole::ADMIN || 
        userRole == database::models::UserRole::MANAGER)
    {
        return true;
    }
    
    auto order = orderRepository->findById(payment.orderId);
    if (order && order->createdBy == userId)
    {
        return true;
    }
    
    return false;
}

bool OrderPaymentService::canUserUpdatePayment(long long userId, database::models::UserRole userRole, long long paymentId)
{
    if (userRole == database::models::UserRole::ADMIN || 
        userRole == database::models::UserRole::MANAGER)
    {
        return true;
    }
    
    return false;
}

bool OrderPaymentService::canUserDeletePayment(long long userId, database::models::UserRole userRole, long long paymentId)
{
    return userRole == database::models::UserRole::ADMIN;
}

bool OrderPaymentService::canUserUpdatePaymentStatus(long long userId, database::models::UserRole userRole, long long paymentId)
{
    if (userRole == database::models::UserRole::ADMIN || 
        userRole == database::models::UserRole::MANAGER)
    {
        return true;
    }
    
    return false;
}

bool OrderPaymentService::validateOrderForPayment(long long orderId, std::string& errorMessage)
{
    auto order = orderRepository->findById(orderId);
    if (!order)
    {
        errorMessage = "Order not found";
        return false;
    }
    
    if (order->isComplete())
    {
        errorMessage = "Cannot process payment for completed or cancelled order";
        return false;
    }
    
    return true;
}

double OrderPaymentService::calculateOrderRemainingBalance(long long orderId)
{
    auto order = orderRepository->findById(orderId);
    if (!order)
    {
        return 0.0;
    }
    
    auto payment = paymentRepository->findByOrderId(orderId);
    if (!payment)
    {
        return order->totalAmount;
    }
    
    double totalPaid = 0.0;
    
    if (payment->isPaid())
    {
        totalPaid = payment->amount;
    }
    
    double remaining = order->totalAmount - totalPaid;
    return remaining > 0 ? remaining : 0.0;
}

bool OrderPaymentService::isOrderFullyPaid(long long orderId)
{
    double remaining = calculateOrderRemainingBalance(orderId);
    return remaining <= 0.0;
}

int OrderPaymentService::countPayments()
{
    try
    {
        return paymentRepository->count();
    }
    catch (const std::exception& e)
    {
        throw std::runtime_error("Failed to count payments: " + std::string(e.what()));
    }
}

int OrderPaymentService::countPaymentsByStatus(const std::string& status)
{
    try
    {
        return paymentRepository->countByPaymentStatus(status);
    }
    catch (const std::exception& e)
    {
        throw std::runtime_error("Failed to count payments by status: " + std::string(e.what()));
    }
}

int OrderPaymentService::countPaymentsByMethod(const std::string& method)
{
    try
    {
        return paymentRepository->countByPaymentMethod(method);
    }
    catch (const std::exception& e)
    {
        throw std::runtime_error("Failed to count payments by method: " + std::string(e.what()));
    }
}

double OrderPaymentService::getTotalPaymentsAmount(const std::string& startDate, const std::string& endDate)
{
    try
    {
        return paymentRepository->getTotalPaymentsAmount(startDate, endDate);
    }
    catch (const std::exception& e)
    {
        throw std::runtime_error("Failed to get total payments amount: " + std::string(e.what()));
    }
}

double OrderPaymentService::getAveragePaymentAmount()
{
    try
    {
        return paymentRepository->getAveragePaymentAmount();
    }
    catch (const std::exception& e)
    {
        throw std::runtime_error("Failed to get average payment amount: " + std::string(e.what()));
    }
}

bool OrderPaymentService::transactionIdExists(const std::string& transactionId)
{
    try
    {
        return paymentRepository->transactionIdExists(transactionId);
    }
    catch (const std::exception& e)
    {
        throw std::runtime_error("Failed to check transaction ID: " + std::string(e.what()));
    }
}

bool OrderPaymentService::hasOrderPayment(long long orderId)
{
    try
    {
        return paymentRepository->hasOrderPayment(orderId);
    }
    catch (const std::exception& e)
    {
        throw std::runtime_error("Failed to check order payment: " + std::string(e.what()));
    }
}

void OrderPaymentService::logPaymentEvent(long long userId, const std::string& action,
                                        long long paymentId, const std::string& ipAddress,
                                        const std::string& userAgent, bool success,
                                        const std::string& details)
{
    std::string timestamp = DateUtils::formatDateTime(DateUtils::now());
    std::string status = success ? "SUCCESS" : "FAILURE";
    
    std::cout << "[" << timestamp << "] "
              << "[PAYMENT] " << action << " "
              << "UserID: " << userId << " "
              << "PaymentID: " << paymentId << " "
              << "IP: " << ipAddress << " "
              << "Status: " << status << " "
              << "Details: " << details << std::endl;
}

bool OrderPaymentService::validatePaymentAmount(double amount, double orderTotal, std::string& errorMessage)
{
    if (amount <= 0)
    {
        errorMessage = "Payment amount must be greater than 0";
        return false;
    }
    
    if (amount > orderTotal * 2)
    {
        errorMessage = "Payment amount exceeds reasonable limit";
        return false;
    }
    
    return true;
}

bool OrderPaymentService::validatePaymentMethod(const std::string& paymentMethod, std::string& errorMessage)
{
    static const std::vector<std::string> acceptedMethods = {
        "credit_card", "debit_card", "bank_transfer", "cash", 
        "check", "paypal", "stripe", "apple_pay", "google_pay"
    };
    
    if (paymentMethod.empty())
    {
        errorMessage = "Payment method cannot be empty";
        return false;
    }
    
    if (std::find(acceptedMethods.begin(), acceptedMethods.end(), paymentMethod) == acceptedMethods.end())
    {
        errorMessage = "Unsupported payment method";
        return false;
    }
    
    return true;
}

bool OrderPaymentService::validatePaymentStatus(const std::string& status, std::string& errorMessage)
{
    static const std::vector<std::string> validStatuses = {
        "pending", "paid", "failed", "refunded", "processing", "authorized", "cancelled"
    };
    
    if (status.empty())
    {
        errorMessage = "Payment status cannot be empty";
        return false;
    }
    
    if (std::find(validStatuses.begin(), validStatuses.end(), status) == validStatuses.end())
    {
        errorMessage = "Invalid payment status";
        return false;
    }
    
    return true;
}

std::string OrderPaymentService::generateTransactionId()
{
    Poco::UUIDGenerator& generator = Poco::UUIDGenerator::defaultGenerator();
    Poco::UUID uuid = generator.createRandom();
    std::string uuidStr = uuid.toString();

    std::string uuidWithoutHyphens;
    uuidWithoutHyphens.reserve(uuidStr.size());
    for (char c : uuidStr) {
        if (c != '-') {
            uuidWithoutHyphens += c;
        }
    }

    std::string timestamp = DateUtils::formatDateTime(DateUtils::now(), "YYYYMMDDHHMMSS");
    
    return "TXN-" + timestamp + "-" + uuidWithoutHyphens.substr(0, 12);
}

void OrderPaymentService::enrichPaymentWithOrderDetails(database::models::OrderPayment& payment)
{
    try
    {
        auto order = orderRepository->findById(payment.orderId);
        if (order)
        {
            payment.orderNumber = order->orderNumber;
            payment.customerName = order->customerName;
            payment.orderTotal = order->totalAmount;
        }
    }
    catch (...)
    {
    }
}

bool OrderPaymentService::canRefundPayment(const database::models::OrderPayment& payment, std::string& errorMessage)
{
    if (!payment.isPaid())
    {
        errorMessage = "Only paid payments can be refunded";
        return false;
    }

    if (!payment.paymentDate.isNull())
    {
        const std::string& dateStr = payment.paymentDate.value();

        Poco::DateTime dt;
        int timeZoneDifferential = 0;

        bool parsed = 
            Poco::DateTimeParser::tryParse(Poco::DateTimeFormat::ISO8601_FORMAT, dateStr, dt, timeZoneDifferential) ||
            Poco::DateTimeParser::tryParse("%Y-%m-%d %H:%M:%S", dateStr, dt, timeZoneDifferential) ||
            Poco::DateTimeParser::tryParse("%Y-%m-%d", dateStr, dt, timeZoneDifferential);

        if (parsed)
        {
            Poco::Timestamp paymentTimestamp = dt.timestamp();
            Poco::Timestamp now;
            Poco::Timespan age = now - paymentTimestamp;

            if (age.days() > 90)
            {
                errorMessage = "Payments older than 90 days cannot be refunded";
                return false;
            }
        }
        else
        {
            errorMessage = "Unable to validate payment date for refund eligibility";
            return false;
        }
    }
    else
    {
        errorMessage = "Payment date is missing; refund not allowed";
        return false;
    }

    return true;
}

bool OrderPaymentService::canMarkAsPaid(const database::models::OrderPayment& payment, std::string& errorMessage)
{
    if (!payment.isPending())
    {
        errorMessage = "Only pending payments can be marked as paid";
        return false;
    }
    
    std::string orderError;
    if (!validateOrderForPayment(payment.orderId, orderError))
    {
        errorMessage = orderError;
        return false;
    }
    
    return true;
}

bool OrderPaymentService::canMarkAsFailed(const database::models::OrderPayment& payment, std::string& errorMessage)
{
    if (!payment.isPending())
    {
        errorMessage = "Only pending payments can be marked as failed";
        return false;
    }
    
    return true;
}

void OrderPaymentService::notifyPaymentCreated(const database::models::OrderPayment& payment)
{
    std::cout << "Notification: Payment created for order " << payment.orderId 
              << " amount: " << payment.amount << std::endl;
}

void OrderPaymentService::notifyPaymentStatusChanged(const database::models::OrderPayment& payment, 
                                                   const std::string& oldStatus, const std::string& newStatus)
{
    std::cout << "Notification: Payment " << payment.id 
              << " status changed from " << oldStatus 
              << " to " << newStatus << std::endl;
}

void OrderPaymentService::notifyPaymentFailed(const database::models::OrderPayment& payment, const std::string& reason)
{
    std::cout << "Notification: Payment " << payment.id 
              << " failed. Reason: " << reason << std::endl;
}

void OrderPaymentService::notifyPaymentRefunded(const database::models::OrderPayment& payment, const std::string& reason)
{
    std::cout << "Notification: Payment " << payment.id 
              << " refunded. Reason: " << reason << std::endl;
}

void OrderPaymentService::createAuditRecord(long long userId, const std::string& action, 
                                          long long paymentId, const std::string& details)
{
    std::string timestamp = DateUtils::formatDateTime(DateUtils::now());
    
    std::cout << "AUDIT: [" << timestamp << "] "
              << "UserID: " << userId << " "
              << "Action: " << action << " "
              << "PaymentID: " << paymentId << " "
              << "Details: " << details << std::endl;
}

} // namespace services
