#pragma once

#include <string>
#include <chrono>
#include <Poco/DateTime.h>
#include <Poco/Timestamp.h>
#include <Poco/Timespan.h>

namespace warehouse_backend::utils
{

class DateUtils
{
public:
    static Poco::DateTime now();
    
    static Poco::Timestamp currentTimestamp();
    
    static std::string formatDate(const Poco::DateTime& date, 
                                   const std::string& format = "%Y-%m-%d");
    
    static std::string formatDateTime(const Poco::DateTime& datetime, 
                                       const std::string& format = "%Y-%m-%d %H:%M:%S");
    
    static Poco::DateTime parseDate(const std::string& dateStr, 
                                     const std::string& format = "%Y-%m-%d");
    
    static Poco::DateTime parseDateTime(const std::string& datetimeStr, 
                                         const std::string& format = "%Y-%m-%d %H:%M:%S");
    
    static Poco::DateTime addDays(const Poco::DateTime& date, int days);
    
    static Poco::DateTime addMonths(const Poco::DateTime& date, int months);
    
    static Poco::DateTime addYears(const Poco::DateTime& date, int years);
    
    static int daysBetween(const Poco::DateTime& date1, const Poco::DateTime& date2);
    
    static bool isLeapYear(int year);
    
    static int getLastDayOfMonth(int year, int month);
    
    static bool isValidDate(int year, int month, int day);
    
    static time_t toUnixTime(const Poco::DateTime& date);
    
    static Poco::DateTime fromUnixTime(time_t unixTime);
    
    static Poco::DateTime startOfDay(const Poco::DateTime& date);
    
    static Poco::DateTime endOfDay(const Poco::DateTime& date);
    
    static Poco::DateTime startOfMonth(const Poco::DateTime& date);
    
    static Poco::DateTime endOfMonth(const Poco::DateTime& date);
    
    static Poco::DateTime startOfYear(const Poco::DateTime& date);
    
    static Poco::DateTime endOfYear(const Poco::DateTime& date);
    
    static bool isInRange(const Poco::DateTime& date, 
                          const Poco::DateTime& start, 
                          const Poco::DateTime& end);
    
    static int getDayOfWeek(const Poco::DateTime& date);
    
    static std::string getDayName(const Poco::DateTime& date, bool shortForm = false);
    
    static std::string getMonthName(const Poco::DateTime& date, bool shortForm = false);
    
    static bool isWorkingDay(const Poco::DateTime& date);
    
    static Poco::DateTime nextWorkingDay(const Poco::DateTime& date);
    
    static int calculateAge(const Poco::DateTime& birthDate, 
                            const Poco::DateTime& onDate);
    
    static std::string formatDuration(const Poco::Timespan& duration);
    
    static int getCurrentQuarter(const Poco::DateTime& date);
    
    static Poco::DateTime startOfQuarter(const Poco::DateTime& date);
    
    static Poco::DateTime endOfQuarter(const Poco::DateTime& date);
};

} // namespace utils
