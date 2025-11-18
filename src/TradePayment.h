#pragma once

#include "Trade.h"
#include "Macros.h"
#include <cmath>

namespace minirisk {

struct TradePayment : Trade<TradePayment>
{
    friend struct Trade<TradePayment>;

    static const guid_t m_id;
    static const std::string m_name;

    TradePayment() {}

    void init(const std::string& ccy, double quantity, const Date& delivery_date)
    {
        // Validate currency code
        MYASSERT(!ccy.empty(), "Currency code cannot be empty");
        MYASSERT(ccy.length() == 3, "Currency code must be 3 characters (ISO 4217 code), got: " << ccy);
        
        // Validate quantity
        MYASSERT(std::isfinite(quantity), "Quantity must be a finite number, got: " << quantity);
        
        // Validate delivery date (Date constructor already validates, but we can add explicit check)
        // The Date constructor will throw if invalid, so we just need to ensure it's not default-constructed
        
        Trade::init(quantity);
        m_ccy = ccy;
        m_delivery_date = delivery_date;
    }

    virtual ppricer_t pricer(const std::string& configuration) const;

    const string& ccy() const
    {
        return m_ccy;
    }

    const Date& delivery_date() const
    {
        return m_delivery_date;
    }

private:
    void save_details(my_ofstream& os) const
    {
        os << m_ccy << m_delivery_date;
    }

    void load_details(my_ifstream& is)
    {
        is >> m_ccy >> m_delivery_date;
    }

    void print_details(std::ostream& os) const
    {
        os << format_label("Currency") << m_ccy << std::endl;
        os << format_label("Delivery Date") << m_delivery_date << std::endl;
    }

private:
    string m_ccy;
    Date m_delivery_date;
};

} // namespace minirisk
