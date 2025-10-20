#pragma once

#include "IPricer.h"
#include "TradeFXForward.h"

namespace minirisk {

struct PricerFXForward : IPricer
{
    PricerFXForward(const TradeFXForward& trd, const std::string& base_ccy);

    virtual double price(Market& m, const FixingDataServer* fds = nullptr) const;

private:
    double m_notional;
    std::string m_ccy1;
    std::string m_ccy2;
    double m_strike;
    Date m_fixing_date;
    Date m_settle_date;
    std::string m_base_ccy;
    std::string m_fx_pair; // from ccy2 to base ccy, empty if already base
};

} // namespace minirisk
