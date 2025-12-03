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
    // Получение текущей даты-времени
    static Poco::DateTime now();
    
    // Получение текущего timestamp
    static Poco::Timestamp currentTimestamp();
    
    // Форматирование даты в строку
    static std::string formatDate(const Poco::DateTime& date, 
                                   const std::string& format = "%Y-%m-%d");
    
    // Форматирование даты-времени в строку
    static std::string formatDateTime(const Poco::DateTime& datetime, 
                                       const std::string& format = "%Y-%m-%d %H:%M:%S");
    
    // Парсинг строки в дату
    static Poco::DateTime parseDate(const std::string& dateStr, 
                                     const std::string& format = "%Y-%m-%d");
    
    // Парсинг строки в дату-время
    static Poco::DateTime parseDateTime(const std::string& datetimeStr, 
                                         const std::string& format = "%Y-%m-%d %H:%M:%S");
    
    // Добавление дней к дате
    static Poco::DateTime addDays(const Poco::DateTime& date, int days);
    
    // Добавление месяцев к дате
    static Poco::DateTime addMonths(const Poco::DateTime& date, int months);
    
    // Добавление лет к дате
    static Poco::DateTime addYears(const Poco::DateTime& date, int years);
    
    // Разница между двумя датами в днях
    static int daysBetween(const Poco::DateTime& date1, const Poco::DateTime& date2);
    
    // Проверка, является ли год високосным
    static bool isLeapYear(int year);
    
    // Получение последнего дня месяца
    static int getLastDayOfMonth(int year, int month);
    
    // Проверка валидности даты
    static bool isValidDate(int year, int month, int day);
    
    // Конвертация в Unix timestamp
    static time_t toUnixTime(const Poco::DateTime& date);
    
    // Конвертация из Unix timestamp
    static Poco::DateTime fromUnixTime(time_t unixTime);
    
    // Получение начала дня (00:00:00)
    static Poco::DateTime startOfDay(const Poco::DateTime& date);
    
    // Получение конца дня (23:59:59)
    static Poco::DateTime endOfDay(const Poco::DateTime& date);
    
    // Получение начала месяца
    static Poco::DateTime startOfMonth(const Poco::DateTime& date);
    
    // Получение конца месяца
    static Poco::DateTime endOfMonth(const Poco::DateTime& date);
    
    // Получение начала года
    static Poco::DateTime startOfYear(const Poco::DateTime& date);
    
    // Получение конца года
    static Poco::DateTime endOfYear(const Poco::DateTime& date);
    
    // Проверка, находится ли дата в диапазоне
    static bool isInRange(const Poco::DateTime& date, 
                          const Poco::DateTime& start, 
                          const Poco::DateTime& end);
    
    // Получение дня недели (1-понедельник, 7-воскресенье)
    static int getDayOfWeek(const Poco::DateTime& date);
    
    // Получение названия дня недели
    static std::string getDayName(const Poco::DateTime& date, bool shortForm = false);
    
    // Получение названия месяца
    static std::string getMonthName(const Poco::DateTime& date, bool shortForm = false);
    
    // Проверка, является ли день рабочим (понедельник-пятница)
    static bool isWorkingDay(const Poco::DateTime& date);
    
    // Получение следующего рабочего дня
    static Poco::DateTime nextWorkingDay(const Poco::DateTime& date);
    
    // Расчет возраста на указанную дату
    static int calculateAge(const Poco::DateTime& birthDate, 
                            const Poco::DateTime& onDate);
    
    // Форматирование интервала времени
    static std::string formatDuration(const Poco::Timespan& duration);
    
    // Получение текущего квартала
    static int getCurrentQuarter(const Poco::DateTime& date);
    
    // Получение начала квартала
    static Poco::DateTime startOfQuarter(const Poco::DateTime& date);
    
    // Получение конца квартала
    static Poco::DateTime endOfQuarter(const Poco::DateTime& date);
};

} // namespace utils
