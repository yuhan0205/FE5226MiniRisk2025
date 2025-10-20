#pragma once

#include "Macros.h"
#include <string>
#include <array>

namespace minirisk {

struct Date {
public:
    
    static constexpr unsigned FIRST_YEAR = 1900;
    static constexpr unsigned LAST_YEAR = 2200;
    static constexpr unsigned N_YEARS = LAST_YEAR - FIRST_YEAR;
    static constexpr unsigned DEFAULT_SERIAL = 25567;

    Date() : m_serial(DEFAULT_SERIAL) {}
    Date(unsigned serial) : m_serial(serial) {}
    Date(unsigned year, unsigned month, unsigned day);

    void init(unsigned year, unsigned month, unsigned day);
    unsigned serial() const { return m_serial; }
    std::string to_string(bool pretty = true) const;
    void serial_to_calendar(unsigned& year, unsigned& month, unsigned& day) const;
    static unsigned calendar_to_serial(unsigned year, unsigned month, unsigned day);

    
    bool operator<(const Date& other) const { return m_serial < other.m_serial; }
    bool operator<=(const Date& other) const { return m_serial <= other.m_serial; }
    bool operator>(const Date& other) const { return m_serial > other.m_serial; }
    bool operator>=(const Date& other) const { return m_serial >= other.m_serial; }
    bool operator==(const Date& other) const { return m_serial == other.m_serial; }
    bool operator!=(const Date& other) const { return m_serial != other.m_serial; }

    static bool is_leap_year(unsigned year);
    static void check_valid(unsigned year, unsigned month, unsigned day);

private:
    
    unsigned m_serial;  ///< Number of days since January 1, 1900
    
    static const std::array<unsigned, 12> DAYS_IN_MONTH;  ///< Days in each month (normal year)
    static const std::array<unsigned, 12> DAYS_YTD;       ///< Cumulative days to start of each month
    static const std::array<unsigned, N_YEARS> DAYS_EPOCH; ///< Cumulative days to start of each year
    
    static std::string padding_dates(unsigned value);
    
    friend long operator-(const Date& d1, const Date& d2);
};


long operator-(const Date& d1, const Date& d2);

inline double time_frac(const Date& d1, const Date& d2) {
    return static_cast<double>(d2 - d1) / 365.0;
}
}