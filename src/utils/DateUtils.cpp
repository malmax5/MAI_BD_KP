#include "DateUtils.hpp"
#include <Poco/DateTimeFormat.h>
#include <Poco/DateTimeFormatter.h>
#include <Poco/DateTimeParser.h>
#include "Poco/Exception.h"
#include <Poco/Timespan.h>
#include <Poco/LocalDateTime.h>
#include <Poco/Timezone.h>
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace warehouse_backend::utils
{

Poco::DateTime DateUtils::now()
{
    return Poco::DateTime();
}

Poco::Timestamp DateUtils::currentTimestamp()
{
    return Poco::Timestamp();
}

std::string DateUtils::formatDate(const Poco::DateTime& date, 
                                   const std::string& format)
{
    try
    {
        return Poco::DateTimeFormatter::format(date, format);
    }
    catch (...)
    {
        return Poco::DateTimeFormatter::format(date, "%Y-%m-%d");
    }
}

std::string DateUtils::formatDateTime(const Poco::DateTime& datetime, 
                                       const std::string& format)
{
    try
    {
        return Poco::DateTimeFormatter::format(datetime, format);
    }
    catch (...)
    {
        return Poco::DateTimeFormatter::format(datetime, "%Y-%m-%d %H:%M:%S");
    }
}

Poco::DateTime DateUtils::parseDate(const std::string& dateStr, 
                                     const std::string& format)
{
    Poco::DateTime result;
    int timeZoneDifferential = 0;
    
    try
    {
        if (!Poco::DateTimeParser::tryParse(format, dateStr, result, timeZoneDifferential))
        {
            throw Poco::SyntaxException("Invalid date format");
        }
    }
    catch (...)
    {
        if (!Poco::DateTimeParser::tryParse(Poco::DateTimeFormat::ISO8601_FORMAT, dateStr, result, timeZoneDifferential))
        {
            result = Poco::DateTime(1900, 1, 1);
        }
    }
    
    return result;
}

Poco::DateTime DateUtils::parseDateTime(const std::string& datetimeStr, 
                                         const std::string& format)
{
    Poco::DateTime result;
    int timeZoneDifferential = 0;
    
    try
    {
        if (!Poco::DateTimeParser::tryParse(format, datetimeStr, result, timeZoneDifferential))
        {
            throw Poco::SyntaxException("Invalid datetime format");
        }
    }
    catch (...) 
    {
        if (!Poco::DateTimeParser::tryParse(Poco::DateTimeFormat::ISO8601_FORMAT, datetimeStr, result, timeZoneDifferential))
        {
            result = now();
        }
    }
    
    return result;
}

Poco::DateTime DateUtils::addDays(const Poco::DateTime& date, int days)
{
    Poco::DateTime result(date);
    result += Poco::Timespan(days, 0, 0, 0, 0);

    return result;
}

Poco::DateTime DateUtils::addMonths(const Poco::DateTime& date, int months)
{
    Poco::DateTime result(date);
    
    int year = result.year();
    int month = result.month() + months;
    int day = result.day();
    
    while (month > 12)
    {
        month -= 12;
        year++;
    }
    
    while (month < 1)
    {
        month += 12;
        year--;
    }
    
    int lastDayOfMonth = getLastDayOfMonth(year, month);
    if (day > lastDayOfMonth)
    {
        day = lastDayOfMonth;
    }
    
    result.assign(year, month, day, result.hour(), result.minute(), result.second());

    return result;
}

Poco::DateTime DateUtils::addYears(const Poco::DateTime& date, int years)
{
    Poco::DateTime result(date);
    result.assign(result.year() + years, result.month(), result.day(),
                  result.hour(), result.minute(), result.second());
    
    if (result.month() == 2 && result.day() == 29 && !isLeapYear(result.year()))
    {
        result.assign(result.year(), 2, 28,
                      result.hour(), result.minute(), result.second());
    }
    
    return result;
}

int DateUtils::daysBetween(const Poco::DateTime& date1, const Poco::DateTime& date2)
{
    Poco::DateTime start = date1 < date2 ? date1 : date2;
    Poco::DateTime end = date1 < date2 ? date2 : date1;
    
    Poco::DateTime startDate(start.year(), start.month(), start.day());
    Poco::DateTime endDate(end.year(), end.month(), end.day());
    
    Poco::Timespan diff = endDate - startDate;
    return static_cast<int>(diff.days());
}

bool DateUtils::isLeapYear(int year)
{
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

int DateUtils::getLastDayOfMonth(int year, int month)
{
    static const int daysInMonth[] = {31, 28, 31, 30, 31, 30, 
                                      31, 31, 30, 31, 30, 31};
    
    if (month == 2 && isLeapYear(year))
    {
        return 29;
    }
    
    if (month >= 1 && month <= 12)
    {
        return daysInMonth[month - 1];
    }
    
    return 0;
}

bool DateUtils::isValidDate(int year, int month, int day)
{
    if (year < 1900 || year > 2100) return false;
    if (month < 1 || month > 12) return false;
    if (day < 1 || day > getLastDayOfMonth(year, month)) return false;

    return true;
}

time_t DateUtils::toUnixTime(const Poco::DateTime& date) 
{
    Poco::DateTime epoch(1970, 1, 1);
    Poco::Timespan diff = date - epoch;

    return diff.totalSeconds();
}

Poco::DateTime DateUtils::fromUnixTime(time_t unixTime)
{
    Poco::DateTime epoch(1970, 1, 1);

    return epoch + Poco::Timespan(unixTime, 0);
}

Poco::DateTime DateUtils::startOfDay(const Poco::DateTime& date)
{
    return Poco::DateTime(date.year(), date.month(), date.day(), 0, 0, 0);
}

Poco::DateTime DateUtils::endOfDay(const Poco::DateTime& date)
{
    return Poco::DateTime(date.year(), date.month(), date.day(), 23, 59, 59, 999999);
}

Poco::DateTime DateUtils::startOfMonth(const Poco::DateTime& date)
{
    return Poco::DateTime(date.year(), date.month(), 1, 0, 0, 0);
}

Poco::DateTime DateUtils::endOfMonth(const Poco::DateTime& date)
{
    int lastDay = getLastDayOfMonth(date.year(), date.month());
    return Poco::DateTime(date.year(), date.month(), lastDay, 23, 59, 59, 999999);
}

Poco::DateTime DateUtils::startOfYear(const Poco::DateTime& date)
{
    return Poco::DateTime(date.year(), 1, 1, 0, 0, 0);
}

Poco::DateTime DateUtils::endOfYear(const Poco::DateTime& date)
{
    return Poco::DateTime(date.year(), 12, 31, 23, 59, 59, 999999);
}

bool DateUtils::isInRange(const Poco::DateTime& date, 
                          const Poco::DateTime& start, 
                          const Poco::DateTime& end)
{
    return date >= start && date <= end;
}

int DateUtils::getDayOfWeek(const Poco::DateTime& date)
{
    int pocoDayOfWeek = date.dayOfWeek();

    return pocoDayOfWeek == 0 ? 7 : pocoDayOfWeek;
}

std::string DateUtils::getDayName(const Poco::DateTime& date, bool shortForm)
{
    static const std::string fullNames[] = {
        "Sunday", "Monday", "Tuesday", "Wednesday", 
        "Thursday", "Friday", "Saturday"
    };
    
    static const std::string shortNames[] = {
        "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
    };
    
    int dayOfWeek = date.dayOfWeek();
    
    if (dayOfWeek >= 0 && dayOfWeek <= 6)
    {
        return shortForm ? shortNames[dayOfWeek] : fullNames[dayOfWeek];
    }
    
    return "Unknown";
}

std::string DateUtils::getMonthName(const Poco::DateTime& date, bool shortForm)
{
    static const std::string fullNames[] = {
        "January", "February", "March", "April", "May", "June",
        "July", "August", "September", "October", "November", "December"
    };
    
    static const std::string shortNames[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };
    
    int month = date.month();
    
    if (month >= 1 && month <= 12)
    {
        return shortForm ? shortNames[month - 1] : fullNames[month - 1];
    }
    
    return "Unknown";
}

bool DateUtils::isWorkingDay(const Poco::DateTime& date)
{
    int dayOfWeek = getDayOfWeek(date);
    return dayOfWeek >= 1 && dayOfWeek <= 5;
}

Poco::DateTime DateUtils::nextWorkingDay(const Poco::DateTime& date)
{
    Poco::DateTime result = date;
    
    do
    {
        result = addDays(result, 1);
    } while (!isWorkingDay(result));
    
    return result;
}

int DateUtils::calculateAge(const Poco::DateTime& birthDate, 
                            const Poco::DateTime& onDate)
{
    int age = onDate.year() - birthDate.year();
    
    if (onDate.month() < birthDate.month() || 
        (onDate.month() == birthDate.month() && onDate.day() < birthDate.day()))
    {
        age--;
    }
    
    return age >= 0 ? age : 0;
}

std::string DateUtils::formatDuration(const Poco::Timespan& duration)
{
    std::ostringstream oss;
    
    int days = duration.days();
    int hours = duration.hours();
    int minutes = duration.minutes();
    int seconds = duration.seconds();
    
    if (days > 0)
    {
        oss << days << "d ";
    }
    
    if (hours > 0 || days > 0)
    {
        oss << std::setw(2) << std::setfill('0') << hours << "h ";
    }
    
    oss << std::setw(2) << std::setfill('0') << minutes << "m "
        << std::setw(2) << std::setfill('0') << seconds << "s";
    
    return oss.str();
}

int DateUtils::getCurrentQuarter(const Poco::DateTime& date)
{
    int month = date.month();
    
    if (month >= 1 && month <= 3) return 1;
    if (month >= 4 && month <= 6) return 2;
    if (month >= 7 && month <= 9) return 3;
    return 4;
}

Poco::DateTime DateUtils::startOfQuarter(const Poco::DateTime& date)
{
    int quarter = getCurrentQuarter(date);
    int startMonth = (quarter - 1) * 3 + 1;
    
    return Poco::DateTime(date.year(), startMonth, 1, 0, 0, 0);
}

Poco::DateTime DateUtils::endOfQuarter(const Poco::DateTime& date)
{
    int quarter = getCurrentQuarter(date);
    int endMonth = quarter * 3;
    int endDay = getLastDayOfMonth(date.year(), endMonth);
    
    return Poco::DateTime(date.year(), endMonth, endDay, 23, 59, 59, 999999);
}

} // namespace utils
