#pragma once

#include "Trade.h"
#include "Macros.h"
#include <cmath>

namespace minirisk {

struct TradeFXForward : Trade<TradeFXForward>
{
    friend struct Trade<TradeFXForward>;

    static const guid_t m_id;
    static const std::string m_name;

    TradeFXForward() {}

    void init(const std::string& ccy1, const std::string& ccy2, double notional, double strike, 
              const Date& fixing_date, const Date& settle_date)
    {
        // Validate currency codes
        MYASSERT(!ccy1.empty(), "Base currency code cannot be empty");
        MYASSERT(ccy1.length() == 3, "Base currency code must be 3 characters (ISO 4217 code), got: " << ccy1);
        MYASSERT(!ccy2.empty(), "Quote currency code cannot be empty");
        MYASSERT(ccy2.length() == 3, "Quote currency code must be 3 characters (ISO 4217 code), got: " << ccy2);
        MYASSERT(ccy1 != ccy2, "Base and quote currencies must be different, got: " << ccy1 << "/" << ccy2);
        
        // Validate notional
        MYASSERT(std::isfinite(notional), "Notional must be a finite number, got: " << notional);
        MYASSERT(notional != 0.0, "Notional cannot be zero");
        
        // Validate strike
        MYASSERT(std::isfinite(strike), "Strike must be a finite number, got: " << strike);
        MYASSERT(strike > 0.0, "Strike must be positive, got: " << strike);
        
        // Validate dates (Date constructor already validates, but ensure fixing_date <= settle_date)
        MYASSERT(fixing_date <= settle_date, "Fixing date must be less than or equal to settlement date");
        
        Trade::init(notional);
        m_ccy1 = ccy1;
        m_ccy2 = ccy2;
        m_strike = strike;
        m_fixing_date = fixing_date;
        m_settle_date = settle_date;
    }

    virtual ppricer_t pricer(const std::string& configuration) const;

    const std::string& ccy1() const
    {
        return m_ccy1;
    }

    const std::string& ccy2() const
    {
        return m_ccy2;
    }

    double strike() const
    {
        return m_strike;
    }

    const Date& fixing_date() const
    {
        return m_fixing_date;
    }

    const Date& settle_date() const
    {
        return m_settle_date;
    }

private:
    void save_details(my_ofstream& os) const
    {
        os << m_ccy1 << m_ccy2 << m_strike << m_fixing_date << m_settle_date;
    }

    void load_details(my_ifstream& is)
    {
        is >> m_ccy1 >> m_ccy2 >> m_strike >> m_fixing_date >> m_settle_date;
    }

    void print_details(std::ostream& os) const
    {
        os << format_label("Strike level") << m_strike << std::endl;
        os << format_label("Base Currency") << m_ccy1 << std::endl;
        os << format_label("Quote Currency") << m_ccy2 << std::endl;
        os << format_label("Fixing Date") << m_fixing_date << std::endl;
        os << format_label("Settlement Date") << m_settle_date << std::endl;
    }

private:
    std::string m_ccy1;
    std::string m_ccy2;
    double m_strike;
    Date m_fixing_date;
    Date m_settle_date;
};

} // namespace minirisk
