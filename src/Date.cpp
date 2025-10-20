#include "Date.h"
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace minirisk {

struct DateInitializer : std::array<unsigned, Date::N_YEARS> {
    DateInitializer() {
        for (unsigned i = 0, cumulative_days = 0, year = Date::FIRST_YEAR; 
             i < size(); ++i, ++year) {
            (*this)[i] = cumulative_days;
            cumulative_days += 365 + (Date::is_leap_year(year) ? 1 : 0);
        }
    }
};

const std::array<unsigned, 12> Date::DAYS_IN_MONTH = {
    {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}
};

const std::array<unsigned, 12> Date::DAYS_YTD = {
    {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334}
};

const std::array<unsigned, Date::N_YEARS> Date::DAYS_EPOCH(
    static_cast<const std::array<unsigned, Date::N_YEARS>&>(DateInitializer())
);


Date::Date(unsigned year, unsigned month, unsigned day) {
    init(year, month, day);
}

void Date::init(unsigned year, unsigned month, unsigned day) {
    check_valid(year, month, day);
    m_serial = calendar_to_serial(year, month, day);
}

std::string Date::to_string(bool pretty) const {
    unsigned year, month, day;
    serial_to_calendar(year, month, day);
    
    if (pretty) {
        return std::to_string(day) + "-" + 
               std::to_string(month) + "-" + 
               std::to_string(year);
    } else {
        return std::to_string(year) + 
               padding_dates(month) + 
               padding_dates(day);
    }
}

void Date::serial_to_calendar(unsigned& year, unsigned& month, unsigned& day) const {

    unsigned year_index = 0;
    for (unsigned i = 0; i < N_YEARS; ++i) {
        if (DAYS_EPOCH[i] > m_serial) {
            year_index = i - 1;
            break;
        }
        year_index = i;
    }
    
    year = FIRST_YEAR + year_index;
    
    unsigned remaining_days = m_serial - DAYS_EPOCH[year_index];
    
    month = 1;
    for (unsigned m = 0; m < 12; ++m) {
        unsigned days_in_month = DAYS_IN_MONTH[m];
        if (m == 1 && is_leap_year(year)) {
            days_in_month = 29;
        }
        
        if (remaining_days < days_in_month) {
            month = m + 1;
            break;
        }
        remaining_days -= days_in_month;
    }
    
    day = remaining_days + 1;
}

unsigned Date::calendar_to_serial(unsigned year, unsigned month, unsigned day) {
    return DAYS_EPOCH[year - FIRST_YEAR] + 
           DAYS_YTD[month - 1] + 
           ((month > 2 && is_leap_year(year)) ? 1 : 0) + 
           (day - 1);
}

bool Date::is_leap_year(unsigned year) {
    return (year % 4 == 0) && ((year % 100 != 0) || (year % 400 == 0));
}

void Date::check_valid(unsigned year, unsigned month, unsigned day) {

    if (year < FIRST_YEAR || year >= LAST_YEAR) {
        throw std::invalid_argument(
            "Year must be between " + std::to_string(FIRST_YEAR) + 
            " and " + std::to_string(LAST_YEAR - 1) + 
            ", got " + std::to_string(year)
        );
    }
    
    if (month < 1 || month > 12) {
        throw std::invalid_argument(
            "Month must be between 1 and 12, got " + std::to_string(month)
        );
    }
    
    unsigned max_days = DAYS_IN_MONTH[month - 1];
    if (month == 2 && is_leap_year(year)) {
        max_days = 29;
    }
    
    if (day < 1 || day > max_days) {
        throw std::invalid_argument(
            "Day must be between 1 and " + std::to_string(max_days) + 
            " for month " + std::to_string(month) + 
            " in year " + std::to_string(year) + 
            ", got " + std::to_string(day)
        );
    }
}


std::string Date::padding_dates(unsigned value) {
    std::ostringstream os;
    os << std::setw(2) << std::setfill('0') << value;
    return os.str();
}

long operator-(const Date& d1, const Date& d2) {
    return static_cast<long>(d1.m_serial) - static_cast<long>(d2.m_serial);
}

}